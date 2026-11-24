// The crews' bail-out's cloud test (release 2, -TheaterLandBailOut): every land way of the fates table with a crew_out
// step, made for many wrecks and seeds, each man posed every tenth of a second for three minutes. Checks: no NaN; never
// shown before he starts out; nobody out of a brew-up after its blast, and a man still climbing when it goes is caught;
// once down he stays on the ground (no floating, no sinking); the same wreck gives the same crew; a crew_in brings its
// crouching men back in. The art director's notes (23 November): a man's first leg runs straight back in her lee, her
// hull between him and the enemy; a man caught by her blast is thrown 2 to 5 m; every man on the ground within 15 m
// flinches flat at it; a man on fire out of at most about one burning wreck in eight, a third of them living; the odds
// in the open by the enemy in sight (about 1 in 6 running with the enemy close, much less with none). Prints how the
// crews went, for the log. With LAND_BAIL_CLIPS=1 the same with the soldiers' bail clips (Wreck::bClips): he climbs out
// in bail_climb_out, walks over her turret's roof and steps down onto her deck in bail_step_down (or walks from a hatch
// on her deck), walks to her hull's edge and jumps down in bail_jump_down, their roots landing him where the ground game
// takes him on, his body never jumping between them; a man on fire runs in burning_run, his stride the clip's, and falls
// in burning_fall_roll.
#include "Battle/Fates.h"
#include "Battle/LandBail.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

int main(int Argc, char** Argv)
{
	const std::string Content = Argc > 1 ? Argv[1] : "Content";
	std::ifstream In(Content + "/Data/Battle/fates.json");
	std::stringstream Text;
	Text << In.rdbuf();
	Fates::Table Table;
	std::string Error;
	if (!Table.Load(Text.str(), Error))
	{
		std::printf("FAIL: fates.json: %s\n", Error.c_str());
		return 1;
	}
	auto Ground = [](double X, double Y) { return 0.3 * std::sin(X * 0.07) + 0.2 * std::cos(Y * 0.11); };
	const char* ClipsEnv = std::getenv("LAND_BAIL_CLIPS");
	const bool bClips = ClipsEnv && std::string(ClipsEnv) == "1";
	if (bClips)
	{
		std::printf("with the soldiers' bail clips (bail_climb_out, bail_step_down, bail_jump_down, burning_run, burning_fall_roll)\n");
	}
	// (and with LAND_BAIL_SMOKE=1, -TheaterLandBailSmoke: her commander's smoke grenade to cover them)
	const char* SmokeEnv = std::getenv("LAND_BAIL_SMOKE");
	const bool bSmokeOn = SmokeEnv && std::string(SmokeEnv) == "1";
	if (bSmokeOn)
	{
		std::printf("with smoke to cover them (throw_grenade from her commander's hatch)\n");
	}
	int Bad = 0, Wrecks = 0, Men = 0;
	int SmokeCan = 0, SmokeThrown = 0, RunScreened = 0, HitScreened = 0;
	int BurnWrecks = 0, OnFire = 0, Lives = 0, Flinched = 0;
	int RunNear = 0, HitNear = 0, RunFar = 0, HitFar = 0;
	int Steps = 0, Walks = 0, Ends = 0, Sides[4] = { 0, 0, 0, 0 }, Flanks = 0;
	double WayMost = 0.0, ClipsLong[2] = { 1e9, 0.0 };
	std::map<std::string, std::map<std::string, int>> Tally;
	for (const Fates::Way& Way : Table.Ways())
	{
		if (Way.Domain != "land")
		{
			continue;
		}
		bool bOut = false;
		for (size_t I = 0; I < Way.Beats.Count(); ++I)
		{
			bOut = bOut || Way.Beats.At(I)["do"].Str() == "crew_out";
		}
		if (!bOut)
		{
			continue;
		}
		const bool bBrew = Way.Key == "tank.brew_up" || Way.Key == "tank.turret_off";
		for (uint64_t Seed = 1; Seed <= 300; ++Seed)
		{
			LandBail::Wreck W;
			W.Fate = Way.Key;
			W.Way = &Way;
			W.Middle = LandBail::V3{ 10.0, -4.0, Ground(10.0, -4.0) };
			W.Heading = 0.3 * static_cast<double>(Seed % 20);
			W.Wide = 3.4;
			W.Long = 6.4;
			W.Threat = Seed % 2 ? 1.0 : 0.1; // (odd seeds: the enemy close and in sight; even: none in sight)
			const double S = std::sin(W.Heading), C = std::cos(W.Heading);
			// (the commander's hatch on her turret, the driver's on her hull's front, the loader's beside the commander's,
			// the hull gunner's beside the driver's)
			for (const auto& H : { std::pair<double, double>{ -0.3, -0.5 }, { 1.6, -0.8 }, { -0.3, 0.5 }, { 1.6, 0.8 } })
			{
				const double Z = H.first < 0.0 ? 2.9 : 1.8;
				W.Exits.push_back(LandBail::V3{ W.Middle.X + S * H.first + C * H.second, W.Middle.Y + C * H.first - S * H.second, W.Middle.Z + Z });
			}
			// (her enemy ahead, or ahead and off to one side or the other; one seed in five a vehicle close on her right,
			// one in seven on her left, one in nine behind her)
			W.Away = W.Heading + 3.14159 + (Seed % 3 == 0 ? 0.0 : Seed % 3 == 1 ? 0.8 : -0.8);
			W.Blocked[0] = Seed % 5 == 0;
			W.Blocked[1] = Seed % 7 == 0;
			W.Blocked[2] = Seed % 9 == 0;
			W.Blast = bBrew ? 5.5 + 4.5 * static_cast<double>(Seed % 10) / 9.0 : -1.0;
			W.bBurns = bBrew || Way.Key == "tank.engine_fire" || Way.Key == "tank.knocked_out_later_burns" || Way.Key == "soft.burns";
			W.MostMen = Way.Key.rfind("tank.", 0) == 0 ? 5 : 3;
			// (each way its own seeds, so the tallies are not one draw seen thirteen times)
			uint64_t KeyHash = 1469598103934665603ull;
			for (const char Ch : Way.Key)
			{
				KeyHash = (KeyHash ^ static_cast<unsigned char>(Ch)) * 1099511628211ull;
			}
			W.Seed = Seed * 7919u ^ KeyHash;
			W.Ground = Ground;
			W.bClips = bClips;
			W.bStep = bClips;
			W.bSmoke = bSmokeOn;
			// (the wind: none, across her, from her enemy, or toward it, by seed)
			W.Wind = Seed % 4 == 0 ? LandBail::V3{ 0.0, 0.0, 0.0 } : LandBail::V3{ 2.6 * std::sin(W.Away + 1.2 * static_cast<double>(Seed % 4)), 2.6 * std::cos(W.Away + 1.2 * static_cast<double>(Seed % 4)), 0.0 };
			const LandBail::Crew Cr = LandBail::Make(W);
			const LandBail::Crew Again = LandBail::Make(W);
			++Wrecks;
			// (her smoke: only with the switch on, her enemy close and in sight and no blast to come; thrown by her first
			// man out from his hatch, seen throwing it with the clips, before he climbs out; landing past her hull toward
			// her enemy, on the ground, as it leaves his hand and flies; a grenade's cloud for a minute or a minute and a half)
			{
				const bool bCan = bSmokeOn && W.Blast < 0.0 && W.bUnderFire && W.Threat >= 0.5 && !Cr.Men.empty();
				SmokeCan += bCan;
				if (Cr.SmokeAt >= 0.0)
				{
					++SmokeThrown;
					const LandBail::Man& T0 = Cr.Men[0];
					const double Toward = -((Cr.SmokeTo.X - W.Middle.X) * std::sin(W.Away) + (Cr.SmokeTo.Y - W.Middle.Y) * std::cos(W.Away));
					const char* Wrong = !bCan ? "with nobody to throw it (switch off, her blast to come, or no enemy close)"
						: T0.ThrowAt < 0.3 - 1e-9 || std::abs(T0.Out - (T0.ThrowAt + 1.45)) > 1e-9 ? "not her first man's throw just before he climbs out"
						: std::abs(Cr.SmokeAt - (T0.ThrowAt + 0.95 + 0.9)) > 1e-9 ? "landing at the wrong moment"
						: Toward < 4.0 ? "not out toward her enemy, past her hull"
						: std::abs(Cr.SmokeTo.Z - Ground(Cr.SmokeTo.X, Cr.SmokeTo.Y)) > 1e-9 ? "off the ground"
						: Cr.SmokeS < 60.0 || Cr.SmokeS > 90.0 || Cr.SmokeAcross < 10.0 || Cr.SmokeAcross > 15.0 ? "too long, short, wide or narrow" : nullptr;
					// (a little upwind of the line from his hatch to her enemy: 1 to 3 m against the wind from a point 9 to 14 m
					// out on that line; on it on a still day)
					{
						const LandBail::V3 Line{ -std::sin(W.Away), -std::cos(W.Away), 0.0 };
						const double Gx = Cr.SmokeTo.X - T0.Hatch.X, Gy = Cr.SmokeTo.Y - T0.Hatch.Y;
						const double Breeze = std::hypot(W.Wind.X, W.Wind.Y);
						const double U = Breeze > 0.0 ? std::clamp(Breeze, 1.0, 3.0) / Breeze : 0.0;
						const double Bx = Gx + W.Wind.X * U, By = Gy + W.Wind.Y * U;
						const double Along = Bx * Line.X + By * Line.Y, Aside = Bx * Line.Y - By * Line.X;
						Wrong = Wrong ? Wrong : std::abs(Aside) > 1e-6 || Along < 9.0 - 1e-6 || Along > 14.0 + 1e-6 ? "not 1 to 3 m upwind of the line" : nullptr;
						const double Face = std::remainder(T0.ThrowFace - std::atan2(Gx, Gy), 2.0 * 3.14159265358979);
						Wrong = Wrong ? Wrong : std::abs(Face) > 1e-6 ? "not thrown the way it goes" : nullptr;
					}
					for (size_t K = 1; K < Cr.Men.size() && !Wrong; ++K)
					{
						Wrong = Cr.Men[K].ThrowAt >= 0.0 ? "thrown by more than one man" : nullptr;
					}
					if (!Wrong && bClips)
					{
						const LandBail::Pose Tp = LandBail::PoseAt(T0, T0.ThrowAt + 0.5, Ground);
						Wrong = !Tp.bShown || Tp.Clip != "throw_grenade" ? "not seen thrown" : nullptr;
					}
					if (Wrong)
					{
						std::printf("FAIL %s seed %llu: her smoke %s\n", Way.Key.c_str(), static_cast<unsigned long long>(Seed), Wrong);
						++Bad;
					}
				}
			}
			BurnWrecks += W.bBurns && !Cr.Men.empty();
			if (Again.Men.size() != Cr.Men.size() || Again.Story != Cr.Story)
			{
				std::printf("FAIL %s seed %llu: not the same crew twice\n", Way.Key.c_str(), static_cast<unsigned long long>(Seed));
				++Bad;
			}
			for (const LandBail::Man& M : Cr.Men)
			{
				// (off the side away from her enemy; another way only where that side is blocked; never off an end that
				// faces her enemy, nor off a burning tank's end)
				{
					const double AwayLean = std::sin(W.Away) * std::cos(W.Heading) - std::cos(W.Away) * std::sin(W.Heading);
					const int AwaySide = std::abs(AwayLean) >= 0.26 ? (AwayLean > 0.0 ? 1 : -1) : 0;
					const bool bBlockedHis = W.Blocked[M.Side > 0 ? 0 : 1];
					const double EndAway = (std::sin(W.Away) * std::sin(W.Heading) + std::cos(W.Away) * std::cos(W.Heading)) * M.OffEnd;
					const char* Wrong = M.OffEnd != 0 && (W.bBurns || EndAway < -0.26 || W.Blocked[M.OffEnd > 0 ? 3 : 2]) ? "off a burning, blocked or enemy-facing end"
						: M.OffEnd == 0 && AwaySide != 0 && M.Side != AwaySide && !W.Blocked[AwaySide > 0 ? 0 : 1] ? "off the side toward her enemy, the other clear"
						: M.OffEnd == 0 && bBlockedHis && !W.Blocked[M.Side > 0 ? 1 : 0] ? "off a blocked side, the other clear"
						: M.OffEnd != 0 && !W.Blocked[0] && !W.Blocked[1] ? "off an end, both sides clear" : nullptr;
					if (Wrong)
					{
						std::printf("FAIL %s: a man drops %s\n", Way.Key.c_str(), Wrong);
						++Bad;
					}
					Ends += M.OffEnd != 0;
					Sides[M.OffEnd != 0 ? 2 : AwaySide == 0 ? 3 : M.Side == AwaySide ? 0 : 1]++;
				}
				++Men;
				const char* Hows[] = { "clear", "cut down", "wounded", "on fire", "caught" };
				Tally[Way.Key][Hows[static_cast<int>(M.How)]]++;
				if (M.BackAt >= 0.0)
				{
					Tally[Way.Key]["back in"]++;
				}
				if (bBrew && M.Out >= W.Blast)
				{
					std::printf("FAIL %s: a man starts out after the blast\n", Way.Key.c_str());
					++Bad;
				}
				if (bBrew && M.How != LandBail::End::Caught && M.Out + LandBail::OnGroundOf(M) > W.Blast + 1e-9)
				{
					std::printf("FAIL %s: a man still climbing at the blast is not caught\n", Way.Key.c_str());
					++Bad;
				}
				OnFire += M.How == LandBail::End::Burning;
				Lives += M.How == LandBail::End::Burning && M.bLives;
				if (M.To != "beside" && M.Go == "run" && M.How != LandBail::End::Caught && M.How != LandBail::End::Burning && W.bUnderFire)
				{
					const bool bHit = M.How == LandBail::End::Shot || M.How == LandBail::End::Wounded;
					// (on the ground once her smoke is up: counted apart, hit about half as often)
					const bool bScreened = Cr.SmokeAt >= 0.0 && M.Out + LandBail::OnGroundOf(M) >= Cr.SmokeAt + 2.5;
					(bScreened ? RunScreened : W.Threat > 0.5 ? RunNear : RunFar)++;
					(bScreened ? HitScreened : W.Threat > 0.5 ? HitNear : HitFar) += bHit;
				}
				// (his first leg: back from where he lands to past her rear as the enemy sees her, never through her hull,
				// ending in her hull's shadow or as far toward it as it keeps clear)
				if (M.To != "beside")
				{
					const double Lx = std::sin(W.Away), Ly = std::cos(W.Away);
					const double Back = (M.Lee.X - W.Middle.X) * Lx + (M.Lee.Y - W.Middle.Y) * Ly;
					const double Aside = (M.Lee.X - W.Middle.X) * Ly - (M.Lee.Y - W.Middle.Y) * Lx;
					const double DownAside = (M.Down.X - W.Middle.X) * Ly - (M.Down.Y - W.Middle.Y) * Lx;
					const double Rel = W.Heading - W.Away;
					const double ShadowHalf = 0.5 * (W.Wide * std::abs(std::cos(Rel)) + W.Long * std::abs(std::sin(Rel)));
					const double Rear = 0.5 * (W.Long * std::abs(std::cos(Rel)) + W.Wide * std::abs(std::sin(Rel)));
					const double Leg = std::hypot(M.Lee.X - M.Down.X, M.Lee.Y - M.Down.Y);
					bool bThrough = false;
					for (int Q = 0; Q <= 50; ++Q)
					{
						const double U = Q / 50.0;
						const double Px = M.Down.X + (M.Lee.X - M.Down.X) * U - W.Middle.X, Py = M.Down.Y + (M.Lee.Y - M.Down.Y) * U - W.Middle.Y;
						bThrough = bThrough || (std::abs(Px * S + Py * C) < 0.5 * W.Long && std::abs(Px * C - Py * S) < 0.5 * W.Wide);
					}
					// (or, down on the side her enemy sees where his own was blocked, straight along her flank to past her
					// end away from her enemy)
					const double EndWay = Lx * S + Ly * C >= 0.0 ? 1.0 : -1.0;
					const double LeeEnd = ((M.Lee.X - W.Middle.X) * S + (M.Lee.Y - W.Middle.Y) * C) * EndWay;
					const bool bFlank = !bThrough && LeeEnd >= 0.5 * W.Long + 0.75 && Leg <= 0.5 * W.Long + 3.5 + 3.5 + 1e-6
						&& std::abs(((M.Lee.X - M.Down.X) * C - (M.Lee.Y - M.Down.Y) * S)) < 1e-6;
					Flanks += bFlank;
					if (!bFlank && (Back < Rear + 0.75 || std::abs(Aside) > std::max(std::max(0.6, 0.75 * ShadowHalf), std::abs(DownAside)) + 1e-6 || Leg < 2.5 || Leg > 18.0 || bThrough))
					{
						std::printf("FAIL %s: his first leg is not back in her lee (end %d, side %d; %.1f m behind her, %.1f aside from %.2f, %.1f long%s, rear %.2f)\n", Way.Key.c_str(), M.OffEnd, M.Side, Back, Aside, DownAside, Leg, bThrough ? ", through her" : "", Rear);
						++Bad;
					}
				}
				// (caught by her blast: thrown 2 to 5 m from where he was)
				if (M.How == LandBail::End::Caught)
				{
					const LandBail::Pose A = LandBail::PoseAt(M, M.FallAt, Ground), B = LandBail::PoseAt(M, M.FallAt + 3.0, Ground);
					const double Throw = std::hypot(B.At.X - A.At.X, B.At.Y - A.At.Y);
					if (Throw < 1.9 || Throw > 5.1)
					{
						std::printf("FAIL %s: thrown %.1f m by her blast\n", Way.Key.c_str(), Throw);
						++Bad;
					}
				}
				// (on the ground within 15 m of her as her ammunition goes, and not down: flat for a second or two)
				if (bBrew && M.How != LandBail::End::Caught && M.How != LandBail::End::Burning && M.Out + LandBail::OnGroundOf(M) <= W.Blast && !(M.FallAt >= 0.0 && M.FallAt <= W.Blast))
				{
					const LandBail::Pose A = LandBail::PoseAt(M, W.Blast - 0.01, Ground);
					const bool bNear = std::hypot(A.At.X - W.Middle.X, A.At.Y - W.Middle.Y) <= 14.5;
					const LandBail::Pose B = LandBail::PoseAt(M, W.Blast + 0.5, Ground);
					if (bNear && (M.FlinchAt < 0.0 || B.Clip != "go_prone"))
					{
						std::printf("FAIL %s: %.1f m from her at the blast and no flinch (%s)\n", Way.Key.c_str(), std::hypot(A.At.X - W.Middle.X, A.At.Y - W.Middle.Y), B.Clip.c_str());
						++Bad;
					}
					Flinched += M.FlinchAt >= 0.0;
				}
				// (the clips: out of the hatch and off her hull in the soldiers' clips, their roots landing him where his
				// way on begins; on fire, burning_run and the roll)
				if (bClips && M.How != LandBail::End::Caught)
				{
					// (climb, scramble over her roof, step down, scramble on her deck, jump; or climb, scramble to her edge, jump: each
					// clip where it should be, and his body never jumping between them, nor at the ground where his run
					// takes him on)
					const double OnRoof = M.Out + 1.5 + M.RoofS, OffRoof = OnRoof + (M.bStep ? 0.7333 : 0.0), Jump = OffRoof + M.DeckS;
					const LandBail::Pose Up = LandBail::PoseAt(M, M.Out + 0.75, Ground), Off = LandBail::PoseAt(M, Jump + 0.8, Ground);
					const LandBail::Pose Walk = LandBail::PoseAt(M, M.bStep ? M.Out + 1.5 + 0.5 * M.RoofS : 0.5 * (OffRoof + Jump), Ground);
					const LandBail::Pose Stepping = LandBail::PoseAt(M, OnRoof + 0.3, Ground);
					double Most = 0.0;
					for (double T = M.ThrowAt >= 0.0 ? M.ThrowAt : M.Out; T < M.Out + LandBail::OnGroundOf(M) + 0.5; T += 1.0 / 120.0)
					{
						const LandBail::Pose A = LandBail::PoseAt(M, T, Ground), B = LandBail::PoseAt(M, T + 1.0 / 120.0, Ground);
						Most = std::max(Most, std::sqrt((A.Root.X - B.Root.X) * (A.Root.X - B.Root.X) + (A.Root.Y - B.Root.Y) * (A.Root.Y - B.Root.Y) + (A.Root.Z - B.Root.Z) * (A.Root.Z - B.Root.Z)));
					}
					const LandBail::Pose On = LandBail::PoseAt(M, M.Out + LandBail::OnGroundOf(M) + 1e-6, Ground);
					// (crouched between the clips: the art director, 19:33Z)
					const bool bWay = Up.Clip == "bail_climb_out" && Walk.Clip == "advance" && (!M.bStep || Stepping.Clip == "bail_step_down")
						&& Off.Clip == "bail_jump_down" && (M.bStep || M.DeckS >= 0.25);
					// (a body at a run moves 6 m a second at most: 0.05 m a frame at 120)
					if (!bWay || Most > 0.06 || std::abs(On.At.Z - Ground(On.At.X, On.At.Y)) > 0.01)
					{
						std::printf("FAIL %s: the clips' way out (%s, %s, %s, %s; his body's biggest jump %.3f m in a 120th of a second)\n", Way.Key.c_str(), Up.Clip.c_str(), Walk.Clip.c_str(), Stepping.Clip.c_str(), Off.Clip.c_str(), Most);
						++Bad;
					}
					Steps += M.bStep;
					Walks += !M.bStep;
					WayMost = std::max(WayMost, Most);
					ClipsLong[0] = std::min(ClipsLong[0], LandBail::OnGroundOf(M));
					ClipsLong[1] = std::max(ClipsLong[1], LandBail::OnGroundOf(M));
					if (M.How == LandBail::End::Burning)
					{
						const LandBail::Pose Run = LandBail::PoseAt(M, 0.5 * (M.Out + LandBail::OnGroundOf(M) + M.FallAt), Ground), Fell = LandBail::PoseAt(M, M.FallAt + 5.0, Ground);
						const double Rolled = std::hypot(Fell.Root.X - Fell.At.X, Fell.Root.Y - Fell.At.Y);
						if (Run.Clip != "burning_run" || Fell.Clip != "burning_fall_roll" || Rolled < 1.4 || Rolled > 1.65)
						{
							std::printf("FAIL %s: on fire in %s, then %s (rolled %.2f m; to %s, end %d side %d, run %.2f s, lee %.1f m, goal %.1f m from down)\n", Way.Key.c_str(), Run.Clip.c_str(), Fell.Clip.c_str(), Rolled, M.To.c_str(), M.OffEnd, M.Side, M.FallAt - M.Out - LandBail::OnGroundOf(M), std::hypot(M.Lee.X - M.Down.X, M.Lee.Y - M.Down.Y), std::hypot(M.Goal.X - M.Down.X, M.Goal.Y - M.Down.Y));
							++Bad;
						}
					}
				}
				bool bWasIn = false;
				for (double T = 0.0; T <= 180.0; T += 0.1)
				{
					const LandBail::Pose P = LandBail::PoseAt(M, T, Ground);
					if (T < M.Out && P.bShown && !(M.ThrowAt >= 0.0 && T >= M.ThrowAt))
					{
						std::printf("FAIL %s: shown before he starts out\n", Way.Key.c_str());
						++Bad;
						break;
					}
					if (!P.bShown)
					{
						bWasIn = bWasIn || (M.BackAt >= 0.0 && T > M.BackAt);
						continue;
					}
					if (!std::isfinite(P.At.X) || !std::isfinite(P.At.Y) || !std::isfinite(P.At.Z) || !std::isfinite(P.Facing) || !std::isfinite(P.Time) || P.Clip.empty())
					{
						std::printf("FAIL %s: NaN or no clip at %.1f s\n", Way.Key.c_str(), T);
						++Bad;
						break;
					}
					const bool bDown = T > M.Out + LandBail::OnGroundOf(M) + 0.05 && (M.BackAt < 0.0 || T < M.BackAt) && !(M.How == LandBail::End::Caught && T < M.FallAt + 0.95);
					if (bDown && std::abs(P.At.Z - Ground(P.At.X, P.At.Y)) > 0.01)
					{
						std::printf("FAIL %s: off the ground by %.2f m at %.1f s (%s)\n", Way.Key.c_str(), P.At.Z - Ground(P.At.X, P.At.Y), T, P.Clip.c_str());
						++Bad;
						break;
					}
					if (std::hypot(P.At.X - W.Middle.X, P.At.Y - W.Middle.Y) > 60.0)
					{
						std::printf("FAIL %s: %.0f m from her\n", Way.Key.c_str(), std::hypot(P.At.X - W.Middle.X, P.At.Y - W.Middle.Y));
						++Bad;
						break;
					}
				}
				if (M.BackAt >= 0.0 && !bWasIn)
				{
					std::printf("FAIL %s: a man going back in never gets in\n", Way.Key.c_str());
					++Bad;
				}
			}
			if (Seed == 1)
			{
				std::printf("  %-34s %s\n", Way.Key.c_str(), Cr.Story.c_str());
			}
		}
	}
	std::printf("\nHOW THE CREWS WENT (300 wrecks each)\n");
	for (const auto& Row : Tally)
	{
		std::printf("  %-34s", Row.first.c_str());
		for (const auto& Count : Row.second)
		{
			std::printf(" %s %d,", Count.first.c_str(), Count.second);
		}
		std::printf("\n");
	}
	// The art director's odds: a man on fire out of no more than about one burning wreck in eight, about a third of them
	// living; a runner hit about 1 in 6 with the enemy close, much less with none in sight.
	const double FireShare = BurnWrecks ? static_cast<double>(OnFire) / BurnWrecks : 0.0;
	const double LiveShare = OnFire ? static_cast<double>(Lives) / OnFire : 0.0;
	const double NearShare = RunNear ? static_cast<double>(HitNear) / RunNear : 0.0, FarShare = RunFar ? static_cast<double>(HitFar) / RunFar : 0.0;
	std::printf("\n  on fire: %d of %d burning wrecks (%.3f), %d of them living (%.2f); %d men flinched flat at a blast\n", OnFire, BurnWrecks, FireShare, Lives, LiveShare, Flinched);
	std::printf("  runners hit: %.3f with the enemy close (%d), %.3f with none in sight (%d)\n", NearShare, RunNear, FarShare, RunFar);
	if (bSmokeOn)
	{
		const double SmokeShare = SmokeCan ? static_cast<double>(SmokeThrown) / SmokeCan : 0.0;
		const double ScreenShare = RunScreened ? static_cast<double>(HitScreened) / RunScreened : 0.0;
		std::printf("  smoke: thrown by %d of %d crews who could (%.2f); runners hit under it %.3f (%d)\n", SmokeThrown, SmokeCan, SmokeShare, ScreenShare, RunScreened);
		if (SmokeShare < 0.5 || SmokeShare > 0.7 || RunScreened < 200 || ScreenShare > 0.6 * NearShare + 0.02)
		{
			std::printf("FAIL: smoke too seldom or too often, or no cover under it\n");
			++Bad;
		}
	}
	else if (SmokeThrown)
	{
		std::printf("FAIL: smoke with the switch off\n");
		++Bad;
	}
	if (bClips)
	{
		std::printf("  the clips' way out: %d stepped down off her turret, %d scrambled from her roof (the driver's hatch); %.1f to %.1f s to the ground; his body's biggest jump %.3f m in a 120th of a second\n", Steps, Walks, ClipsLong[0], ClipsLong[1], WayMost);
		if (Steps == 0 || Walks == 0)
		{
			std::printf("FAIL: no man stepped down off a turret, or none walked off a hull's roof\n");
			++Bad;
		}
	}
	std::printf("  which way off her: %d off the side away from her enemy, %d toward it (that side blocked), %d off her back or bow, %d with her enemy dead ahead (either side)\n", Sides[0], Sides[1], Sides[2], Sides[3]);
	std::printf("  %d ran along her flank to past her end first (down on the side her enemy sees)\n", Flanks);
	if (Ends == 0 || Sides[1] == 0)
	{
		std::printf("FAIL: no man went off her end, or none off the side toward her enemy where his was blocked\n");
		++Bad;
	}
	if (FireShare > 0.135 || (OnFire > 30 && (LiveShare < 0.2 || LiveShare > 0.47)))
	{
		std::printf("FAIL: men on fire too often or living too seldom\n");
		++Bad;
	}
	if (RunNear > 300 && (NearShare < 0.12 || NearShare > 0.21 || FarShare > 0.05))
	{
		std::printf("FAIL: the odds in the open are not the enemy's in sight\n");
		++Bad;
	}
	std::printf("\n%d wrecks, %d men: %s (%d faults)\n", Wrecks, Men, Bad ? "FAIL" : "ok", Bad);
	return Bad ? 1 : 0;
}
