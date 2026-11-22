// The ground battle's replay, sound and camera cores played through a whole recorded battle at 60 frames a second,
// with no engine (Battle/LandFeed, LandPlay, LandSound, LandEye, LandDesk): what the game plays, counted.
//
//   land_play_test <recording.json> <mix.json> [out folder]
//
// It checks: every round fired once and landed once; every 'shell' event one blast (paired with its kill round where
// the rules fired one); every shot heard has a take, none on the screen dropped; the same take never twice within
// 0.15 s; the eye never under the ground, never cutting faster than every 3 s, and a lens near the ground never down
// among standing corn or tall grass (a stand-in for the field's corn: the rules' 'field' cells a metre high, 'rough'
// 0.8 m); the bar starts level and ends on the winner's side. It lays a shell hole for every shell that lands in the
// ground (LandCrater) and checks them: torn earth, brown, a rim over the ground, each round's size, the cost. It reports
// how many of the falls the camera saw, and the cores' time a frame.
//
// The camera runs over the field the game lays (LandField, the same ground, hedges, trees and houses) with the men and
// vehicles placed from the recording (LandSight: a man a rod, a tank a hull and turret box with a barrel). It checks the
// art director's rules on the first film: a quarter of a second at most with the lens within 1.5 m of a thing or 0.9 m
// of a man, or the picture over 30 per cent filled near the lens, before the shot is left; no aerial in the middle of
// the battle; a fall gone to is in the picture; and it counts why each shot was left.

#include "Battle/LandCrater.h"
#include "Battle/LandDesk.h"
#include "Battle/LandEye.h"
#include "Battle/LandFeed.h"
#include "Battle/LandField.h"
#include "Battle/LandGeneralEye.h"
#include "Battle/LandJson.h"
#include "Battle/LandPlay.h"
#include "Battle/LandSight.h"
#include "Battle/LandSound.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace
{
	using LandPlay::Vec;

	int Faults = 0;

	void Fault(const char* What, double T = -1.0)
	{
		if (Faults < 30) std::printf("  FAULT%s%s\n", T >= 0.0 ? (" at " + std::to_string(T) + ": ").c_str() : ": ", What);
		++Faults;
	}

	// (whether a point is in the camera's picture: LandEye::InPicture, 16:9, the pose's vertical lens)

	/** The camera director's rules from the second cut's frame check (21 November), measured on a run of the camera: a
	 * shell landing at the lens cut on its flash, its own man never in the lens or filling the picture or lost from it,
	 * no man walking into the lens, no leaves or trunk at the lens, no kick as a pan starts, every tank knocked out in
	 * the picture. Fed every frame; asks the director's own measures every quarter second of real time. */
	struct FilmMeasure
	{
		const LandFeed::Recording* Rec = nullptr;
		LandEye::Settings Set;
		int Frames = 0, Looks = 0;
		int Bursts = 0, BurstsKept = 0;
		double BurstDue = -1.0;
		int OwnFrames = 0, OwnNear = 0, OwnFilled = 0, OwnLooks = 0;
		int Crossed = 0, Edged = 0;
		double LostSince = -1.0, LostLongest = 0.0, LostAt = 0.0;
		double Next = 0.0;
		// (the look's way: yaw and pitch, degrees; its swing a frame ago, degrees a second)
		bool bWay = false, bSwing = false;
		double Yaw = 0.0, Pitch = 0.0, SwingYaw = 0.0, SwingPitch = 0.0, Kick = 0.0, KickAt = 0.0, EyeZWas = 0.0, LookZWas = 0.0;
		double PanKick = 0.0, PanKickAt = 0.0;
		Vec EyeWas, LookWas;
		std::vector<double> Kicks;
		int Knocked = 0, KnockedSeen = 0;
		// (Brian's war-film feel: in the fight, how much of the film is at a soldier's eye level, and on the film's own
		// section)
		int FightFrames = 0, EyeLevel = 0, OnSquad = 0;
		// (the lens inside a burst's cloud in its first seconds: the art director's 4 s by the round's size, never held
		// past half a second of the screen's clock)
		int CloudFrames = 0;
		double CloudSince = -1.0, CloudLongest = 0.0, CloudAt = 0.0;
		// (and within a vehicle's fuel ball in its first 2 s on the screen: the art director's 16 m, cut on the flash)
		// (measured here from the recording, not asked of the director: the tanks and lorries knocked out, by when)
		int FuelFrames = 0;
		double FuelSince = -1.0, FuelLongest = 0.0, FuelAt = 0.0, FuelNearest = 1e18, FuelNearestAt = 0.0;
		std::vector<int> Burnt;
		// (the whole mass, the production lead's CD1: one wide shot in its window holding 80% of the men drawn in its
		// picture, every frame of it; every man on the field the player can see, the batteries far back and the fallen too)
		int MassShots = 0, MassMen = 0;
		bool bMassGate = true;
		double MassFrom = 0.0, MassTo = 0.0, MassStart = -1.0, MassReal = 0.0, MassLeast = 1.0, MassLeastAt = 0.0, MassHeight = 0.0, MassFar = 0.0;

		void Feed(const LandEye::Director& D, const LandEye::Pose& P, double From, double B, double R, double Dt)
		{
			++Frames;
			const bool bAir = P.Shot == LandEye::Kind::Open || P.Shot == LandEye::Kind::Wide;
			if (P.bCut) { BurstDue = -1.0; LostSince = -1.0; }
			if (Frames == 1)
			{
				D.MassWindow(MassFrom, MassTo);
				for (size_t I = 0; I < Rec->Tracks.size(); ++I)
				{
					const LandFeed::Track& Tr = Rec->Tracks[I];
					if (!Tr.bTwin && Tr.Fell >= 0.0 && (Tr.Role == "tank" || Tr.Role == "lorry")) Burnt.push_back(static_cast<int>(I));
				}
			}
			if (D.MassShot())
			{
				if (P.bCut)
				{
					++MassShots;
					MassStart = B;
					MassHeight = P.Eye.Z - Rec->Ground.HeightAt(P.Eye.X, P.Eye.Y);
					MassFar = std::hypot(P.Look.X - P.Eye.X, P.Look.Y - P.Eye.Y);
				}
				MassReal += Dt;
				int Men = 0, In = 0;
				for (size_t I = 0; I < Rec->Tracks.size(); ++I)
				{
					const LandFeed::Track& Tr = Rec->Tracks[I];
					if (Tr.bTwin || !Tr.IsMan() || Tr.Start() > B || (Tr.Out >= 0.0 && Tr.Out < B) || D.HiddenNow(static_cast<int>(I))) continue;
					const LandFeed::Spot S = LandFeed::SpotAt(Tr, B);
					++Men;
					if (LandEye::InPicture(P, Vec(S.X, S.Y, Rec->Ground.HeightAt(S.X, S.Y) + 0.9))) ++In;
				}
				const double Share = static_cast<double>(In) / std::max(1, Men);
				if (Share < MassLeast) { MassLeast = Share; MassLeastAt = B; MassMen = Men; }
			}
			// A shell landing at the lens this frame: the shot must be cut on its flash.
			// (on the screen's clock: the scene plays the fight at half the battle's)
			if (BurstDue >= 0.0 && R > BurstDue) { ++BurstsKept; BurstDue = -1.0; }
			if (!bAir && D.BurstAt(P, From, B)) { ++Bursts; if (BurstDue < 0.0) BurstDue = R + Set.BurstFlash + 0.15; }
			if (!bAir && D.InCloud(P.Eye, B))
			{
				++CloudFrames;
				if (CloudSince < 0.0) CloudSince = R;
				if (R - CloudSince > CloudLongest) { CloudLongest = R - CloudSince; CloudAt = B; }
			}
			else CloudSince = -1.0;
			{
				// (the nearest the lens is to a ball in its first seconds on the screen; the fight's pace, battle seconds a
				// screen second, this frame)
				const double PaceNow = (B - From) / std::max(1e-9, Dt);
				double Nearest = 1e18;
				for (const int I : Burnt)
				{
					const LandFeed::Track& Tr = Rec->Tracks[static_cast<size_t>(I)];
					if (Tr.Fell > B || Tr.Fell < B - Set.FuelFor * PaceNow) continue;
					const LandFeed::Spot S = LandFeed::SpotAt(Tr, Tr.Fell);
					Nearest = std::min(Nearest, (Vec(S.X, S.Y, Rec->Ground.HeightAt(S.X, S.Y) + 2.0) - P.Eye).Length());
				}
				if (!bAir && Nearest < FuelNearest) { FuelNearest = Nearest; FuelNearestAt = B; }
				if (!bAir && Nearest < Set.FuelVehicle)
				{
					++FuelFrames;
					if (FuelSince < 0.0 || P.bCut) FuelSince = R;
					if (R + Dt - FuelSince > FuelLongest) { FuelLongest = R + Dt - FuelSince; FuelAt = B; }
				}
				else FuelSince = -1.0;
			}
			// Its own man (a man's shot, he not fallen): never within the lens's clearance of his head or chest.
			const bool bMan = P.Subject >= 0 && !Rec->Tracks[static_cast<size_t>(P.Subject)].IsVehicle();
			const LandFeed::Track* Own = P.Subject >= 0 ? &Rec->Tracks[static_cast<size_t>(P.Subject)] : nullptr;
			const bool bUp = Own && !(Own->Fell >= 0.0 && Own->Fell <= B);
			if (bMan && bUp && (P.Shot == LandEye::Kind::Shoulder || P.Shot == LandEye::Kind::Low))
			{
				++OwnFrames;
				if (D.OwnGap(P, B) < Set.OwnClear * 0.8)
				{
					++OwnNear;
					if (std::getenv("LAND_PLAY_OWN")) std::printf("  own man near at %.2f: %s on %d, %.2f m\n", B, LandEye::NameOf(P.Shot), P.Subject, D.OwnGap(P, B));
				}
			}
			// The pan's kick: the change in the look's swing from one frame to the next (not across a cut).
			{
				const Vec W = P.Look - P.Eye;
				const double Y = std::atan2(W.X, W.Y) * 180.0 / 3.14159265358979;
				const double Pt = std::atan2(W.Z, std::hypot(W.X, W.Y)) * 180.0 / 3.14159265358979;
				if (P.bCut) { bWay = false; bSwing = false; }
				if (bWay)
				{
					double Dy = Y - Yaw;
					while (Dy > 180.0) Dy -= 360.0;
					while (Dy < -180.0) Dy += 360.0;
					const double Sy = Dy / Dt, Sp = (Pt - Pitch) / Dt;
					if (bSwing)
					{
						const double K = std::hypot(Sy - SwingYaw, Sp - SwingPitch) / Dt;
						const double Pan = std::abs(Sy - SwingYaw) / Dt;
						Kicks.push_back(K);
						if (K > Kick) { Kick = K; KickAt = B; }
						if (Pan > PanKick) { PanKick = Pan; PanKickAt = B; }
						if (K > 500.0 && std::getenv("LAND_PLAY_KICKS")) std::printf("  kick %.0f at %.2f: %s on %d, yaw swing %.1f -> %.1f, pitch %.1f -> %.1f, eye z %+.3f look z %+.3f, eye %.2f over the ground, eye moved %.3f look moved %.3f, look %.1f m off\n", K, B, LandEye::NameOf(P.Shot), P.Subject, SwingYaw, Sy, SwingPitch, Sp, P.Eye.Z - EyeZWas, P.Look.Z - LookZWas, P.Eye.Z - Rec->Ground.HeightAt(P.Eye.X, P.Eye.Y),
							std::hypot(P.Eye.X - EyeWas.X, P.Eye.Y - EyeWas.Y), std::hypot(P.Look.X - LookWas.X, P.Look.Y - LookWas.Y), (P.Look - P.Eye).Length());
					}
					SwingYaw = Sy;
					SwingPitch = Sp;
					bSwing = true;
				}
				Yaw = Y;
				Pitch = Pt;
				bWay = true;
				EyeZWas = P.Eye.Z;
				LookZWas = P.Look.Z;
				EyeWas = P.Eye;
				LookWas = P.Look;
			}
			if (Rec->Contact >= 0.0 && B >= Rec->Contact && B <= Rec->End)
			{
				++FightFrames;
				if (!bAir && P.Eye.Z - Rec->Ground.HeightAt(P.Eye.X, P.Eye.Y) <= 2.2) ++EyeLevel;
				if (D.InSquad(P.Subject)) ++OnSquad;
			}
			// Every tank knocked out: in the picture as she goes.
			for (const LandFeed::Track& Tr : Rec->Tracks)
			{
				if (!Tr.IsVehicle() || Tr.bTwin || !(Tr.Fell > From && Tr.Fell <= B)) continue;
				++Knocked;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, Tr.Fell);
				const Vec At(S.X, S.Y, Rec->Ground.HeightAt(S.X, S.Y) + 1.3);
				if (LandEye::InPicture(P, At) && (At - P.Eye).Length() < (P.Fov < 32.0 ? 400.0 : 200.0)) ++KnockedSeen;
				else if (std::getenv("LAND_PLAY_KO"))
					std::printf("  knock-out missed at %.2f (track %d): %s on %d, %.0f m off, held %s\n", Tr.Fell, static_cast<int>(&Tr - &Rec->Tracks[0]),
						LandEye::NameOf(P.Shot), P.Subject, (At - P.Eye).Length(), LandEye::NameOf(D.LastWhy()));
				if (!(LandEye::InPicture(P, At) && (At - P.Eye).Length() < (P.Fov < 32.0 ? 400.0 : 200.0)) && std::getenv("LAND_PLAY_KO"))
					for (const auto& C : D.Log())
						if (C.first > Tr.Fell - 8.0 && C.first <= Tr.Fell) std::printf("    cut at %.2f to %s\n", C.first, LandEye::NameOf(C.second));
			}
			if (bAir || R < Next) return;
			Next = R + 0.25;
			++Looks;
			if (bMan && bUp && (P.Shot == LandEye::Kind::Shoulder || P.Shot == LandEye::Kind::Low))
			{
				++OwnLooks;
				if (D.OwnFill(P) > Set.OwnMost) ++OwnFilled;
			}
			if (D.MenCross(P, B, B)) ++Crossed;
			if (D.EdgeFill(P) > Set.EdgeMost) ++Edged;
			if (bMan && bUp && P.Shot != LandEye::Kind::Tank && P.Shot != LandEye::Kind::Track && D.Lost(B))
			{
				if (LostSince < 0.0) LostSince = R;
				if (R - LostSince > LostLongest) { LostLongest = R - LostSince; LostAt = B; }
			}
			else LostSince = -1.0;
		}

		void Report(const char* Label)
		{
			std::sort(Kicks.begin(), Kicks.end());
			const double Kick999 = Kicks.empty() ? 0.0 : Kicks[static_cast<size_t>(0.999 * (Kicks.size() - 1))];
			const double Share = 1.0 / std::max(1, Looks);
			std::printf("\nfilm, %s: shells at the lens %d, %d not cut on the flash; its own man within %.1f m %.2f%% of %d frames, filling over %.0f%% %.1f%% of looks; "
				"a man in the lens %.1f%%, leaves or a trunk at it %.1f%% of looks; own man lost longest %.2f s (battle %.1f); "
				"the look's kick most %.0f deg/s/s (battle %.1f), 99.9%% %.0f, the pan's alone %.0f (battle %.1f); tanks knocked out %d, %d in the picture",
				Label, Bursts, BurstsKept, Set.OwnClear * 0.8, 100.0 * OwnNear / std::max(1, OwnFrames), OwnFrames, 100.0 * Set.OwnMost,
				100.0 * OwnFilled / std::max(1, OwnLooks), 100.0 * Crossed * Share, 100.0 * Edged * Share, LostLongest, LostAt, Kick, KickAt, Kick999,
				PanKick, PanKickAt, Knocked, KnockedSeen);
			std::printf("; in the fight %.0f%% at a soldier's eye level (2.2 m or under), %.0f%% on the film's own section", 100.0 * EyeLevel / std::max(1, FightFrames),
				100.0 * OnSquad / std::max(1, FightFrames));
			std::printf("; the lens in a burst's cloud %.2f%% of frames, the longest %.2f s (battle %.1f)", 100.0 * CloudFrames / std::max(1, Frames), CloudLongest, CloudAt);
			if (CloudLongest > 0.6) Fault("the lens stayed inside a burst's cloud past half a second (the art director: never black past 0.5 s)");
			std::printf("; within %.0f m of a vehicle's fuel ball in its first %.0f s %d frames, the longest %.2f s (battle %.1f), the nearest %.0f m (battle %.1f)",
				Set.FuelVehicle, Set.FuelFor, FuelFrames, FuelLongest, FuelAt, FuelNearest < 1e17 ? FuelNearest : -1.0, FuelNearestAt);
			if (FuelLongest > 0.35) Fault("the lens stayed within a fuel ball's reach past its flash (the art director: cut on the flash)");
			std::printf("; the whole mass %d shot%s", MassShots, MassShots == 1 ? "" : "s");
			if (MassShots > 0)
				std::printf(", at battle %.1f s (its window %.0f to %.0f s) for %.1f s, %.0f m up and %.0f m from where it looks, the men drawn in its picture %.0f%% at the least (battle %.1f, %d men)",
					MassStart, MassFrom, MassTo, MassReal, MassHeight, MassFar, 100.0 * MassLeast, MassLeastAt, MassMen);
			if (bMassGate && Rec->End >= MassTo && MassFrom > 0.0 && MassShots != 1) Fault("the whole mass was not shot once in its window (CD1)");
			if (MassShots > 0 && (MassStart < MassFrom - 0.05 || MassStart > MassTo + 0.05)) Fault("the whole mass was shot outside its window (CD1)");
			if (MassShots > 0 && MassLeast < 0.80) Fault("the whole mass's picture held under 80% of the men drawn (CD1)");
			if (MassShots > 0 && MassReal < Set.MassLength * 0.9) Fault("the whole mass was cut short");
			if (BurstsKept > 0) Fault("a shell landed at the lens and the shot was not cut on its flash");
			if (static_cast<double>(OwnNear) / std::max(1, OwnFrames) > 0.005) Fault("the lens came within its own man's face more than 0.5% of the time");
			if (static_cast<double>(OwnFilled) / std::max(1, OwnLooks) > 0.03) Fault("its own man filled the picture more than 3% of the time");
			if (Crossed * Share > 0.02) Fault("a man was in the lens more than 2% of the time");
			if (Edged * Share > 0.03) Fault("leaves or a trunk were at the lens more than 3% of the time");
			if (LostLongest > Set.ShotLeast + Set.LostAfter + 0.75) Fault("a shot stayed on with its own man lost from the picture");
			// (the second cut's lurches ran to 1700-4200 degrees a second a second: a lens shoved by its man, coming off a
			// bank, a look dropped in a frame; a hand-held pan's start is some 200-600)
			if (Kick > 1200.0) Fault("the camera's swing jolted in a frame (over 1200 degrees a second a second)");
		}
	};
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::printf("usage: land_play_test <recording.json> <mix.json> [out folder]\n");
		return 2;
	}
	std::string Text, MixText, Error;
	auto T0 = std::chrono::steady_clock::now();
	if (!LandJson::ReadFile(argv[1], Text)) { std::printf("cannot read %s\n", argv[1]); return 2; }
	LandFeed::Recording Rec;
	if (!LandFeed::ReadRecording(Text, Rec, Error)) { std::printf("cannot read the recording: %s\n", Error.c_str()); return 2; }
	const double ReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	if (!LandJson::ReadFile(argv[2], MixText)) { std::printf("cannot read %s\n", argv[2]); return 2; }
	std::printf("%s: \"%s\", %s, %zu blocks, %zu tracks (%zu twins left out), %zu rounds, %zu events, %.1f s; read in %.0f ms\n",
		Rec.Key.c_str(), Rec.Title.c_str(), Rec.Date.c_str(), Rec.Blocks.size(), Rec.Tracks.size(),
		static_cast<size_t>(std::count_if(Rec.Tracks.begin(), Rec.Tracks.end(), [](const LandFeed::Track& T) { return T.bTwin; })),
		Rec.Rounds.size(), Rec.Events.size(), Rec.End, ReadMs);

	LandPlay::World PW;
	LandPlay::Replay Play;
	Play.Begin(Rec, PW);
	{
		// A tank's machine guns at the foot (Brian, 22 November: the gunner's inside "also shoots some times at
		// infantry"; the roof gun "cant be used unless the tank commander mans the gun"): which gun each of her bursts
		// is fired from. The roof gun only with the commander up in his open hatch and only where his army mounted one;
		// the hull gunner's only toward her bow.
		std::map<std::tuple<int, int, int>, int> TankOf;
		for (size_t I = 0; I < Rec.Tracks.size(); ++I)
		{
			const LandFeed::Track& Tr = Rec.Tracks[I];
			if (Tr.Role == "tank") TankOf[{ Tr.Block, Tr.K, Tr.J }] = static_cast<int>(I);
		}
		int Bursts = 0, Roof = 0, Hull = 0, Coax = 0, RoofShut = 0, HullWide = 0, RoofNone = 0, Up = 0, ByTanks = 0, Named = 0, Struck = 0, StruckNotTank = 0;
		// (the rules' commanders struck up in their hatches: each on a tank)
		for (const LandFeed::Round& R : Rec.Rounds)
			if (R.bCm)
			{
				++Struck;
				bool bTank = false;
				for (const auto& Kv : TankOf) bTank = bTank || (std::get<0>(Kv.first) == R.On && std::get<1>(Kv.first) == R.Ok);
				if (!bTank) ++StruckNotTank;
			}
		std::set<int> Firing;
		for (const LandFeed::Round& R : Rec.Rounds)
		{
			if (R.W != "mg") continue;
			const auto Of = TankOf.find({ R.By, R.K, R.J });
			if (Of == TankOf.end()) continue;
			++Bursts;
			Firing.insert(Of->second);
			const LandFeed::Track& Tr = Rec.Tracks[static_cast<size_t>(Of->second)];
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, R.T);
			const bool bOpen = C && !C->bButtoned;
			if (bOpen) ++Up;
			const std::string Gun = Play.TankMg(Of->second, R.T, R.N, LandPlay::Vec(R.X, R.Y, 0.0), R.G);
			if (!R.G.empty()) ++Named;
			const LandFeed::Block* B = Rec.BlockOf(Tr.Block);
			if (Gun == "roof_mg")
			{
				++Roof;
				if (!bOpen) ++RoofShut;
				if (!B || (B->Army != "USA" && B->Army != "GER")) ++RoofNone;
			}
			else if (Gun == "hull_mg")
			{
				++Hull;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, R.T);
				const double Off = std::fabs(std::remainder(std::atan2(R.X - S.X, R.Y - S.Y) - S.F, 2.0 * 3.14159265358979));
				if (Off > 25.5 * 3.14159265358979 / 180.0) ++HullWide;
			}
			else ++Coax;
		}
		for (const auto& Kv : TankOf) if (Firing.count(Kv.second)) ++ByTanks;
		std::printf("tank machine guns: %d bursts at the foot from %d of %zu tanks (%d with the gun the rules named): the coaxial %d, the hull gunner's %d, the commander's on the roof %d (%d of the bursts with him up in his hatch); commanders struck in their hatches %d\n",
			Bursts, ByTanks, TankOf.size(), Named, Coax, Hull, Roof, Up, Struck);
		if (StruckNotTank > 0) Fault("a commander struck on what is not a tank");
		if (RoofShut > 0) Fault("a tank's roof gun fired with her commander's hatch shut");
		if (RoofNone > 0) Fault("a roof gun fired on a tank whose army mounted none");
		if (HullWide > 0) Fault("a hull gunner fired wide of his tank's bow");
	}
	LandSound::Mixer Mix;
	if (!Mix.Begin(MixText, Rec.Seed, Error)) { std::printf("cannot read the mix: %s\n", Error.c_str()); return 2; }
	// LAND_PLAY_LIFE=2 replays the fight at half the battle clock, as the scene's cut 4 plays it (life seconds a battle
	// second; the approach stays at 2 battle seconds a real second); 1, the default, keeps the battle clock real.
	const char* LifeText = std::getenv("LAND_PLAY_LIFE");
	const double Life = LifeText ? std::max(0.25, std::atof(LifeText)) : 1.0;
	Mix.Life = Life;
	// How long each take plays, from the recordings' own headers (Sounds/Real beside Content), as the game reads them:
	// the limits and the same-piece rule use it. A take not in Sounds/Real is counted (the game would play it silent).
	std::map<std::string, double> Seconds;
	std::set<std::string> Missing;
	{
		std::string Real0 = argv[2];
		const size_t Cut = Real0.rfind("Content/");
		Real0 = (Cut == std::string::npos ? std::string("./") : Real0.substr(0, Cut)) + "Sounds/Real/";
		Mix.Length = [&Seconds, &Missing, Real0](const std::string& Name)
		{
			auto Known = Seconds.find(Name);
			if (Known != Seconds.end()) return Known->second;
			double Long = 1.5;
			std::ifstream F(Real0 + Name + ".wav", std::ios::binary);
			if (!F) Missing.insert(Name);
			else
			{
				// RIFF: the fmt chunk's byte rate, the data chunk's size.
				char Head[12];
				F.read(Head, 12);
				uint32_t Rate = 0;
				while (F)
				{
					char Id[4];
					uint32_t Size = 0;
					F.read(Id, 4);
					F.read(reinterpret_cast<char*>(&Size), 4);
					if (!F) break;
					if (std::string(Id, 4) == "fmt ")
					{
						char Fmt[16];
						F.read(Fmt, 16);
						std::memcpy(&Rate, Fmt + 8, 4);
						F.seekg(static_cast<std::streamoff>(Size) - 16 + (Size & 1), std::ios::cur);
					}
					else if (std::string(Id, 4) == "data") { if (Rate > 0) Long = static_cast<double>(Size) / Rate; break; }
					else F.seekg(static_cast<std::streamoff>(Size) + (Size & 1), std::ios::cur);
				}
			}
			Seconds[Name] = Long;
			return Long;
		};
	}
	// The field the game lays (LandField) and what stands on it (LandSight), for the camera.
	const auto FieldT0 = std::chrono::steady_clock::now();
	const LandField::Field FieldWords = LandSight::FieldOf(Rec);
	const LandField::Ground Field(FieldWords);
	LandSight::Room Sight;
	Sight.Lay(&Field);
	std::printf("field: laid in %.2f s, %zu things standing for the camera\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - FieldT0).count(), Sight.Things());
	auto GroundAt = [&Field](double X, double Y) { return Field.HeightAt(X, Y); };
	LandEye::World EW;
	EW.Height = GroundAt;
	// (the scene's own: a house's floor and a wall's line no place to stand, a bank's hedge up to 3.2 m; a line of sight
	// stopped by a bank and its hedge 3 m up, a wall 2, a house 7, a wood 14)
	EW.Free = [&Field](const Vec& At)
	{
		LandField::Drawn HW = LandField::Drawn::Pasture;
		const double HZ = Field.HeightAt(At.X, At.Y, &HW);
		if (HW == LandField::Drawn::Floor || HW == LandField::Drawn::Wall) return false;
		return !(HW == LandField::Drawn::Bank && At.Z < HZ + 3.2);
	};
	EW.Open = [&Field](const Vec& From, const Vec& To)
	{
		const Vec Way = To - From;
		const double Long = Way.Length();
		const int Steps = std::clamp(static_cast<int>(Long / 2.0), 2, 200);
		for (int K = 1; K < Steps; ++K)
		{
			const double F = static_cast<double>(K) / Steps;
			if (F * Long < 1.5 || (1.0 - F) * Long < 2.5) continue;
			const Vec At = From + Way * F;
			LandField::Drawn HW = LandField::Drawn::Pasture;
			const double HZ = Field.HeightAt(At.X, At.Y, &HW);
			double Top = HZ;
			switch (HW)
			{
			case LandField::Drawn::Bank: Top += 3.0; break;
			case LandField::Drawn::Wall: Top += 2.0; break;
			case LandField::Drawn::Floor: Top += 7.0; break;
			case LandField::Drawn::Woods: Top += 14.0; break;
			default: break;
			}
			if (At.Z < Top) return false;
		}
		return true;
	};
	// (the corn round a place as the scene asks it: the tallest standing corn or tall grass the field lays within 0.7 m)
	auto CornAt = [&Field](double X, double Y)
	{
		std::vector<LandField::Placed> Near;
		Field.Clutter(X - 0.7, Y - 0.7, X + 0.7, Y + 0.7, Near);
		const double Ground = Field.At(X, Y).Z;
		double Top = 0.0;
		for (const LandField::Placed& One : Near)
		{
			if (One.Kind != LandField::Base::Wheat && One.Kind != LandField::Base::TallGrass) continue;
			Top = std::max(Top, static_cast<double>(One.Z) + static_cast<double>(Field.BaseOf(One.Kind).Height) * static_cast<double>(One.ScaleZ) - Ground);
		}
		return Top;
	};
	EW.Crop = CornAt;
	EW.Solid = [&Sight](const Vec& At, int Except) { return static_cast<int>(Sight.At(At, Except)); };
	// The men and vehicles where the recording has them now (the game asks its own cores: the skinned men, the posed tanks).
	auto PlaceMovers = [&Rec, &Sight, &GroundAt](double T)
	{
		Sight.ClearMovers();
		for (size_t K = 0; K < Rec.Tracks.size(); ++K)
		{
			const LandFeed::Track& Tr = Rec.Tracks[K];
			if (Tr.bTwin || Tr.Start() > T || (Tr.Out >= 0.0 && T > Tr.Out + 1.0)) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, T);
			const Vec Feet(S.X, S.Y, GroundAt(S.X, S.Y));
			if (Tr.IsVehicle())
			{
				double Turret = S.F;
				if (const LandFeed::Change* C = LandFeed::ChangeAt(Tr, T); C && C->bAim) Turret = LandFeed::FacingTo(S.X, S.Y, C->AimX, C->AimY);
				const bool bTank = Tr.Role == "tank";
				Sight.Add(bTank ? LandSight::Machine::Plain(static_cast<int>(K), Feet, S.F, Turret, 6.4, 3.4, 2.8, true)
				                : Tr.Role == "gun" ? LandSight::Machine::Plain(static_cast<int>(K), Feet, S.F, S.F, 5.5, 2.0, 1.6, false)
				                                   : LandSight::Machine::Plain(static_cast<int>(K), Feet, S.F, S.F, 6.0, 2.3, 2.8, false));
				continue;
			}
			LandSight::Man M;
			M.Track = static_cast<int>(K);
			M.Feet = Feet + Vec(0.0, 0.0, 0.3);
			const bool bFallen = Tr.Fell >= 0.0 && Tr.Fell <= T;
			const LandFeed::Change* C = LandFeed::ChangeAt(Tr, T);
			const std::string Pose = C ? C->Pose : std::string("idle");
			const bool bDown = Pose == "fire_prone" || Pose == "hide" || Pose == "observe" || Pose == "crawl" || Pose == "cower" || Pose == "lmg_burst";
			if (bFallen || bDown) M.Top = Feet + Vec(std::sin(S.F) * 1.4, std::cos(S.F) * 1.4, 0.3);
			else M.Top = Feet + Vec(0.0, 0.0, Pose == "fire_kneel" || Pose == "wait" ? 1.1 : 1.6);
			Sight.Add(M);
		}
	};
	LandEye::Settings ES;
	ES.Seed = Rec.Seed;
	LandEye::Director Eye;
	Eye.Begin(Rec, EW, ES);
	LandDesk::Desk Desk;
	Desk.Begin(Rec, 0);

	LandEye::Pose Pose;
	Mix.OnScreen = [&](const Vec& At) { return InPicture(Pose, At, 1.1); };
	FilmMeasure Film;
	Film.Rec = &Rec;
	Film.Set = ES;

	const double Fps = 60.0, Dt = 1.0 / Fps;
	// The clock: twice life before the armies meet, then as life (the scene's own default), to the end and 6 s more.
	double Battle = 0.0, Real = 0.0;
	std::vector<LandPlay::Effect> Fx;
	std::vector<LandPlay::Noise> Noises;
	std::vector<LandPlay::Loss> Losses;
	std::vector<LandSound::Take> Takes;
	std::vector<LandSound::Bed> Beds;
	std::map<int, int> Fired, Landed;
	std::map<std::string, int> FxCount, NoiseCount;
	std::map<std::string, std::vector<std::pair<double, double>>> TakeAt; // piece -> (starts, pitch) of every copy
	double MostLate = 0.0;
	int VoiceLines = 0;
	int SameTakeTooSoon = 0, FallsSeen = 0, FallsNear = 0, Falls = 0, KnockedSeen = 0, Knocked = 0;
	double LastCutReal = -100.0, ShortestShot = 1e9, MeterMax = 0.0, MeterSum = 0.0;
	int MeterN = 0;
	// The meter's raw heat every 0.25 s of real time (the sound director sets fullAt from it): battle second, heat.
	std::vector<std::pair<double, double>> HeatSamples;
	std::vector<std::pair<double, double>> MeterSamples;
	double NextHeatSample = 0.0, BlockHeat = 0.0;
	double Slowest = 0.0, Total = 0.0, SlowestAt = 0.0;
	int Frames = 0;
	// (every frame's cost, and each battle second's: the 99th in the hundred, and the busiest second's own frames)
	std::vector<double> FrameCosts;
	std::map<int, std::pair<double, int>> BySecond;
	// The camera's own share: its slowest frame, its frames over 2 ms, its average.
	double EyeSlowest = 0.0, EyeTotal = 0.0;
	int EyeOver2 = 0;
	// (the camera's work each frame by its own measure, the ground asked under its rays: the same on any machine)
	std::vector<long> StepsByFrame;
	// (the frame the whole mass was placed in: its steps and time)
	long MassSteps = -1;
	double MassMs = 0.0;
	std::map<std::string, double> ShotSeconds;
	// The lens looked round every quarter second: how often it was too near a thing or a man, or its picture filled near
	// it; why each shot was left; aerials in the middle of the battle.
	double NextLensLook = 0.0;
	int LensLooks = 0, LensFilled = 0, LensCrowded = 0, MiddleAerials = 0;
	// (and screened farther out, by trees, hedges and banks short of what it looks at: the second cut's film had a watch
	// shot through a hedge under a tree for 10 s, a tracking shot drifted into a hedge, 6 s on one orchard tree)
	int LensScreened = 0, LensScreenedLeave = 0;
	double ScreenedSince = -1.0, LongestScreened = 0.0, LongestScreenedAt = 0.0;
	std::map<std::string, int> CutsWhy;
	const double Contact = Rec.Contact >= 0.0 ? Rec.Contact : 0.0;
	double KnotStart = -1.0, KnotEnd = -1.0;
	double LowestEye = 1e9;
	std::vector<std::string> CutLines;
	// The lens near the ground over corn: frames, and how far over the ears it was at the lowest.
	int LowOverCorn = 0, LowInCorn = 0, LensBehindStalks = 0;
	double LeastOverEars = 1e9;
	// The shell holes, laid as the game lays them (LandBattle's MakeCraters): every shell, bomb and grenade that lands
	// in the ground, none on armour.
	LandCrater::Field Holes;
	Holes.Begin([&Rec](double X, double Y) { return Rec.Ground.HeightAt(X, Y); }, Rec.Seed ? Rec.Seed : 1);
	// (dug where the game digs them, and the ground drawn cut over their bowls as the game cuts it: LandCrater::Dug)
	Holes.Dig = LandCrater::Field::DigOn(Field);
	LandCrater::Dug Dug;
	Dug.Begin(&Field.Tiles());
	size_t DugSentMost = 0;
	int DugSplit = 0, DugSentPartsMost = 0;
	double DugMsMost = 0.0, DugMs = 0.0;
	std::map<std::string, int> HolesBy;
	double HoleMs = 0.0, HoleMsMost = 0.0;
	// What the game sends again for them in a frame: the most points, the most meshes, the widest mesh (metres).
	size_t HoleSentMost = 0;
	int HoleSentMeshesMost = 0, HolePatchesSeen = 0;
	double HoleMeshWidest = 0.0;
	// The drill's sections (a recording whose tracks have followers): each round's shooter and the man it struck are
	// found by (block, k, j), a bullet that struck a follower lands on him, and a gun's or mortar's kill round on a
	// follower with no 'shell' event is a burst of its own beside him.
	const bool bSections = Rec.HasSections();
	std::map<int, size_t> RoundIx;
	for (size_t I = 0; I < Rec.Rounds.size(); ++I) RoundIx[Rec.Rounds[I].N] = I;
	int FollowerShots = 0, FollowerStruck = 0, FollowerOnHim = 0, FollowerBursts = 0, FollowerBurstsBeside = 0;
	double FollowerWorst = 0.0;

	while (Battle < Rec.End + 6.0)
	{
		const double Speed = Battle < Contact - 10.0 ? 2.0 : 1.0 / Life;
		const double From = Battle;
		Battle += Dt * Speed;
		Real += Dt;
		auto F0 = std::chrono::steady_clock::now();
		Fx.clear();
		Noises.clear();
		Losses.clear();
		Takes.clear();
		Play.Step(From, Battle, Fx, Noises, Losses);
		PlaceMovers(Battle);
		const auto E0 = std::chrono::steady_clock::now();
		const long Steps0 = Eye.Steps();
		Pose = Eye.Update(Battle, Real, Dt);
		const double EyeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - E0).count();
		const long FrameSteps = Eye.Steps() - Steps0;
		StepsByFrame.push_back(FrameSteps);
		if (Pose.bCut && Eye.MassShot()) { MassSteps = FrameSteps; MassMs = EyeMs; }
		EyeSlowest = std::max(EyeSlowest, EyeMs);
		if (EyeMs > 2.0) ++EyeOver2;
		if (EyeMs > 2.0 && std::getenv("LAND_PLAY_SLOW")) std::printf("slow eye %.2f ms (%ld steps) at %.2f: %s%s, why %s\n", EyeMs, FrameSteps, Battle, LandEye::NameOf(Pose.Shot), Pose.bCut ? " (cut)" : "", LandEye::NameOf(Eye.LastWhy()));
		EyeTotal += EyeMs;
		// (the film's measuring is the check's own, not the game's: kept out of the frame's time)
		const auto M0 = std::chrono::steady_clock::now();
		Film.Feed(Eye, Pose, From, Battle, Real, Dt);
		const double MeasureMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - M0).count();
		// The lens, by the art director's rules, every quarter of a second of real time (the rules' own asking).
		if (Pose.Shot != LandEye::Kind::Open && Pose.Shot != LandEye::Kind::Wide && Real >= NextLensLook)
		{
			NextLensLook = Real + 0.25;
			++LensLooks;
			const double Filled = Eye.Blocked(Pose);
			if (Filled > ES.BlockedMost) ++LensFilled;
			if (!Eye.Roomy(Pose)) ++LensCrowded;
			const double Screened = Eye.Screened(Pose);
			if (Screened > ES.ScreenMost) ++LensScreened;
			if (Screened > ES.ScreenLeave)
			{
				++LensScreenedLeave;
				if (ScreenedSince < 0.0) ScreenedSince = Real;
				if (Real - ScreenedSince > LongestScreened) { LongestScreened = Real - ScreenedSince; LongestScreenedAt = Battle; }
			}
			else ScreenedSince = -1.0;
		}
		if (Pose.bCut) ScreenedSince = -1.0;
		if (Pose.bCut)
		{
			++CutsWhy[LandEye::NameOf(Eye.LastWhy())];
			// (the whole mass is the one wide shot asked for in the middle: CD1)
			if (Pose.Shot == LandEye::Kind::Wide && Battle > 12.0 && Battle < Rec.End && !Eye.MassShot()) ++MiddleAerials;
		}
		const double Ground = Rec.Ground.HeightAt(Pose.Look.X, Pose.Look.Y);
		LandSound::Listener Ear;
		Ear.CameraHeight = Pose.Eye.Z - Rec.Ground.HeightAt(Pose.Eye.X, Pose.Eye.Y);
		Ear.At = Vec(Pose.Look.X, Pose.Look.Y, Ground + Mix.EarHeightFactor() * std::max(0.0, Ear.CameraHeight));
		// (on the ground among the men the ear is where the eye is: the look point may be 70 m ahead)
		if (Ear.CameraHeight < 4.0) Ear.At = Pose.Eye;
		for (const LandPlay::Noise& N : Noises) Mix.Hear(N, Real, Battle, Speed, Ear, Takes);
		int Fighting = 0, Blocks = 0;
		if (const LandFeed::View* V = Rec.ViewAt(Battle))
		{
			for (const LandFeed::Unit& U : V->Units)
			{
				++Blocks;
				if (U.Act == "fight" && std::hypot(U.X - Ear.At.X, U.Y - Ear.At.Y) < 600.0) ++Fighting;
			}
		}
		Mix.Tick(Real, Dt, Ear, Fighting, Blocks ? static_cast<double>(Fighting) / Blocks : 0.0, Battle >= Contact, "open", Beds);
		const LandDesk::Words& W = Desk.Update(Battle, Dt);
		const double FrameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - F0).count() - MeasureMs;
		if (FrameMs > Slowest) { Slowest = FrameMs; SlowestAt = Battle; }
		Total += FrameMs;
		++Frames;
		FrameCosts.push_back(FrameMs);
		{
			auto& Sec = BySecond[static_cast<int>(Battle)];
			Sec.first += FrameMs;
			++Sec.second;
		}

		if (KnotStart < 0.0) KnotStart = W.Knot;
		KnotEnd = W.Knot;
		for (const LandPlay::Effect& E : Fx)
		{
			if ((E.Kind == LandPlay::FxKind::Shell || E.Kind == LandPlay::FxKind::Size) && !E.bArmour)
			{
				auto H0 = std::chrono::steady_clock::now();
				if (Holes.Add(E.At.X, E.At.Y, E.Name, E.CalibreMm, E.Ground.empty() ? std::string("earth") : E.Ground)) ++HolesBy[E.Name];
				const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - H0).count();
				HoleMs += Ms;
				HoleMsMost = std::max(HoleMsMost, Ms);
			}
			static const char* Names[] = { "muzzle", "shots", "shell", "size", "skip", "glance", "plate" };
			++FxCount[Names[static_cast<int>(E.Kind)]];
			if (std::isnan(E.At.X) || std::isnan(E.At.Y) || std::isnan(E.At.Z)) Fault("an effect at no number", Battle);
			if (!bSections || !RoundIx.count(E.Round)) continue;
			const LandFeed::Round& R = Rec.Rounds[RoundIx[E.Round]];
			if (R.Oj <= 0 || R.R != LandFeed::Result::Kill) continue;
			const int Struck = Rec.TrackIndexOf(R.On, R.Ok, R.Oj);
			if (Struck < 0) continue;
			const LandFeed::Spot S = LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(Struck)], R.L);
			if (E.Kind == LandPlay::FxKind::Shots)
			{
				const double Off = std::hypot(E.To.X - S.X, E.To.Y - S.Y);
				FollowerWorst = std::max(FollowerWorst, Off);
				if (Off < 0.05) ++FollowerOnHim;
				else Fault("a bullet that struck a follower did not land on him", Battle);
			}
			else if (E.Kind == LandPlay::FxKind::Shell || E.Kind == LandPlay::FxKind::Size)
			{
				++FollowerBursts;
				if (std::hypot(E.At.X - S.X, E.At.Y - S.Y) <= 15.0) ++FollowerBurstsBeside;
				else Fault("a follower's burst was not beside him", Battle);
			}
		}
		// The ground cut over the frame's dug holes (LandBattle's MakeCraters and SendDug): the parts sent again.
		{
			const auto D0 = std::chrono::steady_clock::now();
			for (const std::vector<double>& Cut : Holes.TakeCuts()) Dug.Cut(Cut);
			const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - D0).count();
			DugMs += Ms;
			DugMsMost = std::max(DugMsMost, Ms);
			DugSplit += static_cast<int>(Dug.TakeSplit().size());
			size_t Points = 0;
			const std::vector<LandCrater::Dug::Place> Parts = Dug.TakeChanged();
			for (const LandCrater::Dug::Place& Where : Parts) Points += Dug.PartOf(Where).Points();
			DugSentMost = std::max(DugSentMost, Points);
			DugSentPartsMost = std::max(DugSentPartsMost, static_cast<int>(Parts.size()));
		}
		// What the game sends again this frame (LandBattle's MakeCraters): each changed section's patch is one mesh, and
		// the engine copies all of that mesh's sections and redraws the shadows under its bounds. Count those points
		// and how wide the widest such mesh is: a patch, never the whole field.
		{
			const std::vector<LandCrater::Field::Place> Sent = Holes.TakeChanged();
			std::vector<int> Meshes;
			for (const LandCrater::Field::Place& Where : Sent)
			{
				if (Where.Patch < 0 || Where.Patch >= Holes.Patches() || Where.Section < 0 || Where.Section >= static_cast<int>(Holes.PatchAt(Where.Patch).Sections.size()))
				{
					Fault("a shell hole's section is sent to no patch", Battle);
					continue;
				}
				if (Where.Patch > HolePatchesSeen) Fault("a shell hole patch is numbered past the next", Battle);
				HolePatchesSeen = std::max(HolePatchesSeen, Where.Patch + 1);
				if (std::find(Meshes.begin(), Meshes.end(), Where.Patch) == Meshes.end()) Meshes.push_back(Where.Patch);
			}
			size_t Points = 0;
			for (const int K : Meshes)
			{
				const LandCrater::Field::Patch& Pa = Holes.PatchAt(K);
				double X0 = 1e18, X1 = -1e18, Y0 = 1e18, Y1 = -1e18;
				for (const LandField::Mesh& M : Pa.Sections)
				{
					Points += M.Points();
					for (size_t I = 0; I < M.Points(); ++I)
					{
						X0 = std::min(X0, static_cast<double>(M.Positions[I * 3]));
						X1 = std::max(X1, static_cast<double>(M.Positions[I * 3]));
						Y0 = std::min(Y0, static_cast<double>(M.Positions[I * 3 + 1]));
						Y1 = std::max(Y1, static_cast<double>(M.Positions[I * 3 + 1]));
					}
				}
				if (X1 >= X0) HoleMeshWidest = std::max(HoleMeshWidest, std::max(X1 - X0, Y1 - Y0));
			}
			HoleSentMost = std::max(HoleSentMost, Points);
			HoleSentMeshesMost = std::max(HoleSentMeshesMost, static_cast<int>(Meshes.size()));
		}
		for (const LandPlay::Noise& N : Noises)
		{
			static const char* Names[] = { "shot", "landing", "glance", "holed", "fall", "knocked_out", "contact", "broken" };
			++NoiseCount[Names[static_cast<int>(N.Kind)]];
		}
		for (const LandSound::Take& T : Takes)
		{
			// The same piece of a recording never within 0.15 s of itself, or within its own length when shorter.
			const double Starts = Real + T.Delay;
			char Piece[32];
			std::snprintf(Piece, sizeof(Piece), "@%.3f", T.From);
			const double Plays = T.Cut > 0.0 ? T.Cut : std::max(0.02, Mix.Length(T.Name) - T.From);
			// (on time at another pitch, 3.5% or more from EVERY copy still sounding, is no copy)
			std::vector<std::pair<double, double>>& Copies = TakeAt[T.Name + Piece];
			for (const auto& Copy : Copies)
			{
				const bool bOtherPitch = Copy.second > 0.0 && std::abs(T.Pitch / Copy.second - 1.0) >= 0.035;
				if (Starts < Copy.first + std::min(0.15, Plays) - 1e-9 && Starts > Copy.first - 1e-9 && !bOtherPitch) { if (std::getenv("LAND_PLAY_SAME")) std::printf("  same take %s%s at real %.3f (copy %.3f, pitch %.3f / %.3f, delay %.3f, plays %.3f)\n", T.Name.c_str(), Piece, Starts, Copy.first, T.Pitch, Copy.second, T.Delay, Plays); ++SameTakeTooSoon; break; }
			}
			Copies.push_back({ Starts, T.Pitch });
			// (a shot heard more than a quarter second after it is seen, by the rule's holding: none)
			if (T.Delay > 0.0 && T.Family != "body" && T.Family != "voices") MostLate = std::max(MostLate, T.Delay);
			if (T.Bus == "voices") ++VoiceLines;
		}
		for (const LandPlay::Loss& L : Losses)
		{
			const double Far = (L.At - Pose.Eye).Length();
			const bool bSeen = InPicture(Pose, L.At + Vec(0.0, 0.0, 0.8)) && Far < (Pose.Fov < 32.0 ? 400.0 : 200.0);
			if (std::getenv("LAND_PLAY_FALLS")) std::printf("  fall at %.1f: %s on %d, seen %d, %.0f m, lens %.0f, at %.1f %.1f %.1f, eye %.1f %.1f %.1f, look %.1f %.1f %.1f\n", Battle, LandEye::NameOf(Pose.Shot), Pose.Subject, bSeen ? 1 : 0, Far, Pose.Fov, L.At.X, L.At.Y, L.At.Z, Pose.Eye.X, Pose.Eye.Y, Pose.Eye.Z, Pose.Look.X, Pose.Look.Y, Pose.Look.Z);
			if (L.bVehicle) { ++Knocked; if (bSeen) ++KnockedSeen; }
			else
			{
				++Falls;
				if (bSeen) ++FallsSeen;
				if (Far < 150.0) ++FallsNear;
			}
		}
		if (Pose.bCut)
		{
			// (a rescue, something come in front of the lens, and a cut on the flash of a shell landing at it, are the cuts
			// allowed sooner)
			if (Real - LastCutReal < ShortestShot && LastCutReal > 0.0 && Eye.LastWhy() != LandEye::Why::Rescue && Eye.LastWhy() != LandEye::Why::Burst) ShortestShot = Real - LastCutReal;
			LastCutReal = Real;
			char Line[160];
			std::snprintf(Line, sizeof(Line), "%6.1f %-8s subject %d", Battle, LandEye::NameOf(Pose.Shot), Pose.Subject);
			CutLines.push_back(Line);
		}
		ShotSeconds[LandEye::NameOf(Pose.Shot)] += Dt;
		const double Under = Pose.Eye.Z - GroundAt(Pose.Eye.X, Pose.Eye.Y);
		LowestEye = std::min(LowestEye, Under);
		if (Under < 0.2) Fault("the eye is under the ground", Battle);
		if (Pose.Shot == LandEye::Kind::Low || Pose.Shot == LandEye::Kind::Shoulder || Pose.Shot == LandEye::Kind::Tank)
		{
			const double Corn = CornAt(Pose.Eye.X, Pose.Eye.Y);
			// (and the stalks a step or two in front, short of what it looks at: in the picture they stand like planks)
			{
				LandPlay::Vec Ahead = Pose.Look - Pose.Eye;
				Ahead.Z = 0.0;
				const double Flat = Ahead.Length();
				for (const double Step : { 0.6, 1.2, 1.8, 2.5 })
				{
					if (Flat < 0.5 || Step > Flat - 0.4) break;
					const LandPlay::Vec P = Pose.Eye + Ahead * (Step / Flat);
					const double Front = CornAt(P.X, P.Y);
					if (Front > 0.0 && Pose.Eye.Z - GroundAt(P.X, P.Y) < Front + 0.05) { ++LensBehindStalks; break; }
				}
			}
			if (Corn > 0.0)
			{
				const double OverEars = Under - Corn;
				if (Pose.Shot == LandEye::Kind::Low) ++LowOverCorn;
				LeastOverEars = std::min(LeastOverEars, OverEars);
				if (OverEars < 0.05)
				{
					++LowInCorn;
					Fault("a lens near the ground is down among the corn", Battle);
				}
			}
		}
		if (std::isnan(Pose.Eye.X) || std::isnan(Pose.Look.X)) Fault("the eye at no number", Battle);
		MeterMax = std::max(MeterMax, Mix.Meter());
		if (Battle > Contact) { MeterSum += Mix.Meter(); ++MeterN; }
		if (Real >= NextHeatSample) { HeatSamples.emplace_back(Battle, Mix.Heat()); MeterSamples.emplace_back(Battle, Mix.Meter()); NextHeatSample += 0.25; }
		BlockHeat += 3.0 * Fighting * Dt / 6.0;
	}

	// Every round fired once and landed once (the replay's own count: the cursor passes each).
	if (Play.RoundsPlayed() != static_cast<int>(Rec.Rounds.size())) Fault("not every round was fired");
	int ShellEvents = 0;
	for (const LandFeed::Event& E : Rec.Events) if (E.What == "shell") ++ShellEvents;
	if (Play.BlastsPaired() + Play.BlastsAlone() != ShellEvents) Fault("a 'shell' event was not one blast");
	const int ShotNoises = NoiseCount["shot"];
	if (ShotNoises != static_cast<int>(Rec.Rounds.size())) Fault("a round had no shot to hear");
	if (Mix.ShotsOnScreenDropped > 0) Fault("a shot on the screen was dropped");
	if (SameTakeTooSoon > 0) Fault("the same take started twice within 0.15 s");
	if (ShortestShot < 3.0) Fault("the camera cut within 3 s");
	if (std::abs(KnotStart - 0.5) > 0.06) Fault("the bar did not start level");
	const bool bAttWon = Rec.Result.Winner == "att";
	if ((KnotEnd > 0.5) != bAttWon) Fault("the bar did not end on the winner's side");

	std::printf("rounds: %d fired, %d shell events (%d one blast with their kill round, %d alone)\n", Play.RoundsPlayed(), ShellEvents, Play.BlastsPaired(), Play.BlastsAlone());
	if (bSections)
	{
		int Followers = 0, Unknown = 0;
		for (const LandFeed::Track& T : Rec.Tracks) Followers += T.J > 0;
		for (const LandFeed::Round& R : Rec.Rounds)
		{
			if (R.J > 0) { ++FollowerShots; if (R.By >= 0 && Rec.TrackIndexOf(R.By, R.K, R.J) < 0) ++Unknown; }
			if (R.Oj > 0 && R.R != LandFeed::Result::Miss) { ++FollowerStruck; if (Rec.TrackIndexOf(R.On, R.Ok, R.Oj) < 0) ++Unknown; }
		}
		if (Unknown) Fault("a round names a man of a section who has no track");
		if (FollowerBursts != Play.FollowerBurstCount()) Fault("a gun's or mortar's kill round on a follower was not one burst");
		std::printf("sections: %d followers; %d rounds fired by followers, %d struck one (%d bullets landed on him, the farthest %.3f m off; %d bursts of their own, %d beside him, of %d due)\n",
			Followers, FollowerShots, FollowerStruck, FollowerOnHim, FollowerWorst, FollowerBursts, FollowerBurstsBeside, Play.FollowerBurstCount());
	}
	std::printf("effects:");
	for (auto& P : FxCount) std::printf(" %s %d", P.first.c_str(), P.second);
	std::printf("\nsound: %d shots, %d taken, %d out of hearing, %d dropped by the limits (%d on the screen), %d heard in a copy already sounding (a volley), %d cracks overhead, %d landings, %d falls; %d takes moved for the 0.15 s rule\n",
		Mix.ShotsHeard, Mix.ShotTakes, Mix.ShotsOutOfHearing, Mix.ShotsDropped, Mix.ShotsOnScreenDropped, Mix.ShotsMerged, Mix.Cracks, Mix.Landings, Mix.Falls, Mix.Delayed);
	// (a shot heard in a copy is the last resort of a volley in the picture, every recording it could take crowded at
	// every pitch near it. Never a shot nearer than its family's far takes: that one is heard on its own, and a merge
	// would hide it. Far off, in a crowd, it is one crack of the crackle (the live 1,989-man battle's mass wide put
	// 159 of 34,555 there, all 307 to 470 m off, in seven seconds); more than one in a hundred and the rule is hiding
	// the battle, not sparing a flanged copy)
	std::printf("heard in a copy: %d shots (%.2f in a thousand), %d takes of any kind (a shot, a crack, a blast) nearer than their family's far takes; %d machine-gun bursts crowded out of one recording took another on their list, %d another stretch of firing (moreParts)\n",
		Mix.ShotsMerged, 1000.0 * Mix.ShotsMerged / std::max(1, Mix.ShotsHeard), Mix.SameMergedNear, Mix.MgToOther, Mix.MgToOtherPart);
	for (const auto& Kv : Mix.MergedNearBy) if (Kv.second > 0) std::printf("  heard in a copy though nearer than its far takes: %s %d\n", Kv.first.c_str(), Kv.second);
	if (Mix.SameMergedNear > 0) Fault("a take nearer than its family's far takes (a shot, a crack, a blast) was heard only in a copy already sounding (a sound heard on its own, hidden)");
	if (Mix.ShotsMerged * 100 > Mix.ShotsHeard) Fault("more than one shot in a hundred was heard only in a copy already sounding");
	std::printf("land: %d bursts cut to their rounds, %d runs of cracks, %d rounds struck the ground near the ear (%d whined off), %d Garand clips pinged (%d reloads), %d shouts (%d voice lines in all), %d shells heard coming in; the longest a take was held for the 0.15 s rule %.2f s, %d started on time at another pitch instead\n",
		Mix.BurstsCut, Mix.CrackRuns, Mix.GroundStrikes, Mix.Whines, Mix.Clips, Mix.Reloads, Mix.Shouts, VoiceLines, Mix.Incoming, Mix.MostDelay, Mix.Repitched);
	if (Mix.MostDelay > 0.1) Fault("a take was held more than 0.1 s by the same-take rule (heard after its flash)");
	{
		// A tank's machine guns are her own (land.tankMg, by army; its roof row the guns on her commander's hatch):
		// every close burst from a tank plays a take on her army's row, the roof's where it has one.
		std::printf("tank machine guns heard close, by gun, army and take:");
		int OffRow = 0, RoofOff = 0;
		for (const auto& Kv : Mix.TankMgTakes)
		{
			std::printf(" %s %d;", Kv.first.c_str(), Kv.second);
			const size_t A = Kv.first.find(' '), B = Kv.first.find(' ', A + 1);
			const std::string Gun = Kv.first.substr(0, A), Army = Kv.first.substr(A + 1, B - A - 1), Name = Kv.first.substr(B + 1);
			const std::string Roof = "tankMg.roof." + Army;
			const std::string Row = Mix.HasList("tankMg." + Army) ? "tankMg." + Army : Mix.HasList("tankMg.*") ? std::string("tankMg.*")
				: Mix.HasList("byNation.mg." + Army) ? "byNation.mg." + Army : std::string("byNation.mg.*");
			if (Gun == "roof_mg" && Mix.HasList(Roof)) { if (!Mix.InList(Roof, Name)) RoofOff += Kv.second; }
			else if (!Mix.InList(Row, Name)) OffRow += Kv.second;
		}
		std::printf("\n");
		if (OffRow > 0) Fault("a tank's machine gun played a take off her army's tank row");
		if (RoofOff > 0) Fault("a roof gun played a take off its roof row");
		if (Mix.TankMgWhole > 0) Fault("a tank's burst played a take shorter than the burst, whole (her row holds a take too short for her bursts)");
	}
	std::printf("bursts: the most any cut MG burst plays past its last round %.3f s (0 or less: no extra rounds)\n", Mix.MostBurstOver);
	std::printf("same take, no pitch free: %d left out off the screen, %d held to a clear moment, %d close rifle shots moved to a far recording, %d far rifle shots moved to another far recording, %d heard in a copy (every far recording crowded), %d at the farthest pitch\n",
		Mix.SameLeftOut, Mix.SameHeldClear, Mix.SameToFar, Mix.SameToOtherFar, Mix.SameMerged, Mix.SameCrowded);
	{
		// The GI's clip cues: a reload is two pieces of garand_reload (clip in now, the rod slamming 0.246 s later, the
		// slam's onset at 2.744 s into the file landing 0.33 s after the cue), and the squad leader's Thompson is 4
		// rounds and no more.
		LandSound::Listener Near;
		Near.At = Vec(0.0, 0.0, 1.5);
		Near.CameraHeight = 1.5;
		std::vector<LandSound::Take> Cue;
		Mix.HearBody("garand_reload", "USA", Vec(5.0, 0.0, 1.0), 10000.0, Near, Cue);
		const bool bReload = Cue.size() == 2 && Cue[0].Name == "garand_reload" && Cue[1].Name == "garand_reload"
			&& std::abs(Cue[1].Delay + (2.744 - Cue[1].From) - 0.33) < 0.02 && Cue[0].Delay < 0.02;
		Cue.clear();
		Mix.HearBody("thompson_burst", "USA", Vec(5.0, 0.0, 1.0), 10010.0, Near, Cue);
		bool bSmg = Cue.size() == 1;
		if (bSmg)
		{
			const LandSound::Take& B = Cue[0];
			const double Fifth = B.Name == "mp40_burst_01" ? 0.382 : 0.75;
			// (mp40_burst_02 is four rounds as recorded: played whole, Cut 0)
			const double Ends = B.Cut > 0.0 ? B.From + B.Cut + B.CutFade : Fifth;
			bSmg = B.Family == "small_arms" && Ends <= Fifth + 1e-6 && Ends > 0.292;
		}
		// After his first burst, a USA leader's rifle round plays no take (his clip carries it); a rifleman's still does.
		LandPlay::Noise Lead;
		Lead.Kind = LandPlay::NoiseKind::Shot;
		Lead.At = Vec(8.0, 0.0, 1.2);
		Lead.From = Lead.At;
		Lead.To = Vec(300.0, 0.0, 0.0);
		Lead.Weapon = "rifle";
		Lead.Army = "USA";
		Lead.Role = "officer";
		Lead.Track = -1;
		Cue.clear();
		Mix.Hear(Lead, 10020.0, 10020.0, 1.0, Near, Cue);
		const bool bLeaderQuiet = std::none_of(Cue.begin(), Cue.end(), [](const LandSound::Take& K) { return K.Family == "small_arms"; });
		Lead.Role = "rifle";
		Cue.clear();
		Mix.Hear(Lead, 10030.0, 10030.0, 1.0, Near, Cue);
		const bool bRiflemanHeard = !Cue.empty();
		// (a vehicle cue with no recording: the mix names it silent, so nothing missing is asked for)
		if (!Mix.CueTake("field_gun_breech").empty() && !Mix.Length("field_gun_breech")) Fault("the field gun's breech cue asks for a take that is not in Sounds/Real");
		if (!bLeaderQuiet) Fault("a squad leader's round played a rifle take on top of his Thompson burst");
		if (!bRiflemanHeard) Fault("a rifleman's round after the Thompson burst was not heard");
		std::printf("clip cues: reload %s (%zu pieces), Thompson burst %s\n", bReload ? "in step" : "WRONG", static_cast<size_t>(2), bSmg ? "4 rounds" : "WRONG");
		if (!bReload) Fault("the GI's garand_reload cue did not give the two reload pieces in step with his hands");
		if (!bSmg) Fault("the squad leader's thompson_burst cue did not give one 4-round burst");
		// A machine-gun round glancing off a tank: with no armourStrikes recording listed it keeps its ricochet; with one
		// listed (a probe mix) it rings the steel, as a small-arms take.
		LandPlay::Noise Off;
		Off.Kind = LandPlay::NoiseKind::Glance;
		Off.At = Vec(6.0, 0.0, 1.5);
		Off.Weapon = "mg";
		Off.Army = "GER";
		Cue.clear();
		Mix.Hear(Off, 10040.0, 10040.0, 1.0, Near, Cue);
		const bool bRicochetKept = !Cue.empty() && (Cue[0].Name.rfind("ricochet", 0) == 0 || Mix.ArmourRings > 0);
		std::string ProbeText = MixText;
		const size_t LandAt = ProbeText.find("\"land\": {");
		bool bRing = false;
		if (LandAt != std::string::npos)
		{
			ProbeText.insert(LandAt + 9, " \"armourStrikes\": { \"smallArms\": [\"steel_probe\"], \"db\": -4, \"whineChance\": 1 },");
			LandSound::Mixer Probe;
			std::string ProbeError;
			if (Probe.Begin(ProbeText, Rec.Seed, ProbeError))
			{
				Cue.clear();
				Probe.Hear(Off, 50.0, 50.0, 1.0, Near, Cue);
				bRing = !Cue.empty() && Cue[0].Name == "steel_probe" && Cue[0].Family == "small_arms" && Probe.ArmourRings == 1;
			}
		}
		std::printf("armour: a machine-gun round off a tank %s (%d rang off armour in the battle); with a steel recording listed it %s\n", bRicochetKept ? (Mix.ArmourRings > 0 ? "rings the steel" : "keeps its ricochet") : "WRONG", Mix.ArmourRings, bRing ? "rings the steel" : "is WRONG");
		if (!bRicochetKept) Fault("a small-arms round off a tank lost its ricochet with no armourStrikes recording listed");
		if (!bRing) Fault("land.armourStrikes did not ring a small-arms round off a tank");
		// A mortar bomb's own landings past the near band (land.landings), and a tank's engine by her model
		// (land.engines): a probe mix with both listed; near, the shells' takes as before.
		std::string BandText = MixText;
		const size_t BandAt = BandText.find("\"land\": {");
		std::string Bands = "WRONG";
		bool bEngine = false;
		double LifeOne = -1.0, LifeTwo = -1.0;
		if (BandAt != std::string::npos)
		{
			BandText.insert(BandAt + 9, " \"landings\": { \"mortar\": { \"middle\": [\"mortar_probe_mid\"], \"far\": [\"mortar_probe_far\"] } }, \"engines\": { \"Panzer IV\": { \"tank_loop\": \"panzer_probe_loop\" } },");
			LandSound::Mixer Probe;
			std::string ProbeError;
			if (Probe.Begin(BandText, Rec.Seed, ProbeError))
			{
				std::string Heard[3];
				const double Out[3] = { 30.0, 100.0, 300.0 };
				for (int K = 0; K < 3; ++K)
				{
					LandPlay::Noise Bomb;
					Bomb.Kind = LandPlay::NoiseKind::Landing;
					Bomb.Weapon = "mortar";
					Bomb.Size = "mortar";
					Bomb.At = Vec(Out[K], 0.0, 0.0);
					Cue.clear();
					Probe.Hear(Bomb, 100.0 + 20.0 * K, 100.0 + 20.0 * K, 1.0, Near, Cue);
					if (!Cue.empty()) Heard[K] = Cue[0].Name;
				}
				const bool bNear = !Heard[0].empty() && Heard[0].rfind("mortar_probe", 0) != 0;
				if (bNear && Heard[1] == "mortar_probe_mid" && Heard[2] == "mortar_probe_far") Bands = "near " + Heard[0] + ", 100 m " + Heard[1] + ", 300 m " + Heard[2];
				// (the battle clock at half life's: Life 2, the fight heard at Speed 0.5; a bang 700 m off still comes
				// 700 / 343 s after its flash, as at Life 1 and Speed 1)
				LandPlay::Noise Bang;
				Bang.Kind = LandPlay::NoiseKind::Landing;
				Bang.Weapon = "mortar";
				Bang.Size = "mortar";
				Bang.At = Vec(700.0, 0.0, 0.0);
				Cue.clear();
				Probe.Hear(Bang, 200.0, 200.0, 1.0, Near, Cue);
				LifeOne = Cue.empty() ? -1.0 : Cue[0].Delay;
				Probe.Life = 2.0;
				Cue.clear();
				Probe.Hear(Bang, 210.0, 210.0, 0.5, Near, Cue);
				LifeTwo = Cue.empty() ? -1.0 : Cue[0].Delay;
				bEngine = Probe.EngineTake("Panzer IV", "tank_loop") == "panzer_probe_loop" && Probe.EngineTake("Panzer IV", "tank_idle_loop") == "tank_idle_loop"
					&& Probe.EngineTake("M4 Sherman", "tank_sherman_drive_loop") == "tank_sherman_drive_loop";
			}
		}
		std::printf("landings and engines: a mortar bomb with its own takes listed lands %s; a Panzer IV's listed engine %s; in this mix a Panzer IV drives on %s, a Tiger on %s\n", Bands.c_str(), bEngine ? "plays, others keep their own" : "is WRONG",
			Mix.EngineTake("Panzer IV", "tank_loop").c_str(), Mix.EngineTake("Tiger", "tank_loop").c_str());
		if (Bands == "WRONG") Fault("land.landings did not give a mortar bomb its own takes by band (near keeps the shells')");
		if (!bEngine) Fault("land.engines did not give a Panzer IV her listed engine, or changed one not listed");
		const bool bLife = LifeOne > 1.9 && std::fabs(LifeTwo - LifeOne) < 1e-6;
		std::printf("flash to bang 700 m off: %.2f s at life's clock, %.2f s with the battle clock at half life's (Life 2, Speed 0.5)%s\n", LifeOne, LifeTwo, bLife ? "" : " WRONG");
		if (!bLife) Fault("a bang's delay after its flash changed with the battle clock's pace against life's (Mixer::Life)");
		for (const char* Model : { "Panzer IV", "Tiger", "M4 Sherman" })
		{
			for (const char* Loop : { "tank_loop", "tank_idle_loop", "tank_sherman_drive_loop", "tank_idle_sherman_loop" })
			{
				const std::string Take = Mix.EngineTake(Model, Loop);
				if (Take != Loop && !Mix.Length(Take)) Fault((std::string("land.engines names an engine that is not in Sounds/Real: ") + Take).c_str());
			}
		}
	}
	if (Mix.MostBurstOver > 1e-6) Fault("a cut MG burst plays past its last round (more rounds heard than fired)");
	if (!Missing.empty())
	{
		std::printf("takes the game has no recording of (silent):");
		for (const std::string& M : Missing) std::printf(" %s", M.c_str());
		std::printf("\n");
	}
	std::printf("takes:");
	int Shown = 0;
	std::vector<std::pair<int, std::string>> ByCount;
	for (auto& P : Mix.TakesByName) ByCount.emplace_back(P.second, P.first);
	std::sort(ByCount.rbegin(), ByCount.rend());
	for (auto& P : ByCount) { if (Shown++ < 18) std::printf(" %s %d", P.second.c_str(), P.first); }
	std::printf("\nmeter: highest %.2f, mean after contact %.2f\n", MeterMax, MeterN ? MeterSum / MeterN : 0.0);
	{
		auto Percentiles = [](std::vector<double> V, const char* What)
		{
			if (V.empty()) return;
			std::sort(V.begin(), V.end());
			auto At = [&](double P) { return V[std::min(V.size() - 1, static_cast<size_t>(P * (V.size() - 1) + 0.5))]; };
			std::printf("heat %s (%zu samples): p10 %.1f, p50 %.1f, p90 %.1f, p99 %.1f, most %.1f\n", What, V.size(), At(0.10), At(0.50), At(0.90), At(0.99), V.back());
		};
		std::vector<double> All, After, Before;
		for (auto& H : HeatSamples) { All.push_back(H.second); (H.first >= Contact ? After : Before).push_back(H.second); }
		Percentiles(All, "the whole run");
		Percentiles(Before, "before contact");
		Percentiles(After, "after contact");
		// The quietest 10 s after contact (40 samples), and the busiest, as means.
		double Quiet = 1e9, QuietAt = 0.0, Busy = -1.0, BusyAt = 0.0;
		for (size_t I = 0; I + 40 <= HeatSamples.size(); ++I)
		{
			if (HeatSamples[I].first < Contact) continue;
			double Sum = 0.0;
			for (size_t J = I; J < I + 40; ++J) Sum += HeatSamples[J].second;
			if (Sum / 40 < Quiet) { Quiet = Sum / 40; QuietAt = HeatSamples[I].first; }
			if (Sum / 40 > Busy) { Busy = Sum / 40; BusyAt = HeatSamples[I].first; }
		}
		std::printf("heat: the run %.0f s of battle, %.0f s real, contact at %.0f s; quietest 10 s after contact %.1f (from %.0f s), busiest %.1f (from %.0f s)\n",
			Battle, Real, Contact, Quiet, QuietAt, Busy, BusyAt);
		// Each key's share of the heat (each noise stays in the 6 s window, so a block fighting for 6 s weighs as one noise).
		double Sum = BlockHeat;
		for (auto& P : Mix.HeatBy) Sum += P.second;
		// The music's marks: its thick-of-it drop over 0.6, its lull under 0.3.
		int Over = 0, Under = 0, OverAfter = 0, UnderAfter = 0, AfterN = 0;
		for (auto& M : MeterSamples)
		{
			Over += M.second > 0.6;
			Under += M.second < 0.3;
			if (M.first >= Contact) { ++AfterN; OverAfter += M.second > 0.6; UnderAfter += M.second < 0.3; }
		}
		const double N = static_cast<double>(std::max<size_t>(1, MeterSamples.size()));
		std::printf("meter time: over 0.6 %.0f%%, under 0.3 %.0f%% of the run; after contact over 0.6 %.0f%%, under 0.3 %.0f%%\n",
			100.0 * Over / N, 100.0 * Under / N, 100.0 * OverAfter / std::max(1, AfterN), 100.0 * UnderAfter / std::max(1, AfterN));
		std::printf("heat by kind:");
		for (auto& P : Mix.HeatBy) std::printf(" %s %.0f%%", P.first.c_str(), 100.0 * P.second / std::max(1e-9, Sum));
		std::printf(" blocks fighting %.0f%%\n", 100.0 * BlockHeat / std::max(1e-9, Sum));
	}
	std::printf("camera: %d cuts, shortest shot %.1f s, lowest eye %.2f m over the ground; time in each shot:", Eye.Cuts(), ShortestShot, LowestEye);
	for (auto& P : ShotSeconds) std::printf(" %s %.0f s", P.first.c_str(), P.second);
	std::printf("\ncamera: shots left for:");
	for (auto& P : CutsWhy) std::printf(" %s %d", P.first.c_str(), P.second);
	const double FilledShare = static_cast<double>(LensFilled) / std::max(1, LensLooks), CrowdedShare = static_cast<double>(LensCrowded) / std::max(1, LensLooks);
	std::printf("\nlens: looked round %d times; picture over %.0f%% filled near the lens %.1f%%, lens too near a thing or a man %.1f%%; aerials in the middle %d",
		LensLooks, 100.0 * ES.BlockedMost, 100.0 * FilledShare, 100.0 * CrowdedShare, MiddleAerials);
	if (FilledShare > 0.03) Fault("the picture was filled near the lens more than 3% of the time");
	if (CrowdedShare > 0.03) Fault("the lens was too near a thing or a man more than 3% of the time");
	if (MiddleAerials > 0) Fault("an aerial in the middle of the battle");
	const double ScreenedShare = static_cast<double>(LensScreened) / std::max(1, LensLooks);
	std::printf("\nlens: picture over %.0f%% screened by things short of what it looks at %.1f%% of looks, over %.0f%% %.1f%%; the longest stretch over %.0f%% %.2f s (battle %.1f s)",
		100.0 * ES.ScreenMost, 100.0 * ScreenedShare, 100.0 * ES.ScreenLeave, 100.0 * LensScreenedLeave / std::max(1, LensLooks), 100.0 * ES.ScreenLeave, LongestScreened, LongestScreenedAt);
	if (ScreenedShare > 0.05) Fault("the picture was screened by things more than 5% of the time");
	if (LongestScreened > 1.0) Fault("a shot stayed screened by things for over a second");
	Film.Report("this run");
	{
		int Men = 0;
		for (size_t I = 0; I < Rec.Tracks.size(); ++I) Men += Eye.InSquad(static_cast<int>(I)) ? 1 : 0;
		int Shots = 0;
		std::printf("\ncamera: the film's own section is block %d section %d (%d men)", Eye.SquadBlock(), Eye.SquadK(), Men);
		(void)Shots;
	}
	// The camera alone, on three more seeds: the film's own choices part from any one run within a minute, so the screen
	// rules must hold on whatever shots a seed chooses, not on this run's alone. These play the fight as the scene now
	// does, at half the battle's clock (its rules run at twice life's pace): the camera's rules are timed on the screen.
	for (const uint32_t More : { 2u, 3u, 4u })
	{
		LandEye::Settings MoreSet = ES;
		MoreSet.Seed = Rec.Seed + More * 7919u;
		LandEye::Director Other;
		Other.Begin(Rec, EW, MoreSet);
		double B = 0.0, R = 0.0, Next = 0.0, Since = -1.0, Longest = 0.0, LongestAt = 0.0;
		int Looks = 0, Over = 0, OverLeave = 0, Crowded = 0, Filled = 0;
		FilmMeasure MoreFilm;
		MoreFilm.Rec = &Rec;
		MoreFilm.Set = MoreSet;
		while (B < Rec.End + 6.0)
		{
			const double BFrom = B;
			B += Dt * (B < Contact - 10.0 ? 2.0 : 0.5);
			R += Dt;
			PlaceMovers(B);
			const LandEye::Pose P = Other.Update(B, R, Dt);
			MoreFilm.Feed(Other, P, BFrom, B, R, Dt);
			if (P.bCut) Since = -1.0;
			if (P.Shot == LandEye::Kind::Open || P.Shot == LandEye::Kind::Wide || R < Next) continue;
			Next = R + 0.25;
			++Looks;
			if (Other.Blocked(P) > MoreSet.BlockedMost) ++Filled;
			if (!Other.Roomy(P)) ++Crowded;
			const double S = Other.Screened(P);
			if (S > MoreSet.ScreenMost) ++Over;
			if (S > MoreSet.ScreenLeave)
			{
				++OverLeave;
				if (Since < 0.0) Since = R;
				if (R - Since > Longest) { Longest = R - Since; LongestAt = B; }
			}
			else Since = -1.0;
		}
		const double Share = static_cast<double>(Over) / std::max(1, Looks);
		std::printf("\nlens, seed %u: %d cuts; screened over %.0f%% %.1f%% of looks, over %.0f%% %.1f%%, longest %.2f s (battle %.1f s); filled %.1f%%, crowded %.1f%%",
			More, Other.Cuts(), 100.0 * MoreSet.ScreenMost, 100.0 * Share, 100.0 * MoreSet.ScreenLeave, 100.0 * OverLeave / std::max(1, Looks), Longest, LongestAt,
			100.0 * Filled / std::max(1, Looks), 100.0 * Crowded / std::max(1, Looks));
		if (Share > 0.05) Fault("on another seed the picture was screened by things more than 5% of the time");
		if (Longest > 1.0) Fault("on another seed a shot stayed screened by things for over a second");
		if (static_cast<double>(Filled) / std::max(1, Looks) > 0.03) Fault("on another seed the picture was filled near the lens more than 3% of the time");
		if (static_cast<double>(Crowded) / std::max(1, Looks) > 0.03) Fault("on another seed the lens was too near a thing or a man more than 3% of the time");
		const std::string Label = "seed " + std::to_string(More);
		MoreFilm.Report(Label.c_str());
	}
	// The general's view (release 2: the player commands the battle himself): a scripted player zooms from the deploy
	// view to a man's shoulder, walks the view through the hedges, turns it round, slides it, runs it to the field's
	// edges, follows a tank and his own section, then wanders at random. The eye must never stand under the ground, in a
	// thing or among the stalks, the look-at never leave the field by more than its margin, and nothing jolt.
	{
		LandGeneralEye::Eye Gen;
		const double* Za = Rec.Ground.ZoneAtt;
		const double* Zd = Rec.Ground.ZoneDef;
		const double Ox = 0.5 * (Za[0] + Za[2]), Oy = 0.5 * (Za[1] + Za[3]);
		const double Ex = 0.5 * (Zd[0] + Zd[2]), Ey = 0.5 * (Zd[1] + Zd[3]);
		Gen.Begin(Rec, EW, Vec(Ox, Oy, 0.0), LandFeed::FacingTo(Ox, Oy, Ex, Ey), 250.0);
		int TankBlock = -1;
		for (const LandFeed::Block& Bk : Rec.Blocks)
		{
			if (Bk.Side == ES.Own && !Bk.Model.empty() && TankBlock < 0)
			{
				for (const LandFeed::Track& Tr : Rec.Tracks)
				{
					if (Tr.Block == Bk.Id && Tr.Role == "tank") { TankBlock = Bk.Id; break; }
				}
			}
		}
		uint64_t Dice = Rec.Seed * 0x9E3779B97F4A7C15ull + 99;
		auto Roll = [&Dice]() { Dice ^= Dice << 13; Dice ^= Dice >> 7; Dice ^= Dice << 17; return static_cast<double>(Dice % 1000000) / 1000000.0; };
		LandGeneralEye::Input Wander;
		double B = 0.0, R = 0.0, NextWander = 0.0, NextNotch = 0.0;
		int GFrames = 0, Under = 0, InThing = 0, InCorn = 0, OffField = 0, Lifted = 0, Followed = 0, Kicks = 0;
		double Lowest = 1e9, Highest = -1e9, LowestNear = 1e9, WorstKick = 0.0, WorstKickAt = 0.0, GSlowest = 0.0, Spent = 0.0;
		double Yaw = 0.0, Pitch = 0.0, SwingYaw = 0.0, SwingPitch = 0.0;
		int Steady = 0;
		double ZWas = 0.0, ZWas2 = 0.0, WorstRise = 0.0, WorstRiseAt = 0.0;
		int GCuts = 0;
		bool CutWas = false;
		const double End = 260.0;
		while (R < End)
		{
			LandGeneralEye::Input In;
			auto Notch = [&](double Way, double Every) { if (R >= NextNotch) { In.Zoom = Way; NextNotch = R + Every; } };
			if (R < 2.0) {}
			else if (R < 8.0) Notch(1.0, 0.25);
			else if (R < 28.0) In.PanAhead = 1.0;
			else if (R < 36.0) In.Turn = 45.0 * Dt;
			else if (R < 46.0) { In.PanRight = 1.0; In.PanAhead = 0.5; }
			else if (R < 50.0) Notch(-1.0, 1.0);
			else if (R < 60.0) { In.SlideRight = (static_cast<int>(R / 2.0) % 2 ? 1.0 : -1.0) * 0.012; In.SlideAhead = 0.006; }
			else if (R < 66.0) Notch(-1.0, 0.25);
			else if (R < 73.0) In.PanAhead = 1.0;
			else if (R < 80.0) { In.EdgeRight = -1.0; In.bLockEdgePan = R > 77.0; }
			else if (R < 110.0)
			{
				if (R - Dt < 80.0) In.GoBlock = TankBlock;
				In.bFollow = true;
				if (R < 84.0) Notch(1.0, 0.25);
			}
			else if (R < 140.0)
			{
				if (R - Dt < 110.0) In.GoBlock = Eye.SquadBlock();
				In.bFollow = true;
				if (R < 118.0) Notch(1.0, 0.3);
			}
			else
			{
				if (R >= NextWander)
				{
					NextWander = R + 1.5 + 3.0 * Roll();
					Wander = LandGeneralEye::Input();
					const double Pick = Roll();
					if (Pick < 0.45) { Wander.PanRight = Roll() * 2.0 - 1.0; Wander.PanAhead = Roll() * 2.0 - 1.0; }
					else if (Pick < 0.6) { Wander.EdgeRight = Roll() < 0.5 ? -1.0 : 1.0; Wander.bLockEdgePan = Roll() < 0.3; }
					else if (Pick < 0.75) Wander.Turn = (Roll() * 2.0 - 1.0) * 60.0 * Dt;
					else if (Pick < 0.9) Wander.Zoom = Roll() < 0.5 ? 1.0 : -1.0;
					else Wander.SlideRight = (Roll() * 2.0 - 1.0) * 0.01;
				}
				In = Wander;
				if (In.Zoom != 0.0) { if (R < NextNotch) In.Zoom = 0.0; else NextNotch = R + 0.3; }
			}
			B += Dt * (B < Contact - 10.0 ? 2.0 : 0.5);
			R += Dt;
			PlaceMovers(B);
			const auto GT0 = std::chrono::steady_clock::now();
			const LandEye::Pose P = Gen.Update(In, Dt, B);
			const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - GT0).count();
			Spent += Ms;
			GSlowest = std::max(GSlowest, Ms);
			if (Ms > 3.0 && std::getenv("LAND_PLAY_SLOW")) std::printf("  slow general's eye %.2f ms at real %.2f: zoom %.3f, lift %.2f need %.2f hurried %d aside %.2f waited %d braked %d, asked %d\n", Ms, R + Dt, Gen.Zoom(), Gen.LiftNow(), Gen.NeedNow(), Gen.HurriedNow(), Gen.PushNow(), Gen.WaitedFrames(), Gen.BrakedFrames(), Gen.AskedNow());
			++GFrames;
			const double G = GroundAt(P.Eye.X, P.Eye.Y);
			const double Over = P.Eye.Z - G;
			Lowest = std::min(Lowest, Over);
			Highest = std::max(Highest, P.Eye.Z - GroundAt(P.Look.X, P.Look.Y));
			if (Gen.Zoom() < 0.02) LowestNear = std::min(LowestNear, Over);
			if (Over < 0.5) ++Under;
			if (!EW.Free(P.Eye) || EW.Solid(P.Eye, -1) != 0) { ++InThing; if (std::getenv("LAND_PLAY_GEN")) std::printf("  general's eye in a thing at real %.2f: free %d solid %d, zoom %.3f, eye %.2f over the ground, following %d\n", R, EW.Free(P.Eye) ? 1 : 0, EW.Solid(P.Eye, -1), Gen.Zoom(), Over, Gen.Following()); }
			if (P.Eye.Z < G + CornAt(P.Eye.X, P.Eye.Y) + 0.1) ++InCorn;
			if (const char* Win = std::getenv("LAND_PLAY_GEN_AT"))
			{
				double W0 = 0.0, W1 = 0.0;
				if (std::sscanf(Win, "%lf,%lf", &W0, &W1) == 2 && R >= W0 && R <= W1)
				{
					const Vec La = Gen.LookAt();
					std::printf("  at %.3f: zoom %.4f heading %.2f look (%.2f %.2f) eye (%.2f %.2f %.2f) %.2f over, lift %.3f need %.3f hurried %d aside %.2f waited %d braked %d, free %d solid %d\n", R, Gen.Zoom(), Gen.Heading() * 57.2958, La.X, La.Y, P.Eye.X, P.Eye.Y, P.Eye.Z, Over, Gen.LiftNow(), Gen.NeedNow(), Gen.HurriedNow(), Gen.PushNow(), Gen.WaitedFrames(), Gen.BrakedFrames(), EW.Free(P.Eye) ? 1 : 0, EW.Solid(P.Eye, -1));
				}
			}
			const double M = Gen.Margin();
			const Vec At = Gen.LookAt();
			if (At.X < -M - 0.5 || At.Y < -M - 0.5 || At.X > Rec.Ground.W + M + 0.5 || At.Y > Rec.Ground.H + M + 0.5) ++OffField;
			Followed += Gen.Following() >= 0 ? 1 : 0;
			const Vec D = (P.Look - P.Eye).Unit();
			const double Y = std::atan2(D.X, D.Y) * 180.0 / 3.14159265358979, Pt = std::asin(std::clamp(D.Z, -1.0, 1.0)) * 180.0 / 3.14159265358979;
			if (GFrames > 1 && !P.bCut)
			{
				double Dy = Y - Yaw;
				while (Dy > 180.0) Dy -= 360.0;
				while (Dy < -180.0) Dy += 360.0;
				const double Sy = Dy / Dt, Sp = (Pt - Pitch) / Dt;
				if (Steady > 1)
				{
					const double K = std::hypot(Sy - SwingYaw, Sp - SwingPitch) / Dt;
					if (K > WorstKick) { WorstKick = K; WorstKickAt = R; }
					if (K > 1200.0) ++Kicks;
					if (K > 1200.0 && std::getenv("LAND_PLAY_KICKS")) std::printf("  general's kick %.0f at real %.2f: zoom %.3f, eye %.2f over the ground, yaw %.1f pitch %.1f\n", K, R, Gen.Zoom(), Over, Y, Pt);
				}
				SwingYaw = Sy;
				SwingPitch = Sp;
				++Steady;
			}
			GCuts += P.bCut && GFrames > 1 ? 1 : 0;
			if (GFrames > 2 && !P.bCut && !CutWas)
			{
				// (a rise or fall of the whole view, as the picture sees it: over the eye's distance to what it looks at)
				const double Az = ((P.Eye.Z - ZWas) - (ZWas - ZWas2)) / (Dt * Dt) / std::max(1.0, (P.Look - P.Eye).Length()) * 180.0 / 3.14159265358979;
				if (std::abs(Az) > WorstRise) { WorstRise = std::abs(Az); WorstRiseAt = R; }
				Lifted += std::abs(Az) > 1200.0 ? 1 : 0;
				if (std::abs(Az) > 1200.0 && std::getenv("LAND_PLAY_GEN")) std::printf("  general's eye jumps %.0f deg/s/s at real %.2f: zoom %.3f, eye %.2f over the ground, lift %.3f need %.3f hurried %d, aside %.2f, eye z %+.3f %+.3f\n", Az, R, Gen.Zoom(), Over, Gen.LiftNow(), Gen.NeedNow(), Gen.HurriedNow(), Gen.PushNow(), ZWas - ZWas2, P.Eye.Z - ZWas);
			}
			CutWas = P.bCut;
			ZWas2 = ZWas;
			ZWas = P.Eye.Z;
			Yaw = Y;
			Pitch = Pt;
		}
		std::printf("\ngeneral's view: %d frames over %.0f s; eye under the ground %d, in a thing %d, among the stalks %d, look-at off the field %d; eye %.2f m over the ground at the lowest (%.2f at the nearest zoom), %.0f m over the look-at at the highest; followed a block %.0f s; worst look kick %.0f deg/s/s (real %.1f s), %d frames over 1200; the view's worst rise or fall %.0f deg/s/s as the picture sees it (real %.1f s), %d frames over 1200; waited at a thing %d frames, braked %d, cut %d times; %.3f ms a frame on average, %.2f ms the slowest",
			GFrames, End, Under, InThing, InCorn, OffField, Lowest, LowestNear, Highest, Followed * Dt, WorstKick, WorstKickAt, Kicks, WorstRise, WorstRiseAt, Lifted, Gen.WaitedFrames(), Gen.BrakedFrames(), GCuts, Spent / std::max(1, GFrames), GSlowest);
		if (Under > 0) Fault("the general's view went under the ground");
		if (InThing > 0) Fault("the general's view stood in a thing or a man");
		if (InCorn > 0) Fault("the general's view stood among the stalks");
		if (OffField > 0) Fault("the general's view looked off the field past its margin");
		if (Kicks > 0) Fault("the general's view jolted (a look kick over 1200 deg/s/s)");
		if (Lifted > 0) Fault("the general's view jolted up or down (over 1200 deg/s/s as the picture sees it)");
		if (LowestNear > 2.5) Fault("the general's view never came down to a man's shoulder");
		if (Highest < 300.0) Fault("the general's view never rose to look down on the battle");
		if (GSlowest > 8.0) Fault("a frame of the general's view took over 8 ms");
	}
	// A tank's own shot and her knock-out count as action for a lens straight off her nose or tail (her hull runs 3.2 m
	// from her middle: the pre-check found it hid her own shot from the dead-shot rule within some 35-40 degrees of
	// her nose or tail, the head-on "Tiger fires at the lens" shot). Each at 30, 60 and 120 m, where nothing else is in
	// the way.
	{
		LandEye::Director Fresh;
		Fresh.Begin(Rec, EW, ES);
		int Tried = 0, Counted = 0, Hidden = 0;
		auto Try = [&](size_t Tk, double When, double Up)
		{
			const LandFeed::Track& Tr = Rec.Tracks[Tk];
			PlaceMovers(When);
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, When);
			const double Gz = GroundAt(S.X, S.Y);
			for (const double Turn : { 0.0, 3.14159265358979 })
			{
				for (const double Far : { 30.0, 60.0, 120.0 })
				{
					LandEye::Pose P;
					P.Eye = Vec(S.X + std::sin(S.F + Turn) * Far, S.Y + std::cos(S.F + Turn) * Far, 0.0);
					P.Eye.Z = GroundAt(P.Eye.X, P.Eye.Y) + 1.6;
					P.Look = Vec(S.X, S.Y, Gz + 1.0);
					if (!Fresh.TrackSeen(P.Eye, static_cast<int>(Tk), When, Up)) { ++Hidden; continue; }
					++Tried;
					if (Fresh.ActionSeen(P, When - 0.01, When + 0.01)) ++Counted;
					else std::printf("\n  not counted: track %zu (%s) at %.2f s (fell %.2f), %s %.0f m", Tk, Tr.Role.c_str(), When, Tr.Fell, Turn > 1.0 ? "tail" : "nose", Far);
				}
			}
		};
		for (size_t Tk = 0; Tk < Rec.Tracks.size(); ++Tk)
		{
			const LandFeed::Track& Tr = Rec.Tracks[Tk];
			if (Tr.Role != "tank" || Tr.bTwin) continue;
			for (const LandFeed::Round& R : Rec.Rounds)
			{
				if (R.T < Tr.Start() || (Tr.Fell >= 0.0 && R.T >= Tr.Fell)) continue;
				const LandFeed::Track* By = Rec.TrackOf(R.By, R.K, R.J);
				if (By == &Tr) { Try(Tk, R.T, 1.0); break; } // (her first round)
			}
			if (Tr.Fell >= 0.0) Try(Tk, Tr.Fell, 0.8);
		}
		std::printf("\nlens: a tank's own shot or knock-out seen off her nose and tail at 30, 60, 120 m: %d of %d counted as action (%d places with something else in the way)", Counted, Tried, Hidden);
		if (Tried == 0) Fault("no tank shot to look at off her nose or tail");
		if (Counted < Tried) Fault("a tank's own hull hid her shot or knock-out from the dead-shot rule");
	}
	std::printf("\ncorn: %d frames of the low shot over corn, the lens near the ground at least %.2f m over the ears (%d frames among the stalks, %d behind stalks a step or two in front)\n", LowOverCorn, LeastOverEars < 1e8 ? LeastOverEars : 0.0, LowInCorn, LensBehindStalks);
	// The forest ambush (Brian, 22 November): every enemy block hidden from the player through the first minute of the
	// fight; the film never takes one of their men or tanks as its subject, and still never goes up to an aerial.
	{
		LandEye::World Wh = EW;
		const double HideTo = Contact + 60.0;
		double HideClock = 0.0;
		Wh.Hidden = [&Rec, &ES, &HideClock, HideTo](int Block)
		{
			const LandFeed::Block* B = Rec.BlockOf(Block);
			return B && B->Side != ES.Own && HideClock < HideTo;
		};
		LandEye::Director Hid;
		Hid.Begin(Rec, Wh, ES);
		int OnHidden = 0, Shots = 0, Aerials = 0;
		const double From = std::max(0.0, Contact - 10.0);
		for (double B = From; B < std::min(Rec.End, HideTo + 10.0); B += Dt)
		{
			HideClock = B;
			PlaceMovers(B);
			const LandEye::Pose P = Hid.Update(B, 2000.0 + (B - From), Dt);
			Shots += P.bCut ? 1 : 0;
			if (P.bCut && P.Shot == LandEye::Kind::Wide && B > 12.0 && !Hid.MassShot()) ++Aerials;
			if (P.Subject >= 0 && B < HideTo)
			{
				const LandFeed::Block* Bk = Rec.BlockOf(Rec.Tracks[static_cast<size_t>(P.Subject)].Block);
				if (Bk && Bk->Side != ES.Own) ++OnHidden;
			}
		}
		std::printf("ambush: every enemy block hidden to %.0f s: %d shots, %d frames on a hidden enemy, %d aerials\n", HideTo, Shots, OnHidden, Aerials);
		if (OnHidden > 0) Fault("the film took a hidden enemy as its subject (it gives the ambush away)");
		if (Shots < 3) Fault("with the enemy hidden the film hardly cut");
	}
	// Ride with this unit (Brian's yes, 22 November 03:36Z): asked to stay with one block, the film cuts to its men at
	// once and stays with them, by every screen rule, until let go; then it goes back to the field. A block whose last man
	// goes down or off the field lets go of itself, with no aerial for want of a man.
	{
		// (the own side's foot block with the most men up 10 s after contact; and the block of three or more whose last man
		// is down or gone earliest, at least 40 s after contact)
		auto Up = [&Rec](int Block, double T)
		{
			int N = 0;
			for (const LandFeed::Track& Tr : Rec.Tracks)
				if (Tr.Block == Block && !Tr.bTwin && Tr.IsMan() && Tr.Start() <= T && !(Tr.Fell >= 0.0 && Tr.Fell <= T) && !(Tr.Out >= 0.0 && Tr.Out <= T)) ++N;
			return N;
		};
		const double RideFrom = Contact + 10.0, RideTo = std::min(Rec.End - 10.0, Contact + 90.0);
		int RideBlk = -1, RideMen = 0, GoneBlk = -1;
		double GoneAt = 1e18;
		for (const LandFeed::Block& Bk : Rec.Blocks)
		{
			if (Bk.Side == ES.Own && Bk.Kind == "inf")
			{
				const int N = Up(Bk.Id, RideFrom);
				if (N > RideMen) { RideMen = N; RideBlk = Bk.Id; }
			}
			// (its last man: the latest fall or leaving of its men, every one of them down or gone)
			double Last = -1.0;
			int Men = 0;
			bool bAll = true;
			for (const LandFeed::Track& Tr : Rec.Tracks)
			{
				if (Tr.Block != Bk.Id || Tr.bTwin || Tr.Role == "lorry" || Tr.Role == "gun") continue;
				++Men;
				const double End = Tr.Fell >= 0.0 ? Tr.Fell : Tr.Out;
				if (End < 0.0) { bAll = false; break; }
				Last = std::max(Last, End);
			}
			if (bAll && Men >= 3 && Last > Contact + 40.0 && Last < Rec.End - 15.0 && Last < GoneAt) { GoneAt = Last; GoneBlk = Bk.Id; }
		}
		if (RideBlk >= 0)
		{
			LandEye::Settings RS = ES;
			LandEye::Director Rd;
			Rd.Begin(Rec, EW, RS);
			FilmMeasure RideFilm;
			RideFilm.Rec = &Rec;
			RideFilm.Set = RS;
			// (riding through the whole mass's window: none is taken while riding, so its own count is not asked here)
			RideFilm.bMassGate = false;
			double B = 0.0, R = 0.0, AskedAt = -1.0, OnAt = -1.0, LetGoAt = -1.0, ElsewhereAt = -1.0;
			int Riding = 0, OnIt = 0, Cuts = 0, Aerials = 0;
			while (B < Rec.End + 6.0)
			{
				const double BFrom = B;
				B += Dt * (B < Contact - 10.0 ? 2.0 : 0.5);
				R += Dt;
				PlaceMovers(B);
				if (AskedAt < 0.0 && B >= RideFrom) { Rd.Ride(RideBlk); AskedAt = R; }
				if (LetGoAt < 0.0 && B >= RideTo) { Rd.Ride(-1); LetGoAt = R; }
				const LandEye::Pose P = Rd.Update(B, R, Dt);
				RideFilm.Feed(Rd, P, BFrom, B, R, Dt);
				const bool bOn = P.Subject >= 0 && Rec.Tracks[static_cast<size_t>(P.Subject)].Block == RideBlk;
				if (AskedAt >= 0.0 && LetGoAt < 0.0)
				{
					if (OnAt < 0.0 && bOn) OnAt = R;
					if (OnAt >= 0.0 && Rd.Riding() == RideBlk)
					{
						++Riding;
						if (bOn) ++OnIt;
						if (P.bCut) ++Cuts;
						if (P.bCut && P.Shot == LandEye::Kind::Wide && B < Rec.End) ++Aerials;
					}
				}
				if (LetGoAt >= 0.0 && ElsewhereAt < 0.0 && P.Subject >= 0 && !bOn) ElsewhereAt = R;
			}
			const double Share = static_cast<double>(OnIt) / std::max(1, Riding);
			std::printf("ride: block %d (%d men up) asked at %.0f s, let go at %.0f s: on its men %.2f s after the ask, then %d cuts, %.1f%% of the frames on its own men, %d aerials; back to the field %.1f s after letting go",
				RideBlk, RideMen, RideFrom, RideTo, OnAt - AskedAt, Cuts, 100.0 * Share, Aerials, ElsewhereAt >= 0.0 ? ElsewhereAt - LetGoAt : -1.0);
			RideFilm.Report("ride");
			std::printf("\n");
			if (OnAt < 0.0 || OnAt - AskedAt > 1.0) Fault("asked to ride with a unit, the film was not on its men within a second");
			if (Share < 0.97) Fault("riding with a unit, the film left its men");
			if (Aerials > 0) Fault("riding with a unit, the film went up to an aerial");
			if (ElsewhereAt < 0.0 || ElsewhereAt - LetGoAt > 25.0) Fault("let go of the unit, the film did not go back to the field");
		}
		if (GoneBlk >= 0)
		{
			LandEye::Director Rd;
			Rd.Begin(Rec, EW, ES);
			double B = 0.0, R = 0.0, FreeAt = -1.0;
			int Aerials = 0;
			bool bAsked = false;
			while (B < std::min(Rec.End, GoneAt + 30.0))
			{
				B += Dt * (B < Contact - 10.0 ? 2.0 : 0.5);
				R += Dt;
				PlaceMovers(B);
				if (!bAsked && B >= GoneAt - 30.0) { Rd.Ride(GoneBlk); bAsked = true; }
				const LandEye::Pose P = Rd.Update(B, R, Dt);
				if (bAsked && FreeAt < 0.0 && Rd.Riding() < 0) FreeAt = B;
				if (bAsked && P.bCut && P.Shot == LandEye::Kind::Wide && B < Rec.End && !Rd.MassShot()) ++Aerials;
			}
			std::printf("ride: block %d's last man down or gone at %.1f s; the ride let go of itself at %.1f s, %d aerials\n", GoneBlk, GoneAt, FreeAt, Aerials);
			if (FreeAt < 0.0 || FreeAt > GoneAt + 15.0) Fault("riding with a unit wiped out, the film did not let go of it");
			if (Aerials > 0) Fault("riding with a unit wiped out, the film went up to an aerial");
		}
	}
	// The art director's fuel ball keep-out (16 m of a vehicle's ball, its first 2 s on the screen) seldom bites on these
	// recordings: the film's own shots of a tank knocked out stand further off. So the rule is shown working here at a
	// reach wide enough to bite (60 m): no shot placed in it, and one held there cut on the flash.
	{
		LandEye::Settings FS = ES;
		FS.FuelVehicle = 60.0;
		LandEye::Director Fd;
		Fd.Begin(Rec, EW, FS);
		FilmMeasure FuelFilm;
		FuelFilm.Rec = &Rec;
		FuelFilm.Set = FS;
		double B = 0.0, R = 0.0;
		while (B < Rec.End + 6.0)
		{
			const double BFrom = B;
			B += Dt * (B < Contact - 10.0 ? 2.0 : 0.5);
			R += Dt;
			PlaceMovers(B);
			const LandEye::Pose P = Fd.Update(B, R, Dt);
			FuelFilm.Feed(Fd, P, BFrom, B, R, Dt);
		}
		FuelFilm.Report("fuel balls kept at 60 m");
		std::printf("\n");
	}
	// The art director's dark rule (cut 4 fix 6: over 30% of a frame under L 25, reject the shot). The game's scene reads
	// its own drawn frame; the rays cannot see light, so here a stand-in for that read: the share of the picture short of
	// what the lens looks at (out to 40 m) that is a thing (a hedge's shrubs, a crown, a hull) or a bank's face across its
	// upper half, as the shaded faces of the cut 4 stills were. Read four times a second of the frame last drawn.
	{
		auto DarkOf = [&Sight, &GroundAt](const LandEye::Pose& P) -> double
		{
			const Vec Way = P.Look - P.Eye;
			const double Long = Way.Length();
			if (Long < 1e-6) return 0.0;
			const Vec Fwd = Way * (1.0 / Long);
			Vec Right(Fwd.Y, -Fwd.X, 0.0);
			Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
			const Vec Up(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
			const double TanV = std::tan(P.Fov * 0.5 * 3.14159265358979 / 180.0), TanH = TanV * 16.0 / 9.0;
			const double Far = std::min(40.0, std::max(2.0, Long - 2.0));
			int Dark = 0;
			for (int R = 0; R < 5; ++R)
			{
				for (int C = 0; C < 8; ++C)
				{
					const double Sx = (C + 0.5) / 8.0 * 2.0 - 1.0, Sy = (R + 0.5) / 5.0 * 2.0 - 1.0;
					const Vec Dir = (Fwd + Right * (Sx * TanH) + Up * (Sy * TanV)).Unit();
					for (double D = 0.3; D <= Far; D += D < 6.0 ? 0.35 : 1.0)
					{
						const Vec Q = P.Eye + Dir * D;
						if (Q.Z < GroundAt(Q.X, Q.Y)) { Dark += Sy > 0.0 && D <= 12.0 ? 1 : 0; break; }
						const int What = static_cast<int>(Sight.At(Q, P.Subject));
						if (What == 2) { ++Dark; break; }
						if (What == 1) break;
					}
				}
			}
			return Dark / 40.0;
		};
		LandEye::World Wd = EW;
		double DarkRead = -1.0;
		Wd.DarkNow = [&DarkRead]() { return DarkRead; };
		LandEye::Director Dk;
		Dk.Begin(Rec, Wd, ES);
		int Shots = 0, Looked = 0, DarkFrames = 0, Again = 0;
		double Stretch = 0.0, Longest = 0.0, LongestAt = 0.0, ShotAt = 0.0, NextRead = 0.0;
		std::vector<Vec> LeftAt;
		LandEye::Pose Drawn;
		bool bDrawn = false;
		for (double B = 0.0; B < Rec.End + 3.0; B += Dt)
		{
			const double R = 3000.0 + B;
			if (bDrawn && R >= NextRead)
			{
				NextRead = R + 0.25;
				DarkRead = Drawn.Shot == LandEye::Kind::Open || Drawn.Shot == LandEye::Kind::Wide ? 0.0 : DarkOf(Drawn);
			}
			PlaceMovers(B);
			const LandEye::Pose P = Dk.Update(B, R, Dt);
			if (P.bCut)
			{
				++Shots;
				ShotAt = R;
				if (Dk.LastWhy() == LandEye::Why::Dark) LeftAt.push_back(Drawn.Eye);
				else
					for (const Vec& W : LeftAt)
						if (P.Shot != LandEye::Kind::Open && P.Shot != LandEye::Kind::Wide && std::hypot(P.Eye.X - W.X, P.Eye.Y - W.Y) < ES.DarkClear - 0.5) ++Again;
			}
			Drawn = P;
			bDrawn = true;
			++Looked;
			// (a stretch over the line past a shot's first half second, read as the scene reads it, the rule not resting)
			const bool bOver = DarkRead > ES.DarkLeave && R - ShotAt > 0.5 && Dk.DarkRun() < ES.DarkRunMost;
			DarkFrames += DarkRead > ES.DarkLeave ? 1 : 0;
			Stretch = bOver ? Stretch + Dt : 0.0;
			if (Stretch > Longest) { Longest = Stretch; LongestAt = B; }
		}
		std::printf("dark (a stand-in for the drawn frame's share under L 25): %d shots, %d left for a dark picture; frames over %.0f%% %.1f%%, the longest stretch past a shot's first half second %.2f s (battle %.1f s); %d places within %.0f m of a dark one taken again\n",
			Shots, Dk.DarkCuts(), ES.DarkLeave * 100.0, 100.0 * DarkFrames / std::max(1, Looked), Longest, LongestAt, Again, ES.DarkClear);
		if (Longest > ES.DarkHold + 0.6) Fault("a shot held a dark picture past the dark rule");
		if (Again > 0) Fault("a shot stood again where one was left for a dark picture");
	}
	// The picture run's low shot asked of a man in the corn (as the art director's 04 asks one): it keeps its kind and
	// is held just over the ears, never among the stalks.
	{
		int InCorn = -1;
		double AskAt = 0.0;
		for (size_t K = 0; K < Rec.Tracks.size() && InCorn < 0; ++K)
		{
			const LandFeed::Track& T = Rec.Tracks[K];
			if (T.bTwin || !T.IsMan() || T.Role == "mortar" || T.Role == "fo") continue;
			for (double When = std::max(T.Start(), Contact); When < Rec.End - 6.0; When += 5.0)
			{
				if (T.Fell >= 0.0 && T.Fell < When + 6.0) break;
				const LandFeed::Spot Sp = LandFeed::SpotAt(T, When);
				// (well in it, the corn all round him: at a field's edge the low eye beside him stands on the grass, as
				// the platoon recording's first man in the corn did)
				bool bDeep = CornAt(Sp.X, Sp.Y) > 0.5;
				for (int Q = 0; Q < 8 && bDeep; ++Q) bDeep = CornAt(Sp.X + 2.5 * std::sin(Q * 0.785398), Sp.Y + 2.5 * std::cos(Q * 0.785398)) > 0.5;
				if (bDeep) { InCorn = static_cast<int>(K); AskAt = When; break; }
			}
		}
		if (InCorn >= 0)
		{
			LandEye::Director Asked;
			Asked.Begin(Rec, EW, ES);
			int Seen = 0, Among = 0;
			double Least = 1e9;
			for (int F = 0; F < 4 * 60; ++F)
			{
				if (F == 0) Asked.Ask(LandEye::Kind::Low, InCorn);
				PlaceMovers(AskAt + F * Dt);
				const LandEye::Pose P = Asked.Update(AskAt + F * Dt, 1000.0 + F * Dt, Dt);
				if (std::getenv("LAND_PLAY_CORN") && F % 20 == 0) std::printf("  corn ask frame %d: %s on %d, eye corn %.2f, eye %.2f over the ground, man corn %.2f\n", F, LandEye::NameOf(P.Shot), P.Subject, CornAt(P.Eye.X, P.Eye.Y), P.Eye.Z - GroundAt(P.Eye.X, P.Eye.Y), CornAt(LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(InCorn)], AskAt + F * Dt).X, LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(InCorn)], AskAt + F * Dt).Y));
				if (P.Shot != LandEye::Kind::Low) continue;
				const double Corn = CornAt(P.Eye.X, P.Eye.Y);
				if (Corn <= 0.0) continue;
				++Seen;
				const double Over = P.Eye.Z - GroundAt(P.Eye.X, P.Eye.Y) - Corn;
				Least = std::min(Least, Over);
				if (Over < 0.05) ++Among;
			}
			std::printf("corn: a low shot asked of track %d in the corn at %.0f s: %d frames over corn, at least %.2f m over the ears\n", InCorn, AskAt, Seen, Seen ? Least : 0.0);
			if (Among > 0) Fault("an asked low shot is down among the corn");
			if (Seen == 0) Fault("an asked low shot on a man in the corn was not taken over the corn");
		}
	}
	// The shell holes: torn earth, brown, a rim standing over the ground, each round's size; and their cost.
	{
		size_t Points = 0, Tris = 0;
		int Bad = 0, NotBrown = 0, FlatRim = 0, Sections = 0, Astray = 0, BadNumber = 0, BadSunk = 0, BadIndex = 0, BadBare = 0;
		std::vector<const LandField::Mesh*> Each;
		for (int K = 0; K < Holes.Patches(); ++K)
		{
			for (const LandField::Mesh& M : Holes.PatchAt(K).Sections)
			{
				Each.push_back(&M);
			}
		}
		for (const LandField::Mesh* Mp : Each)
		{
			const LandField::Mesh& M = *Mp;
			++Sections;
			Points += M.Points();
			Tris += M.Tris();
			for (size_t I = 0; I < M.Points(); ++I)
			{
				const float* P = &M.Positions[I * 3];
				const float* N = &M.Normals[I * 3];
				if (!std::isfinite(P[0]) || !std::isfinite(P[1]) || !std::isfinite(P[2]) || std::abs(std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]) - 1.0f) > 1e-3f) { ++Bad; ++BadNumber; }
			}
			for (const uint32_t T : M.Triangles) if (T >= M.Points()) { ++Bad; ++BadIndex; }
			for (const LandField::Rgb8& C : M.Colours)
			{
				// (earth: red over green over blue, or the torn turf's olive; the damp hollow dark, but never a grey or a black)
				const bool bBrown = C.R >= C.B + 10 && C.G >= C.B + 6 && C.R + C.G + C.B >= 75;
				if (!bBrown) ++NotBrown;
			}
		}
		// each hole's crest over the ground: its highest point within its radius at least half its rim; its middle bare
		const std::vector<LandCrater::Field::Hole>& All = Holes.All();
		for (size_t Hk = 0; Hk < All.size(); ++Hk)
		{
			const LandCrater::Field::Hole& H = All[Hk];
			double Most = 0.0;
			const LandField::Mesh& M = Holes.Section(H.In);
			// (its middle in its patch's square: the patches are places, not the order the holes were laid in)
			const LandCrater::Field::Patch& Pa = Holes.PatchAt(H.In.Patch);
			if (std::floor(H.X / LandCrater::Field::PatchSize) != Pa.I || std::floor(H.Y / LandCrater::Field::PatchSize) != Pa.J) ++Astray;
			for (size_t I = 0; I < M.Points(); ++I)
			{
				const float* P = &M.Positions[I * 3];
				if (std::hypot(P[0] - H.X, P[1] - H.Y) < 1.15 * H.R) Most = std::max(Most, P[2] - Rec.Ground.HeightAt(P[0], P[1]));
			}
			// (a hole torn in an older dug one is laid on that one's floor, under the ground)
			bool bInOlder = false;
			for (size_t Ok = 0; Ok < Hk && !bInOlder; ++Ok) bInOlder = !All[Ok].Cut.empty() && std::hypot(All[Ok].X - H.X, All[Ok].Y - H.Y) < All[Ok].R + 1.2 * H.R;
			if (Most < 0.5 * H.Rim && !bInOlder) ++FlatRim;
			if (!Holes.Bare(H.X, H.Y)) { ++Bad; ++BadBare; }
			// (nothing it laid sunk under the ground, but a dug hole's bowl: its own points, each laid on the ground there;
			// the points a later hole's cut made between them lie on its triangles, as the ground between its points may
			// not)
			const size_t Own = 1u + static_cast<size_t>(9 * H.Sides + 6 * H.Clods);
			for (size_t I = H.First; I < H.First + Own && I < M.Points(); ++I)
			{
				const float* P = &M.Positions[I * 3];
				if (P[2] < Rec.Ground.HeightAt(P[0], P[1]) - 0.02 && !Holes.InCut(P[0], P[1]))
				{
					++Bad;
					++BadSunk;
					if (std::getenv("LAND_PLAY_SUNK")) std::printf("  sunk: hole %zu (R %.2f, deep %.2f, cut %zu) its point %zu (ring %zu) at %.2f radii, %.3f m under\n", Hk, H.R, H.Deep, H.Cut.size() / 2, I - H.First, (I - H.First - 1) / static_cast<size_t>(H.Sides), std::hypot(P[0] - H.X, P[1] - H.Y) / H.R, Rec.Ground.HeightAt(P[0], P[1]) - P[2]);
				}
			}
		}
		std::printf("holes: %d laid (", Holes.Count());
		for (auto& P : HolesBy) std::printf(" %s %d", P.first.c_str(), P.second);
		std::printf(" ), %zu points and %zu triangles in %d sections of %d patches (one mesh each); %.3f ms each on average, %.3f ms the most\n", Points, Tris, Sections, Holes.Patches(), HoleMs / std::max(1, Holes.Count()), HoleMsMost);
		std::printf("holes sent again in a frame: at most %zu points in %d meshes, the widest mesh %.1f m across\n", HoleSentMost, HoleSentMeshesMost, HoleMeshWidest);
		int DugCount = 0;
		for (const LandCrater::Field::Hole& H : All) DugCount += !H.Cut.empty();
		std::printf("dug: %d of the holes dug, %d cuts; %d tiles split; the ground cut in %.3f ms a frame at most (%.1f ms in all); at most %zu points in %d parts sent again in a frame\n",
			DugCount, Dug.Cuts(), DugSplit, DugMsMost, DugMs, DugSentMost, DugSentPartsMost);
		if (Holes.Count() > 20 && DugCount == 0) Fault("no shell hole was dug");
		if (DugMsMost > 8.0) Fault("cutting the ground for a frame's holes took more than 8 ms");
		if (DugSentPartsMost > 0 && DugSentMost > static_cast<size_t>(DugSentPartsMost) * 4000) Fault("a dug part sent again holds more than 4000 points");
		if (Astray > 0) Fault("a shell hole is kept in a patch its middle is not in");
		if (HolePatchesSeen != Holes.Patches()) Fault("a shell hole patch was never sent to the game");
		// (a hole's thrown soil and clods reach some 2.5 times its radius: a 150's 5 m past the square's edge)
		if (HoleMeshWidest > LandCrater::Field::PatchSize + 12.0) Fault("a shell hole mesh the game sends again is wider than its patch and the thrown soil round it");
		if (Bad > 0)
		{
			std::printf("holes broken: %d at no number or a normal not a unit, %d sunk under the ground outside every cut, %d indices past their points, %d middles not bare\n", BadNumber, BadSunk, BadIndex, BadBare);
			Fault("a shell hole's mesh is broken (a point at no number, a normal not a unit, an index past its points, sunk under the ground, its middle not bare)");
		}
		if (NotBrown > 0) Fault("a shell hole has a colour that is no earth's");
		if (FlatRim > 0) Fault("a shell hole's rim does not stand over the ground");
		if (Holes.Count() > 0 && Points > static_cast<size_t>(Holes.Count()) * 400) Fault("the shell holes cost more than 400 points each");
		// each round's size: a mortar's under a 75's under an 88's under a 105's
		const double Mortar = LandCrater::SizeOf("mortar", 81.0).Across, G75 = LandCrater::SizeOf("gun75", 75.0).Across, G88 = LandCrater::SizeOf("gun75", 88.0).Across, H105 = LandCrater::SizeOf("howitzer", 105.0).Across;
		if (!(Mortar < G75 && G75 < G88 && G88 < H105)) Fault("the shell holes are not sized by the round");
		if (LandCrater::SizeOf("grenade", 0.0).Across <= 0.0) Fault("a grenade leaves no scuff");
	}
	std::printf("falls: %d men, %d in the picture as they fell, %d within 150 m of the eye; tanks knocked out %d, %d in the picture\n", Falls, FallsSeen, FallsNear, Knocked, KnockedSeen);
	std::printf("bar: starts %.3f, ends %.3f (%s won)\n", KnotStart, KnotEnd, Rec.Result.Winner.c_str());
	std::printf("cores: %.3f ms a frame on average, %.2f ms the slowest, over %d frames; the camera alone %.3f ms on average, %.2f ms the slowest, %d frames over 2 ms\n",
		Total / std::max(1, Frames), Slowest, Frames, EyeTotal / std::max(1, Frames), EyeSlowest, EyeOver2);
	{
		std::vector<long> Sorted = StepsByFrame;
		std::sort(Sorted.begin(), Sorted.end());
		const long Most = Sorted.empty() ? 0 : Sorted.back();
		const long P999 = Sorted.empty() ? 0 : Sorted[std::min(Sorted.size() - 1, static_cast<size_t>(Sorted.size() * 0.999))];
		std::printf("camera's work: the most ray steps in a frame %ld, 99.9%% of frames under %ld; a cut chosen over %d frames at most; the whole mass placed in a frame of %ld steps (%.2f ms)\n", Most, P999, Eye.LongestChoice(), MassSteps, MassMs);
	}
	{
		std::vector<double> Sorted = FrameCosts;
		std::sort(Sorted.begin(), Sorted.end());
		const double P99 = Sorted.empty() ? 0.0 : Sorted[std::min(Sorted.size() - 1, static_cast<size_t>(Sorted.size() * 0.99))];
		int Busiest = 0;
		double BusiestMs = 0.0;
		for (const auto& Kv : BySecond)
		{
			const double Mean = Kv.second.first / std::max(1, Kv.second.second);
			if (Mean > BusiestMs) { BusiestMs = Mean; Busiest = Kv.first; }
		}
		std::printf("cores: 99%% of frames under %.2f ms; the slowest at battle %.1f s; the busiest battle second %d s at %.2f ms a frame\n", P99, SlowestAt, Busiest, BusiestMs);
	}
	if (argc > 3)
	{
		std::ofstream Log(std::string(argv[3]) + "/" + Rec.Key + "-cuts.txt");
		for (const std::string& L : CutLines) Log << L << "\n";
		// Every take the battle started, and how often (which recordings the game must carry in Sounds/Real).
		std::ofstream Heard(std::string(argv[3]) + "/" + Rec.Key + "-takes.txt");
		for (auto& P : ByCount) Heard << P.second << " " << P.first << "\n";
	}
	std::printf("%s\n", Faults == 0 ? "PASS" : ("FAIL: " + std::to_string(Faults) + " faults").c_str());
	return Faults == 0 ? 0 : 1;
}
