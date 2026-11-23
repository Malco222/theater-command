// The crews' bail-out's cloud test (release 2, -TheaterLandBailOut): every land way of the fates table with a crew_out
// step, made for many wrecks and seeds, each man posed every tenth of a second for three minutes. Checks: no NaN; never
// shown before he starts out; nobody out of a brew-up after its blast, and a man still climbing when it goes is caught;
// once down he stays on the ground (no floating, no sinking); the same wreck gives the same crew; a crew_in brings its
// crouching men back in. The art director's notes (23 November): a man's first leg runs straight back in her lee, her
// hull between him and the enemy; a man caught by her blast is thrown 2 to 5 m; every man on the ground within 15 m
// flinches flat at it; a man on fire out of at most about one burning wreck in eight, a third of them living; the odds
// in the open by the enemy in sight (about 1 in 6 running with the enemy close, much less with none). Prints how the
// crews went, for the log. With LAND_BAIL_CLIPS=1 the same with the soldiers' bail clips (Wreck::bClips): he climbs out
// in bail_climb_out and jumps down in bail_jump_down, their roots landing him where the ground game takes him on; a man
// on fire runs in burning_run, his stride the clip's, and falls in burning_fall_roll.
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
		std::printf("with the soldiers' bail clips (bail_climb_out, bail_jump_down, burning_run, burning_fall_roll)\n");
	}
	int Bad = 0, Wrecks = 0, Men = 0;
	int BurnWrecks = 0, OnFire = 0, Lives = 0, Flinched = 0;
	int RunNear = 0, HitNear = 0, RunFar = 0, HitFar = 0;
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
			W.Away = W.Heading + 3.14159;
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
			const LandBail::Crew Cr = LandBail::Make(W);
			const LandBail::Crew Again = LandBail::Make(W);
			++Wrecks;
			BurnWrecks += W.bBurns && !Cr.Men.empty();
			if (Again.Men.size() != Cr.Men.size() || Again.Story != Cr.Story)
			{
				std::printf("FAIL %s seed %llu: not the same crew twice\n", Way.Key.c_str(), static_cast<unsigned long long>(Seed));
				++Bad;
			}
			for (const LandBail::Man& M : Cr.Men)
			{
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
					(W.Threat > 0.5 ? RunNear : RunFar)++;
					(W.Threat > 0.5 ? HitNear : HitFar) += bHit;
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
					if (Back < Rear + 0.75 || std::abs(Aside) > std::max(std::max(0.6, 0.75 * ShadowHalf), std::abs(DownAside)) + 1e-6 || Leg < 2.5 || Leg > 18.0 || bThrough)
					{
						std::printf("FAIL %s: his first leg is not back in her lee (%.1f m behind her, %.1f aside from %.2f, %.1f long%s, rear %.2f)\n", Way.Key.c_str(), Back, Aside, DownAside, Leg, bThrough ? ", through her" : "", Rear);
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
					const LandBail::Pose Up = LandBail::PoseAt(M, M.Out + 0.75, Ground), Off = LandBail::PoseAt(M, M.Out + 3.0, Ground);
					const LandBail::Pose Landed = LandBail::PoseAt(M, M.Out + LandBail::OnGroundOf(M) - 1e-6, Ground);
					const LandBail::Pose On = LandBail::PoseAt(M, M.Out + LandBail::OnGroundOf(M) + 1e-6, Ground);
					const double Gap = std::hypot(Landed.Root.X - On.Root.X, Landed.Root.Y - On.Root.Y);
					const double Rise = Landed.Root.Z - On.Root.Z;
					if (Up.Clip != "bail_climb_out" || Off.Clip != "bail_jump_down" || Gap > 0.05 || std::abs(Rise) > 0.05 || std::abs(On.At.Z - Ground(On.At.X, On.At.Y)) > 0.01)
					{
						std::printf("FAIL %s: the clips' way out (%s, %s; lands %.2f m off and %.2f m up from his way on)\n", Way.Key.c_str(), Up.Clip.c_str(), Off.Clip.c_str(), Gap, Rise);
						++Bad;
					}
					if (M.How == LandBail::End::Burning)
					{
						const LandBail::Pose Run = LandBail::PoseAt(M, 0.5 * (M.Out + LandBail::OnGroundOf(M) + M.FallAt), Ground), Fell = LandBail::PoseAt(M, M.FallAt + 5.0, Ground);
						const double Rolled = std::hypot(Fell.Root.X - Fell.At.X, Fell.Root.Y - Fell.At.Y);
						if (Run.Clip != "burning_run" || Fell.Clip != "burning_fall_roll" || Rolled < 1.4 || Rolled > 1.65)
						{
							std::printf("FAIL %s: on fire in %s, then %s (rolled %.2f m)\n", Way.Key.c_str(), Run.Clip.c_str(), Fell.Clip.c_str(), Rolled);
							++Bad;
						}
					}
				}
				bool bWasIn = false;
				for (double T = 0.0; T <= 180.0; T += 0.1)
				{
					const LandBail::Pose P = LandBail::PoseAt(M, T, Ground);
					if (T < M.Out && P.bShown)
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
