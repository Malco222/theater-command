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
#include "Battle/LandNight.h"
#include "Battle/LandPlay.h"
#include "Battle/LandSight.h"
#include "Battle/LandSound.h"
#include "Battle/LandWalkPan.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
		double CutReal = 0.0;
		double LostSince = -1.0, LostLongest = 0.0, LostAt = 0.0;
		/** The special shot the longest lost spell ended in, and the shot's kind (the special shots' diagnosis). */
		LandEye::Special LostSp = LandEye::Special::None;
		LandEye::Kind LostKind = LandEye::Kind::Open;
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
		// (the production lead's GS3 note: men the scene would draw at its near level by their height on the screen, over
		// 150 of the 665 pixels drawn, by the kind of shot; the long lens's Track and far Watch must not put more men near)
		bool bNearReport = false;
		long NearSum[7] = {}, NearFrames[7] = {};
		int NearMost[7] = {};
		double NearMostAt[7] = {};
		// (the whole mass, the production lead's CD1: one wide shot in its window holding 80% of the men drawn in its
		// picture, every frame of it; every man on the field the player can see, the batteries far back and the fallen too)
		int MassShots = 0, MassMen = 0;
		bool bMassGate = true;
		double MassTallLeast = 1.0, MassWholeLeast = 1.0, MassStretchWide = 0.0;
		bool MassStretched = false;
		int MassStretchMen = 0;
		double MassFrom = 0.0, MassTo = 0.0, MassStart = -1.0, MassReal = 0.0, MassLeast = 1.0, MassLeastAt = 0.0, MassHeight = 0.0, MassFar = 0.0;

		void Feed(const LandEye::Director& D, const LandEye::Pose& P, double From, double B, double R, double Dt)
		{
			++Frames;
			const bool bAir = P.Shot == LandEye::Kind::Open || P.Shot == LandEye::Kind::Wide;
			if (P.bCut) { BurstDue = -1.0; LostSince = -1.0; CutReal = R; }
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
				int Men = 0, In = 0, Tall = 0, StretchMen = 0, StretchIn = 0;
				// (a front too wide for one picture: the share of the men in the stretch it holds is the gate, the whole
				// front's logged beside it; a man is in the stretch by where he stood at the cut)
				Vec SAt, SAlong;
				double SFrom = 0.0, STo = 0.0;
				const bool bStretched = D.MassStretch(SAt, SAlong, SFrom, STo);
				// (and how many of them stand 2 px or more tall on the 665 px the Mac draws: the ground scene's GS3 culls the
				// men under that, so they are seen in the wide only as the men over it)
				const Vec Fwd = (P.Look - P.Eye).Unit();
				const double PxAMetre = 665.0 / (2.0 * std::tan(P.Fov * 0.5 * 3.14159265358979 / 180.0));
				for (size_t I = 0; I < Rec->Tracks.size(); ++I)
				{
					const LandFeed::Track& Tr = Rec->Tracks[I];
					if (Tr.bTwin || !Tr.IsMan() || Tr.Start() > B || (Tr.Out >= 0.0 && Tr.Out < B) || D.HiddenNow(static_cast<int>(I))) continue;
					const LandFeed::Spot S = LandFeed::SpotAt(Tr, B);
					++Men;
					const Vec At(S.X, S.Y, Rec->Ground.HeightAt(S.X, S.Y) + 0.9);
					bool bInStretch = false;
					if (bStretched)
					{
						const LandFeed::Spot S0 = LandFeed::SpotAt(Tr, D.MassAt());
						const double U = (Vec(S0.X, S0.Y, 0.0) - Vec(SAt.X, SAt.Y, 0.0)).Dot(SAlong);
						bInStretch = U >= SFrom && U <= STo;
						StretchMen += bInStretch ? 1 : 0;
					}
					if (!LandEye::InPicture(P, At)) continue;
					++In;
					StretchIn += bInStretch ? 1 : 0;
					const double Depth = (At - P.Eye).Dot(Fwd);
					if (Depth > 0.5 && 1.8 / Depth * PxAMetre >= 2.0) ++Tall;
				}
				MassTallLeast = std::min(MassTallLeast, static_cast<double>(Tall) / std::max(1, In));
				const double Whole = static_cast<double>(In) / std::max(1, Men);
				MassWholeLeast = std::min(MassWholeLeast, Whole);
				const double Share = bStretched ? static_cast<double>(StretchIn) / std::max(1, StretchMen) : Whole;
				if (bStretched) { MassStretched = true; MassStretchMen = StretchMen; MassStretchWide = STo - SFrom; }
				if (Share < MassLeast) { MassLeast = Share; MassLeastAt = B; MassMen = Men; }
			}
			if (bNearReport)
			{
				const Vec Way = P.Look - P.Eye;
				const double TanV = std::tan(P.Fov * 0.5 * 3.14159265358979 / 180.0);
				int Near = 0;
				if (Way.Length() > 1e-6)
				{
					const Vec Fwd = Way.Unit();
					// (a man 1.8 m tall stands over 150 px of 665 within this depth)
					const double Within = 1.8 / (2.0 * TanV * (150.0 / 665.0));
					for (const LandFeed::Track& Tr : Rec->Tracks)
					{
						if (Tr.bTwin || !Tr.IsMan() || Tr.Start() > B || (Tr.Out >= 0.0 && Tr.Out < B)) continue;
						const LandFeed::Spot S = LandFeed::SpotAt(Tr, B);
						const Vec At(S.X, S.Y, Rec->Ground.HeightAt(S.X, S.Y) + 0.9);
						const double Depth = (At - P.Eye).Dot(Fwd);
						if (Depth > Within || Depth < 0.5) continue;
						if (LandEye::InPicture(P, At)) ++Near;
					}
				}
				const int Kd = static_cast<int>(P.Shot);
				NearSum[Kd] += Near;
				++NearFrames[Kd];
				if (Near > NearMost[Kd]) { NearMost[Kd] = Near; NearMostAt[Kd] = B; }
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
			if (D.MenCross(P, B, B))
			{
				++Crossed;
				if (std::getenv("LAND_PLAY_CROSS"))
					std::printf("  a man in the lens at battle %.2f (real %.2f): %s on %d, eye %.2f m up, fov %.0f, held %.2f s, cut for %s\n", B, R,
						LandEye::NameOf(P.Shot), P.Subject, P.Eye.Z - Rec->Ground.HeightAt(P.Eye.X, P.Eye.Y), P.Fov, R - CutReal, LandEye::NameOf(D.LastWhy()));
			}
			if (D.EdgeFill(P) > Set.EdgeMost) ++Edged;
			if (bMan && bUp && P.Shot != LandEye::Kind::Tank && P.Shot != LandEye::Kind::Track && D.Lost(B))
			{
				if (LostSince < 0.0) LostSince = R;
				if (R - LostSince > LostLongest) { LostLongest = R - LostSince; LostAt = B; LostSp = P.Sp; LostKind = P.Shot; }
				if (std::getenv("LAND_PLAY_LOST") && R - LostSince >= 3.9 && R - LostSince < 4.15)
				{
					std::printf("  lost 4 s at battle %.2f (real %.2f): %s on %d, held %.2f s; falls near:", B, R, LandEye::NameOf(P.Shot), P.Subject, R - CutReal);
					for (size_t I = 0; I < Rec->Tracks.size(); ++I)
						if (Rec->Tracks[I].Fell >= B - 3.0 && Rec->Tracks[I].Fell <= B + 3.0) std::printf(" %zu@%.2f", I, Rec->Tracks[I].Fell);
					std::printf("\n");
				}
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
			if (LostSp != LandEye::Special::None) std::printf(" (in a %s shot, %s)", LandEye::NameOf(LostSp), LandEye::NameOf(LostKind));
			std::printf("; in the fight %.0f%% at a soldier's eye level (2.2 m or under), %.0f%% on the film's own section", 100.0 * EyeLevel / std::max(1, FightFrames),
				100.0 * OnSquad / std::max(1, FightFrames));
			std::printf("; the lens in a burst's cloud %.2f%% of frames, the longest %.2f s (battle %.1f)", 100.0 * CloudFrames / std::max(1, Frames), CloudLongest, CloudAt);
			if (CloudLongest > 0.6) Fault("the lens stayed inside a burst's cloud past half a second (the art director: never black past 0.5 s)");
			std::printf("; within %.0f m of a vehicle's fuel ball in its first %.0f s %d frames, the longest %.2f s (battle %.1f), the nearest %.0f m (battle %.1f)",
				Set.FuelVehicle, Set.FuelFor, FuelFrames, FuelLongest, FuelAt, FuelNearest < 1e17 ? FuelNearest : -1.0, FuelNearestAt);
			if (FuelLongest > 0.35) Fault("the lens stayed within a fuel ball's reach past its flash (the art director: cut on the flash)");
			if (bNearReport)
			{
				std::printf("; men near by their height on the screen (over 150 of 665 px), a frame on average and at the most, by shot:");
				for (int Kd = 0; Kd < 7; ++Kd)
					if (NearFrames[Kd] > 0) std::printf(" %s %.1f, %d (battle %.0f)", LandEye::NameOf(static_cast<LandEye::Kind>(Kd)), static_cast<double>(NearSum[Kd]) / NearFrames[Kd], NearMost[Kd], NearMostAt[Kd]);
			}
			std::printf("; the whole mass %d shot%s", MassShots, MassShots == 1 ? "" : "s");
			if (MassShots > 0)
				std::printf(", at battle %.1f s (its window %.0f to %.0f s) for %.1f s, %.0f m up and %.0f m from where it looks, the men drawn in its picture %.0f%% at the least (battle %.1f, %d men)",
					MassStart, MassFrom, MassTo, MassReal, MassHeight, MassFar, 100.0 * MassLeast, MassLeastAt, MassMen);
			if (MassShots > 0 && MassStretched)
				std::printf(" (the front too wide for one picture: those of the densest %.0f m of it, %d men; the whole front's %.0f%% at the least)", MassStretchWide, MassStretchMen, 100.0 * MassWholeLeast);
			if (MassShots > 0) std::printf(", of them 2 px tall or more on the 665 px drawn %.0f%% at the least", 100.0 * MassTallLeast);
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

	/** The tilt the view takes by itself at a zoom, degrees: what a tilt of his own is measured against. */
	double AutoTiltDeg(double Zoom)
	{
		const LandGeneralEye::Settings TiltSet;
		return TiltSet.TiltNear + (TiltSet.TiltFar - TiltSet.TiltNear) * std::clamp(Zoom, 0.0, 1.0);
	}

	/** The general's eye on a bare field with no recording behind it: flat ground, one track. A rule of the kept views
	 * that a scripted battle cannot be relied on to walk into is checked here instead, at its own set-up. */
	struct EyeBench
	{
		LandFeed::Recording Rec;
		LandEye::World W;
		LandGeneralEye::Eye Gen;
		double T = 0.0;
		EyeBench()
		{
			Rec.Ground.W = 3000.0;
			Rec.Ground.H = 2000.0;
			W.Height = [](double, double) { return 0.0; };
			LandFeed::Track Tr;
			Tr.Block = 3;
			Tr.Role = "rifle";
			Tr.At.push_back({ 0.0, 400.0, 400.0, 0.0 });
			Tr.At.push_back({ 900.0, 400.0, 400.0, 0.0 });
			Rec.Tracks.push_back(Tr);
			Gen.Begin(Rec, W, Vec(1500.0, 1000.0, 0.0), 0.0, 250.0);
		}
		void Frame(const LandGeneralEye::Input& In) { Gen.Update(In, 1.0 / 60.0, 0.0); T += 1.0 / 60.0; }
		void Still(int Frames) { for (int K = 0; K < Frames; ++K) Frame(LandGeneralEye::Input()); }
		/** How much of his own tilt the picture shows over the automatic one, degrees. */
		double Shown() const { return Gen.TiltDegrees() - AutoTiltDeg(Gen.Zoom()); }
	};

	/** A flight to a view kept with a tilt of his own, taken back by his own hand. Rule 21, the picture is the truth:
	 * the tilt he is left with is his own only if the picture HAS it as his hand takes the flight, and WILL have it
	 * where the taking move leaves him. What the picture shows when he takes the view is what he keeps.
	 * Will have it: farther out the automatic tilt is steeper, so a tilt of his own that shows now can be swallowed
	 * by it, and right down by a man's shoulder his tilt is handed toward the automatic one; either way he must
	 * never be left standing with a tilt of his own he cannot see. Three moves change the zoom in the very frame
	 * that takes the flight: a notch of the wheel either way, and a unit's card, which follows the unit no nearer
	 * than FollowNearest (but not with a pan in the same frame, which ends the follow, nor where the unit is no
	 * longer on the field). It is asked of where the zoom and the tilt come to REST from the swing they have: a pan
	 * as the flight's turning of his tilt first shows leaves it his where its own swing carries it on to show.
	 * Has it: asked of the picture in the taking frame and of nothing the zoom or the tilt have yet to do. The call
	 * sets his tilt off just under the automatic one, and whatever lowers the automatic one after that bares a tilt
	 * that neither he nor the flight has turned: a notch IN on the flight's first frame; a zoom still coasting in
	 * from his own wheel as the view is called; a second call on a flight's way down. And a tilt the picture does
	 * not have as he takes the flight is handed back though the zoom's own rest, or his taking notch, would bare
	 * it: on the way down before the picture shows it; a frame or two before the flight's turning shows; a flight
	 * to a view kept without his tilt as the last of it leaves the picture; a tilt of his own hidden by a notch out
	 * before the call and the notch back in as the flight begins. The other way about, a tilt plainly on the
	 * picture stays his where a notch in takes the flight while the zoom still coasts out.
	 * A tilt press that takes the flight must tilt the view from where it stands, not send it on to the kept view's
	 * tilt; and a tilt handed back is handed back whole: on the way down to a view at a man's shoulder, with the
	 * flight's tilt full on the picture, a tilt press takes the flight, the shoulder hides the tilt where he is
	 * left, and his press must tilt the view from the automatic one, not keep the flight's tilt for his own to come
	 * out of nowhere when he next zooms out. On the last of that way down the shoulder is already pressing his tilt
	 * down, and a notch out keeps what it has left of it as the take finds it: never one he cannot see, never more
	 * than the picture had. Each is set up here with its own control beside it, since no scripted battle can be
	 * relied on to walk into any of them. */
	void TiltTakeBench()
	{
		const LandGeneralEye::Settings Set;
		// (a notch of the zoom is this much of the automatic tilt, degrees)
		const double ZoomTilt = Set.TiltFar - Set.TiltNear, NotchTilt = ZoomTilt * Set.ZoomStep;
		const bool bSay = std::getenv("LAND_PLAY_VIEWS") != nullptr;
		struct TakeCase
		{
			const char* Name;
			/** Where the view is kept: 0 the middle of the zoom; 1 low over the field, between the zoom under which
			 * his tilt is handed toward a man's shoulder and the nearest a followed unit is watched from; 2 right
			 * down, well under the first, where a notch or two in brings the zoom to its nearest; 3 at a man's
			 * shoulder itself, his tilt set in the middle of the zoom and carried down to where the shoulder hides
			 * it (the flight to it comes down from far up, his tilt on the picture until the last of the way). */
			int Height;
			/** How much of his own tilt the picture shows as it is set, degrees (Want 0: the view is kept without
			 * a tilt of his own). */
			double Want, ShownLeast, ShownMost;
			/** The taking move: 0 a notch out, 1 a unit's card (followed), 2 a key's pan, 3 a card and a pan in one
			 * frame (the pan ends the follow as it begins, so nothing holds the zoom off), 4 a notch in (right down:
			 * two, to the zoom's nearest), 5 a tilt press of half a degree, 6 the card of a unit no longer on the
			 * field (nothing to follow, so nothing holds the zoom off), 7 five notches in. */
			int Move;
			/** When the flight is taken: 0 late on, with the tilt at the kept one (to the shoulder: on the way down,
			 * his tilt still full on the picture); 1 in its first frame, before the flight has turned his tilt;
			 * 2 on the way down from the flight's height, while the zoom is still too far out for the picture to
			 * show his tilt though the zoom's own rest would; 3 in its first frames, as the flight's turning of his
			 * tilt begins to show, the tilt's own swing still to carry it on; 4 on the last of the way down to a
			 * man's shoulder, the shoulder already pressing his tilt down, three degrees or more of it still on the
			 * picture; 5 a frame or two before 3, the flight's turning not yet on the picture; 6 on a flight to a
			 * view kept without his tilt, as the last of his tilt leaves the picture, the zoom still coming down. */
			int When;
			/** What must come of his tilt: 0 it stays his own and shows; 1 handed back, the zoom he is left at hiding
			 * it; 2 handed back, the picture not having had it as the move took the flight; 3 within a degree of
			 * the automatic one, his own or not (his press's half degree is all that was asked); 4 as 0, of a tilt
			 * that a second notch out would have hidden (so a notch counted twice is caught); 5 as 3, and still
			 * within a degree of it once he has zoomed out again to where no shoulder hides a tilt (the flight's
			 * tilt, handed back, is not kept for his own under his press, out of sight until he next zooms out);
			 * 6 as 0, and no more of it than the picture had as he took the flight. */
			int Must;
			/** What goes before the call: 0 the view left by a pan and his tilt turned back by hand, so the call and
			 * not his hand has the turning of it; 1 as 0, then the zoom far out and flicked five notches in six
			 * frames before the call, still coasting in as the view is called; 2 the view left by a pan with his
			 * tilt left on, and the zoom flicked six notches out twelve frames before the call, still coasting out;
			 * 3 the view left by a pan, the zoom far out and the top view pressed (a tilt of his own the flight
			 * turns back); 4 as 0, then a second view kept without his tilt and left for far out, that view called,
			 * and this one called on its flight's way down; 5 the view left by a pan with his tilt left on, then one
			 * notch out, which hides it. */
			int Before;
		};
		// (what the picture would show of a tilt of his own, now showing Shown degrees at one zoom, at another: from
		// the settings alone, by the soft meeting of his tilt with the automatic one, (D + Soft)^2 / 4 Soft across
		// Soft either side. A case whose answer hangs on a zoom is only judged where this puts that answer well
		// clear of the quarter of a degree that counts as showing, so a change of the notch, of the soft meeting or
		// of a followed unit's nearest zoom cannot leave a case asking the rule its question at a set-up whose
		// answer is the other one.)
		auto ShownAt = [&Set, ZoomTilt](double Shown, double ZoomFrom, double ZoomTo)
		{
			const double Soft = Set.TiltSoft;
			const double D = (Shown >= Soft ? Shown : 2.0 * std::sqrt(Soft * std::max(Shown, 0.0)) - Soft) - ZoomTilt * (ZoomTo - ZoomFrom);
			return D >= Soft ? D : D <= -Soft ? 0.0 : (D + Soft) * (D + Soft) / (4.0 * Soft);
		};
		const TakeCase Cases[] = {
			{ "a notch out that hides it", 0, 0.35 * NotchTilt, 0.25 * NotchTilt, 0.6 * NotchTilt, 0, 0, 1, 0 },
			{ "a notch out that leaves it showing", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 0, 0, 0, 0 },
			{ "a notch's worth of it, a notch out that leaves it showing", 0, NotchTilt, 0.9 * NotchTilt, 1.25 * NotchTilt, 0, 0, 4, 0 },
			{ "a unit's card, low, whose follow hides it", 1, 0.5, 0.35, 0.75, 1, 0, 1, 0 },
			{ "a pan, low, that leaves it showing", 1, 0.5, 0.35, 0.75, 2, 0, 0, 0 },
			{ "a unit's card and a pan at once, low, that leave it showing", 1, 0.5, 0.35, 0.75, 3, 0, 0, 0 },
			{ "the card of a unit no longer on the field, low, that leaves it showing", 1, 0.5, 0.35, 0.75, 6, 0, 0, 0 },
			{ "a notch in on the flight's first frame, baring a tilt nobody turned", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 4, 1, 2, 0 },
			{ "a notch in late on, that leaves it showing", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 4, 0, 0, 0 },
			{ "notches in right down, to a man's shoulder, which hides it", 2, 3.0, 2.5, 4.5, 4, 0, 1, 0 },
			{ "a pan right down, that leaves it showing", 2, 3.0, 2.5, 4.5, 2, 0, 0, 0 },
			{ "a tilt press on the flight's first frame", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 5, 1, 3, 0 },
			{ "down to a view at a man's shoulder, a notch out on the way that leaves it showing", 3, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 0, 0, 0, 0 },
			{ "down to a view at a man's shoulder, a pan on the way that lets the shoulder hide it", 3, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 2, 0, 1, 0 },
			{ "down to a view at a man's shoulder, a notch out on the last of the way", 3, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 0, 4, 6, 0 },
			{ "a pan on the way down, before the picture shows it, though the zoom's own rest would", 0, 0.5 * NotchTilt, 0.43 * NotchTilt, 0.59 * NotchTilt, 2, 2, 2, 0 },
			{ "a pan as the flight's turning of it first shows, its swing carrying it on", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 2, 3, 0, 0 },
			{ "a pan a frame or two before the flight's turning of it shows", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 2, 5, 2, 0 },
			{ "down to a view at a man's shoulder, a tilt press on the way", 3, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 5, 0, 5, 0 },
			{ "called as the zoom coasts in from his wheel, a pan the next frame", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 2, 1, 2, 1 },
			{ "his tilt on the picture, the zoom coasting out from his wheel, five notches in the frame after the call", 0, 3.2, 2.6, 4.2, 7, 1, 0, 2 },
			{ "to a view kept without his tilt, a pan as the last of the top view's tilt leaves the picture", 0, 0.0, -0.1, 0.1, 2, 6, 2, 3 },
			{ "called on another flight's way down, a pan the next frame", 0, NotchTilt + 4.0, NotchTilt + 2.0, 90.0, 2, 1, 2, 4 },
			{ "his tilt hidden by a notch out before the call, the notch back in on the flight's first frame", 0, 0.35 * NotchTilt, 0.25 * NotchTilt, 0.6 * NotchTilt, 4, 1, 2, 5 },
		};
		for (const TakeCase& C : Cases)
		{
			EyeBench B;
			B.Still(60);
			// (to the zoom the case sets his tilt at: a notch at a time, or a quarter of one lower down, each let
			// settle, so the wish is never driven past the zoom asked for)
			const int SetAt = C.Height == 3 ? 0 : C.Height;
			const double ZoomOver = SetAt == 2 ? 0.07 : SetAt == 1 ? Set.FollowNearest - 0.025 : 0.52, ZoomStepIn = SetAt > 0 ? 0.25 : 1.0;
			for (int K = 0; K < 200 && B.Gen.Zoom() > ZoomOver; ++K) { LandGeneralEye::Input In; In.Zoom = ZoomStepIn; B.Frame(In); B.Still(20); }
			for (int K = 0; K < 60 && SetAt == 0 && B.Gen.Zoom() < 0.48; ++K) { LandGeneralEye::Input In; In.Zoom = -1.0; B.Frame(In); B.Still(20); }
			B.Still(180);
			// (his own tilt turned down by hand until the picture shows what this case wants of it, in the same way)
			for (int K = 0; K < 400 && C.Want > 0.0 && B.Shown() < C.Want; ++K) { LandGeneralEye::Input In; In.Tilt = SetAt > 0 ? 0.1 : 0.15; B.Frame(In); B.Still(12); }
			B.Still(180);
			const double ShownBefore = B.Shown(), ZoomSet = B.Gen.Zoom();
			const bool bZoomSet = SetAt == 2 ? ZoomSet > 0.045 && ZoomSet < 0.075
				: SetAt == 1 ? ZoomSet > Set.TiltOwnFrom + 0.005 && ZoomSet < Set.FollowNearest - 0.015 : ZoomSet > 0.4 && ZoomSet < 0.6;
			const double Out1 = ShownAt(ShownBefore, 0.0, Set.ZoomStep), Out2 = ShownAt(ShownBefore, 0.0, 2.0 * Set.ZoomStep);
			const double AtFollow = ShownAt(ShownBefore, ZoomSet, Set.FollowNearest);
			// (a notch out late on in the middle of the zoom, or one before the call that is to hide his tilt: the
			// answer a notch, and two, farther out; a followed unit's card low down: the answer at its nearest zoom)
			const bool bNotched = SetAt == 0 && ((C.Move == 0 && C.When == 0 && C.Before == 0) || C.Before == 5);
			const bool bClear = (!bNotched || (C.Must == 0 || C.Must == 4 ? Out1 > 0.5 && (C.Must != 4 || Out2 < 0.15) : Out1 < 0.15))
				&& (C.Move != 1 || AtFollow < 0.15);
			if (B.Gen.OwnTilt() != (C.Want > 0.0) || !bZoomSet || ShownBefore < C.ShownLeast || ShownBefore > C.ShownMost || !bClear)
			{
				std::printf("  the tilt bench (%s) could not set its view up: own %d, shown %.2f degrees over the automatic one (%.2f to %.2f wanted) at zoom %.3f, which a notch out would leave at %.2f, two at %.2f and a followed unit's nearest zoom at %.2f\n",
					C.Name, B.Gen.OwnTilt() ? 1 : 0, ShownBefore, C.ShownLeast, C.ShownMost, ZoomSet, Out1, Out2, AtFollow);
				Fault("the tilt bench could not set up a tilt of his own to judge");
				continue;
			}
			if (C.Height == 3)
			{
				// (and right down with it, a quarter notch at a time, to where a man's shoulder hides it)
				for (int K = 0; K < 300 && (B.Gen.Zoom() > 0.01 || std::abs(B.Shown()) > 0.05); ++K) { LandGeneralEye::Input In; In.Zoom = 0.25; B.Frame(In); B.Still(20); }
				B.Still(120);
				if (!B.Gen.OwnTilt() || B.Gen.Zoom() > 0.02 || std::abs(B.Shown()) > 0.1)
				{
					std::printf("  the tilt bench (%s) could not hide his tilt at a man's shoulder: own %d, shown %.2f degrees at zoom %.3f\n", C.Name, B.Gen.OwnTilt() ? 1 : 0, B.Shown(), B.Gen.Zoom());
					Fault("the tilt bench could not set up a tilt of his own to judge");
					continue;
				}
			}
			const double ZoomBefore = B.Gen.Zoom();
			LandGeneralEye::Input Keep;
			Keep.KeepView = 0;
			B.Frame(Keep);
			if (!B.Gen.ViewKept(0)) { Fault("the tilt bench's view was not kept"); continue; }
			// (away from it, up from a man's shoulder first; and his own tilt turned back by hand, so the call and not
			// his hand has the turning of the tilt on the way back, but where the case leaves his tilt on)
			const bool bTiltLeftOn = C.Before == 2 || C.Before == 5;
			for (int K = 0; K < 60 && C.Height == 3 && B.Gen.Zoom() < 0.28; ++K) { LandGeneralEye::Input In; In.Zoom = -1.0; B.Frame(In); B.Still(20); }
			for (int K = 0; K < 240; ++K) { LandGeneralEye::Input In; In.PanRight = 1.0; B.Frame(In); }
			for (int K = 0; K < 4000 && !bTiltLeftOn && B.Gen.OwnTilt(); ++K) { LandGeneralEye::Input In; In.Tilt = -0.5; B.Frame(In); }
			B.Still(120);
			if (B.Gen.OwnTilt() != bTiltLeftOn) { Fault("the tilt bench could not turn his own tilt back by hand, or lost one it meant to leave on"); continue; }
			bool bSetUp = true;
			if (C.Before == 4)
			{
				// (a second view, kept here without his tilt, and left for far out)
				LandGeneralEye::Input Keep1;
				Keep1.KeepView = 1;
				B.Frame(Keep1);
				for (int K = 0; K < 240; ++K) { LandGeneralEye::Input In; In.PanRight = 1.0; B.Frame(In); }
			}
			if (C.Before == 1 || C.Before == 3 || C.Before == 4)
			{
				// (far out, a notch at a time)
				const double Far = C.Before == 1 ? 0.74 : C.Before == 3 ? 0.96 : 0.85;
				for (int K = 0; K < 60 && B.Gen.Zoom() < Far; ++K) { LandGeneralEye::Input In; In.Zoom = -1.0; B.Frame(In); B.Still(20); }
				B.Still(180);
				bSetUp = bSetUp && B.Gen.Zoom() >= Far;
			}
			if (C.Before == 1)
			{
				// (the wheel flicked five notches in, the zoom still coasting in as the view is called)
				LandGeneralEye::Input In;
				In.Zoom = 5.0;
				B.Frame(In);
				B.Still(6);
			}
			if (C.Before == 2)
			{
				// (the wheel flicked six notches out, the zoom still coasting out as the view is called, his tilt
				// still on the picture)
				LandGeneralEye::Input In;
				In.Zoom = -6.0;
				B.Frame(In);
				B.Still(11);
				bSetUp = bSetUp && B.Gen.OwnTilt() && B.Shown() > 0.5;
			}
			if (C.Before == 3)
			{
				// (the top view: a tilt of his own, almost straight down, that the flight will turn back)
				LandGeneralEye::Input In;
				In.bTopView = true;
				B.Frame(In);
				B.Still(360);
				bSetUp = bSetUp && B.Gen.OwnTilt() && B.Shown() > 10.0;
			}
			if (C.Before == 4)
			{
				// (the other view called, and this one called on that flight's way down)
				LandGeneralEye::Input Call1;
				Call1.CallView = 1;
				B.Frame(Call1);
				bool bHigh = false, bDown = false;
				for (int K = 0; K < 900 && !bDown && B.Gen.FlyingTo() == 1; ++K)
				{
					bHigh = bHigh || B.Gen.Zoom() > ZoomBefore + 0.1;
					bDown = bHigh && B.Gen.Zoom() < ZoomBefore + 0.07;
					if (!bDown) B.Still(1);
				}
				bSetUp = bSetUp && bDown && !B.Gen.OwnTilt();
			}
			if (C.Before == 5)
			{
				// (one notch out, which hides his tilt: his own still, the picture without it)
				LandGeneralEye::Input In;
				In.Zoom = -1.0;
				B.Frame(In);
				B.Still(180);
				bSetUp = bSetUp && B.Gen.OwnTilt() && std::abs(B.Shown()) < 0.2;
			}
			if (!bSetUp)
			{
				std::printf("  the tilt bench (%s) could not set up what goes before its call: own %d, shown %.2f degrees at zoom %.3f\n", C.Name, B.Gen.OwnTilt() ? 1 : 0, B.Shown(), B.Gen.Zoom());
				Fault("the tilt bench could not set up what goes before a call");
				continue;
			}
			const double ZoomCalled = B.Gen.Zoom();
			LandGeneralEye::Input Call;
			Call.CallView = 0;
			B.Frame(Call);
			// (the flight taken back by his hand. On its first frame. Or late on: in the middle of the zoom, once the
			// picture shows his tilt to within 0.3 of a degree of what it showed as set; right down, that and the zoom
			// within a hundredth of the kept one; low, where the flight climbs first, once it has come down again to
			// within half a hundredth of the kept zoom, by when the tilt has long been there; to a man's shoulder, on the
			// way down, with ten degrees of his tilt and more still on the picture. Or on the way down from the
			// flight's height, the zoom still 0.07 off the kept one and nothing of his tilt on the picture yet. Or as
			// the flight's turning of his tilt first shows, a third of a degree of it; or a frame or two before, an
			// eighth. Or on the last of the way down to a man's shoulder, three degrees to eight still on the picture. Or,
			// to a view kept without his tilt, as the last of his tilt leaves the picture)
			int Took = -1;
			bool bClimbed = false, bCruised = false, bShowed = false;
			double ZoomTook = 0.0, ShownTook = 0.0;
			for (int K = 0; K < 900 && Took < 0 && B.Gen.FlyingTo() == 0; ++K)
			{
				bClimbed = bClimbed || B.Gen.Zoom() > ZoomBefore + 0.05;
				bCruised = bCruised || B.Gen.Zoom() > ZoomBefore + 0.1;
				bShowed = bShowed || B.Shown() > 1.0;
				const bool bNow = C.When == 1 || (C.When == 2 ? bCruised && B.Gen.Zoom() < ZoomBefore + 0.07 && B.Shown() < 0.1
					: C.When == 3 ? B.Shown() >= 0.3
					: C.When == 4 ? bClimbed && B.Gen.Zoom() < 0.03 && B.Shown() >= 3.0 && B.Shown() < 8.0
					: C.When == 5 ? B.Shown() >= 0.12
					: C.When == 6 ? bShowed && B.Shown() < 0.2
					: C.Height == 3 ? bClimbed && B.Gen.Zoom() < 0.09 && B.Shown() >= 10.0
					: C.Height == 1 ? bClimbed && B.Gen.Zoom() < ZoomBefore + 0.005
					: B.Shown() >= ShownBefore - 0.3 && (C.Height == 0 || B.Gen.Zoom() < ZoomBefore + 0.01));
				if (!bNow) { B.Still(1); continue; }
				ZoomTook = B.Gen.Zoom();
				ShownTook = B.Shown();
				LandGeneralEye::Input In;
				if (C.Move == 0) In.Zoom = -1.0;
				if (C.Move == 1 || C.Move == 3) { In.GoBlock = B.Rec.Tracks[0].Block; In.bFollow = true; }
				if (C.Move == 2 || C.Move == 3) In.PanRight = 1.0;
				if (C.Move == 4) In.Zoom = C.Height == 2 ? 2.0 : 1.0;
				if (C.Move == 5) In.Tilt = 0.5;
				if (C.Move == 6) { In.GoBlock = B.Rec.Tracks[0].Block + 90; In.bFollow = true; }
				if (C.Move == 7) In.Zoom = 5.0;
				B.Frame(In);
				Took = K;
			}
			if (Took < 0) { Fault("the tilt bench's flight was over, or never came down, before its tilt could be taken"); continue; }
			if (C.When == 1 && C.Before != 2 && (Took != 0 || std::abs(ShownTook) > 0.2)) Fault("the tilt bench's early take was not of a flight that had yet to turn his tilt");
			if (C.When == 3 && (Took > 30 || ShownTook > 0.45)) Fault("the tilt bench's take was not of a tilt the flight had only begun to show");
			if (C.When == 5 && (Took > 30 || ShownTook > 0.2)) Fault("the tilt bench's take was not of a tilt the flight had yet to show");
			if (C.When == 4 && (ZoomTook > 0.03 || ShownTook < 3.0)) Fault("the tilt bench's take was not on the last of the way down to a man's shoulder with his tilt still on the picture");
			if (C.Must == 2 && std::abs(ShownTook) > 0.2) Fault("the tilt bench's take was not of a picture without his tilt");
			if (C.Before == 2 && (Took != 0 || ShownTook < 0.5 || ZoomTook < ZoomCalled)) Fault("the tilt bench's take was not of a tilt on the picture with the zoom coasting out");
			B.Still(360);
			const double ZoomAfter = B.Gen.Zoom();
			if (C.Move == 0 && ZoomAfter < ZoomBefore + 0.5 * Set.ZoomStep) Fault("the tilt bench's notch did not take the zoom out");
			if (C.Move == 1 && std::abs(ZoomAfter - Set.FollowNearest) > 0.01) Fault("the tilt bench's card did not leave the zoom at a followed unit's nearest");
			if ((C.Move == 2 || C.Move == 3 || C.Move == 6) && C.When == 0 && std::abs(ZoomAfter - ZoomBefore) > 0.02) Fault("the tilt bench's pan did not leave the zoom where the flight was taking it");
			if ((C.When == 2 || C.When == 6) && (ZoomAfter > ZoomTook - 0.02 || ZoomAfter < ZoomBefore - 0.02)) Fault("the tilt bench's pan on the way down did not let the zoom run on down to its own rest");
			if ((C.Before == 1 || C.Before == 4) && ZoomAfter > ZoomTook - 0.03) Fault("the tilt bench's call was not made with the zoom coasting in");
			if (C.Move == 4 && C.Before == 0 && (C.Height == 2 ? ZoomAfter > 0.005 : ZoomAfter > ZoomBefore - 0.5 * Set.ZoomStep)) Fault("the tilt bench's notch did not take the zoom in");
			if (C.Move == 4 && C.Before == 5 && std::abs(ZoomAfter - ZoomBefore) > 0.02) Fault("the tilt bench's notch back in did not bring the zoom back to where his tilt had shown");
			if (C.Move == 5 && std::abs(ZoomAfter - ZoomBefore) > 0.02) Fault("the tilt bench's tilt press did not leave the zoom where the flight was taking it");
			if (C.Move == 7 && ZoomAfter > ZoomTook - 2.0 * Set.ZoomStep) Fault("the tilt bench's notches did not take the zoom in");
			if (bSay)
				std::printf("  the tilt bench (%s): shown %.2f degrees at zoom %.3f as set, kept at zoom %.3f, called at zoom %.3f, taken on frame %d of the flight at zoom %.3f showing %.2f, left %.2f degrees at zoom %.3f with his own tilt %s\n",
					C.Name, ShownBefore, ZoomSet, ZoomBefore, ZoomCalled, Took, ZoomTook, ShownTook, B.Shown(), ZoomAfter, B.Gen.OwnTilt() ? "on" : "off");
			if ((C.Must == 0 || C.Must == 4) && (!B.Gen.OwnTilt() || B.Shown() < 0.25))
				Fault("a flight taken by his hand handed back a tilt of his own that the picture had and still has where he is left");
			if (C.Must == 1 && (B.Gen.OwnTilt() || std::abs(B.Shown()) > 1.0))
				Fault("a flight taken by his hand left a tilt of his own on that the zoom he is left at hides");
			if (C.Must == 2 && (B.Gen.OwnTilt() || std::abs(B.Shown()) > 0.2))
				Fault(C.When == 1 && C.Before == 0 ? "a flight taken by his hand in its first frame left him a tilt of his own that neither he nor the flight had turned"
					: "a flight taken by his hand left him a tilt of his own that the picture did not have as he took it");
			if ((C.Must == 3 || C.Must == 5) && std::abs(B.Shown()) > 1.0)
				Fault(C.When == 1 ? "a flight taken by his hand's tilt press in its first frame went on to the kept view's tilt"
					: "a flight taken by his hand's tilt press went on to the kept view's tilt");
			if (C.Must == 5)
			{
				// (out again, a notch at a time, to well over the zoom under which a man's shoulder hides a tilt)
				for (int K = 0; K < 60 && B.Gen.Zoom() < Set.TiltOwnFrom + 0.1; ++K) { LandGeneralEye::Input In; In.Zoom = -1.0; B.Frame(In); B.Still(20); }
				B.Still(240);
				if (bSay) std::printf("  the tilt bench (%s): and zoomed out again, %.2f degrees at zoom %.3f with his own tilt %s\n", C.Name, B.Shown(), B.Gen.Zoom(), B.Gen.OwnTilt() ? "on" : "off");
				if (B.Gen.Zoom() < Set.TiltOwnFrom + 0.1) Fault("the tilt bench could not zoom out again from a man's shoulder");
				else if (std::abs(B.Shown()) > 1.0)
					Fault("a flight taken by his hand's tilt press left him the flight's tilt for his own, out of sight at a man's shoulder until he zoomed out");
			}
			if (C.Must == 6 && (!B.Gen.OwnTilt() || B.Shown() < 0.25 || B.Shown() > ShownTook + 0.5))
				Fault("a flight taken by his hand on the last of the way down to a man's shoulder handed back a tilt of his own that the picture had and still has where he is left, or left him more of it than the picture had");
		}
	}
}

namespace
{
	/**
	 * Effects (R2), small arms off armour: armoured or soft, every vehicle model the game draws (Content/Models/Battle)
	 * under each role the rules give it (land-rules.js: a block's tanks and its tank destroyers, "tank"; its armoured
	 * cars, AA vehicles, jeeps and motorcycles, "tank" with their own model, ml; a battery's guns, "gun", its
	 * self-propelled ones with their own model; armoured infantry's carriers, "lorry" with their own model, and a
	 * block's lorries, "lorry"), written out by hand from what each is (not from ArmouredVehicle): a rifle's or a machine
	 * gun's round bounces off armour and goes into a soft one. Every model file must have its rows, so a new model is
	 * decided here before it is drawn. Returns the faults; prints what it checked.
	 */
	int RicochetClasses(const std::string& Content)
	{
		struct Row { const char* Model; const char* Role; bool bOwn; bool bArmour; };
		static const Row Rows[] = {
			// a block's tanks (her block's model) and the same drawn as escorts' stand-ins (her own)
			{ "Sherman", "tank", false, true }, { "Tiger", "tank", false, true }, { "PanzerIV", "tank", false, true }, { "Panther", "tank", false, true },
			{ "T34", "tank", false, true }, { "LightTank", "tank", false, true }, { "Sherman", "tank", true, true },
			// tank destroyers and assault guns (a tank block's, their own model; or a block of them)
			{ "StuG3", "tank", true, true }, { "M10", "tank", true, true }, { "SU85", "tank", true, true }, { "Semovente", "tank", true, true },
			{ "StuG3", "tank", false, true },
			// armoured cars, AA half-tracks (drawn as tanks of their own model)
			{ "ArmouredCar", "tank", true, true }, { "SdKfz222", "tank", true, true }, { "M8", "tank", true, true },
			{ "FlakHalftrack", "tank", true, true }, { "M16", "tank", true, true },
			// the soft escorts: the flak lorry, jeeps, field and staff cars, motorcycles
			{ "FlakTruck", "tank", true, false }, { "Jeep", "tank", true, false }, { "Kubelwagen", "tank", true, false }, { "FieldCar", "tank", true, false },
			{ "GAZ67", "tank", true, false }, { "Fiat508", "tank", true, false }, { "Kurogane", "tank", true, false }, { "Motorcycle", "tank", true, false },
			{ "R75", "tank", true, false }, { "M72", "tank", true, false }, { "WLA", "tank", true, false }, { "M20", "tank", true, false },
			{ "Alce", "tank", true, false }, { "Rikuo", "tank", true, false },
			// a battery's self-propelled guns (their own model) and its towed guns (the battery's)
			{ "M7", "gun", true, true }, { "Hummel", "gun", true, true }, { "Sexton", "gun", true, true }, { "SU76", "gun", true, true },
			{ "Howitzer105", "gun", false, false }, { "leFH18", "gun", false, false }, { "ZiS3", "gun", false, false },
			// armoured infantry's half-tracks and the lorries (a lorry of a tank block: her block's model is no lorry's)
			{ "Hanomag", "lorry", true, true }, { "M3Halftrack", "lorry", true, true }, { "Truck", "lorry", true, false }, { "Truck", "lorry", false, false },
			{ "Sherman", "lorry", false, false },
		};
		// (models the game draws that are no vehicle: a field telephone, a cable reel, a stretcher)
		static const char* NotVehicles[] = { "CableReel", "FieldPhone", "Stretcher" };
		int Wrong = 0, Missing = 0, Armour = 0, Soft = 0, Files = 0;
		std::error_code Ec;
		for (const auto& Entry : std::filesystem::directory_iterator(Content + "/Models/Battle", Ec))
		{
			if (Entry.path().extension() != ".json") continue;
			++Files;
			const std::string Stem = Entry.path().stem().string();
			bool bFound = std::find_if(std::begin(NotVehicles), std::end(NotVehicles), [&Stem](const char* N) { return Stem == N; }) != std::end(NotVehicles);
			for (const Row& R : Rows) bFound = bFound || Stem == R.Model;
			if (!bFound)
			{
				++Missing;
				std::printf("  ricochet classes: %s has no row (armoured or soft?)\n", Stem.c_str());
			}
		}
		for (const Row& R : Rows)
		{
			LandFeed::Track Tr;
			Tr.Role = R.Role;
			LandFeed::Block B;
			if (R.bOwn) Tr.Model = R.Model;
			else B.Model = R.Model;
			const bool bGot = LandPlay::ArmouredVehicle(Tr, &B);
			(R.bArmour ? Armour : Soft) += 1;
			if (bGot != R.bArmour)
			{
				++Wrong;
				std::printf("  ricochet classes: %s as %s (%s) is drawn %s, should be %s\n", R.Model, R.Role, R.bOwn ? "her own model" : "her block's", bGot ? "armoured" : "soft", R.bArmour ? "armoured" : "soft");
			}
		}
		std::printf("ricochet classes: %d model files (%zu no vehicle), %zu rows (%d armoured, %d soft): %d wrong, %d models with no row\n",
			Files, sizeof(NotVehicles) / sizeof(NotVehicles[0]), sizeof(Rows) / sizeof(Rows[0]), Armour, Soft, Wrong, Missing);
		if (Files == 0) std::printf("  ricochet classes: no model files read under %s/Models/Battle\n", Content.c_str());
		// The carriers the rules give armoured infantry (land-rules.js, mech: "armoured", the half-tracks whose riders a
		// bullet cannot reach, and each army's rows and kit), each a lorry with her own model: the armoured ones must be
		// drawn armoured, the rest (the lorry) soft. Read from the rules themselves, so a carrier the rules come to write
		// is decided here before it is drawn.
		int CarrierWrong = 0, Carriers = 0;
		std::string Rules;
		if (LandJson::ReadFile(Content + "/Rules/land-rules.js", Rules))
		{
			std::set<std::string> Armoured, Named;
			auto Quoted = [&Rules](size_t From, size_t To, std::set<std::string>& Out)
			{
				for (size_t Q = Rules.find('"', From); Q != std::string::npos && Q < To; Q = Rules.find('"', Q + 1))
				{
					const size_t E = Rules.find('"', Q + 1);
					if (E == std::string::npos || E > To) break;
					const std::string W = Rules.substr(Q + 1, E - Q - 1);
					if (!W.empty() && std::isupper(static_cast<unsigned char>(W[0])) && W.find(' ') == std::string::npos && W.size() < 24) Out.insert(W);
					Q = E;
				}
			};
			const size_t A = Rules.find("armoured: [\"");
			if (A != std::string::npos) Quoted(A, Rules.find(']', A), Armoured);
			// (the carrier rows and kit sit just before "armoured" in the mech block: from its "rows" on)
			const size_t RowsAt = A == std::string::npos ? std::string::npos : Rules.rfind("rows: {", A);
			if (RowsAt != std::string::npos) Quoted(RowsAt, A, Named);
			for (const std::string& M : Named)
			{
				if (M == "USA" || M == "GER" || M == "UK" || M == "ITA" || M == "JAP" || M == "SOV" || M.size() <= 3) continue; // (the armies)
				++Carriers;
				LandFeed::Track Tr;
				Tr.Role = "lorry";
				Tr.Model = M;
				LandFeed::Block Bk;
				const bool bShould = Armoured.count(M) > 0, bGot = LandPlay::ArmouredVehicle(Tr, &Bk);
				if (bGot != bShould)
				{
					++CarrierWrong;
					std::printf("  ricochet classes: the rules' carrier %s is drawn %s, should be %s\n", M.c_str(), bGot ? "armoured" : "soft", bShould ? "armoured" : "soft");
				}
			}
			std::printf("ricochet carriers: the rules name %zu armoured (", Armoured.size());
			for (const std::string& M : Armoured) std::printf("%s%s", M == *Armoured.begin() ? "" : ", ", M.c_str());
			std::printf(") and %d carrier models in all: %d wrong\n", Carriers, CarrierWrong);
			if (Armoured.empty() || Carriers == 0) { std::printf("  ricochet carriers: no carriers read from the rules\n"); ++CarrierWrong; }
		}
		else
		{
			std::printf("  ricochet carriers: cannot read %s/Rules/land-rules.js\n", Content.c_str());
			++CarrierWrong;
		}
		return Wrong + Missing + CarrierWrong + (Files == 0 ? 1 : 0);
	}

	/** Effects (R2), small arms off armour: land_play's stand-in for the scene's cast plate (World::Plate): her plain box's
	 * face at S, its room shrunk 0.1 m on every side and the strike kept inside it, so the replay's path for a known plate
	 * (handed over as it is, its room carried) is run and checked as the game's is. */
	LandPlay::PlateSpot StandInPlate(const LandFeed::Track& Tr, const LandFeed::Block* B, const LandFeed::Spot& S, double Ground, const std::string& Face, double U, double H)
	{
		LandPlay::PlateSpot P = LandPlay::PlainPlate(Tr, B, S, Ground, Face, U, H);
		const double LA = P.RoomA.Length(), LB = P.RoomB.Length();
		const double NA = std::max(0.0, LA - 0.1), NB = std::max(0.0, LB - 0.1);
		const LandPlay::Vec UA = LA > 1e-9 ? P.RoomA * (1.0 / LA) : LandPlay::Vec(), UB = LB > 1e-9 ? P.RoomB * (1.0 / LB) : LandPlay::Vec();
		P.RoomA = UA * NA;
		P.RoomB = UB * NB;
		const LandPlay::Vec Rel = P.At - P.RoomMid;
		const double A = std::clamp(Rel.Dot(UA), -NA, NA), Bb = std::clamp(Rel.Dot(UB), -NB, NB);
		P.At = P.RoomMid + UA * A + UB * Bb;
		return P;
	}
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
	// (as the scene: a recording's night patrol has its men written in before it is read, LandFeed::AddPatrolMen)
	int PatrolMen = LandFeed::AddPatrolMen(Text);
	LandFeed::Recording Rec;
	if (!LandFeed::ReadRecording(Text, Rec, Error)) { std::printf("cannot read the recording: %s\n", Error.c_str()); return 2; }
	// (the scene's -TheaterLandPatrol: a stand-in patrol for a recording without one, LAND_PLAY_PATROL=found,caught)
	if (const char* PatrolText = std::getenv("LAND_PLAY_PATROL"); PatrolText && !Rec.Night.bOn)
	{
		const char* Comma = std::strchr(PatrolText, ',');
		const bool bFound = PatrolText[0] != '0', bCaught = !Comma || Comma[1] != '0';
		PatrolMen = LandNight::AddStandInPatrol(Text, Rec, bFound, bCaught, Error);
		std::printf("patrol: a stand-in of %d men from block %d (%s, %s)\n", PatrolMen, Rec.Night.Unit, bFound ? "the post found" : "nothing found", bCaught ? "caught under a flare" : "not seen");
		if (PatrolMen == 0) Fault("no stand-in patrol could be made");
	}
	const double ReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
	if (!LandJson::ReadFile(argv[2], MixText)) { std::printf("cannot read %s\n", argv[2]); return 2; }
	std::printf("%s: \"%s\", %s, %zu blocks, %zu tracks (%zu twins left out), %zu rounds, %zu events, %.1f s; read in %.0f ms\n",
		Rec.Key.c_str(), Rec.Title.c_str(), Rec.Date.c_str(), Rec.Blocks.size(), Rec.Tracks.size(),
		static_cast<size_t>(std::count_if(Rec.Tracks.begin(), Rec.Tracks.end(), [](const LandFeed::Track& T) { return T.bTwin; })),
		Rec.Rounds.size(), Rec.Events.size(), Rec.End, ReadMs);

	LandPlay::World PW;
	// Effects (R2), small arms off armour: off unless LAND_PLAY_RICOCHET is set, as the game is off unless
	// -TheaterLandRicochet (release 2, on trial); off, every output as before
	PW.bRicochet = std::getenv("LAND_PLAY_RICOCHET") != nullptr;
	// (life's seconds a battle second, LAND_PLAY_LIFE as below: the kit lays a burst's strikes on life's clock)
	if (const char* LifeAsked = std::getenv("LAND_PLAY_LIFE")) PW.Life = std::max(0.25, std::atof(LifeAsked));
	// (with it on, the odd rounds' plates come from a stand-in for the scene's cast (World::Plate: StandInPlate at her
	// place at the battle time played to, PlateNowT), the even rounds' from the replay's own plain box)
	double PlateNowT = 0.0;
	std::map<int, size_t> PlateRound;
	if (PW.bRicochet)
	{
		for (size_t I = 0; I < Rec.Rounds.size(); ++I) PlateRound[Rec.Rounds[I].N] = I;
		PW.Plate = [&Rec, &PlateNowT, &PlateRound](int Track, int Round, LandPlay::PlateSpot& Out)
		{
			auto It = PlateRound.find(Round);
			if (Round % 2 == 0 || It == PlateRound.end() || Track < 0 || static_cast<size_t>(Track) >= Rec.Tracks.size()) return false;
			const LandFeed::Round& R = Rec.Rounds[It->second];
			const LandFeed::Track& Tr = Rec.Tracks[static_cast<size_t>(Track)];
			const LandFeed::Spot S = LandFeed::SpotAt(Tr, PlateNowT);
			Out = StandInPlate(Tr, Rec.BlockOf(Tr.Block), S, Rec.Ground.HeightAt(S.X, S.Y), R.Fc.empty() ? std::string("front") : R.Fc, R.U, R.H);
			return true;
		};
		// (armoured or soft by the game's models, the recording's folder's Content: Content/Data/LandBattles/x.json)
		const std::filesystem::path RecPath(argv[1]);
		const std::string Content = RecPath.parent_path().parent_path().parent_path().string();
		if (RicochetClasses(Content.empty() ? std::string(".") : Content) > 0) Fault("a vehicle model drawn armoured where it is soft, or soft where armoured, or with no row");
	}
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
	// (the hedge cutters' test switch, the scene's -TheaterLandCutters=N: the first N crossings of a bank by the
	// attacker's tanks made cuts, from the field laid once without them; the field then laid whole there until she tears
	// it)
	if (const char* CutText = std::getenv("LAND_PLAY_CUTTERS"); CutText && std::atoi(CutText) > 0 && Rec.Cuts.empty())
	{
		const LandField::Field First = LandSight::FieldOf(Rec);
		if (!First.Steppe.bOn)
		{
			const LandField::Ground Plain(First);
			Rec.Cuts = LandSight::CutsFromCrossings(Rec, Plain, std::atoi(CutText));
		}
		std::printf("cutters: %zu cuts made from the tanks' crossings\n", Rec.Cuts.size());
	}
	// (the field telephones' test switch, the scene's -TheaterLandPhones: the defender's lines, cut and mended)
	if (std::getenv("LAND_PLAY_PHONES") && Rec.Wires.empty())
	{
		LandSight::PhonesFromBattle(Rec);
		double Laid = 0.0, CutOff = 0.0;
		int Lengths = 0, Bad = 0;
		for (const LandFeed::Wire& W : Rec.Wires)
		{
			if (W.Pts.size() < 2) ++Bad;
			for (size_t I = 0; I + 1 < W.Pts.size(); ++I)
			{
				const double D = std::hypot(W.Pts[I + 1].first - W.Pts[I].first, W.Pts[I + 1].second - W.Pts[I].second);
				Laid += D;
				Lengths += std::max(1, static_cast<int>(std::ceil(D)));
			}
		}
		for (const LandFeed::WireCut& C : Rec.WireCuts)
		{
			const LandFeed::Wire* On = nullptr;
			for (const LandFeed::Wire& W : Rec.Wires) if (W.Id == C.Wire) On = &W;
			if (!On) { ++Bad; continue; }
			double Best = 1e9;
			for (size_t I = 0; I + 1 < On->Pts.size(); ++I)
			{
				const double Dx = On->Pts[I + 1].first - On->Pts[I].first, Dy = On->Pts[I + 1].second - On->Pts[I].second;
				const double F = std::clamp(((C.X - On->Pts[I].first) * Dx + (C.Y - On->Pts[I].second) * Dy) / std::max(1e-9, Dx * Dx + Dy * Dy), 0.0, 1.0);
				Best = std::min(Best, std::hypot(On->Pts[I].first + Dx * F - C.X, On->Pts[I].second + Dy * F - C.Y));
			}
			CutOff = std::max(CutOff, Best);
		}
		// (each mend after its wire's cut, and no wire cut again while it is cut)
		std::map<int, double> CutAt;
		std::vector<std::pair<double, std::pair<int, int>>> Times;
		for (const LandFeed::WireCut& C : Rec.WireCuts) Times.push_back({ C.T, { C.Wire, 1 } });
		for (const LandFeed::WireMend& M : Rec.WireMends) Times.push_back({ M.T, { M.Wire, 0 } });
		std::stable_sort(Times.begin(), Times.end(), [](const auto& A, const auto& B) { return A.first < B.first || (A.first == B.first && A.second.second < B.second.second); });
		for (const auto& T : Times)
		{
			const bool bWasCut = CutAt.count(T.second.first) > 0;
			if (T.second.second == 1) { if (bWasCut) ++Bad; CutAt[T.second.first] = T.first; }
			else { if (!bWasCut) ++Bad; CutAt.erase(T.second.first); }
		}
		std::printf("phones: %zu wires, %.0f m laid in %d lengths; %zu cuts (the farthest %.3f m off its wire), %zu mends; %d out of order\n", Rec.Wires.size(), Laid, Lengths,
			Rec.WireCuts.size(), CutOff, Rec.WireMends.size(), Bad);
		if (Bad > 0 || CutOff > 0.05 || Rec.Wires.empty()) Fault("the field telephones' wires, cuts or mends out of order");
	}
	LandField::Field FieldWords = LandSight::FieldOf(Rec);
	// (the field's looks on trial, as the field's own test takes them: LAND_FIELD_TRIAL, Field::Trial; unset, none)
	if (const char* Words = std::getenv("LAND_FIELD_TRIAL")) FieldWords.Trial = Words;
	const LandField::Ground Field(FieldWords);
	LandSight::Room Sight;
	Sight.Lay(&Field);
	std::printf("field: laid in %.2f s, %zu things standing for the camera\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - FieldT0).count(), Sight.Things());
	// (and a hedge cutter's gap, once torn, its floor, as the scene's PlayWorld and eye take it: the holes set below)
	const LandCrater::Field* GapHoles = nullptr;
	int GapsTorn = 0;
	auto GroundAt = [&Field, &GapHoles, &GapsTorn](double X, double Y)
	{
		double Floor = 0.0;
		return GapsTorn > 0 && GapHoles && GapHoles->BreachFloor(X, Y, Floor) ? Floor : Field.HeightAt(X, Y);
	};
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
	// The night before the attack (LandNight, release 2's patrol), played through as the scene plays it at 60 frames a
	// second: each of its shots with the patrol's men in the picture, its clock never going back, a burning flare in the
	// flare's shot, the men home before the battle and no nearer the post than a patrol would crawl, the light risen after.
	if (Rec.Night.bOn)
	{
		LandNight::Prelude Night;
		if (!Night.Begin(Rec, GroundAt, EW.Free, 0.0))
		{
			Fault("the night before the attack did not begin (no patrol men written in)");
		}
		else
		{
			const std::vector<LandNight::Shot>& Plan = Night.Shots();
			std::vector<int> ShotFrames(Plan.size(), 0), ShotSeen(Plan.size(), 0), ShotLeast(Plan.size(), 99), ShotFlares(Plan.size(), 0);
			double Was = -1e18, Real = 0.0, NightReal = 0.0;
			int Back = 0, Shot = -1;
			for (int F = 0; F < 60 * 120 && !Night.Done(); ++F)
			{
				const LandNight::Frame Fr = Night.Update(1.0 / 60.0);
				Real += 1.0 / 60.0;
				if (!Fr.bNight) continue;
				NightReal = Real;
				if (Fr.Pose.bCut) Shot = std::min(Shot + 1, static_cast<int>(Plan.size()) - 1);
				if (Shot < 0) continue;
				if (Fr.Battle < Was - 1e-9) ++Back;
				Was = Fr.Battle;
				int In = 0;
				for (int Ix : Night.Men())
				{
					const LandFeed::Track& Tr = Rec.Tracks[static_cast<size_t>(Ix)];
					if (Tr.Out >= 0.0 && Fr.Battle > Tr.Out) continue;
					const LandFeed::Spot Sp = LandFeed::SpotAt(Tr, Fr.Battle);
					if (LandEye::InPicture(Fr.Pose, Vec(Sp.X, Sp.Y, GroundAt(Sp.X, Sp.Y) + 0.4))) ++In;
				}
				const size_t K = static_cast<size_t>(Shot);
				++ShotFrames[K];
				ShotSeen[K] += In > 0 ? 1 : 0;
				ShotLeast[K] = std::min(ShotLeast[K], In);
				for (const LandNight::Flare& Fl : Fr.Flares) if (!Fl.bRising && Fl.Light > 0.5) { ++ShotFlares[K]; break; }
			}
			// (the men home before the battle opens and out as it opens, and how near they came to the post)
			double Home = -1e18, OutLeast = 1e18, OutMost = -1e18, Nearest = 1e18;
			for (int Ix : Night.Men())
			{
				const LandFeed::Track& Tr = Rec.Tracks[static_cast<size_t>(Ix)];
				Home = std::max(Home, Tr.At.back().T);
				OutLeast = std::min(OutLeast, Tr.Out);
				OutMost = std::max(OutMost, Tr.Out);
				if (Rec.Night.bFound)
				{
					for (double T = Rec.Night.T0; T <= Rec.Night.T1; T += 2.0)
					{
						const LandFeed::Spot Sp = LandFeed::SpotAt(Tr, T);
						Nearest = std::min(Nearest, std::hypot(Sp.X - Rec.Night.FoundX, Sp.Y - Rec.Night.FoundY));
					}
				}
			}
			std::printf("night: %zu of the patrol's men, out %.0f to %.0f s, home by %.0f s, off the field at %.1f to %.1f s; nearest the post %.0f m; the dawn done %.1f real s in (the night's shots %.1f)\n",
				Night.Men().size(), Rec.Night.T0, Rec.Night.T1, Home, OutLeast, OutMost, Rec.Night.bFound ? Nearest : -1.0, Real, NightReal);
			for (size_t K = 0; K < Plan.size(); ++K)
			{
				std::printf("  %s from %.0f s, %.1f real s: %d frames, a man in the picture in %d (at least %d of them), a flare burning in %d\n", LandNight::NameOf(Plan[K].Kind), Plan[K].From, Plan[K].Real,
					ShotFrames[K], ShotSeen[K], ShotLeast[K] == 99 ? 0 : ShotLeast[K], ShotFlares[K]);
				if (ShotFrames[K] == 0 || ShotSeen[K] < 0.9 * ShotFrames[K]) Fault("a night shot without the patrol's men in the picture");
				if (Plan[K].Kind == LandNight::Beat::Flare && ShotFlares[K] < ShotFrames[K] / 2) Fault("the flare's shot without a flare burning over it");
			}
			if (Back > 0) Fault("the night's clock went back within a shot");
			if (!Night.Done()) Fault("the night's light never rose over the battle");
			if (Home > 0.0) Fault("a patrol man not home when the battle opens");
			if (OutLeast < 0.0 || OutMost > 1.0) Fault("a patrol man not off the field as the battle opens (a leaving time under 0 reads as never)");
			if (Rec.Night.bFound && Nearest < 40.0) Fault("the patrol crawled within 40 m of the enemy's post");
		}
	}
	{
		// The walk pan (the Mac recipe's look 7, -TheaterLandWalkPan): a block walking steadily, an eye off its flank with
		// the sight lines open, its men 25 to 140 m off through the pan; asked every 10 s from 30 s, the first that holds.
		int Asked = 0, Held = 0;
		double First = -1.0;
		LandWalkPan::Plan FirstPlan;
		for (double T = 30.0; T + 20.0 <= Rec.End; T += 10.0)
		{
			++Asked;
			const LandWalkPan::Plan P = LandWalkPan::Choose(Rec, EW, T, 20.0);
			const double Near = std::min({ P.FromStart, P.FromMiddle, P.FromEnd }), Far = std::max({ P.FromStart, P.FromMiddle, P.FromEnd });
			if (P.Block < 0 || !P.bOpen || Near < 35.0 || Far > 120.0) continue;
			++Held;
			if (First < 0.0)
			{
				First = T;
				FirstPlan = P;
			}
		}
		if (First >= 0.0)
			std::printf("walk pan (look 7): %d of %d times asked hold; the first at %.0f s: block %d, %zu men at %.1f m/s, the eye %.1f m up, %.0f, %.0f and %.0f m off\n", Held, Asked, First,
				FirstPlan.Block, FirstPlan.Men.size(), FirstPlan.Speed, FirstPlan.Eye.Z - GroundAt(FirstPlan.Eye.X, FirstPlan.Eye.Y), FirstPlan.FromStart, FirstPlan.FromMiddle, FirstPlan.FromEnd);
		else
			std::printf("walk pan (look 7): none of %d times asked holds\n", Asked);
		if (Held == 0) Fault("no walk pan found for the Mac's look 7 at any time asked");
	}
	{
		// The tank pan (-TheaterLandTankPan: the far tanks' places a frame before, LandCrowd): a tank driving steadily, an
		// eye off her flank with the sight lines open, she 150 to 300 m off through the pan (past the 140 m hand-over, so
		// drawn by her model's far pieces); asked every 10 s from 30 s. A battle with no tank driving has none to show.
		int Asked = 0, Held = 0, Driving = 0;
		double First = -1.0;
		LandWalkPan::Plan FirstPlan;
		for (double T = 30.0; T + 20.0 <= Rec.End; T += 10.0)
		{
			++Asked;
			const LandWalkPan::Plan P = LandWalkPan::ChooseTank(Rec, EW, T, 20.0);
			if (P.Block < 0) continue;
			++Driving;
			const double Near = std::min({ P.FromStart, P.FromMiddle, P.FromEnd }), Far = std::max({ P.FromStart, P.FromMiddle, P.FromEnd });
			if (!P.bOpen || Near < 150.0 || Far > 300.0) continue;
			++Held;
			if (First < 0.0)
			{
				First = T;
				FirstPlan = P;
			}
		}
		if (First >= 0.0)
			std::printf("tank pan (far tanks' motion vectors): %d of %d times asked hold (%d with a tank driving); the first at %.0f s: track %d at %.1f m/s, the eye %.1f m up, %.0f, %.0f and %.0f m off\n", Held, Asked, Driving, First,
				FirstPlan.Men.empty() ? -1 : FirstPlan.Men.front(), FirstPlan.Speed, FirstPlan.Eye.Z - GroundAt(FirstPlan.Eye.X, FirstPlan.Eye.Y), FirstPlan.FromStart, FirstPlan.FromMiddle, FirstPlan.FromEnd);
		else
			std::printf("tank pan (far tanks' motion vectors): none of %d times asked holds (%d with a tank driving)\n", Asked, Driving);
		if (Driving > 0 && Held == 0) Fault("tanks drive but no tank pan holds at any time asked");
	}
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
	// (the choice's share by steps alone, so a run is the same on any machine; LAND_PLAY_CHOOSE_MS=ms tries the clock's
	// share as the game has it, ms a frame)
	ES.ChooseMs = std::getenv("LAND_PLAY_CHOOSE_MS") ? std::atof(std::getenv("LAND_PLAY_CHOOSE_MS")) : 0.0;
	// The first bail-out (LAND_PLAY_BAIL=1, as the scene with -TheaterLandBailOut): every tank's crew may get out; at her
	// kill (BailOut, below) five crews in six do, as the effects thread's LandBail plays them.
	const bool bBailTest = std::getenv("LAND_PLAY_BAIL") != nullptr;
	if (bBailTest) EW.Bails = [&Rec](int T) { return Rec.Tracks[static_cast<size_t>(T)].Role == "tank"; };
	// The special shots (Brian's yes, 23 November 19:52Z; each as the scene's own switch): LAND_PLAY_WHISTLE=1 the
	// whistle start, LAND_PLAY_NICK=1 a nickname for every infantry and armoured block (=side: one a side), LAND_PLAY_STAND=1 the last stand.
	const bool bWhistleTest = std::getenv("LAND_PLAY_WHISTLE") != nullptr;
	const bool bNickTest = std::getenv("LAND_PLAY_NICK") != nullptr;
	const bool bStandTest = std::getenv("LAND_PLAY_STAND") != nullptr;
	ES.bWhistle = bWhistleTest;
	ES.bLastStand = bStandTest;
	// (LAND_PLAY_WHISTLE=hidden: the attacker's foot hidden from the player all battle, the officer among them: the whistle
	// cannot be shown, and the film must still open on its aerial. A check of its own: the ride film, on those hidden men,
	// fails its gates in it)
	const bool bWhistleHidden = bWhistleTest && std::string(std::getenv("LAND_PLAY_WHISTLE")) == "hidden";
	if (bWhistleHidden)
		EW.Hidden = [&Rec](int B)
		{
			const LandFeed::Block* Bk = Rec.BlockOf(B);
			return Bk && Bk->Side == 0 && Bk->Kind == "inf";
		};
	if (bNickTest)
		EW.Nickname = [&Rec](int B)
		{
			// (LAND_PLAY_NICK=side: one name for every block of a side, as a corps' name carried by all its blocks)
			const char* How = std::getenv("LAND_PLAY_NICK");
			const LandFeed::Block* Bk = Rec.BlockOf(B);
			if (!Bk || (Bk->Kind != "inf" && Bk->Kind != "arm")) return std::string();
			return How && std::string(How) == "side" ? "Side " + std::to_string(Bk->Side) : "Nick " + std::to_string(B);
		};
	LandEye::Director Eye;
	Eye.Begin(Rec, EW, ES);
	LandDesk::Desk Desk;
	Desk.Begin(Rec, 0);

	LandEye::Pose Pose;
	Mix.OnScreen = [&](const Vec& At) { return InPicture(Pose, At, 1.1); };
	FilmMeasure Film;
	Film.Rec = &Rec;
	Film.Set = ES;
	Film.bNearReport = true;

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
	// The takes' players pooled as the game pools them (LandSound::Voices, land.voices): each take started when it is
	// due (its Delay) on a voice, the voice free once its player stops (its length at its pitch, or its cut and fade),
	// a cut faded on the frame it falls due, as ALandBattle::StartTakes does.
	LandSound::Voices Pool;
	Pool.Size(Mix.VoicesWarm, Mix.VoicesMost);
	struct FPlayer { double Stops = -1.0, CutAt = -1.0; };
	std::vector<FPlayer> Players(static_cast<size_t>(Pool.Count()));
	std::vector<std::pair<double, LandSound::Take>> PoolDue;
	std::vector<int> PoolFades;
	int PoolCutOff = 0, PoolStaleHeld = 0, PoolCuts = 0, PoolSilent = 0;
	double PoolFadeLate = 0.0, PoolFadeEarly = 0.0;
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
	// (radio call, then the barrage: the calls the film went to, looked at as their first salvo lands)
	size_t RadioLooked = 0;
	int RadioBarrages = 0, RadioSeen = 0;
	double RadioGapLeast = 1e9, RadioCutAt = -1.0;
	// (the shallow focus, E9: frames and shots in it, by kind; switched only on a cut; how near the man it holds)
	int FocusFrames = 0, FightFrames = 0, FocusCuts = 0, FocusAllCuts = 0, FocusWrongKind = 0, FocusMidShot = 0;
	bool bFocusWas = false;
	double FocusPullMost = 0.0, FocusWas = 0.0, FocusShotAt = -1.0;
	std::vector<double> FocusOff;
	// (the first bail-out: the crews that got out, those on a bail watch then, the hold's lens at her hit, and the tanks'
	// ends the hold kept the film from)
	int BailCrews = 0, BailWatched = 0, BailEndsPassed = 0;
	// (the special shots: each one's frames held, its card's frames, and the lens at its moment: the step-off, the kill's
	// landing, the middle of the stand)
	struct SpecialSeen
	{
		int HoldFrames = 0, CardFrames = 0, MenIn = 0;
		bool bLooked = false, bIn = false;
		double Off = 0.0, Up = 0.0;
	};
	std::vector<SpecialSeen> SpSeen;
	bool bBailLensSeen = false;
	double BailLensOff = 0.0, BailLensUp = 0.0;
	bool bBailInPicture = false;
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
	// The general's battle, fought as it is shown (release 2, live): the same battle grown delta by delta (Tools/land_live
	// deltas.ts writes header.json and deltas.jsonl; the recording given here is its whole.json), the screen kept 2.5
	// battle seconds behind the rules as the scene keeps it (LandBattleLive.cpp's LiveLead) and waiting on them, and the
	// general's orders deltas.ts gives leaned to (Director::Lean). The film keeps every rule it keeps on the battle read
	// whole, with nothing of it known past what the rules have fought, and goes to the men he sent.
	if (const char* LiveDir = std::getenv("LAND_PLAY_LIVE"))
	{
		std::string HeaderText;
		LandFeed::Recording Live;
		std::ifstream Deltas(std::string(LiveDir) + "/deltas.jsonl");
		if (!LandJson::ReadFile(std::string(LiveDir) + "/header.json", HeaderText) || !LandFeed::ReadRecording(HeaderText, Live, Error) || !Deltas)
			Fault("live: cannot read the live battle's header or deltas");
		else
		{
			LandEye::Director Ld;
			Ld.Begin(Live, EW, ES);
			FilmMeasure LiveFilm;
			LiveFilm.Rec = &Rec;
			LiveFilm.Set = ES;
			std::vector<int> Mine;
			for (const LandFeed::Block& Bk : Live.Blocks) if (Bk.Side == Live.Me) Mine.push_back(Bk.Id);
			// (deltas.ts's orders: an attack by his first block at 20 s, a run by his second at 25; and one more in the thick
			// of the fight, to his third)
			const double OrderAt[3] = { 20.0, 25.0, 120.0 };
			double LeanAskedAt[3] = { -1.0, -1.0, -1.0 }, LeanMetAt[3] = { -1.0, -1.0, -1.0 };
			const int Orders = std::min<int>(3, static_cast<int>(Mine.size()));
			double B = 0.0, R = 0.0, Waited = 0.0;
			int Deltas_ = 0, Frames_ = 0;
			long MostSteps = 0;
			double SlowestMs = 0.0, SumMs = 0.0;
			std::string Line;
			bool bMore = true;
			while (B < Rec.End + 6.0)
			{
				const double BFrom = B;
				const double Step = Dt * (B < Contact - 10.0 ? 2.0 : 0.5);
				// (the rules kept ahead: 2.5 battle seconds at life's pace, more as the screen runs faster)
				const double Lead = 2.5 * std::max(1.0, Step / Dt);
				while (bMore && !Live.bOver && Live.Fought < B + Step + Lead)
				{
					if (!std::getline(Deltas, Line)) { bMore = false; break; }
					LandFeed::Grown G;
					if (!LandFeed::AppendDelta(Line, Live, G, Error)) { Fault("live: a delta not read"); bMore = false; break; }
					++Deltas_;
				}
				// (the screen waits on the rules: never within LiveMargin, a battle second, of what they have fought)
				const double Cap = Live.bOver ? 1e18 : Live.Fought - 1.0;
				if (B + Step > Cap) Waited += Dt;
				B = std::max(BFrom, std::min(B + Step, Cap));
				R += Dt;
				++Frames_;
				PlaceMovers(B);
				for (int K = 0; K < Orders; ++K)
					if (LeanAskedAt[K] < 0.0 && B >= OrderAt[K]) { Ld.Lean(Mine[static_cast<size_t>(K)]); LeanAskedAt[K] = R; }
				const long Steps0 = Ld.Steps();
				const auto Lt0 = std::chrono::steady_clock::now();
				const LandEye::Pose P = Ld.Update(B, R, Dt);
				const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Lt0).count();
				SlowestMs = std::max(SlowestMs, Ms);
				if (Ms > 2.0 && std::getenv("LAND_PLAY_SLOW")) std::printf("  slow live eye %.2f ms (%ld steps) at battle %.2f: %s%s, why %s, tracks %zu rounds %zu\n", Ms, Ld.Steps() - Steps0, B, LandEye::NameOf(P.Shot), P.bCut ? " (cut)" : "", LandEye::NameOf(Ld.LastWhy()), Live.Tracks.size(), Live.Rounds.size());
				SumMs += Ms;
				MostSteps = std::max(MostSteps, Ld.Steps() - Steps0);
				LiveFilm.Feed(Ld, P, BFrom, B, R, Dt);
				if (std::getenv("LAND_PLAY_LIVE_CUTS") && P.bCut)
					std::printf("  live cut at battle %.1f (real %.1f): %s on %d (block %d), why %s, leaning %d\n", B, R, LandEye::NameOf(P.Shot), P.Subject,
						P.Subject >= 0 ? Live.Tracks[static_cast<size_t>(P.Subject)].Block : -1, LandEye::NameOf(Ld.LastWhy()), Ld.Leaning());
				for (int K = 0; K < Orders; ++K)
					if (LeanAskedAt[K] >= 0.0 && LeanMetAt[K] < 0.0 && P.Subject >= 0 && Live.Tracks[static_cast<size_t>(P.Subject)].Block == Mine[static_cast<size_t>(K)])
						LeanMetAt[K] = R;
			}
			std::printf("live: %d deltas, %d frames, the screen waited on the rules %.1f s; the film's own section block %d section %d; the camera's work at most %ld ray steps a frame, %.3f ms on average and %.2f ms the slowest (the lists kept up with the battle included); %d cuts",
				Deltas_, Frames_, Waited, Ld.SquadBlock(), Ld.SquadK(), MostSteps, SumMs / std::max(1, Frames_), SlowestMs, Ld.Cuts());
			// (an order followed by the next before the film came to its men gives way to the next: the film goes to the
			// men last sent)
			bool bGaveWay[3] = { false, false, false };
			for (int K = 0; K < Orders; ++K)
			{
				bGaveWay[K] = K + 1 < Orders && LeanAskedAt[K + 1] >= 0.0 && (LeanMetAt[K] < 0.0 || LeanMetAt[K] > LeanAskedAt[K + 1]);
				if (bGaveWay[K]) std::printf("; the order to block %d at %.0f s: the next order came first", Mine[static_cast<size_t>(K)], OrderAt[K]);
				else std::printf("; the order to block %d at %.0f s: on its men %.1f s after", Mine[static_cast<size_t>(K)], OrderAt[K], LeanMetAt[K] >= 0.0 ? LeanMetAt[K] - LeanAskedAt[K] : -1.0);
			}
			LiveFilm.Report("live");
			std::printf("\n");
			if (Ld.SquadBlock() < 0) Fault("live: the film's own section was never chosen");
			if (MostSteps > 6000) Fault("live: the camera's work in a frame past 6,000 ray steps");
			// (the second order comes while the first's shot may still hold its least: each met within ShotLeast, the
			// choice and the next order's own shot)
			for (int K = 0; K < Orders; ++K)
				if (!bGaveWay[K] && (LeanMetAt[K] < 0.0 || LeanMetAt[K] - LeanAskedAt[K] > 2.0 * ES.ShotLeast + 1.0)) Fault("live: the film did not go to the men the general sent");
		}
		if (std::getenv("LAND_PLAY_ONLY_LIVE"))
		{
			std::printf("%s\n", Faults == 0 ? "PASS" : ("FAIL: " + std::to_string(Faults) + " faults").c_str());
			return Faults == 0 ? 0 : 1;
		}
	}
	double KnotStart = -1.0, KnotEnd = -1.0;
	double LowestEye = 1e9;
	std::vector<std::string> CutLines;
	// The lens near the ground over corn: frames, and how far over the ears it was at the lowest.
	int LowOverCorn = 0, LowInCorn = 0, LensBehindStalks = 0;
	double LeastOverEars = 1e9;
	// The thaw's mud (release 2, seasons: the campaign's weather word "mud", LandField::Field::Weather): the same field
	// laid again under it. Where the dry field's word is "earth" the mud field's is "mud" (the holes' soil, the rounds'
	// thrown mud), every other word and every height the same, and a tank's track on a mud pasture tears it (Ruts'
	// KindOn: earth, not pressed grass). Run before the scars, so LAND_PLAY_ONLY_SCARS runs it too.
	{
		const auto MudT0 = std::chrono::steady_clock::now();
		LandField::Field MudWords = FieldWords;
		MudWords.Weather = "mud";
		const LandField::Ground MudField(MudWords);
		const LandCrater::Ruts::KindFn DryKind = LandCrater::Ruts::KindOn(Field), MudKind = LandCrater::Ruts::KindOn(MudField);
		int Asked = 0, EarthToMud = 0, WordOff = 0, HeightOff = 0, GrassTorn = 0, GrassLeft = 0;
		const double Fw = FieldWords.Cell * FieldWords.Cols, Fh = FieldWords.Cell * FieldWords.Rows;
		for (double Y = 5.0; Y < Fh; Y += 23.0)
			for (double X = 5.0; X < Fw; X += 23.0)
			{
				++Asked;
				const LandField::Surface Dry = Field.At(X, Y), Mud = MudField.At(X, Y);
				const std::string DryWord = Dry.Fx, MudWord = Mud.Fx;
				if (DryWord == "earth") { if (MudWord == "mud") ++EarthToMud; else ++WordOff; }
				else if (MudWord != DryWord) ++WordOff;
				if (std::fabs(Dry.Z - Mud.Z) > 1e-9 || Dry.What != Mud.What) ++HeightOff;
				if (DryKind(X, Y) == LandCrater::Ruts::Under::Grass) { if (MudKind(X, Y) == LandCrater::Ruts::Under::Earth) ++GrassTorn; else ++GrassLeft; }
			}
		std::printf("mud: the field laid again under the thaw's mud in %.2f s; of %d places, %d earth now mud, %d words off, %d heights or surfaces off; pasture torn by a track %d, left pressed grass %d\n",
			std::chrono::duration<double>(std::chrono::steady_clock::now() - MudT0).count(), Asked, EarthToMud, WordOff, HeightOff, GrassTorn, GrassLeft);
		if (EarthToMud == 0) Fault("mud: no open earth turned to mud under the weather word");
		if (WordOff > 0) Fault("mud: a word other than earth's changed, or earth not to mud");
		if (HeightOff > 0) Fault("mud: the weather word moved the ground or what is drawn on it");
		if (GrassLeft > 0) Fault("mud: a track on a mud pasture left pressed grass, not torn earth");
	}
	// Scarred ground (release 2, looks only): the old holes of two battles fought here before, the last six months
	// ago, laid as LandBattle's LayScars lays them (every 4th point of every track the fought-over ground; open
	// ground only), in a field of their own: how many and how fast; none where Dig says no; each weathered (a bare
	// patch of its own, its bowl silted under a fresh 150's); a fresh round still laid after them; and room left for
	// the battle's own (checked against its count at the end). LAND_PLAY_ONLY_SCARS: this and the mud's alone.
	int ScarsLaid = 0;
	{
		LandCrater::Field Old;
		Old.Begin([&Rec](double X, double Y) { return Rec.Ground.HeightAt(X, Y); }, Rec.Seed ? Rec.Seed : 1);
		Old.Dig = LandCrater::Field::DigOn(Field);
		std::vector<std::pair<double, double>> Fought;
		for (const LandFeed::Track& Tr : Rec.Tracks)
			for (size_t K = 0; K < Tr.At.size(); K += 4) Fought.emplace_back(Tr.At[K].X, Tr.At[K].Y);
		const LandCrater::Field* OldPtr = &Old;
		const auto ScarT0 = std::chrono::steady_clock::now();
		ScarsLaid = LandCrater::LayScars(Old, Fought, 2, 6.0, Rec.Seed ? Rec.Seed : 1,
			[&Field, OldPtr](double X, double Y) -> std::string
			{
				const LandField::Surface Under = Field.At(X, Y);
				if (Under.What == LandField::Drawn::Floor || Under.What == LandField::Drawn::Wall || Under.What == LandField::Drawn::Water || Under.What == LandField::Drawn::Marsh) return std::string();
				if (OldPtr->Dig && !OldPtr->Dig(X, Y)) return std::string();
				return std::string(Under.Fx);
			},
			LandCrater::OldLinesOf(Field, Rec.Ground.TownX, Rec.Ground.TownY, Rec.Ground.TownR));
		const double ScarMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - ScarT0).count();
		const int ScarWanted = std::min(LandCrater::Field::Most / 6, 150 * 2);
		int NotDig = 0, NotBare = 0, NoBareR = 0, TooDeep = 0;
		double DeepestOld = 0.0;
		for (const LandCrater::Field::Hole& H : Old.All())
		{
			if (!Old.Dig(H.X, H.Y)) ++NotDig;
			if (!Old.Bare(H.X, H.Y)) ++NotBare;
			if (H.BareR <= 0.0) ++NoBareR;
			DeepestOld = std::max(DeepestOld, H.Deep);
			if (H.Deep > 0.81) ++TooDeep;
		}
		// (along the old lines, the art director's 23 November: most within 25 m of a hedge, a wall or a lane, or 30 m of
		// a junction or the village's edge, where the field has any over the fought-over ground)
		const LandCrater::OldLines OldOnes = LandCrater::OldLinesOf(Field, Rec.Ground.TownX, Rec.Ground.TownY, Rec.Ground.TownR);
		const auto OnOld = [&OldOnes](double Px, double Py)
		{
			for (const auto& Sp : OldOnes.Spots)
				if (std::hypot(Px - Sp.first, Py - Sp.second) <= 30.0) return true;
			for (const auto& Ln : OldOnes.Lines)
			{
				for (size_t K = 0; K + 1 < Ln.size(); ++K)
				{
					const double Ax = Ln[K].first, Ay = Ln[K].second, Bx = Ln[K + 1].first, By = Ln[K + 1].second;
					const double L2 = (Bx - Ax) * (Bx - Ax) + (By - Ay) * (By - Ay);
					const double T = L2 > 0.0 ? std::clamp(((Px - Ax) * (Bx - Ax) + (Py - Ay) * (By - Ay)) / L2, 0.0, 1.0) : 0.0;
					if (std::hypot(Px - (Ax + (Bx - Ax) * T), Py - (Ay + (By - Ay) * T)) <= 25.0) return true;
				}
			}
			return false;
		};
		int OnLine = 0, FoughtOn = 0, FoughtSeen = 0;
		for (const LandCrater::Field::Hole& H : Old.All())
			if (OnOld(H.X, H.Y)) ++OnLine;
		const double OnShare = ScarsLaid > 0 ? static_cast<double>(OnLine) / ScarsLaid : 0.0;
		// (against the fought-over ground itself: as many of its points, read the same way, for the share strewn holes would have)
		for (size_t K = 0; K < Fought.size(); K += std::max<size_t>(1, Fought.size() / 300), ++FoughtSeen)
			if (OnOld(Fought[K].first, Fought[K].second)) ++FoughtOn;
		const double StrewnShare = FoughtSeen > 0 ? static_cast<double>(FoughtOn) / FoughtSeen : 0.0;
		// (its age: a 150's bowl at 6, 12 and 24 months against a fresh one's, laid where nothing stops the dig)
		double AgedDeep[4] = { 0.0, 0.0, 0.0, 0.0 };
		if (!Fought.empty())
		{
			LandCrater::Field Ages;
			Ages.Begin([&Rec](double X, double Y) { return Rec.Ground.HeightAt(X, Y); }, 7);
			const double AgeMonths[4] = { -1.0, 6.0, 12.0, 24.0 };
			for (int K = 0; K < 4; ++K)
			{
				const double Ax = Fought[0].first + 40.0 * K, Ay = Fought[0].second;
				if (AgeMonths[K] < 0.0 ? Ages.Add(Ax, Ay, "heavy", 0.0, "earth") : Ages.AddOld(Ax, Ay, "heavy", 0.0, "earth", AgeMonths[K]))
					AgedDeep[K] = Ages.All().back().Deep;
			}
		}
		std::printf("scars: %.0f%% of the old holes on an old line (the fought-over ground's own points %.0f%%; %zu lines, %zu junctions and village edge); a 150's bowl %.2f m fresh, %.2f at 6 months, %.2f at 12, %.2f at 24\n",
			100.0 * OnShare, 100.0 * StrewnShare, OldOnes.Lines.size(), OldOnes.Spots.size(), AgedDeep[0], AgedDeep[1], AgedDeep[2], AgedDeep[3]);
		// (hedgerow country, where the fought-over ground's own points lie mostly by a line: most of the holes along the
		// lines. A field thin of lines, one lane across open steppe: more along the lane than strewn holes would lie, in
		// knots, and never most of them, a dotted stripe down the lane with the steppe clean: the art director, 25 November
		// 19:00Z.)
		if (!OldOnes.Lines.empty() && ScarsLaid >= 50 && StrewnShare >= 0.4 && OnShare < 0.55) Fault("scars: the old holes strewn, not along the old lines");
		if (!OldOnes.Lines.empty() && ScarsLaid >= 50 && StrewnShare < 0.4 && OnShare < StrewnShare + 0.1) Fault("scars: a thin field's old holes strewn, none drawn to its lane");
		if (!OldOnes.Lines.empty() && ScarsLaid >= 50 && StrewnShare < 0.4 && OnShare > 0.6) Fault("scars: a thin field's old holes a stripe down its lane, its open ground clean");
		if (AgedDeep[0] > 0.0)
		{
			const double Mark[3] = { 0.8, 0.65, 0.5 };
			for (int K = 0; K < 3; ++K)
				if (std::fabs(AgedDeep[K + 1] / AgedDeep[0] - Mark[K]) > 0.01) Fault("scars: an old bowl's depth off the art director's mark for its age");
		}
		// (a fresh 105 after them, on the first open ground of the fought-over ground with no old hole's cut on it)
		bool bFreshLaid = false, bFreshTried = false;
		for (const auto& P : Fought)
		{
			if (!Old.Dig(P.first, P.second) || Old.InCut(P.first, P.second) || Old.Bare(P.first, P.second)) continue;
			bFreshTried = true;
			bFreshLaid = Old.Add(P.first, P.second, "field", 105.0, "earth");
			break;
		}
		std::printf("scars: %d old holes of %d wanted (2 battles, 6 months) laid in %.1f ms in %d patches; deepest old bowl %.2f m; a fresh 105 after them %s\n",
			ScarsLaid, ScarWanted, ScarMs, Old.Patches(), DeepestOld, bFreshLaid ? "laid" : bFreshTried ? "NOT laid" : "(no open ground to try)");
		if (!Fought.empty() && ScarsLaid == 0) Fault("scars: no old hole laid on the fought-over ground");
		if (ScarsLaid > ScarWanted) Fault("scars: more old holes than wanted");
		if (ScarMs > 1500.0) Fault("scars: laying the old holes took past 1.5 s");
		if (NotDig > 0) Fault("scars: an old hole on ground Dig keeps (a lane, a yard, a floor, water)");
		if (NotBare > 0) Fault("scars: the grass not kept off an old hole's middle");
		if (NoBareR > 0) Fault("scars: an old hole without its own bare patch");
		if (TooDeep > 0) Fault("scars: an old hole's bowl deeper than 0.81 m (a 150's at six months is 0.8)");
		if (bFreshTried && !bFreshLaid) Fault("scars: a fresh round's hole not laid after the old ones");
	}
	// The thrown soil's thin edge on the ground as drawn (pre-check's probe, 23 November: laid 2 mm over HeightAt's
	// smooth height, a quarter of its points sank under the drawn ground's flat triangles on a slope's bend): holes laid
	// as the game lays them (on HeightAt, painted by the ground's colour, lit by its normal, their soil on DrawnZ), each
	// outer point at or over the drawn ground but inside a newer hole's cut, where that ground is cut away.
	{
		LandCrater::Field Edge;
		Edge.Begin([&Field](double X, double Y) { return Field.HeightAt(X, Y); }, 29u, [&Field](double X, double Y) { return Field.ColourAt(X, Y); });
		Edge.Lit = [&Field](double X, double Y, double& NX, double& NY, double& NZ) { Field.DrawnUp(X, Y, NX, NY, NZ); };
		Edge.Drawn = [&Field](double X, double Y) { return Field.DrawnZ(X, Y); };
		Edge.Dig = LandCrater::Field::DigOn(Field);
		int Tried = 0;
		for (const LandFeed::Track& Tr : Rec.Tracks)
		{
			for (size_t K = 0; K < Tr.At.size() && Tried < 300; K += 37)
			{
				const double X = Tr.At[K].X, Y = Tr.At[K].Y;
				if (!Edge.Dig(X, Y) || Edge.Bare(X, Y)) continue;
				++Tried;
				Edge.Add(X, Y, Tried % 3 == 0 ? "mortar" : Tried % 3 == 1 ? "field" : "heavy", 0.0, std::string(Field.At(X, Y).Fx));
			}
		}
		int EdgePoints = 0, EdgeUnder = 0;
		double EdgeWorst = 0.0;
		for (const LandCrater::Field::Hole& H : Edge.All())
		{
			if (H.bBreach || H.Sides < 3) continue;
			const LandField::Mesh& M = Edge.Section(H.In);
			for (int K = 0; K < H.Sides; ++K)
			{
				// (its outer ring: the middle, then nine rings of Sides points, the ninth the thin edge)
				const size_t I = static_cast<size_t>(H.First) + 1u + static_cast<size_t>(8 * H.Sides + K);
				if (I * 3 + 2 >= M.Positions.size()) continue;
				const double Px = M.Positions[I * 3], Py = M.Positions[I * 3 + 1], Pz = M.Positions[I * 3 + 2];
				if (Edge.InCut(Px, Py)) continue;
				++EdgePoints;
				const double Below = Field.DrawnZ(Px, Py) - Pz;
				EdgeWorst = std::max(EdgeWorst, Below);
				if (Below > 0.0005) ++EdgeUnder;
			}
		}
		std::printf("thrown soil's edge: %zu holes, %d edge points, %d under the drawn ground (the most %.1f mm)\n", Edge.All().size(), EdgePoints, EdgeUnder, 1000.0 * EdgeWorst);
		if (EdgePoints > 0 && EdgeUnder > 0) Fault("a shell hole's thin edge sank under the ground as drawn");
	}
	// Mud memory (release 2; Brian 23 November): the ruts of two battles fought here in mud before, laid as LandBattle's
	// LayOldRuts lays them (its ways through the hedges' gateways, else along the machines' heading); how many, how fast,
	// within their cap; a month on bare (no grass stands in them), at six grassed (it stands again).
	{
		std::vector<std::pair<double, double>> Fought, Gates;
		double Sx = 0.0, Sy = 0.0;
		for (const LandFeed::Track& Tr : Rec.Tracks)
		{
			for (size_t K = 0; K < Tr.At.size(); K += 4) Fought.emplace_back(Tr.At[K].X, Tr.At[K].Y);
			if (Tr.IsVehicle() && Tr.At.size() >= 2)
			{
				const double Dx = Tr.At.back().X - Tr.At.front().X, Dy = Tr.At.back().Y - Tr.At.front().Y, Long = std::hypot(Dx, Dy);
				if (Long > 20.0) { Sx += Long * std::cos(2.0 * std::atan2(Dy, Dx)); Sy += Long * std::sin(2.0 * std::atan2(Dy, Dx)); }
			}
		}
		for (const LandField::HedgeOpening& O : Field.Openings()) Gates.emplace_back(O.X, O.Y);
		const double Heading = Sx * Sx + Sy * Sy > 0.0 ? 0.5 * std::atan2(Sy, Sx) : std::nan("");
		const auto RutT0 = std::chrono::steady_clock::now();
		// (where a column may drive, as LandBattle's LayOldRuts says)
		const auto Open = [&Field](double X, double Y)
		{
			LandField::Drawn What = LandField::Drawn::Pasture;
			Field.HeightAt(X, Y, &What);
			return What == LandField::Drawn::Pasture || What == LandField::Drawn::Crop || What == LandField::Drawn::Rough || What == LandField::Drawn::Works || What == LandField::Drawn::Beyond;
		};
		const auto Ways = LandCrater::OldWays(Fought, 2, Rec.Seed ? Rec.Seed : 1, Gates, Heading, Open);
		// (none across a hedge's bank, a lane, a wall, a yard, a wood or water, but within 6 m of a gateway: pre-check's
		// 01:36Z, the ways had crossed banks away from any gateway; read every 1.2 m along them)
		int Crossed = 0, Read = 0;
		for (const auto& W : Ways)
			for (size_t K = 0; K + 1 < W.size(); ++K)
			{
				const double L = std::hypot(W[K + 1].first - W[K].first, W[K + 1].second - W[K].second);
				for (int I = 0; I < std::max(1, static_cast<int>(L / 1.2)); ++I)
				{
					const double T = I * 1.2 / std::max(1e-6, L), Px = W[K].first + (W[K + 1].first - W[K].first) * T, Py = W[K].second + (W[K + 1].second - W[K].second) * T;
					bool bNearGate = false;
					for (const auto& G : Gates)
						if (std::hypot(Px - G.first, Py - G.second) < 6.5) { bNearGate = true; break; }
					if (bNearGate) continue;
					++Read;
					if (!Open(Px, Py)) ++Crossed;
				}
			}
		int Laid[2] = { 0, 0 }, FlatOn[2] = { 0, 0 }, Asked = 0;
		double RutMs = 0.0;
		for (int Age = 0; Age < 2; ++Age)
		{
			LandCrater::Ruts Old;
			Old.Begin([&Field](double X, double Y) { return Field.HeightAt(X, Y); }, [&Field](double X, double Y) { return Field.ColourAt(X, Y); }, LandCrater::Ruts::KindOn(Field));
			Laid[Age] = Old.LayOld(Ways, Age == 0 ? 1.0 : 6.0, false, Rec.Seed ? Rec.Seed : 1);
			if (Age == 0) RutMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - RutT0).count();
			// (on each way's ruts, either side of its line some 1.35 m)
			for (const auto& W : Ways)
				for (size_t K = 1; K + 1 < W.size(); K += 3)
				{
					const double Dx = W[K + 1].first - W[K - 1].first, Dy = W[K + 1].second - W[K - 1].second, L = std::max(1e-6, std::hypot(Dx, Dy));
					for (const double Side : { 1.35, -1.35 })
					{
						if (Age == 0) ++Asked;
						if (Old.Flat(W[K].first - Dy / L * Side, W[K].second + Dx / L * Side)) ++FlatOn[Age];
					}
				}
		}
		std::printf("mud memory: %d of %d places along the ways where no column drives (but at a gateway)\n", Crossed, Read);
		if (Crossed > 0) Fault("mud memory: an old way crosses a hedge's bank, a lane, a wall, a yard, a wood or water");
		std::printf("mud memory: %zu ways (%zu gateways), %d pieces of old ruts at a month and %d at six (at most %d) laid in %.1f ms; grass kept off at %d of %d places on them at a month, %d at six\n",
			Ways.size(), Gates.size(), Laid[0], Laid[1], LandCrater::Ruts::MostOld, RutMs, FlatOn[0], Asked, FlatOn[1]);
		if (!Fought.empty() && Ways.empty()) Fault("mud memory: no way laid over the fought-over ground");
		if (!Ways.empty() && Laid[0] == 0) Fault("mud memory: no old rut laid");
		if (Laid[0] > LandCrater::Ruts::MostOld || Laid[1] > LandCrater::Ruts::MostOld) Fault("mud memory: more old ruts than their cap");
		if (Laid[0] != Laid[1]) Fault("mud memory: the same ways laid differently by their age");
		if (RutMs > 500.0) Fault("mud memory: laying the old ruts took past half a second");
		if (Laid[0] > 0 && FlatOn[0] == 0) Fault("mud memory: grass standing in a month-old rut (dried mud)");
		if (FlatOn[1] > 0) Fault("mud memory: grass kept off a rut grassed over at six months");
	}
	if (std::getenv("LAND_PLAY_ONLY_SCARS"))
	{
		std::printf("%s\n", Faults == 0 ? "PASS" : ("FAIL: " + std::to_string(Faults) + " faults").c_str());
		return Faults == 0 ? 0 : 1;
	}
	// The shell holes, laid as the game lays them (LandBattle's MakeCraters): every shell, bomb and grenade that lands
	// in the ground, none on armour.
	LandCrater::Field Holes;
	Holes.Begin([&Rec](double X, double Y) { return Rec.Ground.HeightAt(X, Y); }, Rec.Seed ? Rec.Seed : 1);
	// (dug where the game digs them, and the ground drawn cut over their bowls as the game cuts it: LandCrater::Dug)
	Holes.Dig = LandCrater::Field::DigOn(Field);
	GapHoles = &Holes;
	// (each cut's bank torn at T1, as LandBattle's MakeBreaches tears it: where, how long it took, whether her path
	// then went over the gap's floor)
	std::vector<char> CutTorn(Rec.Cuts.size(), 0);
	int CutsTornCount = 0, CutsNoBank = 0, CutsDrivenThrough = 0;
	double BreachMsMost = 0.0;
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
	// Effects (R2), small arms off armour: the bursts drawn off a plate, by the role of what they struck; the worst way
	// off a unit outward way, the farthest plate from her middle, and how many such bursts had it wrong
	std::map<std::string, int> RicochetBy;
	double RicochetNormalWorst = 0.0, RicochetPlateFarthest = 0.0;
	int RicochetWrong = 0, RicochetOffPlate = 0;
	// (each strike of such a burst, where the kit lays it (her plate as handed over, carried on at her way to its moment:
	// the tracer's 760 m a second, then the burst's rate) against her plain box's face at that moment; and where it would
	// have been left standing at her landing, as before this followed her)
	// (Cast: the odd rounds, their plate from the stand-in for the scene's cast; the even ones from the replay's plain box)
	std::vector<double> RicochetStrikeOff, RicochetStillOff, RicochetCastOff;
	double RicochetInside = 0.0, RicochetStillInside = 0.0, RicochetMovingMost = 0.0, RicochetCastInside = 0.0;
	int RicochetOutOfRoom = 0, RicochetStrikesMoving = 0, RicochetInsideMany = 0, RicochetNoPath = 0, RicochetPastPath = 0;

	// The picture run (LAND_PLAY_STILLS; LAND_PLAY_ONLY_STILLS runs it alone): the scene's ten stills as LandBattle asks
	// them (each asked two battle seconds before it is taken, one film from the start), and for each what the camera made
	// of it: the shot and its man, placed unfit or not, the men of the battle in the picture, and the share of the picture
	// that is a house's wall or roof.
	auto RunStills = [&]()
	{
		const double SContact = Rec.Contact >= 0.0 ? Rec.Contact : 30.0;
		struct Still { const char* Name; double At; int Kind; int Subject; bool bBuilding; };
		std::vector<Still> Stills;
		auto Standing = [&Rec](int Side, double At, bool bVehicle) -> int
		{
			for (size_t K = 0; K < Rec.Tracks.size(); ++K)
			{
				const LandFeed::Track& T = Rec.Tracks[K];
				const LandFeed::Block* B = Rec.BlockOf(T.Block);
				if (!B || B->Side != Side || T.bTwin || T.IsVehicle() != bVehicle || T.Start() > At || (T.Fell >= 0.0 && T.Fell < At + 4.0) || (T.Out >= 0.0 && T.Out < At + 4.0)) continue;
				if (!bVehicle && (T.Role == "mortar" || T.Role == "fo")) continue;
				return static_cast<int>(K);
			}
			return -1;
		};
		const int KWide = static_cast<int>(LandEye::Kind::Wide), KShoulder = static_cast<int>(LandEye::Kind::Shoulder), KLow = static_cast<int>(LandEye::Kind::Low);
		const int KTank = static_cast<int>(LandEye::Kind::Tank), KWatch = static_cast<int>(LandEye::Kind::Watch);
		Stills.push_back({ "01-opening", 4.0, -1, -1, false });
		Stills.push_back({ "02-the-advance", std::max(12.0, SContact - 14.0), KWide, -1, false });
		Stills.push_back({ "03-among-the-men", SContact + 12.0, KShoulder, Standing(0, SContact + 10.0, false), false });
		Stills.push_back({ "04-in-the-grass", SContact + 45.0, KLow, Standing(0, SContact + 43.0, false), false });
		{
			double FirstShot = -1.0;
			int First = -1, FirstFall = -1, FirstKill = -1, Fallen = -1;
			for (size_t K = 0; K < Rec.Tracks.size(); ++K)
			{
				const LandFeed::Track& T = Rec.Tracks[K];
				if (T.Role == "tank" && !T.Shots.empty() && (FirstShot < 0.0 || T.Shots.front() < FirstShot)) { FirstShot = T.Shots.front(); First = static_cast<int>(K); }
				if (T.Fell >= 0.0 && !T.bTwin)
				{
					int& Into = T.IsVehicle() ? FirstKill : FirstFall;
					if (Into < 0 || T.Fell < Rec.Tracks[static_cast<size_t>(Into)].Fell) Into = static_cast<int>(K);
				}
				if (T.IsMan() && T.Fell >= 0.0 && T.Fell < Rec.End * 0.75 && !T.bTwin) Fallen = static_cast<int>(K);
			}
			if (First >= 0) Stills.push_back({ "05-the-tanks", FirstShot + 0.25, KTank, First, false });
			if (FirstFall >= 0) Stills.push_back({ "06-a-man-falls", Rec.Tracks[static_cast<size_t>(FirstFall)].Fell + 0.8, KWatch, FirstFall, false });
			if (FirstKill >= 0) Stills.push_back({ "07-a-tank-knocked-out", Rec.Tracks[static_cast<size_t>(FirstKill)].Fell + 2.0, KWatch, FirstKill, false });
			Stills.push_back({ "08-the-village", Rec.End * 0.6, KWatch, Standing(1, Rec.End * 0.6 - 2.0, false), true });
			if (Fallen >= 0) Stills.push_back({ "09-the-fallen", Rec.End * 0.82, KLow, Fallen, false });
			// (the guns open: the machine guns' long fire after contact, LandFeed::GunsOpen, as the scene asks it)
			double GunsAt = -1.0;
			int GunsTrack = -1;
			if (LandFeed::GunsOpen(Rec, GunsAt, GunsTrack)) Stills.push_back({ "11-the-guns-open", GunsAt, GunsTrack >= 0 ? KWatch : KWide, GunsTrack, false });
		}
		std::sort(Stills.begin(), Stills.end(), [](const Still& A, const Still& B) { return A.At < B.At; });
		// (what a ray from the eye meets first within 250 m: 1 a house, 2 anything else the scene draws or the ground, 0
		// nothing)
		auto RayMeets = [&](const Vec& From, const Vec& Dir) -> int
		{
			for (double S = 1.0; S < 250.0; S += S < 40.0 ? 0.5 : 1.0)
			{
				const Vec P = From + Dir * S;
				LandField::Drawn What = LandField::Drawn::Pasture;
				const double Z = Field.HeightAt(P.X, P.Y, &What);
				if (What == LandField::Drawn::Floor && P.Z < Z + 7.0) return 1;
				if (P.Z < Z) return 2;
				if (What == LandField::Drawn::Wall && P.Z < Z + 2.0) return 2;
				if (What == LandField::Drawn::Bank && P.Z < Z + 3.0) return 2;
				if (Sight.At(P, -1) != LandSight::Hit::None) return 2;
			}
			return 0;
		};
		auto HouseShare = [&](const LandEye::Pose& P) -> double
		{
			Vec F = P.Look - P.Eye;
			F = F * (1.0 / std::max(1e-6, F.Length()));
			Vec R(F.Y, -F.X, 0.0);
			R = R * (1.0 / std::max(1e-6, R.Length()));
			const Vec U(R.Y * F.Z - R.Z * F.Y, R.Z * F.X - R.X * F.Z, R.X * F.Y - R.Y * F.X);
			const double Tv = std::tan(P.Fov * 0.5 * 3.14159265 / 180.0), Th = Tv * 16.0 / 9.0;
			int Hits = 0, All = 0;
			for (int Iy = 0; Iy < 9; ++Iy)
				for (int Ix = 0; Ix < 16; ++Ix)
				{
					const double Sx = (Ix + 0.5) / 8.0 - 1.0, Sy = 1.0 - (Iy + 0.5) / 4.5;
					Vec D = F + R * (Sx * Th) + U * (Sy * Tv);
					D = D * (1.0 / D.Length());
					++All;
					if (RayMeets(P.Eye, D) == 1) ++Hits;
				}
			return static_cast<double>(Hits) / All;
		};
		// (twice: the second with the film cut every 2.3 s between the asks, a run whose history parts from the first,
		// as the photographer's light A/Bs part by the dark rule: each asked still must come out the same)
		struct Took { LandEye::Kind Shot; int Subject; Vec Eye; };
		std::vector<Took> First(Stills.size(), Took{ LandEye::Kind::Open, -1, Vec() });
		int Unfit = 0, Empty = 0, NoHouse = 0, Same = 0, Houses = 0, NearMan = 0;
		double SlowMs = 0.0, SlowAt = 0.0, FindMs = 0.0;
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			LandEye::Director Pic;
			Pic.Begin(Rec, EW, ES);
			// (as the scene's picture run does as the battle opens)
			const auto H0 = std::chrono::steady_clock::now();
			Pic.FindHouses();
			if (Pass == 0) FindMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - H0).count();
			std::vector<bool> AskedYet(Stills.size(), false), Taken(Stills.size(), false);
			double B = 0.0, R = 0.0, NextStir = 30.0;
			const double Last = Stills.empty() ? 0.0 : Stills.back().At;
			while (B < Last + 0.1)
			{
				const double Speed = B < Contact - 10.0 ? 2.0 : 1.0 / Life;
				B += Dt * Speed;
				R += Dt;
				PlaceMovers(B);
				if (Pass == 1 && R >= NextStir)
				{
					NextStir = R + 2.3;
					bool bNearAsk = false;
					for (const Still& St : Stills) bNearAsk = bNearAsk || (B > St.At - 2.6 && B < St.At + 0.2);
					if (!bNearAsk) Pic.CutNow();
				}
				for (size_t I = 0; I < Stills.size(); ++I)
					// (not before the still before it is taken: two asked within 2 s of each other, the later took the
					// earlier's still, as Sedan's 05 took 06's)
					if (!AskedYet[I] && Stills[I].Kind >= 0 && B >= Stills[I].At - 2.0 && (I == 0 || Taken[I - 1]))
					{
						AskedYet[I] = true;
						Pic.Ask(static_cast<LandEye::Kind>(Stills[I].Kind), Stills[I].Subject, Stills[I].bBuilding);
					}
				const auto U0 = std::chrono::steady_clock::now();
				const LandEye::Pose P = Pic.Update(B, R, Dt);
				const double UMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - U0).count();
				if (Pass == 0 && UMs > SlowMs) { SlowMs = UMs; SlowAt = B; }
				for (size_t I = 0; I < Stills.size(); ++I)
				{
					if (Taken[I] || B < Stills[I].At) continue;
					Taken[I] = true;
					const Still& St = Stills[I];
					const bool bUnfit = St.Kind >= 0 && Pic.AskedUnfit();
					if (Pass == 1)
					{
						const Took& W = First[I];
						if (W.Shot == P.Shot && W.Subject == P.Subject && (W.Eye - P.Eye).Length() < 0.5) ++Same;
						else std::printf("  still %s in the stirred run: %s on %d, eye %.1f m from the first run's (%s on %d)\n", St.Name, LandEye::NameOf(P.Shot), P.Subject, (W.Eye - P.Eye).Length(), LandEye::NameOf(W.Shot), W.Subject);
						continue;
					}
					First[I] = { P.Shot, P.Subject, P.Eye };
					// (the battle's men standing in the picture within 150 m, and seen: no bank, house or wood across)
					int Men = 0;
					for (size_t K = 0; K < Rec.Tracks.size(); ++K)
					{
						const LandFeed::Track& T = Rec.Tracks[K];
						if (T.bTwin || !T.IsMan() || T.Start() > B || (T.Out >= 0.0 && B > T.Out)) continue;
						const LandFeed::Spot Sp = LandFeed::SpotAt(T, B);
						const Vec Chest(Sp.X, Sp.Y, GroundAt(Sp.X, Sp.Y) + 1.0);
						if ((Chest - P.Eye).Length() > 150.0 || !LandEye::InPicture(P, Chest, 0.95)) continue;
						if (std::getenv("LAND_PLAY_STILL_MEN") && (Chest - P.Eye).Length() < 80.0) std::printf("    %s: man %zu at %.1f m, open %d, fell %d\n", St.Name, K, (Chest - P.Eye).Length(), EW.Open(P.Eye, Chest) ? 1 : 0, T.Fell >= 0.0 && T.Fell <= B ? 1 : 0);
						if (!EW.Open(P.Eye, Chest)) continue;
						++Men;
					}
					// (the art director on 83749bb4: other men 2 m off a still's lens, wholly in frame or out; nearer only
					// running past, within 2 m no more than half a second on the screen and never within 0.9 m)
					double Nearest = 1e9, Held = 0.0;
					for (size_t K = 0; K < Rec.Tracks.size(); ++K)
					{
						const LandFeed::Track& T = Rec.Tracks[K];
						if (static_cast<int>(K) == P.Subject || T.bTwin || !T.IsMan() || T.Start() > B || (T.Out >= 0.0 && B > T.Out)) continue;
						const LandFeed::Spot Sp = LandFeed::SpotAt(T, B);
						const double Gap = std::hypot(Sp.X - P.Eye.X, Sp.Y - P.Eye.Y);
						if (Gap >= 2.0) continue;
						const Vec Head(Sp.X, Sp.Y, GroundAt(Sp.X, Sp.Y) + 1.0);
						if (!LandEye::InPicture(P, Head, 1.4)) continue;
						const bool bDown = T.Fell >= 0.0 && T.Fell <= B;
						double Span = 0.0, Least = Gap;
						for (int Way = -1; Way <= 1; Way += 2)
							for (double U = Way < 0 ? 0.01 : 0.0; U < 5.0; U += 0.01)
							{
								const double At = B + Way * U;
								if (At < T.Start()) break;
								const LandFeed::Spot S2 = LandFeed::SpotAt(T, At);
								const double G2 = std::hypot(S2.X - P.Eye.X, S2.Y - P.Eye.Y);
								if (G2 >= 2.0) break;
								Least = std::min(Least, G2);
								Span += 0.01;
							}
						// (battle seconds to the screen's: the fight runs at 1 / Life of the screen's pace)
						const double OnScreen = bDown ? 99.0 : Span * Life;
						Nearest = std::min(Nearest, Least);
						Held = std::max(Held, OnScreen);
					}
					if (Held > 0.55 || Nearest < 0.85) ++NearMan;
					const double House = HouseShare(P);
					std::string Asked = "-";
					if (St.Subject >= 0)
					{
						const LandFeed::Track& T = Rec.Tracks[static_cast<size_t>(St.Subject)];
						const LandFeed::Change* C = LandFeed::ChangeAt(T, B);
						Asked = T.Role + " " + (C ? C->Pose : std::string("idle"));
					}
					char NearText[96] = "";
					if (Nearest < 1e8) std::snprintf(NearText, sizeof(NearText), "; a man %.2f m off the lens, within 2 m %.2f s", Nearest, Held);
					std::printf("still %s at %.1f s: asked %s on %d (%s), took %s on %d%s%s; eye %.2f m up, fov %.0f; %d men in the picture, a house %.0f%% of it%s\n",
						St.Name, B, St.Kind >= 0 ? LandEye::NameOf(static_cast<LandEye::Kind>(St.Kind)) : "none", St.Subject, Asked.c_str(), LandEye::NameOf(P.Shot), P.Subject,
						bUnfit ? " UNFIT" : "", St.bBuilding ? (Pic.HouseInShot() ? " (a house kept in it)" : " (no house fit)") : "", P.Eye.Z - GroundAt(P.Eye.X, P.Eye.Y), P.Fov, Men, 100.0 * House, NearText);
					if (bUnfit) ++Unfit;
					const bool bOnMan = St.Subject >= 0 && Rec.Tracks[static_cast<size_t>(St.Subject)].IsMan();
					if ((St.Kind == KShoulder || St.Kind == KLow || (St.Kind == KWatch && bOnMan)) && Men == 0) ++Empty;
					Houses = Pic.HousesFound();
					if (St.bBuilding && Houses > 0 && House < 0.04) ++NoHouse;
				}
			}
		}
		std::printf("stills: %zu, %d placed unfit, %d on a man with no man of the battle in the picture, %d village shots with no house (%d houses on the field), %d with a man held within 2 m of the lens; %d of %d asked the same in a run cut differently between; the camera's slowest frame %.2f ms (battle %.1f s); the houses found in %.1f ms as the battle opens\n", Stills.size(), Unfit, Empty, NoHouse, Houses, NearMan, Same, static_cast<int>(Stills.size()), SlowMs, SlowAt, FindMs);
		if (Unfit > 0) Fault("a still's asked shot found no fit place");
		if (Empty > 0) Fault("a still on a man has no man of the battle in its picture");
		if (NoHouse > 0) Fault("the village still has no house in the picture");
		if (NearMan > 0) Fault("a still holds a man within 2 m of its lens, or one passes within 0.9 m");
		if (Same < static_cast<int>(Stills.size())) Fault("an asked still parts with the run's history");
	};
	// The picture run's low shot asked of a man in the corn (as the art director's 04 asks one): it keeps its kind and
	// is held just over the ears, never among the stalks.
	auto RunCorn = [&]()
	{
		// (up to six men well in the corn, each 20 m from the others: since the art director's 2 m for asked stills
		// (14:32Z) a man in the corn among his section's men is no place for the lens, and the ask goes to another man,
		// at Sedan off the corn's edge; the corn's rule is then tried on the next man in it)
		struct InCornAt { int Track; double At; LandFeed::Spot Sp; };
		std::vector<InCornAt> Tries;
		for (size_t K = 0; K < Rec.Tracks.size() && Tries.size() < 6; ++K)
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
				for (const InCornAt& Was : Tries) bDeep = bDeep && std::hypot(Was.Sp.X - Sp.X, Was.Sp.Y - Sp.Y) >= 20.0;
				if (bDeep) { Tries.push_back({ static_cast<int>(K), When, Sp }); break; }
			}
		}
		int Among = 0, Tried = 0, Seen = 0;
		for (const InCornAt& Try : Tries)
		{
			const int InCorn = Try.Track;
			const double AskAt = Try.At;
			++Tried;
			LandEye::Director Asked;
			Asked.Begin(Rec, EW, ES);
			int TakenOn = -1;
			double Least = 1e9;
			Seen = 0;
			for (int F = 0; F < 4 * 60; ++F)
			{
				if (F == 0) Asked.Ask(LandEye::Kind::Low, InCorn);
				PlaceMovers(AskAt + F * Dt);
				const LandEye::Pose P = Asked.Update(AskAt + F * Dt, 1000.0 + F * Dt, Dt);
				if (std::getenv("LAND_PLAY_CORN") && F % 20 == 0) std::printf("  corn ask frame %d: %s on %d, eye corn %.2f, eye %.2f over the ground, man corn %.2f\n", F, LandEye::NameOf(P.Shot), P.Subject, CornAt(P.Eye.X, P.Eye.Y), P.Eye.Z - GroundAt(P.Eye.X, P.Eye.Y), CornAt(LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(InCorn)], AskAt + F * Dt).X, LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(InCorn)], AskAt + F * Dt).Y));
				if (P.Shot != LandEye::Kind::Low) continue;
				if (TakenOn < 0) TakenOn = P.Subject;
				const double Corn = CornAt(P.Eye.X, P.Eye.Y);
				if (Corn <= 0.0) continue;
				++Seen;
				const double Over = P.Eye.Z - GroundAt(P.Eye.X, P.Eye.Y) - Corn;
				Least = std::min(Least, Over);
				if (Over < 0.05) ++Among;
			}
			std::printf("corn: a low shot asked of track %d in the corn at %.0f s (taken on %d, the man tried %d of %zu): %d frames over corn, at least %.2f m over the ears\n", InCorn, AskAt, TakenOn, Tried, Tries.size(), Seen, Seen ? Least : 0.0);
			if (Seen > 0) break;
		}
		if (Among > 0) Fault("an asked low shot is down among the corn");
	};
	if (std::getenv("LAND_PLAY_ONLY_CORN"))
	{
		RunCorn();
		return 0;
	}
	if (std::getenv("LAND_PLAY_ONLY_STILLS"))
	{
		RunStills();
		return 0;
	}

	// The tanks' wireless aerials near the lens (Brian's picture of 25 November, 16:35Z: the film riding behind a
	// Sherman's turret, her own aerial a thin dark line through the whole picture). The recording says nothing of an
	// aerial, so every tank is given the Sherman's own as her model has it: on her turret, 1.12 m behind its ring's
	// middle and 0.48 m left of it (the ring 0.15 m ahead of her own middle), 2.5 m up, 2.27 m long, turning with the
	// turret about its ring; upright, her hull's pitch and roll left out. Each frame of the film, of the aerials
	// within AerialWhole of the lens: how much of the picture's height the one standing through most of it takes,
	// drawn whole (as it was) and drawn as LandEye::AerialShown has the scene draw it; and what the rule did to them
	// (the ridden tank's own put away; another's gone, within AerialAway; another's drawn short, between the two).
	struct WireCount
	{
		long Frames = 0, Near = 0, RideAway = 0, Gone = 0, Short = 0, HalfWhole = 0, HalfShown = 0, RideWhole = 0;
		double MostWhole = 0.0, MostShown = 0.0;
	};
	WireCount Wires;
	const double WireRing = 0.15, WireBack = 1.12, WireLeft = 0.48, WireUp = 2.5, WireLong = 2.27;
	// (the share of the picture's height a rod from A to B stands through, the part of it inside the picture's width;
	// the picture 16 wide for 9 high, the lens's roll left out)
	auto ThroughPicture = [](const LandEye::Pose& P, const Vec& A, const Vec& B)
	{
		const Vec Way = P.Look - P.Eye;
		if (Way.Length() < 1e-6) return 0.0;
		const Vec Fwd = Way.Unit();
		Vec Right = Vec(Fwd.Y, -Fwd.X, 0.0);
		Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
		const Vec Up(Right.Y * Fwd.Z - Right.Z * Fwd.Y, Right.Z * Fwd.X - Right.X * Fwd.Z, Right.X * Fwd.Y - Right.Y * Fwd.X);
		const double TanV = std::tan(P.Fov * 0.5 * 3.14159265358979323846 / 180.0), TanH = TanV * 16.0 / 9.0;
		double Lo = 1e9, Hi = -1e9;
		for (int K = 0; K <= 48; ++K)
		{
			const Vec To = A + (B - A) * (K / 48.0) - P.Eye;
			const double Depth = To.Dot(Fwd);
			if (Depth < 0.2 || std::abs(To.Dot(Right)) / Depth > TanH) continue;
			const double V = std::clamp(To.Dot(Up) / Depth / TanV, -1.0, 1.0);
			Lo = std::min(Lo, V);
			Hi = std::max(Hi, V);
		}
		return Hi > Lo ? 0.5 * (Hi - Lo) : 0.0;
	};
	// (LAND_PLAY_ONLY_GEN: the general's view alone, for working on it; the film's counts after it are of nothing)
	const bool bOnlyGen = std::getenv("LAND_PLAY_ONLY_GEN") != nullptr;
	while (!bOnlyGen && Battle < Rec.End + 6.0)
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
		PlateNowT = Battle;
		Play.Step(From, Battle, Fx, Noises, Losses);
		PlaceMovers(Battle);
		// (the scene's call at a tank's kill, before the camera's frame: five crews in six get out, the first man up 0.6 to
		// 2.4 life seconds after the hit, every second one brewing up 5.5 to 10 s after it; battle seconds to the camera)
		if (bBailTest)
		{
			for (size_t I = 0; I < Rec.Tracks.size(); ++I)
			{
				const LandFeed::Track& Tr = Rec.Tracks[I];
				if (Tr.Role != "tank" || Tr.bTwin || !(Tr.Fell > From && Tr.Fell <= Battle)) continue;
				const uint64_t H = (static_cast<uint64_t>(I) + 1) * 0x9E3779B97F4A7C15ull ^ Rec.Seed;
				const double U1 = static_cast<double>((H >> 11) % 1000) / 1000.0, U2 = static_cast<double>((H >> 23) % 1000) / 1000.0;
				const bool bOut = (H >> 37) % 6 != 0, bBrew = (H >> 41) % 2 == 0;
				if (bOut)
				{
					++BailCrews;
					if (Pose.Subject == static_cast<int>(I) && Eye.BailWatchNow() && Eye.BailBeats().empty()) ++BailWatched;
				}
				if (!Eye.BailBeats().empty() && Eye.BailBeats().back().Left < 0.0) ++BailEndsPassed;
				if (std::getenv("LAND_PLAY_BAILS")) std::printf("  bail-out: track %zu at %.1f, %s, the film %s on %d%s\n", I, Tr.Fell, bOut ? "her crew gets out" : "nobody gets out",
					LandEye::NameOf(Pose.Shot), Pose.Subject, Eye.BailWatchNow() ? " (a bail watch)" : "");
				Eye.BailOut(static_cast<int>(I), bOut ? (0.6 + 1.8 * U1) / Life : -1.0, bBrew ? (5.5 + 4.5 * U2) / Life : -1.0);
			}
		}
		const auto E0 = std::chrono::steady_clock::now();
		const long Steps0 = Eye.Steps();
		Pose = Eye.Update(Battle, Real, Dt);
		if (bBailTest && !bBailLensSeen && !Eye.BailBeats().empty() && Eye.BailBeats().front().Hit >= 0.0)
		{
			// (the lens as she is hit: how far off her and how high, and whether she is in the picture)
			bBailLensSeen = true;
			const LandFeed::Track& Tb = Rec.Tracks[static_cast<size_t>(Eye.BailBeats().front().Track)];
			const LandFeed::Spot S = LandFeed::SpotAt(Tb, Battle);
			const LandEye::Vec At(S.X, S.Y, GroundAt(S.X, S.Y) + 1.3);
			BailLensOff = std::hypot(Pose.Eye.X - S.X, Pose.Eye.Y - S.Y);
			BailLensUp = Pose.Eye.Z - GroundAt(Pose.Eye.X, Pose.Eye.Y);
			bBailInPicture = LandEye::InPicture(Pose, At);
		}
		const double EyeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - E0).count();
		const long FrameSteps = Eye.Steps() - Steps0;
		{
			// (the tanks' wireless aerials near the lens)
			const int Ridden = LandEye::RiddenBy(Pose);
			double Whole = 0.0, Shown = 0.0;
			bool bNear = false, bRideAway = false, bGone = false, bShort = false, bRideWhole = false;
			for (size_t K = 0; K < Rec.Tracks.size(); ++K)
			{
				const LandFeed::Track& Tr = Rec.Tracks[K];
				if (Tr.Role != "tank" || Tr.bTwin || Tr.Start() > Battle || (Tr.Out >= 0.0 && Battle > Tr.Out + 1.0)) continue;
				const LandFeed::Spot S = LandFeed::SpotAt(Tr, Battle);
				if (std::hypot(S.X - Pose.Eye.X, S.Y - Pose.Eye.Y) > LandEye::AerialWhole + 2.0) continue;
				double Turret = S.F;
				if (const LandFeed::Change* C = LandFeed::ChangeAt(Tr, Battle); C && C->bAim) Turret = LandFeed::FacingTo(S.X, S.Y, C->AimX, C->AimY);
				// (a facing F looks along (sin F, cos F), its left hand (-cos F, sin F): LandFeed::FacingTo)
				const double Sn = std::sin(Turret), Cs = std::cos(Turret);
				const Vec Foot(S.X + WireRing * std::sin(S.F) - WireBack * Sn - WireLeft * Cs, S.Y + WireRing * std::cos(S.F) - WireBack * Cs + WireLeft * Sn, GroundAt(S.X, S.Y) + WireUp);
				const double Beside = std::clamp(Pose.Eye.Z - Foot.Z, 0.0, WireLong);
				if ((Pose.Eye - Vec(Foot.X, Foot.Y, Foot.Z + Beside)).Length() >= LandEye::AerialWhole) continue;
				bNear = true;
				const bool bHers = static_cast<int>(K) == Ridden;
				const double Part = LandEye::AerialShown(Pose.Eye, Foot, WireLong, bHers);
				bRideAway = bRideAway || bHers;
				bGone = bGone || (!bHers && Part <= 0.0);
				bShort = bShort || (!bHers && Part > 0.0 && Part < 1.0);
				const double Through = ThroughPicture(Pose, Foot, Foot + Vec(0.0, 0.0, WireLong));
				Whole = std::max(Whole, Through);
				bRideWhole = bRideWhole || (bHers && Through > 0.5);
				if (Part > 0.0) Shown = std::max(Shown, ThroughPicture(Pose, Foot, Foot + Vec(0.0, 0.0, WireLong * Part)));
			}
			++Wires.Frames;
			Wires.Near += bNear ? 1 : 0;
			Wires.RideAway += bRideAway ? 1 : 0;
			Wires.Gone += bGone ? 1 : 0;
			Wires.Short += bShort ? 1 : 0;
			Wires.HalfWhole += Whole > 0.5 ? 1 : 0;
			Wires.HalfShown += Shown > 0.5 ? 1 : 0;
			Wires.RideWhole += bRideWhole ? 1 : 0;
			Wires.MostWhole = std::max(Wires.MostWhole, Whole);
			Wires.MostShown = std::max(Wires.MostShown, Shown);
		}
		StepsByFrame.push_back(FrameSteps);
		if (Pose.bCut && Eye.MassShot()) { MassSteps = FrameSteps; MassMs = EyeMs; }
		if (Pose.bCut && std::getenv("LAND_PLAY_CUTS")) std::printf("  cut at %.2f (real %.2f) to %s on %d%s%s, the last left for %s\n", Battle, Real, LandEye::NameOf(Pose.Shot), Pose.Subject,
			Pose.Sp != LandEye::Special::None ? ", " : "", Pose.Sp != LandEye::Special::None ? LandEye::NameOf(Pose.Sp) : "", LandEye::NameOf(Eye.LastWhy()));
		if (bWhistleTest || bNickTest || bStandTest)
		{
			const std::vector<LandEye::Director::SpecialBeat>& Sb = Eye.SpecialBeats();
			SpSeen.resize(Sb.size());
			for (size_t K = 0; K < Sb.size(); ++K)
			{
				const LandEye::Director::SpecialBeat& B = Sb[K];
				SpecialSeen& M = SpSeen[K];
				if (B.CutReal < 0.0) continue;
				const bool bInHold = Real >= B.CutReal && Real < B.CutReal + B.Hold && (B.Left < 0.0 || Real < B.Left);
				if (!bInHold) continue;
				++M.HoldFrames;
				if (B.Sp == LandEye::Special::Nick && Pose.Card == B.Block) ++M.CardFrames;
				const bool bMoment = B.Sp == LandEye::Special::Stand ? Real >= B.CutReal + 0.5 * B.Hold : Battle >= B.At;
				if (M.bLooked || !bMoment) continue;
				M.bLooked = true;
				const LandFeed::Track& Tw = Rec.Tracks[static_cast<size_t>(B.Who)];
				const LandFeed::Spot S = LandFeed::SpotAt(Tw, Battle);
				M.Off = std::hypot(Pose.Eye.X - S.X, Pose.Eye.Y - S.Y);
				M.Up = Pose.Eye.Z - GroundAt(Pose.Eye.X, Pose.Eye.Y);
				M.bIn = LandEye::InPicture(Pose, LandEye::Vec(S.X, S.Y, GroundAt(S.X, S.Y) + (Tw.IsVehicle() ? 1.3 : 0.9)));
				for (const int Mn : B.Men)
				{
					const LandFeed::Spot Sm = LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(Mn)], Battle);
					if (LandEye::InPicture(Pose, LandEye::Vec(Sm.X, Sm.Y, GroundAt(Sm.X, Sm.Y) + 0.8))) ++M.MenIn;
				}
			}
		}
		EyeSlowest = std::max(EyeSlowest, EyeMs);
		if (EyeMs > 2.0) ++EyeOver2;
		{
			const std::vector<LandEye::Director::RadioBeat>& Beats = Eye.RadioBeats();
			for (; RadioLooked < Beats.size() && Battle >= Beats[RadioLooked].Land; ++RadioLooked)
			{
				const LandEye::Director::RadioBeat& Bt = Beats[RadioLooked];
				if (!Bt.bBarrage) continue;
				++RadioBarrages;
				const LandEye::Vec At(Bt.X, Bt.Y, GroundAt(Bt.X, Bt.Y) + 1.0);
				// (in the picture, and its 15 m across not a speck: a twenty-fifth of the picture's height at the least)
				const double Off = (At - Pose.Eye).Length();
				const double Share = 15.0 / (2.0 * std::max(1.0, Off) * std::tan(0.5 * Pose.Fov * 3.14159265358979 / 180.0));
				if (LandEye::InPicture(Pose, At, 0.95) && Share >= 0.04) ++RadioSeen;
				else if (std::getenv("LAND_PLAY_RADIO")) std::printf("  radio: the salvo at %.1f not in the picture: %s on %d, %.0f m off, lens %.0f, %.1f%% of its height\n", Bt.Land, LandEye::NameOf(Pose.Shot), Pose.Subject, Off, Pose.Fov, 100.0 * Share);
				if (std::getenv("LAND_PLAY_RADIO"))
				{
					const LandEye::Vec Way = (Pose.Look - Pose.Eye), To = (At - Pose.Eye);
					const double Apart = std::acos(std::clamp((Way.X * To.X + Way.Y * To.Y + Way.Z * To.Z) / std::max(1e-9, Way.Length() * To.Length()), -1.0, 1.0)) * 180.0 / 3.14159265358979;
					std::printf("  radio: the salvo at %.1f: %s on %d, %.0f m off, lens %.0f, %.1f%% of its height, %.1f deg off the look\n", Bt.Land, LandEye::NameOf(Pose.Shot), Pose.Subject, Off, Pose.Fov, 100.0 * Share, Apart);
				}
			}
			// (the cuts to a caller: real seconds between them)
			if (Pose.bCut && !Beats.empty() && Pose.Subject == Beats.back().Caller && Beats.size() > RadioLooked && Real != RadioCutAt)
			{
				if (RadioCutAt >= 0.0) RadioGapLeast = std::min(RadioGapLeast, Real - RadioCutAt);
				RadioCutAt = Real;
			}
		}
		// The shallow focus (E9): on close follow shots only, switched on or off only on a cut, pulled after its man.
		{
			const bool bFocus = Pose.Focus > 0.0;
			if (Battle >= Contact)
			{
				++FightFrames;
				if (bFocus) ++FocusFrames;
			}
			if (Pose.bCut)
			{
				++FocusAllCuts;
				if (bFocus) ++FocusCuts;
				FocusShotAt = Real;
			}
			else if (bFocus != bFocusWas) ++FocusMidShot;
			if (bFocus && (Pose.Shot == LandEye::Kind::Wide || Pose.Shot == LandEye::Kind::Open || Pose.Shot == LandEye::Kind::Tank)) ++FocusWrongKind;
			if (bFocus && bFocusWas && !Pose.bCut) FocusPullMost = std::max(FocusPullMost, std::abs(std::log(Pose.Focus / FocusWas)) / Dt);
			// (on a watch or a tank tracked, a second into the shot: the man's or the tank's distance, his middle 1 m up)
			if (bFocus && !Pose.bCut && Real - FocusShotAt >= 1.0 && Pose.Subject >= 0 && (Pose.Shot == LandEye::Kind::Watch || Pose.Shot == LandEye::Kind::Track))
			{
				const LandFeed::Spot S = LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(Pose.Subject)], Battle);
				const double D = (LandEye::Vec(S.X, S.Y, GroundAt(S.X, S.Y) + 1.0) - Pose.Eye).Length();
				if (D > 1.0) FocusOff.push_back(std::abs(Pose.Focus - D) / D);
			}
			bFocusWas = bFocus;
			FocusWas = Pose.Focus;
		}
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
			if (Screened > ES.ScreenMost && std::getenv("LAND_PLAY_SCREEN")) std::printf("  screened %.0f%% at battle %.2f: %s on %d, %.1f s into it, lens %.0f, eye %.1f over the ground\n", 100.0 * Screened, Battle, LandEye::NameOf(Pose.Shot), Pose.Subject, Real - LastCutReal, Pose.Fov, Pose.Eye.Z - GroundAt(Pose.Eye.X, Pose.Eye.Y));
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
			if (Pose.Shot == LandEye::Kind::Wide && Battle > 12.0 && Battle < Rec.End && !Eye.MassShot())
			{
				++MiddleAerials;
				if (std::getenv("LAND_PLAY_SCREEN")) std::printf("  aerial in the middle at battle %.2f, why %s\n", Battle, LandEye::NameOf(Eye.LastWhy()));
			}
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
		for (size_t K = 0; K < Rec.Cuts.size(); ++K)
		{
			const LandFeed::Cut& Cut = Rec.Cuts[K];
			if (CutTorn[K] || Battle < Cut.T1) continue;
			CutTorn[K] = 1;
			LandField::BankPlace BP;
			if (!Field.NearestBank(Cut.X, Cut.Y, 6.0, BP) || std::hypot(BP.X - Cut.X, BP.Y - Cut.Y) > 2.5) { ++CutsNoBank; continue; }
			const double Left = -std::sin(BP.Yaw) * std::cos(Cut.Yaw) + std::cos(BP.Yaw) * std::sin(Cut.Yaw);
			LandCrater::Breach B;
			B.X = BP.X;
			B.Y = BP.Y;
			B.Yaw = BP.Yaw;
			B.Half = 0.5 * Cut.Width;
			B.Foot = BP.Foot;
			B.Through = Left >= 0.0 ? 1 : -1;
			const auto B0 = std::chrono::steady_clock::now();
			const bool bTorn = Holes.AddBreach(B, [&Field](double X, double Y) { return Field.BankRise(X, Y); }, "earth");
			BreachMsMost = std::max(BreachMsMost, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - B0).count());
			if (!bTorn) { Fault("a hedge cutter's bank not torn", Battle); continue; }
			++CutsTornCount;
			++GapsTorn;
			// (her own track from the moment it gives way: over the gap's floor at some step, as she drives through it)
			bool bOver = Cut.Unit < 0;
			if (Cut.Unit >= 0 && static_cast<size_t>(Cut.Unit) < Rec.Tracks.size())
			{
				for (double T = Cut.T1; T <= Cut.T1 + 6.0 && !bOver; T += 0.05)
				{
					const LandFeed::Spot Sp = LandFeed::SpotAt(Rec.Tracks[static_cast<size_t>(Cut.Unit)], T);
					double Z = 0.0;
					bOver = Holes.BreachFloor(Sp.X, Sp.Y, Z);
				}
			}
			if (bOver) ++CutsDrivenThrough;
			else Fault("a hedge cutter's tank never drove over the gap she tore", Battle);
			if (std::getenv("LAND_PLAY_BREACHES")) std::printf("  cut %zu torn at battle %.1f (T1 %.1f), %.0f, %.0f, through %+d, track %d over the floor %d\n", K, Battle, Cut.T1, BP.X, BP.Y, B.Through, Cut.Unit, bOver ? 1 : 0);
		}
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
			static const char* Names[] = { "muzzle", "shots", "shell", "size", "skip", "glance", "plate", "screen" };
			static_assert(sizeof(Names) / sizeof(Names[0]) == static_cast<size_t>(LandPlay::FxKind::Screen) + 1, "a name for every effect kind");
			++FxCount[Names[static_cast<int>(E.Kind)]];
			if (std::isnan(E.At.X) || std::isnan(E.At.Y) || std::isnan(E.At.Z)) Fault("an effect at no number", Battle);
			if (PW.bRicochet && E.Kind == LandPlay::FxKind::Shots && RoundIx.count(E.Round))
			{
				// A rifle's or a machine gun's burst that struck armour (rules: not a miss) is drawn off her plate, and
				// only such a burst: a lorry, a jeep or a man as ever.
				const LandFeed::Round& Rr = Rec.Rounds[RoundIx[E.Round]];
				const int Hit = Rr.On >= 0 && Rr.Ok >= 0 ? Rec.TrackIndexOf(Rr.On, Rr.Ok, Rr.Oj) : -1;
				const LandFeed::Track* HitTr = Hit >= 0 ? &Rec.Tracks[static_cast<size_t>(Hit)] : nullptr;
				const bool bWant = (Rr.W == "rifle" || Rr.W == "mg") && Rr.R != LandFeed::Result::Miss && HitTr && HitTr->IsVehicle() && LandPlay::ArmouredVehicle(*HitTr, Rec.BlockOf(HitTr->Block));
				if (bWant != E.bArmour) ++RicochetWrong;
				if (E.bArmour && HitTr)
				{
					++RicochetBy[HitTr->Role];
					RicochetNormalWorst = std::max(RicochetNormalWorst, std::abs(E.Normal.Length() - 1.0));
					bool bOffHer = std::isnan(E.To.X) || std::isnan(E.To.Y) || std::isnan(E.To.Z);
					// (the aim inside the room it is given, on the plate's plane)
					auto Along = [](const LandPlay::Vec& V, const LandPlay::Vec& Way) { const double L = Way.Length(); return L > 1e-9 ? V.Dot(Way) / L : 0.0; };
					const LandPlay::Vec Rel = E.To - E.RoomMid;
					if (std::abs(Along(Rel, E.RoomA)) > E.RoomA.Length() + 1e-6 || std::abs(Along(Rel, E.RoomB)) > E.RoomB.Length() + 1e-6 || std::abs(Rel.Dot(E.Normal)) > 1e-6) ++RicochetOutOfRoom;
					// (each strike at its moment: Battle is the kit's Now)
					const std::string Face = Rr.Fc.empty() ? std::string("front") : Rr.Fc;
					const LandFeed::Block* HitBlock = Rec.BlockOf(HitTr->Block);
					const LandFeed::Spot SpL = LandFeed::SpotAt(*HitTr, Rr.L);
					const LandPlay::PlateSpot Still = LandPlay::PlainPlate(*HitTr, HitBlock, SpL, Rec.Ground.HeightAt(SpL.X, SpL.Y), Face, Rr.U, Rr.H);
					const double Fly = (E.To - E.At).Length() / 760.0;
					const bool bCast = Rr.N % 2 != 0;
					if (E.Path.size() < 2) ++RicochetNoPath;
					for (int K = 0; K < E.Rounds; ++K)
					{
						// (the kit's flight and rounds on life's clock: battle seconds at World::Life a battle second)
						const double Ts = Battle + (Fly + K / std::max(E.PerSecond, 0.1)) / PW.Life;
						const LandFeed::Spot SpT = LandFeed::SpotAt(*HitTr, Ts);
						const double GroundT = Rec.Ground.HeightAt(SpT.X, SpT.Y);
						const LandPlay::PlateSpot True = bCast ? StandInPlate(*HitTr, HitBlock, SpT, GroundT, Face, Rr.U, Rr.H) : LandPlay::PlainPlate(*HitTr, HitBlock, SpT, GroundT, Face, Rr.U, Rr.H);
						// (as the kit carries it: along her path through the burst, turned about her middle as she turns)
						const double Since = Ts - Battle;
						if (E.Path.size() >= 2 && Since > E.Path.back().Dt + 1e-9) ++RicochetPastPath; // (struck after her path ends: held there)
						const LandPlay::Vec Laid = LandPlay::PathMoved(E, E.To, Since);
						// (each strike on her at its moment: within 4 m of her middle then, and up her side, not over her)
						const double Off = std::hypot(Laid.X - SpT.X, Laid.Y - SpT.Y), Up = Laid.Z - Rec.Ground.HeightAt(SpT.X, SpT.Y);
						RicochetPlateFarthest = std::max(RicochetPlateFarthest, Off);
						if (Off > 4.0 || Up < -0.2 || Up > 3.2 || std::isnan(Up)) bOffHer = true;
						(bCast ? RicochetCastOff : RicochetStrikeOff).push_back((Laid - True.At).Length());
						if (std::getenv("LAND_PLAY_RICOCHET_SHOW") && (Laid - True.At).Length() > 0.1) std::printf("  strike off %.3f m: round %d face %s u %.2f h %.2f battle %.3f T %.3f L %.3f fly %.3f k %d ts %.3f; heading %.1f at L, %.1f at ts; moved %.2f m; carry %.2f m/s\n", (Laid - True.At).Length(), Rr.N, Face.c_str(), Rr.U, Rr.H, Battle, Rr.T, Rr.L, Fly, K, Ts, SpL.F * 57.2958, SpT.F * 57.2958, std::hypot(SpT.X - SpL.X, SpT.Y - SpL.Y), E.Carry.Length());
						RicochetStillOff.push_back((Still.At - True.At).Length());
						const double Inside = -(Laid - True.At).Dot(True.Normal);
						double& InsideMost = bCast ? RicochetCastInside : RicochetInside;
						InsideMost = std::max(InsideMost, Inside);
						if (Inside > 0.05) ++RicochetInsideMany;
						RicochetStillInside = std::max(RicochetStillInside, -(Still.At - True.At).Dot(True.Normal));
						const double Moved = std::hypot(SpT.X - SpL.X, SpT.Y - SpL.Y);
						RicochetMovingMost = std::max(RicochetMovingMost, Moved);
						if (Moved > 0.03) ++RicochetStrikesMoving;
					}
					if (bOffHer) ++RicochetOffPlate;
				}
			}
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
		{
			// (the pool: players stopped by now free their voices; the takes due now start; the cuts due now fade)
			for (const LandSound::Take& T : Takes) PoolDue.emplace_back(Real + std::max(0.0, T.Delay), T);
			for (int K = 0; K < Pool.Count(); ++K)
			{
				// (the game hears a player has stopped a few frames late, from the audio thread: 50 ms here)
				if (Pool.At(K).bBusy && Players[static_cast<size_t>(K)].Stops + 0.05 <= Real + 1e-9) Pool.Free(K);
			}
			size_t Kept = 0;
			for (size_t K = 0; K < PoolDue.size(); ++K)
			{
				if (PoolDue[K].first > Real + 1e-6)
				{
					if (Kept != K) PoolDue[Kept] = PoolDue[K];
					++Kept;
					continue;
				}
				const LandSound::Take& T = PoolDue[K].second;
				const double Long = Mix.Length ? Mix.Length(T.Name) : 1.5;
				if (Long <= 0.0) { ++PoolSilent; continue; } // (no recording: the game starts nothing)
				const double Fade = T.Cut > 0.0 ? std::max(0.02, T.CutFade) : 0.0;
				const int StolenWas = Pool.Stolen;
				bool bMade = false;
				const int Slot = Pool.Take(Real, Long - T.From, T.Pitch, T.Cut, Fade, bMade);
				if (bMade) Players.emplace_back();
				FPlayer& P = Players[static_cast<size_t>(Slot)];
				// (a voice handed out while its take still sounds, the pool not full: a take cut off)
				if (P.Stops > Real + 1e-9 && Pool.Stolen == StolenWas) ++PoolCutOff;
				// (the take it held before is no longer held: a moving take or the blast's dulling cannot reach the new one)
				if (Pool.UseOf(Slot) > 1 && Pool.Holds(Slot, Pool.UseOf(Slot) - 1)) ++PoolStaleHeld;
				P.Stops = Real + (T.Cut > 0.0 ? T.Cut + Fade : std::max(0.0, Long - T.From) / std::max(0.01, T.Pitch));
				P.CutAt = T.Cut > 0.0 ? Real + T.Cut : -1.0;
				if (T.Cut > 0.0) ++PoolCuts;
			}
			PoolDue.resize(Kept);
			PoolFades.clear();
			Pool.Due(Real, PoolFades);
			for (const int Slot : PoolFades)
			{
				FPlayer& P = Players[static_cast<size_t>(Slot)];
				PoolFadeLate = std::max(PoolFadeLate, Real - P.CutAt);
				PoolFadeEarly = std::max(PoolFadeEarly, P.CutAt - Real);
				P.Stops = Real + Pool.FadeOf(Slot);
				P.CutAt = -1.0;
			}
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
			std::snprintf(Line, sizeof(Line), "%6.1f %-8s subject %d (the shot before left: %s)", Battle, LandEye::NameOf(Pose.Shot), Pose.Subject, LandEye::NameOf(Eye.LastWhy()));
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
	if (PW.bRicochet)
	{
		int OffPlates = 0;
		for (auto& P : RicochetBy) OffPlates += P.second;
		std::printf("ricochets: %d small-arms bursts struck armour and were drawn off her plate (", OffPlates);
		for (auto& P : RicochetBy) std::printf("%s%s %d", P.first == RicochetBy.begin()->first ? "" : ", ", P.first.c_str(), P.second);
		std::printf("); outward ways unit within %.1e, the farthest plate %.2f m from her middle; %d off her, %d drawn the wrong way, %d aimed outside the room given\n", RicochetNormalWorst, RicochetPlateFarthest, RicochetOffPlate, RicochetWrong, RicochetOutOfRoom);
		auto Pct = [](std::vector<double> L, double P) { if (L.empty()) return 0.0; std::sort(L.begin(), L.end()); return L[std::min(L.size() - 1, static_cast<size_t>(P * static_cast<double>(L.size() - 1) + 0.5))]; };
		const size_t Near3 = static_cast<size_t>(std::count_if(RicochetStrikeOff.begin(), RicochetStrikeOff.end(), [](double D) { return D <= 0.03; }));
		const size_t CastNear3 = static_cast<size_t>(std::count_if(RicochetCastOff.begin(), RicochetCastOff.end(), [](double D) { return D <= 0.03; }));
		const size_t StillNear3 = static_cast<size_t>(std::count_if(RicochetStillOff.begin(), RicochetStillOff.end(), [](double D) { return D <= 0.03; }));
		std::printf("ricochets followed: %zu strikes (%d of them on a plate that had moved on, as far as %.2f m), each where the kit lays it against her box's plate at its moment: %zu within 3 cm, median %.3f m, 99th %.3f m, the most %.3f m, at most %.3f m inside her; left standing at her landing, as before: %zu within 3 cm, 99th %.3f m, the most %.3f m, %.3f m inside her\n",
			RicochetStrikeOff.size(), RicochetStrikesMoving, RicochetMovingMost, Near3, Pct(RicochetStrikeOff, 0.5), Pct(RicochetStrikeOff, 0.99), Pct(RicochetStrikeOff, 1.0), RicochetInside,
			StillNear3, Pct(RicochetStillOff, 0.99), Pct(RicochetStillOff, 1.0), RicochetStillInside);
		std::printf("ricochets followed, the plate from the scene's cast (its stand-in, the odd rounds): %zu strikes: %zu within 3 cm, 99th %.3f m, the most %.3f m, at most %.3f m inside her; %d strikes more than 5 cm inside her in all; %d bursts with no path\n",
			RicochetCastOff.size(), CastNear3, Pct(RicochetCastOff, 0.99), Pct(RicochetCastOff, 1.0), RicochetCastInside, RicochetInsideMany, RicochetNoPath);
		std::printf("ricochets on life's clock (%.2f life seconds a battle second): %d strikes after her path through the burst ends\n", PW.Life, RicochetPastPath);
		if (RicochetPastPath) Fault("a strike off armour came after the path she was handed through its burst ended (the path too short for life's clock)");
		if (RicochetWrong) Fault("a small-arms burst at armour was not drawn off her plate, or one at a soft thing or a man was");
		if (RicochetOffPlate) Fault("a burst off armour was drawn off her plate (more than 4 m from her middle or above her)");
		if (RicochetNormalWorst > 1e-6) Fault("a burst off armour has no unit outward way");
		if (RicochetOutOfRoom) Fault("a burst off armour was aimed outside the room on the plate it was given");
		// (her path through the burst, every twentieth of a second or so, follows her as she slows, stops, speeds up and
		// turns: what is left is a turn or a change of speed between two of its moments. Left at her landing, as before it
		// followed her, the two recordings' 99th was 1.22 and 1.73 m; one steady way and turn through the burst left 4 and
		// 42 strikes more than 5 cm inside her, the deepest 0.16 and 0.40 m)
		for (int Way = 0; Way < 2; ++Way)
		{
			const std::vector<double>& Offs = Way == 0 ? RicochetStrikeOff : RicochetCastOff;
			const size_t Near = Way == 0 ? Near3 : CastNear3;
			if (!Offs.empty() && (Near < 0.95 * static_cast<double>(Offs.size()) || Pct(Offs, 0.99) > 0.05 || Pct(Offs, 1.0) > 0.25))
				Fault(Way == 0 ? "strikes off armour did not follow her (fewer than 95 in 100 within 3 cm of her plate at their moment, the 99th over 5 cm, or one over 0.25 m)"
					: "strikes off armour on the scene's plate did not follow her (fewer than 95 in 100 within 3 cm of her plate at their moment, the 99th over 5 cm, or one over 0.25 m)");
		}
		if (RicochetInsideMany) Fault("a strike off armour was drawn more than 5 cm inside her at its moment");
		if (RicochetNoPath) Fault("a burst off armour was handed over with no path through it");
		if (RicochetCastOff.empty() || RicochetStrikeOff.empty()) Fault("strikes off armour were not run both through the scene's plate and through the replay's own");
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
	std::printf("heard in a copy: %d shots (%.2f in a thousand), %d takes of any kind (a shot, a crack, a blast) nearer than their family's far takes; %d machine-gun bursts crowded out of one recording took another on their list, %d another stretch of firing (moreParts), %d a shorter stretch cut at its end (a round or two fewer heard)\n",
		Mix.ShotsMerged, 1000.0 * Mix.ShotsMerged / std::max(1, Mix.ShotsHeard), Mix.SameMergedNear, Mix.MgToOther, Mix.MgToOtherPart, Mix.MgShortPart);
	for (const auto& Kv : Mix.MergedNearBy) if (Kv.second > 0) std::printf("  heard in a copy though nearer than its far takes: %s %d\n", Kv.first.c_str(), Kv.second);
	if (Mix.SameMergedNear > 0) Fault("a take nearer than its family's far takes (a shot, a crack, a blast) was heard only in a copy already sounding (a sound heard on its own, hidden)");
	if (Mix.ShotsMerged * 100 > Mix.ShotsHeard) Fault("more than one shot in a hundred was heard only in a copy already sounding");
	std::printf("land: %d bursts cut to their rounds, %d runs of cracks, %d rounds struck the ground near the ear (%d whined off), %d Garand clips pinged (%d reloads), %d shouts (%d voice lines in all), %d shells heard coming in; the longest a take was held for the 0.15 s rule %.2f s, %d started on time at another pitch instead\n",
		Mix.BurstsCut, Mix.CrackRuns, Mix.GroundStrikes, Mix.Whines, Mix.Clips, Mix.Reloads, Mix.Shouts, VoiceLines, Mix.Incoming, Mix.MostDelay, Mix.Repitched);
	if (Mix.MostDelay > 0.1) Fault("a take was held more than 0.1 s by the same-take rule (heard after its flash)");
	{
		// The takes' players pooled (land.voices): every take played on a voice made once, none cut off by another while
		// it sounds, each cut faded on the frame it falls due, no take reached through a voice it has left.
		int CutsLeft = 0;
		for (int K = 0; K < Pool.Count(); ++K) if (Pool.At(K).bBusy && Pool.At(K).FadeAt >= 0.0) ++CutsLeft;
		std::printf("voices (land.voices, %s): %d takes on %d players (%d made before the battle, %d during it, at most %d), the most sounding at once %d; %d gave way to a new take, %d cut off while sounding, %d stale handles held; %d cuts, %d faded (%d still due at the end), the latest %.1f ms after due, the earliest %.1f ms before; %d takes with no recording\n",
			Mix.bVoicesPooled ? "pooled" : "spawned each in the game", Pool.Taken, Pool.Count(), Pool.Warm(), Pool.MadeLate, Mix.VoicesMost, Pool.MostBusy, Pool.Stolen, PoolCutOff, PoolStaleHeld,
			PoolCuts, Pool.Faded, CutsLeft, 1000.0 * PoolFadeLate, 1000.0 * PoolFadeEarly, PoolSilent);
		if (Pool.Stolen > 0) Fault("every pooled voice was sounding and a take cut another off (land.voices.most too few)");
		if (PoolCutOff > 0) Fault("the voice pool handed out a voice whose take still sounded");
		if (PoolStaleHeld > 0) Fault("a take's handle still held its voice after the voice took another take");
		if (Pool.Faded + CutsLeft + Pool.StolenCuts != PoolCuts) Fault("a cut take was not faded out exactly once");
		if (PoolFadeLate > Dt + 1e-6 || PoolFadeEarly > 1e-6) Fault("a cut take faded more than a frame after its cut, or before it");
	}
	{
		// Each vehicle's engine loop's name worked out only when her cue or model changes (LandSound::EngineNames), the
		// same name the mix gives (Mixer::EngineTake): 40 vehicles over 3,000 frames, idling and driving by turns, one
		// burning out (no cue).
		LandSound::EngineNames Names;
		const char* Models[] = { "Panzer IV", "Tiger", "M4 Sherman", "T-34" };
		const char* Loops[] = { "tank_idle_loop", "tank_loop", "tank_sherman_drive_loop", "tank_idle_sherman_loop" };
		int Changes = 0, Wrong = 0;
		std::vector<std::string> Last(40, "?");
		for (int Frame = 0; Frame < 3000; ++Frame)
		{
			for (int V = 0; V < 40; ++V)
			{
				const std::string Model = Models[V % 4];
				const std::string Cue = V == 7 && Frame > 1500 ? std::string() : std::string(Loops[(V + Frame / (60 + V)) % 4]);
				if (Cue != Last[static_cast<size_t>(V)]) { ++Changes; Last[static_cast<size_t>(V)] = Cue; }
				const int Name = Names.Of(Mix, V, Model, Cue);
				const std::string Want = Cue.empty() ? std::string() : Mix.EngineTake(Model, Cue);
				const std::string Got = Name < 0 ? std::string() : Names.Names()[static_cast<size_t>(Name)];
				if (Got != Want) ++Wrong;
			}
		}
		std::printf("engine names: %d asked over 3,000 frames of 40 vehicles, %d worked out (%d changes of cue), %d names in all, %d wrong\n", Names.Asked, Names.Built, Changes, static_cast<int>(Names.Names().size()), Wrong);
		if (Wrong > 0) Fault("a vehicle's cached engine name differs from the mix's (Mixer::EngineTake)");
		if (Names.Built > Changes) Fault("a vehicle's engine name was worked out again with no change of cue or model");
	}
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
	{
		// A support team's gun is heard on its own row by the gun drawn (land.teamMg, Noise.Mount the file, release 2's
		// "teamguns"), at the burst the ground scene gives it (LandTeamGun::BurstOf, the ground thread's 4252a1c7: the
		// Maxim's 8 rounds at 10 a second and the Vickers' 6 at 8, inside the Maxim take's 0.86 s of firing): every take on
		// its row long enough for the burst; a hmg burst with no file, or a file with no row, takes its army's row as before.
		struct TeamShot
		{
			const char* File;
			const char* Army;
			int Rounds;
			double PerSecond;
		};
		const TeamShot Shots[] = { { "HmgMG42", "GER", 18, 20.0 }, { "HmgMG34", "GER", 13, 15.0 }, { "HmgM1919", "USA", 7, 8.0 },
			{ "HmgM1917", "POL", 7, 8.0 }, { "HmgVickers", "UK", 6, 8.0 }, { "HmgMaxim", "SOV", 8, 10.0 }, { "HmgType92", "JAP", 6, 7.5 },
			{ "HmgHotchkiss", "FRA", 6, 7.5 }, { "HmgBreda37", "ITA", 6, 7.5 }, { "", "GER", 10, 20.0 }, { "HmgNoRow", "SOV", 8, 10.0 } };
		LandSound::Mixer Probe;
		std::string ProbeError;
		int OffRow = 0, Rows = 0, Fallback = 0, Heard = 0;
		std::string Picked;
		if (Probe.Begin(MixText, Rec.Seed, ProbeError))
		{
			LandSound::Listener Ear;
			Ear.At = Vec(0.0, 0.0, 1.5);
			Ear.CameraHeight = 1.5;
			for (int Round = 0; Round < 4; ++Round)
			{
				for (size_t K = 0; K < sizeof(Shots) / sizeof(Shots[0]); ++K)
				{
					const TeamShot& Sh = Shots[K];
					LandPlay::Noise N;
					N.Kind = LandPlay::NoiseKind::Shot;
					N.Weapon = "mg";
					N.Role = "hmg";
					N.Army = Sh.Army;
					N.Mount = Sh.File;
					N.Side = 0;
					N.Rounds = Sh.Rounds;
					N.PerSecond = Sh.PerSecond;
					N.At = Vec(20.0, 3.0 * static_cast<double>(K), 0.5);
					N.From = N.At;
					N.To = Vec(400.0, 0.0, 0.0);
					std::vector<LandSound::Take> Got;
					const double Now = 500.0 + 3.0 * static_cast<double>(K + 12 * static_cast<size_t>(Round));
					Probe.Hear(N, Now, Now, 1.0, Ear, Got);
					std::string Name;
					for (const LandSound::Take& T : Got)
					{
						if (T.Family == "mg") { Name = T.Name; break; }
					}
					if (Name.empty()) continue;
					++Heard;
					const std::string Own = std::string("teamMg.") + Sh.File;
					if (Probe.HasList(Own))
					{
						++Rows;
						if (!Probe.InList(Own, Name)) ++OffRow;
					}
					else
					{
						const std::string Army = "byNation.mg." + std::string(Sh.Army);
						if (Probe.InList(Probe.HasList(Army) ? Army : std::string("byNation.mg.*"), Name)) ++Fallback;
						else ++OffRow;
					}
					if (Round == 0) Picked += std::string(" ") + (Sh.File[0] ? Sh.File : "(none)") + " " + Name + ";";
				}
			}
		}
		std::printf("support teams' guns heard close (land.teamMg), %d of %d bursts on their own row, %d on their army's (no file or no row), %d off; whole takes %d; first round:%s\n",
			Rows, Heard, Fallback, OffRow, Probe.TeamMgWhole, Picked.c_str());
		if (!ProbeError.empty()) Fault(("the probe mix for land.teamMg did not load: " + ProbeError).c_str());
		if (Heard < 40) Fault("a support team's close burst was not heard");
		if (OffRow > 0) Fault("a support team's gun played a take off its row (land.teamMg) or, with none, off its army's");
		// (no burst longer than its take's firing: a row holds a take too short for its gun's bursts)
		if (Probe.TeamMgWhole > 0) Fault("a support team's burst played a take shorter than the burst, whole (its row holds a take too short)");
	}
	{
		// A sniper's aimed round (the rules' sn; release 2's sniper teams) is heard as a sniper's (distance.sniper): near,
		// his army's own rifle; past the family's farTakesFrom, a far rifle take with its echo. While he lies hidden (hd)
		// nothing is heard from his spot: his report is placed where his round arrives, and no bolt or shout of his plays.
		LandSound::Mixer Probe;
		std::string ProbeError;
		std::string Near, FarTake, HiddenAt = "WRONG";
		bool bSpotClear = true, bFamily = true;
		if (Probe.Begin(MixText, Rec.Seed, ProbeError))
		{
			LandSound::Listener Ear;
			Ear.At = Vec(0.0, 0.0, 1.5);
			Ear.CameraHeight = 1.5;
			auto Shoot = [&](const Vec& From, const Vec& To, bool bHidden, double Now, std::vector<LandSound::Take>& Got)
			{
				LandPlay::Noise N;
				N.Kind = LandPlay::NoiseKind::Shot;
				N.Weapon = "rifle";
				N.Role = "sniper";
				N.Army = "GER";
				N.Side = 1;
				N.Track = 7;
				N.bSniper = true;
				N.bHidden = bHidden;
				N.At = From;
				N.From = From;
				N.To = To;
				Probe.Hear(N, Now, Now, 1.0, Ear, Got);
			};
			std::vector<LandSound::Take> Got;
			Shoot(Vec(60.0, 0.0, 1.0), Vec(0.0, 5.0, 0.0), false, 700.0, Got);
			for (const LandSound::Take& T : Got) if (T.Family == "sniper") Near = T.Name;
			Got.clear();
			Shoot(Vec(300.0, 0.0, 1.0), Vec(0.0, 5.0, 0.0), false, 760.0, Got);
			for (const LandSound::Take& T : Got) if (T.Family == "sniper") FarTake = T.Name;
			// (hidden 20 m from the ear: close enough for his bolt and his side's shouts, were they heard)
			const Vec Spot(20.0, 0.0, 1.0), Victim(150.0, 40.0, 0.0);
			for (int K = 0; K < 3; ++K)
			{
				Got.clear();
				Shoot(Spot, Victim, true, 820.0 + 60.0 * K, Got);
				for (const LandSound::Take& T : Got)
				{
					if (std::abs(T.At.X - Spot.X) < 5.0 && std::abs(T.At.Y - Spot.Y) < 5.0) bSpotClear = false;
					if (T.Family == "sniper")
					{
						HiddenAt = std::abs(T.At.X - Victim.X) < 1.0 && std::abs(T.At.Y - Victim.Y) < 1.0 ? "where the round arrived" : "WRONG";
						if (T.Name.empty()) bFamily = false;
					}
				}
			}
		}
		const bool bNear = !Near.empty() && Probe.InList("byNation.rifle.GER", Near);
		const bool bFar = !FarTake.empty() && Probe.InList("rifle.far", FarTake);
		std::printf("snipers: at 60 m %s, at 300 m %s (a far take), hidden heard %s, his spot %s; %d heard found, %d hidden\n",
			Near.empty() ? "SILENT" : Near.c_str(), FarTake.empty() ? "SILENT" : FarTake.c_str(), HiddenAt.c_str(),
			bSpotClear ? "silent" : "HEARD", Probe.SniperSeen, Probe.SniperUnseen);
		if (!ProbeError.empty()) Fault(("the probe mix for snipers did not load: " + ProbeError).c_str());
		if (!bNear) Fault("a found sniper's shot near the ear did not play his army's rifle");
		if (!bFar) Fault("a found sniper's shot at 300 m did not play a far rifle take");
		if (HiddenAt != "where the round arrived" || !bFamily) Fault("a hidden sniper's report was not heard where his round arrived");
		if (!bSpotClear) Fault("a hidden sniper was heard from his own spot (his report, bolt or a shout)");
		if (Probe.SniperSeen != 2 || Probe.SniperUnseen != 3) Fault("the snipers' rounds were not all heard as snipers'");
		// (and with a mix that has no sniper row, as an older one: still nothing from his spot, his report where it lands)
		std::string Older = MixText;
		const size_t SniperRow = Older.find("\"sniper\": {");
		LandSound::Mixer Plain;
		std::string PlainError;
		bool bPlainClear = true, bPlainAtVictim = false;
		if (SniperRow != std::string::npos && Plain.Begin(Older.replace(SniperRow, 8, "\"sniper_x\""), Rec.Seed, PlainError))
		{
			LandSound::Listener Ear;
			Ear.At = Vec(0.0, 0.0, 1.5);
			Ear.CameraHeight = 1.5;
			const Vec Spot(20.0, 0.0, 1.0), Victim(150.0, 40.0, 0.0);
			for (int K = 0; K < 3; ++K)
			{
				LandPlay::Noise N;
				N.Kind = LandPlay::NoiseKind::Shot;
				N.Weapon = "rifle";
				N.Role = "sniper";
				N.Army = "GER";
				N.Side = 1;
				N.Track = 7;
				N.bSniper = true;
				N.bHidden = true;
				N.At = Spot;
				N.From = Spot;
				N.To = Victim;
				std::vector<LandSound::Take> Got;
				Plain.Hear(N, 820.0 + 60.0 * K, 820.0 + 60.0 * K, 1.0, Ear, Got);
				for (const LandSound::Take& T : Got)
				{
					if (std::abs(T.At.X - Spot.X) < 5.0 && std::abs(T.At.Y - Spot.Y) < 5.0) bPlainClear = false;
					if (T.Family == "small_arms" && std::abs(T.At.X - Victim.X) < 1.0 && std::abs(T.At.Y - Victim.Y) < 1.0) bPlainAtVictim = true;
				}
			}
		}
		std::printf("snipers with a mix that has no sniper row: hidden heard %s, his spot %s\n",
			bPlainAtVictim ? "where the round arrived" : "WRONG", bPlainClear ? "silent" : "HEARD");
		if (SniperRow == std::string::npos) Fault("the mix has no distance.sniper row to take out for the older-mix probe");
		if (!PlainError.empty()) Fault(("the probe mix without the sniper row did not load: " + PlainError).c_str());
		if (!bPlainClear || !bPlainAtVictim) Fault("with no sniper row in the mix, a hidden sniper was heard from his own spot");
	}
	std::printf("bursts: the most any cut MG burst plays past its last round %.3f s (0 or less: no extra rounds)\n", Mix.MostBurstOver);
	std::printf("same take, no pitch free: %d left out off the screen, %d held to a clear moment, %d close rifle shots kept on their own recording at a half step, %d other takes nearer than their far takes kept on theirs at a half step, %d moved to a far recording, %d far rifle shots moved to another far recording, %d heard in a copy (every far recording crowded), %d at the farthest pitch\n",
		Mix.SameLeftOut, Mix.SameHeldClear, Mix.SameCloseHalf, Mix.SameNearHalf, Mix.SameToFar, Mix.SameToOtherFar, Mix.SameMerged, Mix.SameCrowded);
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
		if (PW.bRicochet && LandAt != std::string::npos)
		{
			// Effects (R2), small arms off armour: a machine gun's burst (8 rounds at 20 a second, 0.4 s) off a tank
			// rattles: burstShare of its rounds ring (at most burstMost: the mix's own numbers, as shipped), each a take
			// of the list (never the same twice running), spread over the burst; a long one (30 rounds) no more; a
			// rifle's single round rings once. And every burst the game fires (LandPlay::BurstOf: 4 to 10 rounds) a full
			// rattle: a machine gun's of 6 to 10 rounds 5 pings at least, one of 4 rounds 3.
			const int ShipMost = Mix.ArmourPingsMost();
			const double ShipShare = Mix.ArmourPingsShare();
			char ShipKeys[96];
			std::snprintf(ShipKeys, sizeof(ShipKeys), "\"burstMost\": %d, \"burstShare\": %.17g", ShipMost, ShipShare);
			std::string RattleText = MixText;
			RattleText.insert(LandAt + 9, std::string(" \"armourStrikes\": { \"smallArms\": [\"steel_a\", \"steel_b\", \"steel_c\"], \"db\": -4, \"whineChance\": 1, ") + ShipKeys + " },");
			const int Want8 = Mix.PingsOf(8), Want30 = Mix.PingsOf(30);
			std::string PingRows;
			bool bFull = true;
			for (const char* Army : { "GER", "USA", "SOV", "UK", "ITA", "JPN" })
			{
				for (const char* Role : { "mg", "hmg", "tank" })
				{
					const LandPlay::Burst Bu = LandPlay::BurstOf("mg", Army, Role);
					const int Got = Mix.PingsOf(Bu.Rounds);
					PingRows += std::string(PingRows.empty() ? "" : ", ") + Army + " " + Role + " " + std::to_string(Bu.Rounds) + ":" + std::to_string(Got);
					if (Got < (Bu.Rounds >= 6 ? 5 : 3) || Got > Bu.Rounds) bFull = false;
				}
			}
			std::printf("armour: the mix's rattle (burstMost %d, burstShare %.2f), pings for each burst the game fires (rounds:pings): %s; %s\n", ShipMost, ShipShare, PingRows.c_str(), bFull ? "a full rattle" : "WRONG, too thin");
			if (!bFull) Fault("a machine gun's burst off armour rattles too few pings with the mix's own numbers (6 to 10 rounds under 5, 4 rounds under 3)");
			LandSound::Mixer Rattle;
			std::string RattleError;
			int Pings = 0, LongPings = 0, OnePings = 0, Whined = 0, Twice = 0, Clangs = 0, Rings = 0;
			double First = 1e9, Last = -1e9;
			if (Rattle.Begin(RattleText, Rec.Seed, RattleError))
			{
				LandPlay::Noise Burst = Off;
				Burst.Rounds = 8;
				Burst.PerSecond = 20.0;
				Cue.clear();
				Rattle.Hear(Burst, 70.0, 70.0, 1.0, Near, Cue);
				std::string Was;
				for (const LandSound::Take& K : Cue)
				{
					if (K.Name.rfind("steel_", 0) != 0) { Whined += K.Family == "small_arms" ? 1 : 0; continue; }
					++Pings;
					// (every ping but the last a sharp clang, cut to burstRingS (0.25 s unless given) with its fade; the last rings out)
					if (std::abs(K.Cut - 0.25) < 1e-9 && K.CutFade > 0.0) ++Clangs;
					else if (K.Cut == 0.0) ++Rings;
					if (K.Name == Was) ++Twice;
					Was = K.Name;
					First = std::min(First, K.Delay);
					Last = std::max(Last, K.Delay);
				}
				Burst.Rounds = 30;
				Cue.clear();
				Rattle.Hear(Burst, 90.0, 90.0, 1.0, Near, Cue);
				for (const LandSound::Take& K : Cue) LongPings += K.Name.rfind("steel_", 0) == 0 ? 1 : 0;
				Burst.Rounds = 1;
				Cue.clear();
				Rattle.Hear(Burst, 110.0, 110.0, 1.0, Near, Cue);
				for (const LandSound::Take& K : Cue) OnePings += K.Name.rfind("steel_", 0) == 0 ? 1 : 0;
			}
			// (spread over the burst's 0.4 s: the last ping may come a little after it where the mix's same-take rule holds a
			// take back to a clear moment, a few hundredths of a second)
			const bool bRattle = Pings == Want8 && Twice == 0 && First >= 0.0 && Last - First <= 0.4 && Last <= 0.5 && Last - First >= 0.15 && LongPings == Want30 && OnePings == 1 && Clangs == Want8 - 1 && Rings == 1;
			std::printf("armour: a burst of 8 off a tank rattles %d pings over %.2f s (%.2f to %.2f, never one take twice running: %s; %d cut short, %d ringing out), %d whine; 30 rounds %d pings; a single round %d: %s\n",
				Pings, Pings ? Last - First : 0.0, Pings ? First : 0.0, Pings ? Last : 0.0, Twice ? "WRONG" : "so", Clangs, Rings, Whined, LongPings, OnePings, bRattle ? "as it should" : "WRONG");
			if (!bRattle) Fault("a small arm's burst off armour did not rattle (land.armourStrikes' burst keys)");
		}
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
	double MostStraight = 0.0, MostAside = 0.0;
	{
		// (the rule's own figures: none of it within AerialAway of the rod's nearest point, all of it from AerialWhole,
		// never less of it the farther off and never a step; none of the ridden tank's at any distance; the ride is
		// the tank shot and no other)
		const Vec Foot(10.0, 20.0, 2.5);
		bool bRule = true;
		double Was = 0.0, Step = 0.0;
		for (int K = 0; K <= 900; ++K)
		{
			const double Far = 0.01 * K;
			const double Now = LandEye::AerialShown(Foot + Vec(Far, 0.0, 1.0), Foot, WireLong, false);
			if (Far < LandEye::AerialAway - 1e-9 && Now != 0.0) bRule = false;
			if (Far > LandEye::AerialWhole + 1e-9 && Now != 1.0) bRule = false;
			if (Far > LandEye::AerialAway + 0.5 && Far < LandEye::AerialWhole - 0.5 && !(Now > 0.02 && Now < 0.98)) bRule = false;
			if (Now < Was) bRule = false;
			Step = std::max(Step, Now - Was);
			Was = Now;
			if (LandEye::AerialShown(Foot + Vec(Far, 0.0, 1.0), Foot, WireLong, true) != 0.0) bRule = false;
		}
		// (over its tip and under its foot the nearest point is the tip and the foot)
		const double Inside = LandEye::AerialAway - 0.1, Outside = LandEye::AerialWhole + 0.1;
		if (LandEye::AerialShown(Foot + Vec(0.0, 0.0, WireLong + Inside), Foot, WireLong, false) != 0.0 || LandEye::AerialShown(Foot + Vec(0.0, 0.0, WireLong + Outside), Foot, WireLong, false) != 1.0) bRule = false;
		if (LandEye::AerialShown(Foot + Vec(0.0, 0.0, -Inside), Foot, WireLong, false) != 0.0 || LandEye::AerialShown(Foot + Vec(0.0, 0.0, -Outside), Foot, WireLong, false) != 1.0) bRule = false;
		LandEye::Pose Ride;
		Ride.Shot = LandEye::Kind::Tank;
		Ride.Subject = 7;
		LandEye::Pose Beside = Ride;
		Beside.Shot = LandEye::Kind::Track;
		if (LandEye::RiddenBy(Ride) != 7 || LandEye::RiddenBy(Beside) != -1) bRule = false;
		if (!bRule || Step > 0.006) Fault("the wireless aerial's rule is not as it says (none of it within AerialAway, all from AerialWhole, never less the farther off, no step, none of the ridden tank's, the ride the tank shot alone)");
		// (and its picture, whatever tanks this recording's film came near: the longest aerial in the kit, 2.5 m,
		// LOOKED STRAIGHT AT, the lens turned to the nearest point of it, from every distance out to 9 m and every
		// height from a metre under its foot to a metre over its tip, through the general's near lens, 44 degrees. That
		// look alone is held. The same lens turned aside until the aerial stands at the picture's side, where a flat
		// picture draws everything taller, is measured beside it and not held; nor is a narrower lens than his, which
		// draws it taller in proportion: what the film's own lenses made of the aerials it came near is the count above)
		const double HalfWide = std::atan(std::tan(22.0 * 3.14159265358979323846 / 180.0) * 16.0 / 9.0);
		for (int H = 1; H <= 180; ++H)
		{
			for (int Z = -4; Z <= 14; ++Z)
			{
				LandEye::Pose Lens;
				Lens.Eye = Foot + Vec(0.05 * H, 0.0, 0.25 * Z);
				Lens.Look = Foot + Vec(0.0, 0.0, std::clamp(0.25 * Z, 0.0, 2.5));
				Lens.Fov = 44.0;
				const double Part = LandEye::AerialShown(Lens.Eye, Foot, 2.5, false);
				if (Part <= 0.0) continue;
				const Vec Tip = Foot + Vec(0.0, 0.0, 2.5 * Part);
				MostStraight = std::max(MostStraight, ThroughPicture(Lens, Foot, Tip));
				// (the look turned about the upright through the eye, by 19 steps out to just inside the picture's side)
				const Vec Flat = Lens.Look - Lens.Eye;
				for (int T = 1; T <= 19; ++T)
				{
					const double Turn = HalfWide * 0.999 * T / 19.0;
					LandEye::Pose Aside = Lens;
					Aside.Look = Lens.Eye + Vec(Flat.X * std::cos(Turn) - Flat.Y * std::sin(Turn), Flat.X * std::sin(Turn) + Flat.Y * std::cos(Turn), Flat.Z);
					MostAside = std::max(MostAside, ThroughPicture(Aside, Foot, Tip));
				}
			}
		}
	}
	std::printf("\nwireless aerials (the Sherman's own stood on every tank, %.2f m long): the lens within %.0f m of one in %.1f%% of the film's frames; drawn whole, as they were, one stood through over half the picture's height in %ld frames (%.1f s; %ld of them the ridden tank's own), %.0f%% of it at the most; put away near the lens (the ridden tank's own in %ld frames, another's gone within %.0f m in %ld, another's drawn short in %ld), one stood through over half of it in %ld frames, %.0f%% at the most; the longest in the kit, 2.5 m, looked straight at through a 44 degree lens from any place, %.0f%% at the most (turned to the picture's side %.0f%%, not held)",
		WireLong, LandEye::AerialWhole, 100.0 * Wires.Near / std::max(1L, Wires.Frames), Wires.HalfWhole, Wires.HalfWhole * Dt, Wires.RideWhole, 100.0 * Wires.MostWhole, Wires.RideAway, LandEye::AerialAway, Wires.Gone, Wires.Short, Wires.HalfShown, 100.0 * Wires.MostShown, 100.0 * MostStraight, 100.0 * MostAside);
	if (Wires.MostShown > 0.6 || MostStraight > 0.6) Fault("a wireless aerial near the lens stands through most of the picture's height (it is to be put away there: LandEye::AerialShown)");
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
		if (bOnlyGen) break;
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
	// Then again with his own tilt (Brian, 24 November: "it's hard to move the angle of the camera to see a good troop
	// movement from above"): tilted down by hand, the top view pressed and pressed again, zoomed to the shoulder and out
	// with his tilt kept, tilted back to the automatic tilt, a tank and his section followed from above, then a wander
	// that also tilts and presses the top view. The same rules hold; the top view must look down from about 100 m, his
	// tilt be kept through the zoom, and the view tilt with the zoom again once tilted all the way back.
	// And a third time with his kept views (Brian, 24 November 20:38Z, release 2: saved views): three kept (the opening
	// one from high up, one low and turned, one in the top view by the first press of its number), each called back and
	// reached; calls on each other's heels; a tank followed from as near as the view comes, with a first press and a
	// keep there, neither of which may move the view or end the follow, and the turn and the zoom back to their own
	// easing once drawn back; a view kept while still coasting from a pan, and a top view kept as soon as pressed, each
	// kept where the view comes to rest; a flight taken by a pan and by a notch of zoom begun in the middle of it, let
	// go where it is with no rush on to the kept view; calls made with a slide still running on, a pan key still held
	// and the pointer at the screen's edge, all reached; a flight taken by a notch with the pointer resting at the edge,
	// by a unit's card (the look-at must go to the unit), and by a pan from the top view on the way to a view without his tilt, none flying on in its
	// look-at, turn, zoom or tilt; a view kept in the middle of a flight, and with the top view just pressed while
	// following a unit; a reached call's end (flight over, chip lit, the edge panning again); his hand tilting the
	// view away from a lit chip's view, once from a view kept without his own tilt and once from one kept with it (the
	// chip goes out), and each of the two then called back with only the tilt to travel (the flight is not over before
	// the tilt is there); two calls on each other's heels to views kept without his tilt, the second where he stands,
	// taken by a pan (his tilt stays his own); a view kept in the top view, tilted part of the way back and called
	// again, taken by a notch (his own tilt to his own tilt, stopping where its push allows); a long flight from the
	// top view taken by a notch once his tilt has sunk to the automatic one, and another with a view kept in it at
	// that point instead (the view must tilt with the zoom again, not keep a tilt it cannot show); then a wander that also keeps and calls. The same rules hold; a called view must be reached as it was
	// kept, and a chip must not stay lit with the view away from its view.
	// (and the one rule of the kept views no scripted battle can be relied on to walk into, on a bare field of its own)
	if (!std::getenv("LAND_PLAY_NO_VIEWS")) TiltTakeBench();
	for (int GenRun = 0; GenRun < 3; ++GenRun)
	{
		const bool bTiltRun = GenRun == 1, bViewRun = GenRun == 2;
		if (bTiltRun && std::getenv("LAND_PLAY_NO_TILT")) continue;
		if (bViewRun && std::getenv("LAND_PLAY_NO_VIEWS")) continue;
		LandGeneralEye::Eye Gen;
		const double* Za = Rec.Ground.ZoneAtt;
		const double* Zd = Rec.Ground.ZoneDef;
		const double Ox = 0.5 * (Za[0] + Za[2]), Oy = 0.5 * (Za[1] + Za[3]);
		const double Ex = 0.5 * (Zd[0] + Zd[2]), Ey = 0.5 * (Zd[1] + Zd[3]);
		Gen.Begin(Rec, EW, Vec(Ox, Oy, 0.0), LandFeed::FacingTo(Ox, Oy, Ex, Ey), 250.0);
		// (the block his card presses and his follows rest on: one of his own still on the field at the end of the
		// scripted steps, so a block lost on the way cannot fault a build that is right. A tank's block where one
		// lives that long, else any of his; -1 where none does, and the steps that need one are left out)
		double StepsEnd = 0.0;
		for (double T = 0.0; T < 215.0; T += Dt) StepsEnd += Dt * (StepsEnd < Contact - 10.0 ? 2.0 : 0.5);
		int TankBlock = -1, StandingBlock = -1;
		for (const LandFeed::Block& Bk : Rec.Blocks)
		{
			double Ux = 0.0, Uy = 0.0;
			if (Bk.Side != ES.Own || !Gen.BlockAt(Bk.Id, StepsEnd, Ux, Uy)) continue;
			if (StandingBlock < 0) StandingBlock = Bk.Id;
			if (!Bk.Model.empty() && TankBlock < 0)
			{
				for (const LandFeed::Track& Tr : Rec.Tracks)
				{
					if (Tr.Block == Bk.Id && Tr.Role == "tank") { TankBlock = Bk.Id; break; }
				}
			}
		}
		if (TankBlock < 0) TankBlock = StandingBlock;
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
		const double End = bViewRun ? 404.0 : 260.0;
		// (the kept views' own checks: each view as it stood when kept, the call being timed, and the worst a called view
		// was missed by once it should have been reached)
		struct KeptAs { bool bKept = false, bOwn = false; double X = 0.0, Y = 0.0, H = 0.0, S = 0.0, Tilt = 0.0; };
		KeptAs Kept[LandGeneralEye::ViewCount];
		int Called = -1, Calls = 0, Reached = 0, Keeps = 0, FlyFrames = 0;
		double CalledAt = 0.0, CheckAt = -1.0, MissPlace = 0.0, MissZoom = 0.0, MissTurn = 0.0, MissTilt = 0.0, LongestFlight = 0.0, FirstPressMoved = 0.0;
		Vec FirstPressAt;
		double FirstPressUntil = -1.0;
		// (a keep watched: the view as it stood at the press, and whether it was following or flying; the view must go on
		// only to where it was kept, a follow must go on, and left alone the view must come to rest as kept, its chip lit)
		struct KeepWatch { int Number = -1; double Until = 0.0, SettleAt = 0.0, S0 = 0.0, H0 = 0.0; Vec La0; int Follow0 = -1, Braked0 = 0, Waited0 = 0; bool bFlight = false, bHidden = false; };
		KeepWatch Watch;
		int KeptAtRest = 0, KeptWrong = 0, KeepsSettled = 0, SettledWrong = 0, KeepMoved = 0, KeepEndedFollow = 0;
		// (a flight his hand took: the look-at's speed as it was taken, where, and whether by a pan; the look-at must not
		// rush on faster than that and his pan, and must come to rest about where a glide let go would)
		double TakenUntil = -1.0, TakenSpeed = 0.0, WorstRush = 0.0, WorstRunOn = 0.0;
		Vec TakenAt;
		bool bTakenPanned = false;
		int HandTook = 0, NotTaken = 0, Rushes = 0, RunsOn = 0;
		// (a scripted hand on a flight: begun once the flight has covered three tenths of its way)
		double HandAt = -1.0;
		Vec CallFrom;
		// (and its turn, zoom and tilt: each as it stood and how fast it was going as he took it, and what his own hand
		// has asked of it since; none may go on toward the kept view past where its own push stops it)
		double TakenH = 0.0, TakenS = 0.0, TakenTilt = 0.0, TakenHRate = 0.0, TakenSRate = 0.0, TakenTiltRate = 0.0, OwnTurn = 0.0, OwnS = 0.0, OwnTiltIn = 0.0;
		double LastTurnIn = -1e9, LastZoomIn = -1e9, LastTiltIn = -1e9, WorstFlewOn = 0.0;
		bool bTakenTop = false, bTakenToBlock = false, bTakenOwn = false, bTakenHidden = false;
		int FlewOn = 0, BlockTakes = 0, TiltTakes = 0, TiltTakesJudged = 0;
		// (a called view reached: a second and a half on, left alone, the flight is over, the turn and the zoom are
		// free again and its chip is lit)
		int ReachedView = -1, EndsChecked = 0, EndsWrong = 0, ReachedBraked = 0, ReachedWaited = 0;
		double ReachedAt = 0.0;
		// (the pointer at the screen's edge once a flight is over: it pans again)
		Vec EdgeFrom;
		int EdgeChecks = 0, EdgeDead = 0;
		// (his own tilt and the chips: a chip's going out when his hand tilts the view away from its view, judged where
		// the chip was lit and the tilt did go; and a called flight with only the tilt to travel, at its own end)
		int TiltChips = 0, TiltChipsOwn = 0, TiltChipLit = 0, TiltEnds = 0, TiltEndsAuto = 0, TiltEndsWrong = 0, ChipWas = -1;
		// (every flight to a kept view that is over by itself, no hand on it and no keep: the view is there)
		int OwnEnds = 0, OwnEndsWrong = 0;
		// (a flight taken once his own tilt had already sunk to the automatic one, with none of it on the picture:
		// the view must tilt with the zoom again, not keep a tilt of his own it cannot show)
		int HandBacks = 0, HandBacksWrong = 0, KeepBacks = 0, KeepBacksWrong = 0;
		// (the most his own pan asked of the look-at through a flight he took: each axis is asked for in full, so a
		// pan across and ahead at once carries the look-at up to its two axes together)
		double TakenPanMost = 0.0;
		// (keeps made in the middle of a flight and while following a unit: what is not his own hand's kept as it
		// stood, and after a unit the turn, the zoom and the tilt come to rest as kept)
		int FlightKeeps = 0, FlightKeptWrong = 0, FollowKeeps = 0, FollowKeptWrong = 0, FollowSettled = 0, FollowSettledWrong = 0;
		// (calls that must stand: made with a slide still running on, a pan key still held, the pointer at the edge)
		int Stood = 0, StoodReached = 0;
		bool bCalledStands = false;
		// (the turn and the zoom held long after coming to rest; a chip lit with the view away from its view)
		double HeldFor = 0.0, LitFor = 0.0;
		int HeldLong = 0, LitAway = 0, BrakedWas = 0, WaitedWas = 0;
		Vec LaWas(Ox, Oy, 0.0);
		double SWas = 0.0, HWas = 0.0, TiltWas = 0.0;
		// (the tilted run's own checks: the top view's steepest and highest, his tilt after the zoom out, the view back to
		// tilting with the zoom, and how often the top view and his tilt were on)
		double TopTiltBest = 0.0, TopUpBest = 0.0, KeptTilt = -1.0, NearTilt = 0.0;
		int BackToAuto = -1, TopFrames = 0, OwnFrames = 0, TopPresses = 0;
		while (R < End)
		{
			LandGeneralEye::Input In;
			auto Notch = [&](double Way, double Every) { if (R >= NextNotch) { In.Zoom = Way; NextNotch = R + Every; } };
			auto Once = [&](double At) { return R >= At && R - Dt < At; };
			// (the kept views' run: this frame's hand is one the view must leave out, and the call stand)
			bool bStands = false;
			// (or the pointer resting at the edge through a flight that his hand then takes: left out too, but the call
			// is not one that must stand)
			bool bEdgeRests = false;
			// (the tilt the view takes by itself at a zoom, degrees: what his own tilt is measured against)
			auto AutoTilt = [](double Zoom) { return AutoTiltDeg(Zoom); };
			// (and how much of a flight's way to a kept view is covered)
			auto Covered = [&](int Number)
			{
				const Vec La = Gen.LookAt();
				const double All = std::hypot(Kept[Number].X - CallFrom.X, Kept[Number].Y - CallFrom.Y);
				return All < 1.0 ? 1.0 : 1.0 - std::hypot(Kept[Number].X - La.X, Kept[Number].Y - La.Y) / All;
			};
			if (bTiltRun)
			{
				if (R < 2.0) {}
				else if (R < 5.0) In.Tilt = 40.0 * Dt;
				else if (R < 7.0) In.PanAhead = 1.0;
				else if (R < 12.0) {}
				else if (R < 13.0) In.bTopView = Once(12.5);
				else if (R < 18.0) In.Turn = 45.0 * Dt;
				else if (R < 26.0) Notch(1.0, 0.25);
				else if (R < 34.0) In.PanAhead = 1.0;
				else if (R < 40.0) Notch(-1.0, 0.25);
				else if (R < 46.0) { In.bTopView = Once(40.0); if (R > 41.0) In.PanRight = 1.0; }
				else if (R < 52.0) In.Tilt = -40.0 * Dt;
				else if (R < 60.0) { In.SlideRight = (static_cast<int>(R / 2.0) % 2 ? 1.0 : -1.0) * 0.012; In.SlideAhead = 0.006; }
				else if (R < 64.0) In.bTopView = Once(60.0);
				else if (R < 70.0) Notch(1.0, 0.3);
				else if (R < 78.0) In.PanAhead = 1.0;
				else if (R < 110.0)
				{
					if (Once(78.0)) In.GoBlock = TankBlock;
					In.bFollow = true;
					if (R < 84.0) Notch(1.0, 0.25);
				}
				else if (R < 140.0)
				{
					if (Once(110.0)) { In.GoBlock = Eye.SquadBlock(); In.bTopView = true; }
					In.bFollow = true;
					if (R > 115.0 && R < 117.0) In.Tilt = 30.0 * Dt;
					if (R < 118.0) Notch(1.0, 0.3);
				}
				else
				{
					if (R >= NextWander)
					{
						NextWander = R + 1.5 + 3.0 * Roll();
						Wander = LandGeneralEye::Input();
						const double Pick = Roll();
						if (Pick < 0.3) { Wander.PanRight = Roll() * 2.0 - 1.0; Wander.PanAhead = Roll() * 2.0 - 1.0; }
						else if (Pick < 0.4) { Wander.EdgeRight = Roll() < 0.5 ? -1.0 : 1.0; Wander.bLockEdgePan = Roll() < 0.3; }
						else if (Pick < 0.5) Wander.Turn = (Roll() * 2.0 - 1.0) * 60.0 * Dt;
						else if (Pick < 0.65) Wander.Zoom = Roll() < 0.5 ? 1.0 : -1.0;
						else if (Pick < 0.85) { Wander.Tilt = (Roll() * 2.0 - 1.0) * 50.0 * Dt; if (Roll() < 0.5) Wander.PanAhead = Roll() * 2.0 - 1.0; }
						else if (Pick < 0.93) Wander.bTopView = true;
						else Wander.SlideRight = (Roll() * 2.0 - 1.0) * 0.01;
					}
					In = Wander;
					if (In.Zoom != 0.0) { if (R < NextNotch) In.Zoom = 0.0; else NextNotch = R + 0.3; }
					// (a press is one frame's)
					Wander.bTopView = false;
				}
			}
			else if (bViewRun)
			{
				if (R < 2.0) {}
				else if (R < 6.0) { if (Once(2.0)) In.KeepView = 0; Notch(1.0, 0.25); }
				else if (R < 12.0) In.PanAhead = 1.0;
				else if (R < 15.0) In.Turn = 45.0 * Dt;
				else if (R < 16.0) { if (Once(15.5)) In.KeepView = 1; }
				else if (R < 17.0) In.bTopView = Once(16.0);
				else if (R < 22.0) In.PanRight = 1.0;
				// (the first press of a number never kept, in the top view: kept, and the view does not move)
				else if (R < 24.0) { if (Once(22.5)) In.CallView = 2; }
				else if (R < 32.0) { if (Once(24.0)) In.CallView = 0; }
				else if (R < 40.0) { if (Once(32.0)) In.CallView = 1; }
				else if (R < 48.0) { if (Once(40.0)) In.CallView = 2; }
				else if (R < 52.0) In.PanAhead = 1.0;
				else if (R < 62.0)
				{
					// (calls on each other's heels: the last one stands)
					if (Once(52.0)) In.CallView = 1;
					if (Once(53.0)) In.CallView = 0;
					if (Once(53.5)) In.CallView = 2;
					if (Once(54.2)) In.CallView = 1;
				}
				else if (R < 90.0)
				{
					if (Once(62.0)) In.GoBlock = TankBlock;
					In.bFollow = R < 75.0;
					if (R < 66.0) Notch(1.0, 0.25);
					// (following the tank from as near as the view comes: the first press of a number never kept, then
					// that number kept anew; neither may move the view or end the follow)
					if (Once(68.0)) In.CallView = 3;
					if (Once(71.5)) In.KeepView = 3;
					if (Once(75.0)) In.CallView = 0;
				}
				else if (R < 96.0) { if (Once(90.0)) In.CallView = 0; }
				else if (R < 104.0)
				{
					// (a long pan from high up, and the view kept three frames after the key is let go, still coasting: it
					// is kept where it comes to rest)
					if (R < 99.0) In.PanAhead = 1.0;
					if (Once(99.05)) In.KeepView = 3;
				}
				else if (R < 112.0) { if (Once(104.0)) In.CallView = 1; }
				else if (R < 118.0)
				{
					// (a flight across the field taken by a pan begun in the middle of it)
					if (Once(112.0)) { In.CallView = 3; HandAt = -1.0; }
					else if (HandAt < 0.0 && Gen.FlyingTo() == 3 && Covered(3) >= 0.3) HandAt = R;
					if (HandAt >= 0.0 && R < HandAt + 0.6) In.PanAhead = 1.0;
				}
				else if (R < 124.0)
				{
					// (and by a notch of zoom)
					if (Once(118.0)) { In.CallView = 0; HandAt = -1.0; }
					else if (HandAt < 0.0 && Gen.FlyingTo() == 0 && Covered(0) >= 0.3) { HandAt = R; In.Zoom = -1.0; }
				}
				else if (R < 133.0)
				{
					// (a trackpad's slide still running on as a view is called, dying away over most of a second: left
					// out of the flight, and the view is reached)
					if (R < 125.0) { In.SlideRight = 0.012; In.SlideAhead = 0.004; }
					else if (R < 125.8)
					{
						const double Runs = 1.0 - (R - 125.0) / 0.8;
						In.SlideRight = 0.012 * Runs * Runs;
						In.SlideAhead = 0.004 * Runs * Runs;
					}
					if (Once(125.0)) In.CallView = 3;
					bStands = R >= 125.0;
				}
				else if (R < 142.0)
				{
					// (a pan key held as a view is called and for a second after: left out until it is let go)
					if (R < 135.0) In.PanRight = 1.0;
					if (Once(134.0)) In.CallView = 1;
					bStands = R >= 134.0;
				}
				else if (R < 147.0)
				{
					// (the top view pressed and kept a third of a second on, in the middle of its swing: the top view is
					// kept, not the half-way one)
					In.bTopView = Once(142.0);
					if (Once(142.3)) In.KeepView = 0;
				}
				else if (R < 155.0)
				{
					// (the pointer resting at the screen's edge as a view is called: no pan through the flight)
					if (R < 148.4) In.EdgeRight = 1.0;
					if (Once(147.4)) In.CallView = 3;
					bStands = R >= 147.4;
				}
				else if (R < 163.0)
				{
					// (the pointer resting at the edge through a flight that a notch of zoom takes in the middle: the
					// edge pans again from that frame, and the look-at, coasting, must not rush on under it)
					if (R < 161.0) In.EdgeRight = 1.0;
					if (Once(155.4)) { In.CallView = 1; HandAt = -1.0; }
					else if (R > 155.4 && HandAt < 0.0 && Gen.FlyingTo() == 1 && Covered(1) >= 0.3) { HandAt = R; In.Zoom = -1.0; }
					bEdgeRests = R >= 155.4 && HandAt < 0.0;
				}
				else if (R < 172.0)
				{
					// (a flight taken by a unit's card in the middle of it: the look-at glides to the unit, the turn and
					// the zoom are let go where they are)
					if (Once(163.0)) { In.CallView = 3; HandAt = -1.0; }
					else if (HandAt < 0.0 && Gen.FlyingTo() == 3 && Covered(3) >= 0.3) { HandAt = R; In.GoBlock = TankBlock; }
					In.bFollow = HandAt >= 0.0 && R < 171.0;
				}
				else if (R < 184.0)
				{
					// (from the top view, a call to a view kept without his own tilt, taken by a pan a fifth of a second
					// on: his tilt stays his own, about where it was, and does not swing on to the automatic one)
					if (Once(172.0)) In.CallView = 0;
					if (Once(178.0)) In.CallView = 1;
					if (R >= 178.2 && R < 178.6) In.PanAhead = 1.0;
				}
				else if (R < 194.0)
				{
					// (a view kept in the middle of a flight across the field, no hand on it: kept as it stood at the
					// press, and the flight ends there, its chip lit)
					if (Once(184.0)) { In.CallView = 3; HandAt = -1.0; }
					else if (HandAt < 0.0 && Gen.FlyingTo() == 3 && Covered(3) >= 0.3) { HandAt = R; In.KeepView = 2; }
				}
				else if (R < 203.0)
				{
					// (a call left alone to its end, then the pointer at the edge for a second: it pans again)
					if (Once(194.0)) In.CallView = 1;
					if (R >= 200.0 && R < 201.0) In.EdgeRight = 1.0;
				}
				else if (R < 215.0)
				{
					// (following a unit, the top view pressed and the view kept a fifth of a second on, in the middle of
					// its swing: the top view is kept, as with no unit followed)
					if (Once(203.0)) In.GoBlock = TankBlock;
					In.bFollow = R < 214.0;
					In.bTopView = Once(207.0);
					if (Once(207.2)) In.KeepView = 2;
				}
				else if (R < 226.0)
				{
					// (from the top view, a call to a view kept without his own tilt, and a view kept a third of a second
					// into the flight: kept with his own tilt as it stood, and the flight ends there)
					if (Once(215.0)) In.CallView = 0;
					if (Once(221.0)) In.CallView = 3;
					if (Once(221.3)) In.KeepView = 2;
				}
				else if (R < 246.0)
				{
					// (his own tilt and the chips. At a view kept without his own tilt, his hand tilts the view: its chip
					// goes out. That tilted view kept; his hand tilts it part of the way back, still his own tilt: its
					// chip goes out again. Called back, only the tilt has to travel, and the flight is not over before
					// the tilt is there; nor is the flight back to the view without his own tilt, from the same place,
					// before the tilt is the automatic one again)
					if (Once(226.0)) In.CallView = 1;
					if (R >= 231.0 && R < 232.0) In.Tilt = 40.0 * Dt;
					if (Once(233.5)) In.KeepView = 3;
					if (R >= 234.5 && R < 234.875) In.Tilt = -40.0 * Dt;
					if (Once(237.0)) In.CallView = 3;
					if (Once(241.0)) In.CallView = 1;
				}
				else if (R < 264.0)
				{
					// (his own tilt through two calls on each other's heels to views kept without one, the second to
					// a view where he stands, taken by a pan a fifth of a second on: his tilt is still his own,
					// where it was, and stops where its own push allows. Then a view kept in the top view and
					// tilted part of the way back by hand, called again: the tilt flies from his own to his own,
					// and a notch of zoom in the middle must not let it fly on either)
					if (R >= 246.0 && R < 247.0) Notch(-1.0, 0.12);
					if (Once(248.0)) In.KeepView = 3;
					if (Once(249.0)) In.bTopView = true;
					if (Once(252.0)) In.CallView = 3;
					if (Once(252.1)) In.CallView = 1;
					if (R >= 252.2 && R < 252.6) In.PanAhead = 1.0;
					if (Once(256.0)) In.bTopView = true;
					if (Once(258.5)) In.KeepView = 2;
					if (R >= 259.5 && R < 260.5) In.Tilt = -40.0 * Dt;
					if (Once(262.0)) In.CallView = 2;
					if (R >= 262.3 && R < 262.4) In.Zoom = -1.0;
				}
				else if (R < 296.0)
				{
					// (a long flight from the top view to a view kept without his own tilt: the tilt is the first
					// thing there, and a notch of zoom once it has sunk to the automatic one takes the flight with
					// nothing of his tilt left on the picture. The view must tilt with the zoom again, not keep a
					// tilt of his own that it cannot show. Then that flight again with a view kept in it instead of
					// a hand on it: it is kept as the picture stands, the automatic tilt, and his own tilt handed back)
					if (R >= 264.0 && R < 269.0) In.PanAhead = 1.0;
					if (Once(271.0)) In.bTopView = true;
					if (Once(274.0)) { In.CallView = 1; HandAt = -1.0; }
					else if (R > 274.0 && R < 280.0 && HandAt < 0.0 && Gen.FlyingTo() == 1 && Gen.TiltDegrees() - AutoTilt(Gen.Zoom()) < 0.2) { HandAt = R; In.Zoom = -1.0; }
					else if (R >= 281.0 && R < 286.0) In.PanAhead = 1.0;
					else if (Once(288.0)) In.bTopView = true;
					else if (Once(290.0)) { In.CallView = 1; HandAt = -1.0; }
					else if (R > 290.0 && HandAt < 0.0 && Gen.FlyingTo() == 1 && Gen.TiltDegrees() - AutoTilt(Gen.Zoom()) < 0.2) { HandAt = R; In.KeepView = 2; }
				}
				else
				{
					if (R >= NextWander)
					{
						NextWander = R + 1.5 + 3.0 * Roll();
						Wander = LandGeneralEye::Input();
						const double Pick = Roll();
						if (Pick < 0.3) { Wander.PanRight = Roll() * 2.0 - 1.0; Wander.PanAhead = Roll() * 2.0 - 1.0; }
						else if (Pick < 0.4) Wander.Turn = (Roll() * 2.0 - 1.0) * 60.0 * Dt;
						else if (Pick < 0.52) Wander.Zoom = Roll() < 0.5 ? 1.0 : -1.0;
						else if (Pick < 0.6) Wander.Tilt = (Roll() * 2.0 - 1.0) * 50.0 * Dt;
						else if (Pick < 0.65) Wander.bTopView = true;
						else if (Pick < 0.92) Wander.CallView = static_cast<int>(Roll() * 4.0) % 4;
						else Wander.KeepView = static_cast<int>(Roll() * 4.0) % 4;
					}
					In = Wander;
					if (In.Zoom != 0.0) { if (R < NextNotch) In.Zoom = 0.0; else NextNotch = R + 0.3; }
					// (a press is one frame's)
					Wander.bTopView = false;
					Wander.CallView = Wander.KeepView = -1;
				}
			}
			else if (R < 2.0) {}
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
			const double Pi = 3.14159265358979;
			auto Wrapped = [](double Degrees) { while (Degrees > 180.0) Degrees -= 360.0; while (Degrees < -180.0) Degrees += 360.0; return Degrees; };
			// (the view before this frame's move: where it looked, how fast, and what it was doing)
			const Vec LaBefore = Gen.LookAt();
			const double SBefore = Gen.Zoom(), HBefore = Gen.Heading(), TiltBefore = Gen.TiltDegrees();
			const double SpeedBefore = std::hypot(LaBefore.X - LaWas.X, LaBefore.Y - LaWas.Y) / Dt;
			const bool bRestBefore = SpeedBefore < 0.05 && std::abs(SBefore - SWas) / Dt < 0.002 && std::abs(Wrapped((HBefore - HWas) * 180.0 / Pi)) / Dt < 0.05 && std::abs(TiltBefore - TiltWas) / Dt < 0.05;
			const int FlyBefore = Gen.FlyingTo(), FollowBefore = Gen.Following();
			const bool bPanIn = In.PanRight != 0.0 || In.PanAhead != 0.0 || In.EdgeRight != 0.0 || In.SlideRight != 0.0 || In.SlideAhead != 0.0;
			const bool bHandIn = bPanIn || In.Zoom != 0.0 || In.Turn != 0.0 || In.Tilt != 0.0 || In.bTopView || In.GoBlock >= 0;
			// (a view kept: by Shift's press, or by the first press of a number never kept)
			const int Keeping = !bViewRun ? -1 : In.KeepView >= 0 ? In.KeepView : In.CallView >= 0 && !Gen.ViewKept(In.CallView) ? In.CallView : -1;
			// (a flight his own hand takes this frame: a move not left out)
			const bool bLeftOut = bStands || bEdgeRests, bOwnBefore = Gen.OwnTilt();
			const bool bTake = bViewRun && FlyBefore >= 0 && bHandIn && !bLeftOut;
			if (bViewRun)
			{
				if (Keeping < 0 && In.CallView >= 0)
				{
					Called = In.CallView;
					CalledAt = R;
					CheckAt = R + 7.9;
					CallFrom = LaBefore;
					bCalledStands = bStands;
					Stood += bStands ? 1 : 0;
					++Calls;
				}
				// (his own hand on the view: the call is over, but for a hand the view must leave out)
				if (bHandIn && !bLeftOut) Called = -1;
				// (and a keep's watch is over at his next move, call or keep; so is a reached view's)
				if (bHandIn || In.CallView >= 0 || In.KeepView >= 0) Watch.Number = ReachedView = -1;
				if (Once(200.0)) EdgeFrom = LaBefore;
				if (Once(231.0) || Once(234.5)) ChipWas = Gen.ViewAt();
				if (In.CallView >= 0 || In.KeepView >= 0) TakenUntil = -1.0;
			}
			const auto GT0 = std::chrono::steady_clock::now();
			const LandEye::Pose P = Gen.Update(In, Dt, B);
			const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - GT0).count();
			if (bViewRun)
			{
				const Vec LaNow = Gen.LookAt();
				FlyFrames += Gen.Flying() ? 1 : 0;
				if (Keeping >= 0)
				{
					// (the view as the eye has kept it: where a call will bring it)
					KeptAs& K = Kept[Keeping];
					Vec KeptLook;
					K.bKept = Gen.ViewIs(Keeping, KeptLook, K.H, K.S, K.Tilt);
					K.X = KeptLook.X;
					K.Y = KeptLook.Y;
					++Keeps;
					// (kept at rest, it is kept as it stood)
					if (bRestBefore && FlyBefore < 0)
					{
						++KeptAtRest;
						if (std::hypot(K.X - LaBefore.X, K.Y - LaBefore.Y) > 0.2 || std::abs(K.S - SBefore) > 0.002 || std::abs(Wrapped((K.H - HBefore) * 180.0 / Pi)) > 0.2 || std::abs(K.Tilt - TiltBefore) > 0.5)
						{
							++KeptWrong;
							std::printf("  view %d kept at rest at real %.2f, but not as it stood: %.2f m, zoom %.4f, turn %.2f, tilt %.2f off\n", Keeping, R, std::hypot(K.X - LaBefore.X, K.Y - LaBefore.Y), K.S - SBefore,
								Wrapped((K.H - HBefore) * 180.0 / Pi), K.Tilt - TiltBefore);
						}
					}
					// (kept in the middle of a flight: all of it as it stood, his own tilt too where the flight was turning
					// it back; kept while following a unit: where it looks as it stood)
					const double OffPlace = std::hypot(K.X - LaBefore.X, K.Y - LaBefore.Y);
					if (FlyBefore >= 0 && !bHandIn)
					{
						++FlightKeeps;
						if (OffPlace > 0.2 || std::abs(K.S - SBefore) > 0.002 || std::abs(Wrapped((K.H - HBefore) * 180.0 / Pi)) > 0.2 || std::abs(K.Tilt - TiltBefore) > 0.5)
						{
							++FlightKeptWrong;
							std::printf("  view %d kept in a flight at real %.2f, but not as it stood: %.2f m, zoom %.4f, turn %.2f, tilt %.2f off\n", Keeping, R, OffPlace, K.S - SBefore, Wrapped((K.H - HBefore) * 180.0 / Pi),
								K.Tilt - TiltBefore);
						}
					}
					else if (FlyBefore < 0 && FollowBefore >= 0)
					{
						++FollowKeeps;
						if (OffPlace > 0.2)
						{
							++FollowKeptWrong;
							std::printf("  view %d kept following a unit at real %.2f, but %.2f m from where it looked\n", Keeping, R, OffPlace);
						}
					}
					if (!bHandIn)
					{
						Watch.Number = Keeping;
						Watch.Until = R + 3.0;
						// (settled a frame or two before his next scripted press, so a keep is judged at rest even where
					// the next press follows 3.5 s on)
					Watch.SettleAt = R + 3.5 - 2.0 * Dt;
						Watch.S0 = SBefore;
						Watch.H0 = HBefore;
						Watch.La0 = LaBefore;
						Watch.Follow0 = FollowBefore;
						Watch.Braked0 = Gen.BrakedFrames();
						Watch.Waited0 = Gen.WaitedFrames();
						Watch.bFlight = FlyBefore >= 0;
						// (kept with his own tilt on but none of it on the picture: it is handed back, as a hand's take hands
						// it back, and the view tilts with the zoom again)
						Watch.bHidden = bOwnBefore && SBefore > 0.25 && TiltBefore - AutoTilt(SBefore) < 0.3;
					}
					if (In.KeepView < 0 && FollowBefore < 0)
					{
						FirstPressAt = LaBefore;
						FirstPressUntil = R + 1.0;
					}
					// (kept anew while a call to it was being timed, or any view kept in the middle of a flight, which then
					// ends at the view just kept: the call is no longer the test's to judge)
					if (Keeping == Called || FlyBefore >= 0) Called = -1;
				}
				else if (Watch.Number >= 0)
				{
					const KeptAs& K = Kept[Watch.Number];
					if (R < Watch.Until && !Watch.bFlight)
					{
						// (no further than from where it stood to where it was kept: the keep itself moves nothing)
						const double Turned = Wrapped((Gen.Heading() - Watch.H0) * 180.0 / Pi), TurnKept = Wrapped((K.H - Watch.H0) * 180.0 / Pi);
						bool bMoved = Gen.Zoom() < std::min(Watch.S0, K.S) - 0.01 || Gen.Zoom() > std::max(Watch.S0, K.S) + 0.01 || Turned < std::min(0.0, TurnKept) - 1.0 || Turned > std::max(0.0, TurnKept) + 1.0;
						if (Watch.Follow0 >= 0)
						{
							// (not where the unit itself has gone from the field: the keep did not end that follow)
							double Wx = 0.0, Wy = 0.0;
							if (In.bFollow && Gen.Following() != Watch.Follow0 && Gen.BlockAt(Watch.Follow0, B, Wx, Wy)) ++KeepEndedFollow;
						}
						else
						{
							const double Lx = K.X - Watch.La0.X, Ly = K.Y - Watch.La0.Y, L2 = Lx * Lx + Ly * Ly;
							const double U = L2 < 1e-9 ? 0.0 : std::clamp(((LaNow.X - Watch.La0.X) * Lx + (LaNow.Y - Watch.La0.Y) * Ly) / L2, 0.0, 1.0);
							if (std::hypot(LaNow.X - Watch.La0.X - U * Lx, LaNow.Y - Watch.La0.Y - U * Ly) > 1.0) bMoved = true;
						}
						if (bMoved)
						{
							++KeepMoved;
							if (std::getenv("LAND_PLAY_VIEWS")) std::printf("  the keep of view %d moved the view at real %.2f: zoom %.3f (%.3f to %.3f), turned %.1f (to %.1f), following %d\n", Watch.Number, R, Gen.Zoom(), Watch.S0, K.S, Turned, TurnKept, Gen.Following());
						}
					}
					if (R >= Watch.SettleAt)
					{
						// (left alone since: where it came to rest is the view kept, its chip lit; after a unit followed, its
						// turn, zoom and tilt are, where no hedge has made the view wait since)
						if (Watch.Follow0 >= 0)
						{
							if (Gen.Following() == Watch.Follow0 && Gen.BrakedFrames() == Watch.Braked0 && Gen.WaitedFrames() == Watch.Waited0)
							{
								const double Zoomed = std::abs(Gen.Zoom() - K.S), Turned = std::abs(Wrapped((Gen.Heading() - K.H) * 180.0 / Pi)), Tilted = std::abs(Gen.TiltDegrees() - K.Tilt);
								++FollowSettled;
								if (Zoomed > 0.005 || Turned > 0.5 || Tilted > 1.5)
								{
									++FollowSettledWrong;
									std::printf("  view %d kept at real %.2f following a unit: left alone, the view came to rest zoom %.3f, turn %.1f, tilt %.1f off it\n", Watch.Number, Watch.SettleAt - 3.5 + 2.0 * Dt, Zoomed, Turned, Tilted);
								}
							}
						}
						else
						{
							const double Place = std::hypot(LaNow.X - K.X, LaNow.Y - K.Y), Zoomed = std::abs(Gen.Zoom() - K.S), Turned = std::abs(Wrapped((Gen.Heading() - K.H) * 180.0 / Pi)), Tilted = std::abs(Gen.TiltDegrees() - K.Tilt);
							++KeepsSettled;
							if (Watch.bHidden)
							{
								++KeepBacks;
								if (Gen.OwnTilt() || std::abs(Gen.TiltDegrees() - AutoTilt(Gen.Zoom())) > 1.0)
								{
									++KeepBacksWrong;
									std::printf("  view %d kept at real %.2f once his own tilt had sunk to the automatic one: left alone, his own tilt is %s and the view is tilted %.2f degrees off the automatic one\n", Watch.Number, Watch.SettleAt - 3.5 + 2.0 * Dt,
										Gen.OwnTilt() ? "still on" : "off", Gen.TiltDegrees() - AutoTilt(Gen.Zoom()));
								}
							}
							if (Place > 0.5 || Zoomed > 0.005 || Turned > 0.5 || Tilted > 1.5 || Gen.ViewAt() != Watch.Number)
							{
								++SettledWrong;
								std::printf("  view %d kept at real %.2f (%s): left alone, the view came to rest %.1f m, zoom %.3f, turn %.1f, tilt %.1f off it, at view %d\n", Watch.Number, Watch.SettleAt - 3.5 + 2.0 * Dt, Watch.bFlight ? "in a flight" : "by his hand",
									Place, Zoomed, Turned, Tilted, Gen.ViewAt());
							}
						}
						Watch.Number = -1;
					}
				}
				if (bTake)
				{
					// (taken: the flight is over this frame)
					++HandTook;
					if (Gen.FlyingTo() >= 0) ++NotTaken;
					// (a card take counts only where the unit is still on the field: a card for a block that is gone
					// tests nothing)
					double Cx = 0.0, Cy = 0.0;
					BlockTakes += In.GoBlock >= 0 && Gen.BlockAt(In.GoBlock, B, Cx, Cy) ? 1 : 0;
					// (taken with his own tilt on, on the way to a view tilted far otherwise: the tilt's own case.
					// And taken once his own tilt had sunk to the automatic one, with none of it on the picture:
					// the other way about, the view must go back to tilting with the zoom)
					const bool bTiltTake = bOwnBefore && std::abs(Kept[FlyBefore].Tilt - TiltBefore) > 10.0;
					const bool bTiltHidden = bOwnBefore && TiltBefore - AutoTilt(SBefore) < 0.3;
					TiltTakes += bTiltTake ? 1 : 0;
					if (TakenUntil < 0.0)
					{
						TakenUntil = R + 1.5;
						TakenSpeed = SpeedBefore;
						TakenAt = LaBefore;
						bTakenPanned = false;
						bTakenToBlock = In.GoBlock >= 0;
						bTakenTop = false;
						bTakenOwn = bTiltTake;
						bTakenHidden = bTiltHidden;
						TakenPanMost = 0.0;
						TakenH = HBefore;
						TakenS = OwnS = SBefore;
						TakenTilt = TiltBefore;
						TakenHRate = Wrapped((HBefore - HWas) * 180.0 / Pi) / Dt;
						TakenSRate = (SBefore - SWas) / Dt;
						TakenTiltRate = (TiltBefore - TiltWas) / Dt;
						OwnTurn = OwnTiltIn = 0.0;
						LastTurnIn = LastZoomIn = LastTiltIn = -1e9;
					}
				}
				if (TakenUntil >= 0.0)
				{
					const LandGeneralEye::Settings Rule;
					bTakenPanned = bTakenPanned || bPanIn;
					bTakenTop = bTakenTop || In.bTopView;
					// (what his own hand has asked of the turn, the zoom and the tilt since he took it)
					if (In.Turn != 0.0) { OwnTurn += In.Turn; LastTurnIn = R; }
					if (In.Zoom != 0.0) { OwnS = std::clamp(OwnS - In.Zoom * Rule.ZoomStep, 0.0, 1.0); LastZoomIn = R; }
					if (In.Tilt != 0.0) { OwnTiltIn += In.Tilt; LastTiltIn = R; }
					const double Speed = std::hypot(LaNow.X - LaBefore.X, LaNow.Y - LaBefore.Y) / Dt, Arm = (P.Look - P.Eye).Length();
					// (no faster than it was going as he took it, or than his own pan goes if that is faster: a look-at let
					// go dies away as his pan's own speed comes up, and the two never add; not where he sent it to a unit)
					TakenPanMost = std::max(TakenPanMost, std::hypot(std::clamp(In.PanRight + In.EdgeRight, -1.0, 1.0), std::clamp(In.PanAhead + In.EdgeAhead, -1.0, 1.0)));
					const double Over = Speed - (std::max(TakenSpeed, bTakenPanned ? std::max(1.0, TakenPanMost) * (0.75 * Arm + 4.0) : 0.0) + std::max(2.0, 0.05 * TakenSpeed));
					if (Over > 0.0 && !bTakenToBlock)
					{
						++Rushes;
						WorstRush = std::max(WorstRush, Over);
					}
					if (R >= TakenUntil)
					{
						// (taken by a zoom, a turn or a tilt alone: at rest about where a glide let go comes to rest)
						const double Ran = std::hypot(LaNow.X - TakenAt.X, LaNow.Y - TakenAt.Y) - (TakenSpeed / Rule.Glide * 1.15 + 3.0);
						if (!bTakenPanned && !bTakenToBlock && Ran > 0.0)
						{
							++RunsOn;
							WorstRunOn = std::max(WorstRunOn, Ran);
						}
						// (the turn, the zoom and his tilt: each no further on than its own push stops it from the rate it
						// had, and what his own hand has asked since. Not one his hand was still moving in the last second;
						// not the zoom or the tilt once the top view is pressed; the zoom no nearer than a followed unit
						// lets it; the tilt only where it was his own and the kept view's far from it, out of the reach of
						// the shoulder's handing back)
						const double TurnOn = std::abs(Wrapped((Gen.Heading() - TakenH) * 180.0 / Pi - OwnTurn)) - (TakenHRate * TakenHRate / (2.0 * Rule.TurnPush) + 3.0);
						const double ZoomWant = bTakenToBlock ? std::max(OwnS, Rule.FollowNearest) : OwnS;
						const double ZoomOn = std::abs(Gen.Zoom() - ZoomWant) - (TakenSRate * TakenSRate / (2.0 * Rule.ZoomPush * Rule.FlySlower) + 0.02);
						const double TiltOn = std::abs(Gen.TiltDegrees() - TakenTilt - OwnTiltIn) - (TakenTiltRate * TakenTiltRate / (2.0 * Rule.TiltPush * Rule.FlySlower) + 3.0);
						// (taken by a unit's card: the look-at went to the unit and follows it, not on to the kept view)
						if (bTakenToBlock)
						{
							double Ux = 0.0, Uy = 0.0;
							if (Gen.BlockAt(TankBlock, B, Ux, Uy) && (Gen.Following() != TankBlock || std::hypot(LaNow.X - Ux, LaNow.Y - Uy) > 0.5 * std::hypot(TakenAt.X - Ux, TakenAt.Y - Uy) + 5.0))
							{
								Fault("a flight taken by a unit's card did not go to the unit");
							}
						}
						// (his tilt judged where some of it showed on the picture as he took it: it stays his own and
						// stops where its push allows. Where none of it showed, the other way about: it is handed
						// back and the view tilts with the zoom again)
						const bool bTiltJudged = bTakenOwn && !bTakenHidden && !bTakenTop && R - LastTiltIn > 1.0 && TakenS > 0.25 && Gen.Zoom() > 0.25;
						if (bTakenHidden && !bTakenTop && R - LastTiltIn > 1.0 && Gen.Zoom() > 0.25)
						{
							++HandBacks;
							if (Gen.OwnTilt() || std::abs(Gen.TiltDegrees() - AutoTilt(Gen.Zoom())) > 1.0)
							{
								++HandBacksWrong;
								std::printf("  a flight taken at real %.2f once his own tilt had sunk to the automatic one: a second and a half on, his own tilt is %s and the view is tilted %.2f degrees off the automatic one\n", TakenUntil - 1.5,
									Gen.OwnTilt() ? "still on" : "off", Gen.TiltDegrees() - AutoTilt(Gen.Zoom()));
							}
						}
						TiltTakesJudged += bTiltJudged ? 1 : 0;
						const bool bTurnOn = R - LastTurnIn > 1.0 && TurnOn > 0.0, bZoomOn = !bTakenTop && R - LastZoomIn > 1.0 && ZoomOn > 0.0, bTiltOn = bTiltJudged && TiltOn > 0.0;
						if (bTurnOn || bZoomOn || bTiltOn)
						{
							++FlewOn;
							WorstFlewOn = std::max(WorstFlewOn, std::max(bTurnOn ? TurnOn : 0.0, std::max(bZoomOn ? ZoomOn * 100.0 : 0.0, bTiltOn ? TiltOn : 0.0)));
							std::printf("  a flight taken at real %.2f: the view flew on past where it was let go: turn %.1f degrees%s, zoom %.3f%s, tilt %.1f degrees%s over what its own push allows\n", TakenUntil - 1.5,
								TurnOn, bTurnOn ? " (FAULT)" : "", ZoomOn, bZoomOn ? " (FAULT)" : "", TiltOn, bTiltOn ? " (FAULT)" : "");
						}
						TakenUntil = -1.0;
					}
				}
				// (a called view reached and left alone: a second and a half on the flight is over, the turn and the zoom
				// are free again, and its chip is lit; not where a hedge has made the view wait since)
				if (ReachedView >= 0 && R >= ReachedAt + 1.5)
				{
					if (Gen.BrakedFrames() == ReachedBraked && Gen.WaitedFrames() == ReachedWaited)
					{
						++EndsChecked;
						if (Gen.FlyingTo() >= 0 || Gen.Flying() || Gen.ViewAt() != ReachedView)
						{
							++EndsWrong;
							std::printf("  view %d reached at real %.2f: a second and a half on, flying to %d, turn and zoom %s, at view %d\n", ReachedView, ReachedAt, Gen.FlyingTo(), Gen.Flying() ? "held" : "free", Gen.ViewAt());
						}
					}
					ReachedView = -1;
				}
				// (his hand has tilted the view away from the view whose chip was lit, a second and more ago: the chip is
				// out. The first time from a view kept without his own tilt; the second from one kept with it, his tilt
				// still his own, which only the tilt's own angle can tell)
				if (Once(233.0) || Once(236.5))
				{
					const bool bSecond = R > 235.0;
					const int Want = bSecond ? 3 : 1;
					if (ChipWas == Want && std::abs(Gen.TiltDegrees() - Kept[Want].Tilt) > 8.0 && (!bSecond || Gen.OwnTilt()))
					{
						++TiltChips;
						TiltChipsOwn += bSecond ? 1 : 0;
						if (Gen.ViewAt() == Want)
						{
							++TiltChipLit;
							std::printf("  chip %d still lit at real %.2f, his hand having tilted the view %.1f degrees away from it\n", Want, R, Gen.TiltDegrees() - Kept[Want].Tilt);
						}
					}
					else std::printf("  the tilt's chip check at real %.2f was not made: chip %d was lit (wanted %d), tilt %.1f degrees off, his own tilt %d\n", R, ChipWas, Want, Gen.TiltDegrees() - Kept[Want].Tilt, Gen.OwnTilt() ? 1 : 0);
				}
				// (a flight to a kept view over by itself, no hand on it and no keep, be it in the very frame of its call:
				// the view is there, in where it looks, its turn, its zoom and its tilt. The flights called at 237 and
				// 241 have only the tilt to travel: his own to the view's, then back to the automatic one)
				if (const int Ended = !bHandIn && Keeping < 0 && Gen.FlyingTo() < 0 ? (In.CallView >= 0 ? In.CallView : FlyBefore) : -1; Ended >= 0)
				{
					const KeptAs& K = Kept[Ended];
					const double Place = std::hypot(LaNow.X - K.X, LaNow.Y - K.Y), Zoomed = std::abs(Gen.Zoom() - K.S), Turned = std::abs(Wrapped((Gen.Heading() - K.H) * 180.0 / Pi)), Tilted = std::abs(Gen.TiltDegrees() - K.Tilt);
					const bool bTiltOnly = R >= 237.0 && R < 246.0;
					++OwnEnds;
					TiltEnds += bTiltOnly ? 1 : 0;
					TiltEndsAuto += bTiltOnly && !Gen.OwnTilt() ? 1 : 0;
					TiltEndsWrong += bTiltOnly && Tilted > 2.0 ? 1 : 0;
					if (Place > 1.6 || Zoomed > 0.006 || Turned > 1.0 || Tilted > 2.0)
					{
						++OwnEndsWrong;
						std::printf("  the flight to view %d was over at real %.2f with the view not there: %.1f m, zoom %.3f, turn %.1f degrees, tilt %.1f degrees off\n", Ended, R, Place, Zoomed, Turned, Tilted);
					}
				}
				// (the pointer at the edge for a second once a flight is over: the look-at has gone with it)
				if (Once(201.0))
				{
					++EdgeChecks;
					if (std::hypot(LaNow.X - EdgeFrom.X, LaNow.Y - EdgeFrom.Y) < 5.0) ++EdgeDead;
				}
				// (the turn and the zoom held, with no flight on and nothing asked of them, and no hedge braked at: they
				// should long have gone back to their own easing)
				const bool bHeldIdle = Gen.Flying() && Gen.FlyingTo() < 0 && In.Zoom == 0.0 && In.Turn == 0.0 && In.Tilt == 0.0 && !In.bTopView && In.GoBlock < 0 && Gen.BrakedFrames() == BrakedWas && Gen.WaitedFrames() == WaitedWas;
				HeldFor = bHeldIdle ? HeldFor + Dt : 0.0;
				HeldLong += HeldFor > 3.0 ? 1 : 0;
				BrakedWas = Gen.BrakedFrames();
				WaitedWas = Gen.WaitedFrames();
				// (a chip lit, no flight on: the view is at that view, or coming to rest there within a second or two)
				if (Gen.ViewAt() >= 0 && Gen.FlyingTo() < 0)
				{
					const KeptAs& K = Kept[Gen.ViewAt()];
					LitFor += Dt;
					if (LitFor > 2.5 && (std::hypot(LaNow.X - K.X, LaNow.Y - K.Y) > 5.0 || std::abs(Gen.Zoom() - K.S) > 0.03 || std::abs(Wrapped((Gen.Heading() - K.H) * 180.0 / Pi)) > 3.0
						|| std::abs(Gen.TiltDegrees() - K.Tilt) > 3.0))
					{
						++LitAway;
						if (std::getenv("LAND_PLAY_VIEWS") && LitAway < 20) std::printf("  chip %d lit away from its view at real %.2f: %.1f m, zoom %.3f, tilt %.1f off, following %d\n", Gen.ViewAt(), R, std::hypot(LaNow.X - K.X, LaNow.Y - K.Y), Gen.Zoom() - K.S,
							Gen.TiltDegrees() - K.Tilt, Gen.Following());
					}
				}
				else LitFor = 0.0;
				if (R < FirstPressUntil)
				{
					FirstPressMoved = std::max(FirstPressMoved, std::hypot(LaNow.X - FirstPressAt.X, LaNow.Y - FirstPressAt.Y));
				}
				if (Called >= 0)
				{
					const KeptAs& K = Kept[Called];
					const Vec La = Gen.LookAt();
					double Turned = (Gen.Heading() - K.H) * 180.0 / 3.14159265358979;
					while (Turned > 180.0) Turned -= 360.0;
					while (Turned < -180.0) Turned += 360.0;
					const double Place = std::hypot(La.X - K.X, La.Y - K.Y), Zoomed = std::abs(Gen.Zoom() - K.S), Tilted = std::abs(Gen.TiltDegrees() - K.Tilt);
					const bool bThere = Place < 1.5 && Zoomed < 0.01 && std::abs(Turned) < 1.0 && Tilted < 1.5;
					if (std::getenv("LAND_PLAY_VIEWS") && static_cast<int>((R + Dt) * 2.0) != static_cast<int>(R * 2.0))
					{
						std::printf("  view %d called at %.1f, now %.2f: %.1f m off, zoom %.3f off, turn %+.1f off, tilt %.1f off, flying %d at %d own %d top %d\n", Called, CalledAt, R + Dt, Place, Zoomed, Turned, Tilted,
							Gen.Flying() ? 1 : 0, Gen.ViewAt(), Gen.OwnTilt() ? 1 : 0, Gen.TopView() ? 1 : 0);
					}
					if (bThere || R >= CheckAt)
					{
						if (bThere)
						{
							++Reached;
							StoodReached += bCalledStands ? 1 : 0;
							LongestFlight = std::max(LongestFlight, R - CalledAt);
							ReachedView = Called;
							ReachedAt = R;
							ReachedBraked = Gen.BrakedFrames();
							ReachedWaited = Gen.WaitedFrames();
						}
						else
						{
							MissPlace = std::max(MissPlace, Place);
							MissZoom = std::max(MissZoom, Zoomed);
							MissTurn = std::max(MissTurn, std::abs(Turned));
							MissTilt = std::max(MissTilt, Tilted);
							std::printf("  view %d called at real %.1f not reached by %.1f: %.1f m off, zoom %.3f off, turn %+.1f off, tilt %.1f off (waited %d, braked %d)\n", Called, CalledAt, R, Place, Zoomed, Turned, Tilted,
								Gen.WaitedFrames(), Gen.BrakedFrames());
						}
						Called = -1;
					}
				}
			}
			if (bTiltRun)
			{
				TopPresses += In.bTopView ? 1 : 0;
				TopFrames += Gen.TopView() ? 1 : 0;
				OwnFrames += Gen.OwnTilt() ? 1 : 0;
				if (Gen.TopView() && R > 13.0 && R < 18.0)
				{
					TopTiltBest = std::max(TopTiltBest, Gen.TiltDegrees());
					TopUpBest = std::max(TopUpBest, P.Eye.Z - P.Look.Z);
				}
				if (R > 45.0 && R < 45.0 + Dt) KeptTilt = Gen.TiltDegrees();
				// (a second and a half after the zoom came right in)
				if (R >= 25.9 && R < 26.0) NearTilt = std::max(NearTilt, Gen.TiltDegrees());
				if (BackToAuto < 0 && R > 46.0 && !Gen.OwnTilt()) BackToAuto = static_cast<int>(R * 10.0);
				if (std::getenv("LAND_PLAY_TILT") && static_cast<int>((R + Dt) * 4.0) != static_cast<int>(R * 4.0))
				{
					std::printf("  tilt at %.2f: zoom %.3f tilt %.1f own %d top %d, eye %.1f over the look-at, %.1f over the ground\n", R + Dt, Gen.Zoom(), Gen.TiltDegrees(), Gen.OwnTilt() ? 1 : 0, Gen.TopView() ? 1 : 0, P.Eye.Z - P.Look.Z, P.Eye.Z - GroundAt(P.Eye.X, P.Eye.Y));
				}
			}
			LaWas = LaBefore;
			SWas = SBefore;
			HWas = HBefore;
			TiltWas = TiltBefore;
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
			// (Free is the scene's ground-level word, a house's floor or a wall's line at any height: asked, as the view
			// asks it, within FreeUnder of the ground; the eye 400 m over a farm's roof is not in it)
			const bool bFreeAsked = Over < LandGeneralEye::Settings().FreeUnder;
			if ((bFreeAsked && !EW.Free(P.Eye)) || EW.Solid(P.Eye, -1) != 0) { ++InThing; if (std::getenv("LAND_PLAY_GEN")) std::printf("  general's eye in a thing at real %.2f: free %d solid %d, zoom %.3f, eye %.2f over the ground, following %d\n", R, EW.Free(P.Eye) ? 1 : 0, EW.Solid(P.Eye, -1), Gen.Zoom(), Over, Gen.Following()); }
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
		std::printf("\ngeneral's view%s: %d frames over %.0f s; eye under the ground %d, in a thing %d, among the stalks %d, look-at off the field %d; eye %.2f m over the ground at the lowest (%.2f at the nearest zoom), %.0f m over the look-at at the highest; followed a block %.0f s; worst look kick %.0f deg/s/s (real %.1f s), %d frames over 1200; the view's worst rise or fall %.0f deg/s/s as the picture sees it (real %.1f s), %d frames over 1200; waited at a thing %d frames, braked %d, cut %d times; %.3f ms a frame on average, %.2f ms the slowest",
			bTiltRun ? ", tilted by hand" : bViewRun ? ", kept views" : "", GFrames, End, Under, InThing, InCorn, OffField, Lowest, LowestNear < 1e8 ? LowestNear : 0.0, Highest, Followed * Dt, WorstKick, WorstKickAt, Kicks, WorstRise, WorstRiseAt, Lifted, Gen.WaitedFrames(), Gen.BrakedFrames(), GCuts, Spent / std::max(1, GFrames), GSlowest);
		if (Under > 0) Fault("the general's view went under the ground");
		if (InThing > 0) Fault("the general's view stood in a thing or a man");
		if (InCorn > 0) Fault("the general's view stood among the stalks");
		if (OffField > 0) Fault("the general's view looked off the field past its margin");
		if (Kicks > 0) Fault("the general's view jolted (a look kick over 1200 deg/s/s)");
		if (Lifted > 0) Fault("the general's view jolted up or down (over 1200 deg/s/s as the picture sees it)");
		if (LowestNear > 2.5 && !bTiltRun && !bViewRun) Fault("the general's view never came down to a man's shoulder");
		if (Highest < 300.0) Fault("the general's view never rose to look down on the battle");
		if (GSlowest > 8.0) Fault("a frame of the general's view took over 8 ms");
		if (bViewRun)
		{
			std::printf("\ngeneral's view, kept views: %d kept, %d called, %d reached as kept before his hand took the view again (the longest flight %.1f s); the worst miss %.1f m, zoom %.3f, turn %.1f degrees, tilt %.1f degrees; a first press moved the view %.2f m; flying %.0f s",
				Keeps, Calls, Reached, LongestFlight, MissPlace, MissZoom, MissTurn, MissTilt, FirstPressMoved, FlyFrames * Dt);
			std::printf("\ngeneral's view, kept views: %d kept at rest (%d not as they stood); %d left alone came to rest (%d not as kept, or unlit); a keep moved the view on %d frames and ended a follow on %d; his hand took %d flights (%d not let go; the look-at rushed on for %d frames, %.0f m/s over at the worst; %d ran on past where a glide rests, %.0f m at the worst); %d of %d calls stood through a slide running on, a key held and the pointer at the edge; the turn and the zoom stayed held %d frames past 3 s at rest; a chip was lit away from its view on %d frames",
				KeptAtRest, KeptWrong, KeepsSettled, SettledWrong, KeepMoved, KeepEndedFollow, HandTook, NotTaken, Rushes, WorstRush, RunsOn, WorstRunOn, StoodReached, Stood, HeldLong, LitAway);
			if (MissPlace > 0.0 || MissZoom > 0.0 || MissTurn > 0.0 || MissTilt > 0.0) Fault("a kept view called back was not reached as it was kept");
			if (Reached < 5) Fault("the general's kept views were hardly called (under 5 reached)");
			if (FirstPressMoved > 0.5) Fault("the first press of a view never kept moved the view");
			if (!Gen.ViewKept(0) || !Gen.ViewKept(1) || !Gen.ViewKept(2) || !Gen.ViewKept(3)) Fault("a kept view was lost");
			if (KeptWrong > 0) Fault("a view kept at rest was not kept as it stood");
			if (KeptAtRest < 1 || KeepsSettled < 2) Fault("the keeping of a view at rest and still coasting was hardly tested");
			if (SettledWrong > 0) Fault("a kept view was not where the view came to rest, or its chip was not lit there");
			if (KeepMoved > 0) Fault("keeping a view moved the view");
			if (KeepEndedFollow > 0) Fault("keeping a view ended the follow of a unit");
			if (HandTook < 2) Fault("a flight taken by his own hand was hardly tested (under 2)");
			if (NotTaken > 0) Fault("a move by his own hand begun in a flight did not take the view");
			if (Rushes > 0) Fault("a flight taken by his own hand rushed on toward the kept view");
			if (RunsOn > 0) Fault("a flight taken by his own hand did not come to rest where it was let go");
			if (Stood < 3 || StoodReached < Stood) Fault("a call was lost to a slide still running on, a key still held or the pointer at the screen's edge");
			if (HeldLong > 0) Fault("the view's turn and zoom stayed held long after they had come to rest");
			if (LitAway > 0) Fault("a kept view's chip stayed lit with the view away from it");
			std::printf("\ngeneral's view, kept views: of the flights his hand took, %d by a unit's card and %d with his own tilt on (%d judged); the turn, the zoom or the tilt flew on past its own push %d times (%.1f over at the worst, degrees or hundredths of the zoom); %d reached calls left alone were over a second and a half on (%d not, or their chip unlit); the edge panned again after a flight %d of %d times; %d kept in a flight (%d not as they stood), %d following a unit (%d not where it looked; %d left alone came to rest, %d not as kept)",
				BlockTakes, TiltTakes, TiltTakesJudged, FlewOn, WorstFlewOn, EndsChecked, EndsWrong, EdgeChecks - EdgeDead, EdgeChecks, FlightKeeps, FlightKeptWrong, FollowKeeps, FollowKeptWrong, FollowSettled, FollowSettledWrong);
			if (FlewOn > 0) Fault("a flight taken by his own hand flew on in its turn, its zoom or its tilt");
			if (BlockTakes < 1 && TankBlock >= 0) Fault("a flight taken by a unit's card was not tested");
			if (TiltTakesJudged < 3) Fault("a flight taken with his own tilt on was hardly tested (under 3: the call that turns it back, two calls on each other's heels, and his own tilt to a view's own tilt)");
			if (EndsChecked < 3) Fault("the end of a called flight was hardly tested (under 3)");
			if (EndsWrong > 0) Fault("a called view was reached but the flight was not over, or its chip not lit");
			if (EdgeChecks < 1 || EdgeDead > 0) Fault("the pointer at the screen's edge did not pan again once a flight was over");
			if (FlightKeeps < 2) Fault("the keeping of a view in the middle of a flight was hardly tested (under 2)");
			if (FlightKeptWrong > 0) Fault("a view kept in the middle of a flight was not kept as it stood");
			if (FollowKeeps < 2 || (FollowSettled < 1 && TankBlock >= 0)) Fault("the keeping of a view while following a unit was hardly tested");
			if (FollowKeptWrong > 0) Fault("a view kept while following a unit was not kept where it looked");
			if (FollowSettledWrong > 0) Fault("a view kept while following a unit was not as the view came to rest");
			std::printf("\ngeneral's view, kept views: his hand tilted the view away from a lit chip's view %d times (%d of them with his tilt his own before and after) and the chip stayed lit %d times; %d flights were over by themselves, %d with the view not there; of them a called flight with only the tilt to travel %d times (%d back at the automatic tilt), %d before the tilt was there",
				TiltChips, TiltChipsOwn, TiltChipLit, OwnEnds, OwnEndsWrong, TiltEnds, TiltEndsAuto, TiltEndsWrong);
			if (TiltChips < 2 || TiltChipsOwn < 1) Fault("a chip's going out when his hand tilts the view was not tested (twice, once with his tilt his own throughout)");
			if (TiltChipLit > 0) Fault("a chip stayed lit after his own hand tilted the view away from its view");
			if (TiltEnds < 2 || TiltEndsAuto < 1) Fault("a called flight with only the tilt to travel was not tested (to his own tilt and back to the automatic one)");
			if (TiltEndsWrong > 0) Fault("a called flight was over before the tilt of its view was reached");
			if (OwnEndsWrong > TiltEndsWrong) Fault("a called flight was over, by itself, with the view not at its view");
			std::printf("\ngeneral's view, kept views: %d flights taken and %d views kept once his own tilt had sunk to the automatic one with none of it on the picture (%d and %d left his own tilt on, or the view not tilting with the zoom); the block his cards and follows rested on %d",
				HandBacks, KeepBacks, HandBacksWrong, KeepBacksWrong, TankBlock);
			if (HandBacks < 1 || KeepBacks < 1) Fault("a flight taken, or a view kept, once his own tilt had sunk to the automatic one was not tested");
			if (HandBacksWrong > 0 || KeepBacksWrong > 0) Fault("a flight taken, or a view kept, once his own tilt had sunk to the automatic one left it his own, unseen");
			if (TankBlock < 0) std::printf("\n  (no block of his own lived through the scripted steps on this recording: the card press and the follows were left out)");
		}
		if (bTiltRun)
		{
			std::printf("\ngeneral's view, tilted by hand: the top view looked down %.1f degrees from %.0f m over the look-at; his tilt %.1f degrees after the zoom to the shoulder and out; at the nearest zoom tilted %.1f degrees once settled; back to tilting with the zoom at %.1f s; top view on %.0f s (%d presses), his own tilt %.0f s",
				TopTiltBest, TopUpBest, KeptTilt, NearTilt, BackToAuto * 0.1, TopFrames * Dt, TopPresses, OwnFrames * Dt);
			// (his tilt handed back toward the shoulder: zoomed right in, the view looks along the ground as it always has)
			if (NearTilt > LandGeneralEye::Settings().TiltNear + 1.0) Fault("the general's own tilt was not handed back to the automatic one at the nearest zoom");
			if (TopTiltBest < 79.0 || TopUpBest < 95.0) Fault("the general's top view did not look almost straight down from about 100 m");
			if (KeptTilt < 84.0) Fault("the general's own tilt was not kept through the zoom to the shoulder and out");
			if (BackToAuto < 0 || BackToAuto > 560) Fault("the general's view did not go back to tilting with the zoom once tilted all the way back");
		}
	}
	if (bOnlyGen)
	{
		std::printf("\n(the general's view alone)\n");
		return 0;
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
	// The platoon cam (Brian's yes, 22 November 02:40Z), on a recording with the lead marks (the tactics thread's -cam
	// recordings): for each side, the film rides with the platoon leading its assault, goes on to the next as the lead
	// passes, and is with the leading squad's men much of the time; no aerial in the middle of the battle.
	{
		bool bMarks = false;
		for (const LandFeed::Track& Tr : Rec.Tracks) bMarks = bMarks || !Tr.Lead.empty();
		for (int Side = 0; Side < 2 && bMarks; ++Side)
		{
			auto Leads = [&Rec, Side](const LandFeed::Track& Tr, double T)
			{
				const LandFeed::Block* Bk = Rec.BlockOf(Tr.Block);
				if (!Bk || Bk->Side != Side || Tr.bTwin || (Tr.Fell >= 0.0 && Tr.Fell <= T) || (Tr.Out >= 0.0 && Tr.Out <= T)) return false;
				for (const LandFeed::Track::Span& Sp : Tr.Lead)
					if (T >= Sp.From && T <= Sp.To) return true;
				return false;
			};
			LandEye::Director Ld;
			Ld.Begin(Rec, EW, ES);
			Ld.FollowLead(Side);
			int LFrames = 0, OnPlatoon = 0, OnLeaders = 0, Switches = 0, Aerials = 0, Cuts = 0, LeadFrames = 0;
			double FirstLead = -1.0, SwitchAt = -1.0, SlowestTo = 0.0, SlowestToAt = 0.0;
			int LastBlock = -1;
			const double From = std::max(0.0, Contact - 20.0);
			for (double B = From; B < Rec.End; B += Dt)
			{
				PlaceMovers(B);
				const double R = 3000.0 + (B - From);
				const LandEye::Pose P = Ld.Update(B, R, Dt);
				Cuts += P.bCut ? 1 : 0;
				if (P.bCut && P.Shot == LandEye::Kind::Wide && B > 12.0 && B < Rec.End && !Ld.MassShot()) ++Aerials;
				const int Blk = Ld.LeadBlock();
				if (Blk < 0) continue;
				if (FirstLead < 0.0) FirstLead = B;
				if (Blk != LastBlock) { ++Switches; LastBlock = Blk; SwitchAt = R; }
				const bool bOnIt = P.Subject >= 0 && Rec.Tracks[static_cast<size_t>(P.Subject)].Block == Blk;
				if (SwitchAt >= 0.0 && bOnIt)
				{
					if (R - SwitchAt > SlowestTo) { SlowestTo = R - SwitchAt; SlowestToAt = B; }
					SwitchAt = -1.0;
				}
				// (counted from a second after the lead passes: the cut to the new platoon takes its frame or few)
				if (SwitchAt >= 0.0 && R - SwitchAt < 1.0) continue;
				++LFrames;
				OnPlatoon += bOnIt ? 1 : 0;
				if (P.Subject >= 0 && Leads(Rec.Tracks[static_cast<size_t>(P.Subject)], B)) ++OnLeaders;
				bool bAnyLead = false;
				for (const LandFeed::Track& Tr : Rec.Tracks) if (Tr.Block == Blk && Leads(Tr, B)) { bAnyLead = true; break; }
				LeadFrames += bAnyLead ? 1 : 0;
			}
			const double OnShare = 100.0 * OnPlatoon / std::max(1, LFrames);
			std::printf("platoon cam, side %d: the lead first taken at %.0f s, %d platoons in turn, on the new one within %.2f s at the most (battle %.0f); %.1f%% of the frames on the leading platoon's men, %.1f%% on the leading squad's own (they lead in %.0f%% of the frames), %d cuts, %d aerials\n",
				Side, FirstLead, Switches, SlowestTo, SlowestToAt, OnShare, 100.0 * OnLeaders / std::max(1, LFrames), 100.0 * LeadFrames / std::max(1, LFrames), Cuts, Aerials);
			if (FirstLead < 0.0) Fault("the platoon cam never found a platoon leading the assault");
			if (LFrames > 0 && OnShare < 90.0) Fault("the platoon cam strayed from the leading platoon");
			if (SlowestTo > 2.0) Fault("the platoon cam was slow to the next platoon to lead");
			if (Aerials > 0) Fault("the platoon cam went up to an aerial in the middle of the battle");
		}
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
		// The rule's rest, under lights the stand-in above never gives (no recording is fought at dusk): a reading that
		// is the same whatever the lens looks at, as a dusk's or a storm's is. Dark everywhere, the rule leaves its
		// DarkRunMost shots and then rests for the battle: the light is to blame. (Until 25 November its run started
		// again at every ordinary cut, and this battle's film left 90 shots for a dark picture, 138 cuts against 56:
		// two shots of three quarters of a second after nearly every ordinary cut. The scene's reading was blind then,
		// so no battle showed it.) Hovering about the leaving line, it rests as well: only a picture read plainly
		// light, DarkLight for DarkHold seconds on end, starts the run again. And where the light does come back and
		// goes again, the rule works again.
		// (Light: the reading at a screen second, of a shot so many screen seconds old, of a picture the rule judges or
		// of an opening's or a wide's, which it does not. Asked for every Every screen seconds, 0 every frame, the first
		// at Phase; the director has it Lag screen seconds after it was asked for and holds it until the next, as the
		// scene's does. The clocks are the scene's, as the film's other seeds are played above: the battle's runs at
		// twice the screen's until ten seconds before contact and at half of it after, and every time of the rule's and
		// of these lights is the screen's. On one clock for both, as these were played until the ninth check, a hold
		// timed by the battle's seconds read the same as one timed by the screen's: in the scene's approach it would
		// have been half as long.)
		auto LeftUnder = [&](const std::function<double(double, double, bool)>& Light, double Every, double Lag = 0.0, double Phase = 0.0) -> int
		{
			LandEye::World Wl = EW;
			double Read = -1.0, Asked = -1.0, BackAt = 1e18;
			Wl.DarkNow = [&Read]() { return Read; };
			LandEye::Director Dl;
			Dl.Begin(Rec, Wl, ES);
			double B = 0.0, R = 0.0, Next = Phase, CutAt = 0.0;
			bool bJudged = false;
			while (B < Rec.End + 3.0)
			{
				if (R >= BackAt)
				{
					Read = Asked;
					BackAt = 1e18;
				}
				if (R >= Next)
				{
					Next = R + Every;
					Asked = Light(R, R - CutAt, bJudged);
					if (Lag > 0.0) BackAt = R + Lag;
					else Read = Asked;
				}
				B += Dt * (B < Contact - 10.0 ? 2.0 : 0.5);
				R += Dt;
				PlaceMovers(B);
				const LandEye::Pose P = Dl.Update(B, R, Dt);
				if (P.bCut) CutAt = R;
				bJudged = P.Shot != LandEye::Kind::Open && P.Shot != LandEye::Kind::Wide;
			}
			return Dl.DarkCuts();
		};
		const int LeftDusk = LeftUnder([](double, double, bool) { return 0.6; }, 0.25);
		const int LeftHover = LeftUnder([&ES](double R, double, bool) { return std::fmod(R, 4.0) < 2.0 ? ES.DarkLeave + 0.06 : ES.DarkLeave - 0.06; }, 0.25);
		// (the light come back reads nothing dark at all, as an open field does: a reading of 0 is a reading, and light)
		const int LeftBack = LeftUnder([](double R, double, bool) { return R >= 30.0 && R < 45.0 ? 0.0 : 0.6; }, 0.25);
		// (no reading for a second in every six, as under a card: no reading is not a light one)
		const int LeftGaps = LeftUnder([](double R, double, bool) { return std::fmod(R, 6.0) < 1.0 ? -1.0 : 0.6; }, 0.25);
		// (the frame drawn lags the cut, and on a slow machine by more than the scene's quarter second: the last shot's
		// light picture, still the reading six tenths of a second into the new shot, is not this shot's. The light is
		// counted from the shot's first quarter second, so that is 0.35 s of it, short of DarkHold.)
		const int LeftLag = LeftUnder([](double, double Held, bool) { return Held < 0.6 ? 0.05 : 0.6; }, 0.0);
		// (one light reading in twenty, a quarter of a second of a shell's flash: a flash is not the light come back)
		const int LeftFlash = LeftUnder([](double R, double, bool) { return std::fmod(R, 5.0) < 0.25 ? 0.05 : 0.6; }, 0.25);
		// (as the scene reads: a frame asked for four times a second, back a twentieth or three twentieths of a second
		// later and held until the next. An opening's and a wide's pictures light, as from high over the smoke, every
		// other dark: the wide's light picture, asked for just before the cut, is still the reading a quarter of a
		// second into the next shot. The worst of four phases of the asking and the two lags.)
		int LeftWide = 0;
		for (int K = 0; K < 8; ++K)
		{
			LeftWide = std::max(LeftWide, LeftUnder([](double, double, bool bJudged) { return bJudged ? 0.6 : 0.05; }, 0.25, K < 4 ? 0.05 : 0.15, 0.0625 * (K % 4)));
		}
		// (the hold's own length, from both sides, every frame read: a shot light for a twentieth of a second short of
		// its first quarter second and DarkHold, dark after, has not shown the light come back; one light for a tenth
		// longer than them has, in every shot, so the rule never rests: the hold is DarkHold and no longer)
		const int LeftShort = LeftUnder([&ES](double, double Held, bool) { return Held < 0.25 + ES.DarkHold - 0.05 ? 0.05 : 0.6; }, 0.0);
		const int LeftLong = LeftUnder([&ES](double, double Held, bool) { return Held < 0.25 + ES.DarkHold + 0.1 ? 0.05 : 0.6; }, 0.0);
		// (and the dark's own hold, timed on the screen as the light's is: a shot dark for its first six tenths of a
		// second and light after has been dark 0.35 s of the rule's counting, short of DarkHold, and is not left)
		const int LeftBrief = LeftUnder([](double, double Held, bool) { return Held < 0.6 ? 0.6 : 0.05; }, 0.0);
		// (and the leaving line's own height: a picture that stays a little under it all battle, far over the light's
		// line, is no dark picture, and no shot is left for it)
		const int LeftUnderLine = LeftUnder([&ES](double, double, bool) { return ES.DarkLeave - 0.06; }, 0.25);
		std::printf("dark (the rule's rest, the same reading whatever the lens looks at): dark everywhere %d shots left; hovering about the leaving line %d; dark, light from 30 to 45 s, dark again %d; dark with no reading a second in six %d; dark but for each shot's first six tenths of a second %d; dark but for one reading in twenty %d; dark but for the openings' and the wides' pictures, read late as the scene reads, %d at the most; dark but for each shot's first %.2f s %d, its first %.2f s %d; light but for each shot's first six tenths of a second %d; a little under the leaving line all battle %d (it rests at %d running until a picture reads %.0f%% or lighter for %.2f s)\n",
			LeftDusk, LeftHover, LeftBack, LeftGaps, LeftLag, LeftFlash, LeftWide, 0.25 + ES.DarkHold - 0.05, LeftShort, 0.25 + ES.DarkHold + 0.1, LeftLong, LeftBrief, LeftUnderLine, ES.DarkRunMost, ES.DarkLight * 100.0, ES.DarkHold);
		if (LeftDusk < 1) Fault("under a light dark everywhere the dark rule left no shot at all (the rule is not working)");
		if (LeftDusk > ES.DarkRunMost) Fault("under a light dark everywhere the dark rule did not rest after its few shots (a dusk's film cut again and again)");
		if (LeftHover > ES.DarkRunMost) Fault("under a light hovering about the dark rule's line its run started again without a picture plainly light");
		if (LeftBack <= ES.DarkRunMost || LeftBack > 2 * ES.DarkRunMost) Fault("the dark rule did not work again once the light had come back and gone (or did not rest again after it)");
		if (LeftGaps > ES.DarkRunMost) Fault("the dark rule's run started again where there was no reading (no reading is not a light picture)");
		if (LeftLag > ES.DarkRunMost) Fault("the dark rule's run started again on a light reading that did not outlast a shot's first six tenths of a second (the last shot's picture come late, not its own)");
		if (LeftFlash > ES.DarkRunMost) Fault("the dark rule's run started again on one light reading (a flash is not the light come back)");
		if (LeftWide > ES.DarkRunMost) Fault("the dark rule's run started again on a wide's or an opening's light picture read late into the next shot (not that shot's own)");
		if (LeftShort > ES.DarkRunMost) Fault("the dark rule's run started again on a light that held less than DarkHold past a shot's first quarter second (the light's hold is shorter than it says, or is counted from the shot's start)");
		if (LeftLong <= 2 * ES.DarkRunMost) Fault("the dark rule's run did not start again in shots plainly light for longer than DarkHold past their first quarter second (the light's hold is longer than it says)");
		if (LeftBrief > 0) Fault("the dark rule left a shot dark for less than DarkHold past its first quarter second (the dark's hold is shorter than it says, timed by the battle's clock and not the screen's, or counted from the shot's start)");
		if (LeftUnderLine > 0) Fault("the dark rule left a shot whose picture stayed under its leaving line (the line is lower than it says)");
	}
	RunCorn();
	if (std::getenv("LAND_PLAY_STILLS")) RunStills();
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
		if (!Rec.Cuts.empty())
			std::printf("cutters: %d of %zu banks torn (%d with no bank by them), %d of the tanks then over the gap's floor; a breach laid in %.3f ms at most\n", CutsTornCount, Rec.Cuts.size(), CutsNoBank, CutsDrivenThrough, BreachMsMost);
		std::printf("holes: %d laid (", Holes.Count());
		for (auto& P : HolesBy) std::printf(" %s %d", P.first.c_str(), P.second);
		std::printf(" ), %zu points and %zu triangles in %d sections of %d patches (one mesh each); %.3f ms each on average, %.3f ms the most\n", Points, Tris, Sections, Holes.Patches(), HoleMs / std::max(1, Holes.Count()), HoleMsMost);
		std::printf("holes sent again in a frame: at most %zu points in %d meshes, the widest mesh %.1f m across\n", HoleSentMost, HoleSentMeshesMost, HoleMeshWidest);
		int DugCount = 0;
		for (const LandCrater::Field::Hole& H : All) DugCount += !H.Cut.empty();
		std::printf("dug: %d of the holes dug, %d cuts; %d tiles split; the ground cut in %.3f ms a frame at most (%.1f ms in all); at most %zu points in %d parts sent again in a frame\n",
			DugCount, Dug.Cuts(), DugSplit, DugMsMost, DugMs, DugSentMost, DugSentPartsMost);
		if (Holes.Count() > 20 && DugCount == 0) Fault("no shell hole was dug");
		if (Holes.Count() + ScarsLaid > LandCrater::Field::Most) Fault("scars: the old holes left the battle's own rounds no room (the guard reached)");
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
	// Radio call, then the barrage (Brian's release 2 ask): on a recording with the calls' spots (the tactics thread's
	// -cam recordings), the film goes to some callers, no more often than RadioEvery, and from each to the men under his
	// guns' first salvo, in the picture as it lands.
	if (!bOnlyGen)
	{
		const int Calls = Eye.RadioCalls(), Went = static_cast<int>(Eye.RadioBeats().size());
		std::printf("radio call, then the barrage: %d fresh calls for fire, the film went to %d of the callers (%.0f s apart at the least), %d salvos cut to or held for, %d of them in the picture as they landed\n",
			Calls, Went, RadioGapLeast < 1e8 ? RadioGapLeast : 0.0, RadioBarrages, RadioSeen);
		if (std::getenv("LAND_PLAY_RADIO"))
			for (const LandEye::Director::RadioBeat& Bt : Eye.RadioBeats()) std::printf("  radio: call at %.1f by %d, its salvo at %.1f, %s\n", Bt.T, Bt.Caller, Bt.Land, Bt.bBarrage ? "cut to or held for" : "MISSED");
		if (Calls >= 3 && Went == 0) Fault("the film went to none of the radio calls");
		if (RadioGapLeast < ES.RadioEvery - 0.5) Fault("the film went to radio calls more often than RadioEvery");
		if (Went > 0 && RadioBarrages * 2 < Went) Fault("most radio calls were not followed by their barrage");
		if (RadioBarrages > 0 && RadioSeen * 4 < RadioBarrages * 3) Fault("a radio call's barrage was cut to but its first salvo landed out of the picture");
	}
	// The first bail-out (the effects thread's ask, 23 November): with crews getting out, the film's first bail watch on a
	// crew that does is held for it, 10 to 15 s from her hit, from 18 to 45 m off her and no higher than 2.6 m, she in
	// the picture; left early only for a shell or fuel ball at the lens, a man at it, or the scene's own cut; once.
	if (!bOnlyGen && bBailTest)
	{
		const std::vector<LandEye::Director::BailBeat>& Bb = Eye.BailBeats();
		const LandEye::Director::BailBeat* B0 = Bb.empty() ? nullptr : &Bb.front();
		const double HeldFor = B0 && B0->Hit >= 0.0 ? (B0->Left >= 0.0 ? B0->Left : Real) - B0->Hit : 0.0;
		std::printf("bail-out: %d crews got out, %d bail watches placed, %d of the crews on one as they were hit; ", BailCrews, Eye.BailWatches(), BailWatched);
		if (B0) std::printf("the first held on track %d for %.1f s of %.1f asked (left for %s), the lens %.0f m off her, %.1f m up, she %s the picture; %d tanks' ends passed in the hold\n",
			B0->Track, HeldFor, B0->Hold, B0->Left >= 0.0 ? LandEye::NameOf(B0->LeftWhy) : "the battle's end", BailLensOff, BailLensUp, bBailInPicture ? "in" : "NOT in", BailEndsPassed);
		else std::printf("none held\n");
		if (BailWatched > 0 && !B0) Fault("a bail watch on a crew getting out was not held for it");
		if (Bb.size() > 1) Fault("more than one bail-out was held");
		if (B0 && (B0->Hold < ES.BailLeast - 1e-6 || B0->Hold > ES.BailMost + 1e-6)) Fault("a bail-out's hold was asked outside BailLeast to BailMost");
		if (B0 && B0->Left >= 0.0 && HeldFor > B0->Hold + 0.25) Fault("a bail-out was held past its hold");
		const bool bRescue = B0 && (B0->LeftWhy == LandEye::Why::Burst || B0->LeftWhy == LandEye::Why::Rescue || B0->LeftWhy == LandEye::Why::Dark || B0->LeftWhy == LandEye::Why::Gone);
		if (B0 && B0->Left >= 0.0 && HeldFor < B0->Hold - 0.25 && !bRescue) Fault("a bail-out's hold was cut short by the film's own wants");
		if (B0 && bBailLensSeen && (BailLensOff < 18.0 || BailLensOff > 45.0 || BailLensUp > 2.6 || !bBailInPicture)) Fault("a bail-out's lens was not 18 to 45 m off her, low, with her in the picture as she was hit");
	}
	// The special shots (Brian's yes, 23 November 19:52Z, release 2): each held firm for its own time from its cut, left
	// sooner only for a rescue (a shell, a fuel ball, a man or a thing at the lens, a dark picture, its man gone); the
	// whistle on its officer in the picture as he steps off, the film opened on it followed by the opening aerial; a
	// nickname's card on the picture all its hold, its shooter in it as the kill lands, at most NickMost, NickEvery apart;
	// one last stand, at least two of its men in the picture at its middle.
	if (!bOnlyGen && (bWhistleTest || bNickTest || bStandTest))
	{
		const std::vector<LandEye::Director::SpecialBeat>& Sb = Eye.SpecialBeats();
		SpSeen.resize(Sb.size());
		int Whistles = 0, Cards = 0, Stands = 0, NamesAgain = 0;
		double LastCard = -1e9, CardGap = 1e9;
		std::vector<std::string> Carded;
		for (size_t K = 0; K < Sb.size(); ++K)
		{
			const LandEye::Director::SpecialBeat& B = Sb[K];
			const SpecialSeen& M = SpSeen[K];
			const double HeldFor = B.CutReal < 0.0 ? 0.0 : (B.Left >= 0.0 ? B.Left : Real) - B.CutReal;
			std::printf("%s: %s at %.1f on track %d (block %d)", LandEye::NameOf(B.Sp), B.Sp == LandEye::Special::Whistle ? "step-off" : B.Sp == LandEye::Special::Nick ? "first kill lands" : "stand",
				B.At, B.Who, B.Block);
			if (B.CutReal < 0.0)
			{
				std::printf(", MISSED (nothing fit on him, or no cut in time)\n");
				continue;
			}
			std::printf(", cut at %.2f to %s, held %.1f s of %.1f (left for %s)", B.Cut, LandEye::NameOf(B.Shot), HeldFor, B.Hold, B.Left >= 0.0 ? LandEye::NameOf(B.LeftWhy) : "the battle's end");
			if (M.bLooked) std::printf("; the lens %.0f m off him, %.1f m up, he %s the picture", M.Off, M.Up, M.bIn ? "in" : "NOT in");
			else std::printf("; left before its moment");
			if (B.Sp == LandEye::Special::Nick) std::printf("; card on %d of %d frames", M.CardFrames, M.HoldFrames);
			if (B.Sp == LandEye::Special::Stand && M.bLooked) std::printf("; %d of its %zu men in the picture at its middle", M.MenIn, B.Men.size());
			if (B.Sp == LandEye::Special::Stand) std::printf("; its end %.1f", B.To);
			std::printf("\n");
			const bool bRescue = B.LeftWhy == LandEye::Why::Burst || B.LeftWhy == LandEye::Why::Rescue || B.LeftWhy == LandEye::Why::Dark || B.LeftWhy == LandEye::Why::Gone || B.LeftWhy == LandEye::Why::Lost;
			if (B.Left >= 0.0 && HeldFor < B.Hold - 0.25 && !bRescue) Fault("a special shot's hold was cut short by the film's own wants");
			if (B.Left >= 0.0 && HeldFor > B.Hold + 0.25) Fault("a special shot was held past its hold");
			if (M.bLooked && !M.bIn && B.Sp != LandEye::Special::Stand) Fault("a special shot's man was not in the picture at its moment");
			if (B.Sp == LandEye::Special::Whistle)
			{
				++Whistles;
				const auto& Lg = Eye.Log();
				if (B.At <= ES.WhistleOpen && (Lg.size() < 2 || Lg[1].second != LandEye::Kind::Open || B.Cut > 1.0)) Fault("the film opening on the whistle was not followed by the opening aerial");
			}
			if (B.Sp == LandEye::Special::Nick)
			{
				++Cards;
				const std::string Name = EW.Nickname ? EW.Nickname(B.Block) : std::string();
				if (std::find(Carded.begin(), Carded.end(), Name) != Carded.end()) ++NamesAgain;
				Carded.push_back(Name);
				CardGap = std::min(CardGap, B.CutReal - LastCard);
				LastCard = B.CutReal;
				if (M.CardFrames < M.HoldFrames * 9 / 10) Fault("a nickname's card was not on the picture through its hold");
			}
			if (B.Sp == LandEye::Special::Stand)
			{
				++Stands;
				if (M.bLooked && M.MenIn < 2) Fault("a last stand's men were not in the picture");
			}
		}
		std::printf("special shots: %d whistle, %d nickname cards, %d last stands (the whistle's step-off %.1f on track %d)\n", Whistles, Cards, Stands, Eye.WhistleAt(), Eye.WhistleMan());
		if (bWhistleTest && !bWhistleHidden && Eye.WhistleAt() >= 0.0 && Whistles == 0) Fault("the whistle start was not shot");
		if (bWhistleHidden && Whistles > 0) Fault("a whistle was shot on an officer hidden from the player");
		if (bWhistleHidden && !Eye.Log().empty() && Eye.Log()[0].first < 1.0) Fault("the film did not open on its aerial with the whistle's officer hidden");
		if (Whistles > 1) Fault("more than one whistle");
		if (Cards > ES.NickMost) Fault("more nickname cards than NickMost");
		if (Cards > 1 && CardGap < ES.NickEvery - 1e-6) Fault("two nickname cards nearer than NickEvery");
		if (Stands > 1) Fault("more than one last stand");
		if (NamesAgain > 0) Fault("one nickname carded twice in a battle");
	}
	// The film's shallow focus (the graphics audit's E9, Brian's D3): the close follow shots in it, never a wide, never
	// switched in the middle of a shot, the distance held near its man (the Mac's cost is paid only in these frames).
	if (!bOnlyGen)
	{
		std::sort(FocusOff.begin(), FocusOff.end());
		const double Off95 = FocusOff.empty() ? 0.0 : FocusOff[std::min(FocusOff.size() - 1, static_cast<size_t>(0.95 * static_cast<double>(FocusOff.size())))];
		std::printf("focus: %d of %d shots in shallow focus, %.0f%% of the fight's frames; on a wide %d frames, switched mid-shot %d times; off its man %.1f%% (95th in the hundred), %.1f%% at the most, over %zu frames; pulled %.2f a second at the most (in its log)\n",
			FocusCuts, FocusAllCuts, 100.0 * FocusFrames / std::max(1, FightFrames), FocusWrongKind, FocusMidShot, 100.0 * Off95, FocusOff.empty() ? 0.0 : 100.0 * FocusOff.back(), FocusOff.size(), FocusPullMost);
		if (FocusWrongKind > 0) Fault("a wide, the opening or a ride on a tank was shot in shallow focus");
		if (FocusMidShot > 0) Fault("the shallow focus was switched on or off in the middle of a shot");
		if (FocusAllCuts >= 10 && FocusCuts == 0 && ES.FocusFar > 0.0) Fault("no close shot was in shallow focus");
		if (FocusOff.size() >= 60 && Off95 > 0.15) Fault("the shallow focus held off its man");
	}
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
