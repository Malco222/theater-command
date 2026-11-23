// The crews' bail-out's cloud test (release 2, -TheaterLandBailOut): every land way of the fates table with a crew_out
// step, made for many wrecks and seeds, each man posed every tenth of a second for three minutes. Checks: no NaN; never
// shown before he starts out; nobody out of a brew-up after its blast, and a man still climbing when it goes is caught;
// once down he stays on the ground (no floating, no sinking); the same wreck gives the same crew; a crew_in brings its
// crouching men back in. Prints how the crews went, for the log.
#include "Battle/Fates.h"
#include "Battle/LandBail.h"

#include <cmath>
#include <cstdio>
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
	int Bad = 0, Wrecks = 0, Men = 0;
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
			W.Seed = Seed * 7919u;
			W.Ground = Ground;
			const LandBail::Crew Cr = LandBail::Make(W);
			const LandBail::Crew Again = LandBail::Make(W);
			++Wrecks;
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
				if (bBrew && M.How != LandBail::End::Caught && M.Out + 2.05 > W.Blast + 1e-9)
				{
					std::printf("FAIL %s: a man still climbing at the blast is not caught\n", Way.Key.c_str());
					++Bad;
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
					const bool bDown = T > M.Out + 2.1 && (M.BackAt < 0.0 || T < M.BackAt) && !(M.How == LandBail::End::Caught && T < M.FallAt + 0.65);
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
	std::printf("\n%d wrecks, %d men: %s (%d faults)\n", Wrecks, Men, Bad ? "FAIL" : "ok", Bad);
	return Bad ? 1 : 0;
}
