// The looks picker's cloud test (Tools/fates/run.sh): the game's own Battle/Fates and its table, no engine.

#include "Battle/Fates.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Checks = 0, Faults = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Faults;
			std::printf("FAULT: %s\n", What.c_str());
		}
	}

	// Every step a scene is told to play: a typo in the table is a step no scene plays.
	const std::set<std::string> Steps = {
		"blast", "column", "fire", "smoke", "steam", "cookoff", "glance", "spark", "silence", "list", "right", "trim", "settle",
		"clip", "rock", "slew", "stop", "track_off", "turret_off", "hatch", "crew_out", "crew_in", "crew_up", "hold_fire",
		"gun_droop", "flip", "overturn", "man_fall", "drag", "crawl_back", "carry", "lie", "go_down", "duck", "prone",
		"creep_back", "run_back", "turn_fire", "drop_arms", "help", "officer_wave", "gather", "kneel", "swap", "abandon"
	};
	const std::set<std::string> Builtin = { "aspect", "weapon", "family", "size", "glance" };

	// What a blow at each way looks like, to prove each can be reached: its first family, first result, its how, and
	// the first weapon its weights allow.
	Fates::Blow BlowFor(const Fates::Way& W)
	{
		Fates::Blow B;
		B.Domain = W.Domain;
		B.Family = W.Families.front() == "*" ? std::string(W.Domain == "sea" ? "bb" : "tank_medium") : W.Families.front();
		B.Result = W.On.front();
		B.How = W.How;
		B.Weapon = W.Domain == "sea" ? "bomb" : "field";
		const auto Weapons = W.By.find("weapon");
		if (Weapons != W.By.end())
		{
			for (const auto& Pair : Weapons->second)
			{
				if (Pair.first != "*" && Pair.second > 0.0)
				{
					B.Weapon = Pair.first;
					break;
				}
			}
		}
		const auto Families = W.By.find("family");
		if (Families != W.By.end())
		{
			for (const std::string& F : W.Families)
			{
				const auto Got = Families->second.find(F);
				if (Got == Families->second.end() || Got->second > 0.0)
				{
					B.Family = F;
					break;
				}
			}
		}
		for (const auto& Flag : W.If)
		{
			if (Flag.second > 0.0)
			{
				(Flag.first == "harbour" ? B.bHarbour : Flag.first == "moving" ? B.bMoving : Flag.first == "cover" ? B.bCover : B.bNight) = true;
			}
		}
		for (const auto& Flag : W.Unless)
		{
			if (Flag.second <= 0.0)
			{
				(Flag.first == "harbour" ? B.bHarbour : Flag.first == "moving" ? B.bMoving : Flag.first == "cover" ? B.bCover : B.bNight) = true;
			}
		}
		B.Nerve = std::max(W.MinNerve, std::min(W.MaxNerve, 0.6));
		B.Hurt = std::max(W.MinHurt, std::min(W.MaxHurt, 0.3));
		return B;
	}

	double ShareOf(const std::vector<std::pair<std::string, double>>& Odds, const std::set<std::string>& Keys)
	{
		double All = 0.0, Of = 0.0;
		for (const auto& Pair : Odds)
		{
			All += Pair.second;
			if (Keys.count(Pair.first))
			{
				Of += Pair.second;
			}
		}
		return All > 0.0 ? Of / All : 0.0;
	}

	void Collect(const Fates::Json& J, std::vector<std::string>& Out)
	{
		if (J.IsString() && J.Text.size() > 1 && J.Text[0] == '$')
		{
			Out.push_back(J.Text[1] == '-' ? J.Text.substr(2) : J.Text.substr(1));
		}
		for (const Fates::Json& Item : J.Items)
		{
			Collect(Item, Out);
		}
		for (const Fates::Json& Item : J.Values)
		{
			Collect(Item, Out);
		}
	}

	std::string Line(const Fates::Fate& F)
	{
		std::ostringstream S;
		S << F.Key << (F.Aspect.empty() ? "" : " [" + F.Aspect + "]") << ": " << F.Words << "\n";
		for (const Fates::Beat& B : F.Beats)
		{
			char Buffer[256];
			std::snprintf(Buffer, sizeof(Buffer), "      %6.1f s  %-10s %-12s %-8s a %.2f b %.2f c %.2f n %d\n", B.T, B.Do.c_str(), B.At.c_str(), B.Kind.c_str(), B.A, B.B, B.C, B.N);
			S << Buffer;
		}
		return S.str();
	}
}

int main(int Count, char** Args)
{
	if (Count < 2)
	{
		std::printf("usage: fates_test <fates.json>\n");
		return 2;
	}
	std::ifstream In(Args[1]);
	std::stringstream Text;
	Text << In.rdbuf();
	Fates::Table Table;
	std::string Why;
	Check(Table.Load(Text.str(), Why), "the table loads: " + Why);
	if (Faults)
	{
		return 1;
	}
	std::printf("%zu looks in the table\n", Table.Ways().size());

	// 1. The table: every step is one a scene plays; every dial a step names exists; no look lights more than five
	// fires on a ship (the Pearl Harbor stage's room) at its most.
	for (const Fates::Way& W : Table.Ways())
	{
		std::vector<std::string> Named;
		Collect(W.Beats, Named);
		for (const std::string& N : Named)
		{
			Check(Builtin.count(N) || !W.Dials[N].IsNull(), W.Key + ": its steps name a dial it has not got: $" + N);
		}
		int MostFires = 0;
		for (const Fates::Json& Step : W.Beats.Items)
		{
			std::vector<std::string> Dos;
			if (Step["do"].IsString())
			{
				Dos.push_back(Step["do"].Text);
			}
			for (const Fates::Json& D : Step["do"].Items)
			{
				Dos.push_back(D.Str());
			}
			for (const std::string& D : Dos)
			{
				Check(Steps.count(D) > 0, W.Key + ": a step no scene plays: " + D);
			}
			if (Step["do"].Str() == "fire")
			{
				const Fates::Json& Times = Step["times"];
				MostFires += Times.IsArray() ? static_cast<int>(Times.At(1).Num(1)) : static_cast<int>(Times.Num(1));
			}
		}
		Check(W.Domain != "sea" || MostFires <= 5, W.Key + ": more than five fires on one ship");
	}

	// 2. Every look can be reached.
	{
		Fates::Director D(Table, 7);
		for (const Fates::Way& W : Table.Ways())
		{
			const Fates::Blow B = BlowFor(W);
			double Mine = 0.0;
			for (const auto& Pair : D.Odds(B))
			{
				if (Pair.first == W.Key)
				{
					Mine = Pair.second;
				}
			}
			Check(Mine > 0.0, W.Key + ": no blow reaches it (" + B.Family + ", " + B.Weapon + ", " + B.Result + ")");
		}
	}

	// 3. The same battle plays the same looks: two pickers with one seed, one run of blows.
	{
		Fates::Director A(Table, 1941), B(Table, 1941), C(Table, 1942);
		int Same = 0, Differ = 0;
		for (int K = 0; K < 400; ++K)
		{
			Fates::Blow Blow;
			Blow.Domain = K % 2 ? "sea" : "land";
			Blow.Family = K % 2 ? (K % 3 ? "ca" : "dd") : (K % 3 ? "tank_medium" : "infantry");
			Blow.Weapon = K % 2 ? (K % 5 ? "shell_medium" : "torpedo") : (K % 5 ? "field" : "howitzer");
			Blow.Result = K % 7 ? "worn" : "lost";
			Blow.Unit = K % 11;
			const Fates::Fate X = A.Choose(Blow), Y = B.Choose(Blow), Z = C.Choose(Blow);
			bool bSame = X.Key == Y.Key && X.Beats.size() == Y.Beats.size() && X.Dials == Y.Dials;
			for (size_t I = 0; bSame && I < X.Beats.size(); ++I)
			{
				bSame = X.Beats[I].T == Y.Beats[I].T && X.Beats[I].Do == Y.Beats[I].Do && X.Beats[I].A == Y.Beats[I].A;
			}
			Same += bSame ? 1 : 0;
			Differ += (X.Key != Z.Key) ? 1 : 0;
		}
		Check(Same == 400, "one seed, one run of blows, the same looks (" + std::to_string(Same) + " of 400)");
		Check(Differ > 100, "another battle's seed shows other looks (" + std::to_string(Differ) + " of 400 differ)");
	}

	// 4. Never the same look twice running in a group, for the same blow again and again and for a mixed run, and how
	// many looks each run shows.
	{
		struct Run
		{
			const char* Name;
			Fates::Blow Blow;
		};
		std::vector<Run> Runs;
		auto Add = [&Runs](const char* Name, const char* Domain, const char* Family, const char* Weapon, const char* Result, const char* How = "")
		{
			Run R;
			R.Name = Name;
			R.Blow.Domain = Domain;
			R.Blow.Family = Family;
			R.Blow.Weapon = Weapon;
			R.Blow.Result = Result;
			R.Blow.How = How;
			Runs.push_back(R);
		};
		Add("battleship hit by dive bombs", "sea", "bb", "bomb", "worn");
		Add("carrier hit by dive bombs", "sea", "cv", "bomb", "worn");
		Add("cruiser hit by a torpedo", "sea", "ca", "torpedo", "worn");
		Add("destroyer hit by a cruiser", "sea", "dd", "shell_medium", "worn");
		Add("battleships sunk (no how named)", "sea", "bb", "shell_heavy", "lost");
		Add("destroyers sunk by torpedo", "sea", "dd", "torpedo", "lost");
		Add("ships rolled over (how named)", "sea", "ca", "torpedo", "lost", "capsize");
		Add("ships out of the fight burning", "sea", "ca", "shell_heavy", "out");
		Add("medium tanks lost to a 75", "land", "tank_medium", "field", "lost");
		Add("heavy tanks lost to a howitzer", "land", "tank_heavy", "howitzer", "lost");
		Add("tanks hit, not lost", "land", "tank_medium", "light", "worn");
		Add("lorries lost", "land", "truck", "field", "lost");
		Add("guns lost", "land", "gun", "howitzer", "lost");
		Add("men lost to rifles", "land", "infantry", "smallarms", "lost");
		Add("men lost to mortars", "land", "infantry", "mortar", "lost");
		Add("men wavering", "land", "infantry", "smallarms", "waver");
		Add("men routing", "land", "infantry", "smallarms", "rout");
		for (const Run& R : Runs)
		{
			Fates::Director D(Table, 2024);
			std::map<std::string, int> Seen;
			std::string Last;
			int Repeats = 0, Empty = 0;
			const int Allowed = static_cast<int>(D.Odds(R.Blow).size());
			for (int K = 0; K < 300; ++K)
			{
				Fates::Blow B = R.Blow;
				B.Unit = K;
				const Fates::Fate F = D.Choose(B);
				if (F.Key.empty())
				{
					++Empty;
					continue;
				}
				Repeats += F.Key == Last ? 1 : 0;
				Last = F.Key;
				Seen[F.Key]++;
			}
			Check(Empty == 0, std::string(R.Name) + ": every blow has a look");
			Check(Allowed <= 1 || Repeats == 0, std::string(R.Name) + ": " + std::to_string(Repeats) + " times the same look twice running");
			std::printf("  %-34s %2d looks shown of %2d allowed:", R.Name, static_cast<int>(Seen.size()), Allowed);
			for (const auto& Pair : Seen)
			{
				std::printf(" %s %d", Pair.first.c_str() + Pair.first.find('.') + 1, Pair.second);
			}
			std::printf("\n");
			Check(static_cast<int>(Seen.size()) == Allowed, std::string(R.Name) + ": every allowed look is shown in 300");
		}
	}

	// 5. The looks follow the situation.
	{
		Fates::Director D(Table, 5);
		auto Share = [&D](Fates::Blow B, std::set<std::string> Keys) { return ShareOf(D.Odds(B), Keys); };
		Fates::Blow Bb;
		Bb.Domain = "sea";
		Bb.Family = "bb";
		Bb.Weapon = "bomb";
		const double ArmourBb = Share(Bb, { "ship.armour_holds" });
		Fates::Blow Dd = Bb;
		Dd.Family = "dd";
		Check(ArmourBb > 0.2 && Share(Dd, { "ship.armour_holds" }) == 0.0, "a dive bomber's bomb bursts on a battleship's armour, never on a destroyer's");
		Fates::Blow Torp = Bb;
		Torp.Family = "ca";
		Torp.Weapon = "torpedo";
		Check(Share(Torp, { "ship.torpedo_flooding", "ship.torpedo_bulge" }) > 0.55, "a torpedo mostly floods");
		Fates::Blow Cv = Bb;
		Cv.Family = "cv";
		Check(Share(Cv, { "ship.deck_park", "ship.hangar_fire", "ship.fire_spreads", "ship.fire_fought" }) > 0.55, "a bomb on a carrier mostly burns");
		Fates::Blow Steady = Cv, Shaken = Cv;
		Steady.Nerve = 1.0;
		Shaken.Nerve = 0.2;
		Check(Share(Steady, { "ship.fire_fought" }) > Share(Shaken, { "ship.fire_fought" }) && Share(Shaken, { "ship.fire_spreads", "ship.hangar_fire" }) > Share(Steady, { "ship.fire_spreads", "ship.hangar_fire" }),
			"a steady crew puts its fires out more often; a shaken one lets them spread");
		Fates::Blow Harbour = Bb;
		Harbour.Result = "lost";
		Harbour.How = "settle";
		Harbour.bHarbour = true;
		Check(Share(Harbour, { "ship.settle_upright", "ship.settle_listing" }) == 1.0, "in harbour, a ship the numbers settle settles");
		Harbour.bHarbour = false;
		Check(D.Odds(Harbour).empty(), "at sea, no look settles a ship on the bottom");

		Fates::Blow Tank;
		Tank.Domain = "land";
		Tank.Family = "tank_heavy";
		Tank.Weapon = "light";
		Tank.Aspect = "front";
		Fates::Blow Light = Tank;
		Light.Family = "tank_light";
		Check(Share(Tank, { "tank.glance" }) > Share(Light, { "tank.glance" }) * 1.6, "a light gun's shot glances off a heavy tank's front far more often than a light tank's");
		Fates::Blow Lost = Tank;
		Lost.Family = "tank_medium";
		Lost.Weapon = "field";
		Lost.Result = "lost";
		Lost.Aspect = "side";
		Fates::Blow Wet = Lost;
		Wet.Stowage = "wet";
		Check(Share(Wet, { "tank.brew_up" }) < Share(Lost, { "tank.brew_up" }) * 0.6, "wet stowage brews up less");
		Fates::Blow Diesel = Lost, Petrol = Lost;
		Diesel.Fuel = "diesel";
		Petrol.Fuel = "petrol";
		Check(Share(Petrol, { "tank.brew_up", "tank.engine_fire", "tank.knocked_out_later_burns" }) > Share(Diesel, { "tank.brew_up", "tank.engine_fire", "tank.knocked_out_later_burns" }), "a petrol engine burns more often than a diesel");
		Fates::Blow Rear = Lost;
		Rear.Aspect = "rear";
		Check(Share(Rear, { "tank.engine_fire" }) > Share(Lost, { "tank.engine_fire" }) * 1.8, "a hit from behind finds the engine");
		Fates::Blow Bomb = Lost;
		Bomb.Weapon = "bomb";
		Check(Share(Bomb, { "tank.turret_off", "tank.overturned" }) > Share(Lost, { "tank.turret_off", "tank.overturned" }) * 2.0, "a bomb throws turrets and tanks about far more than a 75");
		Fates::Blow Moving = Lost;
		Moving.Weapon = "howitzer";
		Moving.bMoving = true;
		Fates::Blow Still = Moving;
		Still.bMoving = false;
		Check(Share(Moving, { "tank.immobilised_abandoned" }) > Share(Still, { "tank.immobilised_abandoned" }) * 2.0, "a tank on the move is hit in the tracks more often");
		Fates::Blow Rifle;
		Rifle.Domain = "land";
		Rifle.Family = "tank_medium";
		Rifle.Weapon = "smallarms";
		Rifle.Result = "worn";
		Check(Share(Rifle, { "tank.rattle" }) == 1.0, "rifle fire only rattles off a tank");
		Rifle.Result = "lost";
		Check(D.Odds(Rifle).empty(), "rifle fire never knocks out a tank (the numbers never ask it)");
		Fates::Blow Shaky;
		Shaky.Domain = "land";
		Shaky.Family = "tank_medium";
		Shaky.Weapon = "field";
		Shaky.Result = "worn";
		Shaky.Nerve = 0.1;
		Fates::Blow Firm = Shaky;
		Firm.Nerve = 1.0;
		Check(Share(Shaky, { "tank.bail_and_back" }) > Share(Firm, { "tank.bail_and_back" }) * 3.0, "a shaken crew bails out and climbs back in; a steady one seldom does");
		Fates::Blow Man;
		Man.Domain = "land";
		Man.Family = "infantry";
		Man.Weapon = "smallarms";
		Man.Result = "lost";
		Check(Share(Man, { "man.thrown" }) == 0.0, "a bullet never throws a man off his feet");
		Man.Weapon = "howitzer";
		Check(Share(Man, { "man.thrown" }) > 0.2, "a shell often does");
	}

	// 6. Once only, and a ship floods on the side she was struck.
	{
		Fates::Director D(Table, 9);
		Fates::Blow B;
		B.Domain = "land";
		B.Family = "tank_medium";
		B.Weapon = "field";
		B.Result = "worn";
		B.Nerve = 0.05;
		B.Unit = 77;
		int Back = 0;
		for (int K = 0; K < 200; ++K)
		{
			Back += D.Choose(B).Key == "tank.bail_and_back" ? 1 : 0;
		}
		Check(Back == 1, "a crew bails out and climbs back in once on one tank (" + std::to_string(Back) + ")");
		Fates::Director S(Table, 11);
		Fates::Blow T;
		T.Domain = "sea";
		T.Family = "bb";
		T.Weapon = "torpedo";
		T.Unit = 5;
		std::set<std::string> Sides;
		for (int K = 0; K < 60; ++K)
		{
			const Fates::Fate F = S.Choose(T);
			if (F.Key == "ship.torpedo_flooding" || F.Key == "ship.torpedo_bulge")
			{
				Sides.insert(F.DialText("side"));
			}
		}
		Check(Sides.size() == 1, "her flooding stays on one side");
	}

	// 7. A ship's pose: a list taken, most of it taken back, within what a ship afloat shows.
	{
		Fates::Fate F;
		F.Beats.push_back({ 0.0, "list", "", "", 10.0, 1.0, 30.0, 0 });
		F.Beats.push_back({ 60.0, "right", "", "", 0.3, 0.0, 60.0, 0 });
		F.Beats.push_back({ 0.0, "trim", "", "", 2.0, 0.0, 40.0, 0 });
		F.Beats.push_back({ 0.0, "settle", "", "", 9.0, 0.0, 40.0, 0 });
		Fates::Hull H;
		H.Take(F, 100.0);
		double L = 0, Tr = 0, Se = 0;
		H.Pose(100.0, L, Tr, Se);
		Check(L == 0.0 && Tr == 0.0 && Se == 0.0, "a pose starts level");
		H.Pose(140.0, L, Tr, Se);
		Check(std::fabs(L - 10.0) < 1e-6 && std::fabs(Tr - 2.0) < 1e-6, "she lists 10 degrees to starboard and trims 2 by the bow");
		Check(Se <= H.MostSettle + 1e-9, "she settles no deeper than a ship afloat");
		H.Pose(400.0, L, Tr, Se);
		Check(std::fabs(L - 3.0) < 1e-6, "counter-flooded, she keeps 3 of her 10 degrees");
		Fates::Fate G;
		for (int K = 0; K < 6; ++K)
		{
			G.Beats.push_back({ 0.0, "list", "", "", 9.0, -1.0, 10.0, 0 });
		}
		Fates::Hull Over;
		Over.Take(G, 0.0);
		Over.Pose(100.0, L, Tr, Se);
		Check(L >= -Over.MostList - 1e-9, "a list never passes what a ship afloat shows");
		// (the pre-check's case, 20 November: twelve hits 2 to 60 s apart, a new list easing in while an earlier
		// counter-flooding is still easing out; the limits hold at every second, not only on each step's aim)
		double Most = 0.0, MostTr = 0.0, MostSe = 0.0;
		for (uint64_t Run = 1; Run <= 2000; ++Run)
		{
			Fates::Director D(Table, Run * 7919u);
			Fates::Dice Gaps(Run);
			Fates::Hull Her;
			Fates::Blow B;
			B.Domain = "sea";
			B.Family = Run % 2 ? "bb" : "ca";
			B.Weapon = "torpedo";
			B.Unit = 9;
			double At = 0.0;
			for (int K = 0; K < 12; ++K)
			{
				B.Hurt = K / 12.0;
				Her.Take(D.Choose(B), At);
				At += Gaps.U(2.0, 60.0);
			}
			for (double T = 0.0; T < At + 240.0; T += 1.0)
			{
				Her.Pose(T, L, Tr, Se);
				Most = std::max(Most, std::fabs(L));
				MostTr = std::max(MostTr, std::fabs(Tr));
				MostSe = std::max(MostSe, Se);
			}
		}
		Check(Most <= 18.0 + 1e-9 && MostTr <= 6.0 + 1e-9 && MostSe <= 4.0 + 1e-9,
			"twelve torpedoes on one ship, sampled every second: list, trim and settle stay in bounds (" + std::to_string(Most) + ", " + std::to_string(MostTr) + ", " + std::to_string(MostSe) + ")");
	}

	// 7b. A named look, for a test reel that films each in turn.
	{
		Fates::Director D(Table, 77);
		Fates::Blow B;
		B.Domain = "sea";
		B.Family = "bb";
		B.Weapon = "torpedo";
		B.Unit = 4;
		const Fates::Fate F = D.Show(B, "ship.torpedo_flooding");
		Check(F.Key == "ship.torpedo_flooding" && !F.Beats.empty(), "a reel can ask for a torpedo's flooding by name");
		Check(D.Show(B, "ship.deck_park").Key.empty(), "a reel cannot put a carrier's deck park on a battleship");
		B.Weapon = "bomb";
		Check(D.Show(B, "ship.torpedo_flooding").Key.empty(), "nor a torpedo's flooding on a bomb hit");
	}

	// 8. A sample, in plain words (FATES_STORY=1).
	if (const char* Story = std::getenv("FATES_STORY"); Story && Story[0] == '1')
	{
		Fates::Director D(Table, 1942);
		std::printf("\nA cruiser hit again and again by a battleship's shells, then sunk:\n");
		Fates::Blow B;
		B.Domain = "sea";
		B.Family = "ca";
		B.Weapon = "shell_heavy";
		B.Unit = 3;
		for (int K = 0; K < 5; ++K)
		{
			B.Hurt = K * 0.18;
			B.Nerve = 0.8 - K * 0.12;
			std::printf("  %s", Line(D.Choose(B)).c_str());
		}
		B.Result = "lost";
		std::printf("  %s", Line(D.Choose(B)).c_str());
		std::printf("\nEight tanks of a block lost, one by one, to 75 mm guns:\n");
		Fates::Blow T;
		T.Domain = "land";
		T.Family = "tank_medium";
		T.Weapon = "field";
		T.Result = "lost";
		T.Unit = 40;
		for (int K = 0; K < 8; ++K)
		{
			T.Model = K;
			T.bMoving = K % 3 == 0;
			std::printf("  %s", Line(D.Choose(T)).c_str());
		}
	}

	std::printf("%d checks, %d faults\n", Checks, Faults);
	return Faults ? 1 : 0;
}
