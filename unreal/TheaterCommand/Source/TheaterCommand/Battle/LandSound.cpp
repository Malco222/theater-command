#include "Battle/LandSound.h"

#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>

namespace LandSound
{
	using LandJson::Json;
	using LandPlay::Noise;
	using LandPlay::NoiseKind;

	namespace
	{
		const std::string& NoTake()
		{
			static const std::string Empty;
			return Empty;
		}

		/** A list of takes as the mix writes it: names, and "order_english_01..17" for a numbered run. */
		std::vector<std::string> Takes(const Json& J)
		{
			std::vector<std::string> Out;
			if (J.IsString())
			{
				Out.push_back(J.Text);
				return Out;
			}
			for (const Json& T : J.Items)
			{
				if (!T.IsString()) continue;
				const std::string& S = T.Text;
				const size_t Dots = S.find("..");
				if (Dots == std::string::npos)
				{
					Out.push_back(S);
					continue;
				}
				// "stem_01..17": the stem, the first number as written (its width), the last.
				const size_t Under = S.rfind('_', Dots);
				if (Under == std::string::npos) continue;
				const std::string Stem = S.substr(0, Under + 1);
				const std::string First = S.substr(Under + 1, Dots - Under - 1);
				const int A = std::atoi(First.c_str());
				const int B = std::atoi(S.substr(Dots + 2).c_str());
				for (int K = A; K <= B; ++K)
				{
					std::string Number = std::to_string(K);
					while (Number.size() < First.size()) Number = "0" + Number;
					Out.push_back(Stem + Number);
				}
			}
			return Out;
		}

		double Ease(double Value, double Target, double Seconds, double Dt)
		{
			if (Seconds <= 1e-6) return Target;
			return Value + (Target - Value) * (1.0 - std::exp(-Dt / Seconds));
		}

		double Distance(const Vec& A, const Vec& B)
		{
			return (A - B).Length();
		}
	}

	double Mixer::Roll()
	{
		// splitmix64: the picks repeat run to run for one seed.
		Dice += 0x9E3779B97F4A7C15ull;
		uint64_t Z = Dice;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		Z ^= Z >> 31;
		return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
	}

	std::vector<std::string> Mixer::ListOf(const std::string& Name) const
	{
		auto It = Lists.find(Name);
		return It == Lists.end() ? std::vector<std::string>() : It->second;
	}

	const std::string& Mixer::Pick(const std::string& ListName)
	{
		auto It = Lists.find(ListName);
		if (It == Lists.end() || It->second.empty()) return NoTake();
		const std::vector<std::string>& L = It->second;
		// Never the same take twice running (variety.neverSameTakeTwice), where the list has another.
		std::string& Last = LastOfList[ListName];
		size_t K = static_cast<size_t>(Roll() * static_cast<double>(L.size())) % L.size();
		if (L.size() > 1 && L[K] == Last) K = (K + 1 + static_cast<size_t>(Roll() * static_cast<double>(L.size() - 1))) % L.size();
		if (L.size() > 1 && L[K] == Last) K = (K + 1) % L.size();
		Last = L[K];
		return L[K];
	}

	const Family& Mixer::FamilyOf(const std::string& Name) const
	{
		static const Family Plain;
		auto It = Families.find(Name);
		return It == Families.end() ? Plain : It->second;
	}

	bool Mixer::Begin(const std::string& MixText, uint64_t Seed, std::string& OutError)
	{
		Json M;
		if (!LandJson::Parse(MixText, M, OutError)) return false;
		Dice ^= Seed * 0xD6E8FEB86659FD93ull;
		const Json& D = M["distance"];
		for (size_t K = 0; K < D.Keys.size(); ++K)
		{
			const Json& F = D.Values[K];
			if (!F.IsObject() || !F["silent"].IsNumber()) continue;
			Family Row;
			Row.Full = F["full"].Num(25.0);
			Row.Silent = F["silent"].Num(700.0);
			Row.FarFrom = F["farTakesFrom"].Num(1e9);
			Families[D.Keys[K]] = Row;
		}
		const Json& Ftb = M["flashToBang"];
		FtbFrom = Ftb["from"].Num(FtbFrom);
		FtbSpeed = Ftb["metresPerSecond"].Num(FtbSpeed);
		FtbMost = Ftb["maxSeconds"].Num(FtbMost);
		HeightFactor = M["listener"]["heightFactor"].Num(HeightFactor);
		const Json& I = M["intensity"];
		Window = I["window"].Num(Window);
		Radius = I["radius"].Num(Radius);
		BlockRadius = I["blockRadius"].Num(BlockRadius);
		PerBlock = I["perBlockFighting"].Num(PerBlock);
		FullAt = I["fullAt"].Num(FullAt);
		Rise = I["rise"].Num(Rise);
		Fall = I["fall"].Num(Fall);
		for (size_t K = 0; K < I["weights"].Keys.size(); ++K) Weights[I["weights"].Keys[K]] = I["weights"].Values[K].Num(1.0);
		for (const Json& B : M["beds"]["list"].Items)
		{
			if (B["air"].Truth(false) || B.Has("status")) continue;
			BedRow Row{ B["name"].Str(""), B["in"].Num(0.0), B["full"].Num(0.0), B["hold"].Num(1.0), B["out"].Num(1.01), B["gainDb"].Num(0.0) };
			if (Row.Name == "front_rumble_loop") RumbleDb = Row.Db;
			Beds.push_back(Row);
		}
		const Json& Place = M["beds"]["place"];
		PlaceOpen = Place["byCover"]["open"].Str(PlaceOpen.c_str());
		PlaceWoods = Place["byCover"]["woods"].Str(PlaceWoods.c_str());
		PlaceDb = Place[PlaceOpen.c_str()]["gainDb"].Num(PlaceDb);
		PlaceFullDb = Place[PlaceOpen.c_str()]["atFullDb"].Num(PlaceFullDb);
		PlaceTownDb = Place[PlaceOpen.c_str()]["buildingsDb"].Num(PlaceTownDb);
		const Json& L = M["limits"];
		for (size_t K = 0; K < L.Keys.size(); ++K) if (L.Values[K].IsNumber()) Limits[L.Keys[K]] = L.Values[K].Int(10);
		WorldTotal = L["worldTotal"].Int(WorldTotal);

		// The takes, by list name.
		const Json& N = M["byNation"];
		for (const char* Kind : { "rifle", "bolt", "mg", "smg", "orders", "cheer" })
		{
			const Json& Row = N[Kind];
			for (size_t K = 0; K < Row.Keys.size(); ++K)
			{
				const Json& V = Row.Values[K];
				// "CAN": "UK" names another row
				if (V.IsString() && Row[V.Text.c_str()].IsArray()) Lists[std::string("byNation.") + Kind + "." + Row.Keys[K]] = Takes(Row[V.Text.c_str()]);
				else if (V.IsArray()) Lists[std::string("byNation.") + Kind + "." + Row.Keys[K]] = Takes(V);
			}
		}
		const Json& Fm = M["fireModel"];
		Lists["rifle.far"] = Takes(Fm["inf"]["rifle"]["far"]);
		Lists["mg.far"] = Takes(Fm["inf"]["mg"]["far"]);
		Lists["tank.close"] = Takes(Fm["arm"]["gun"]["close"]);
		Lists["tank.far"] = Takes(Fm["arm"]["gun"]["far"]);
		Lists["tank.case"] = Takes(Fm["arm"]["gun"]["then"]["takes"]);
		CaseWithin = Fm["arm"]["gun"]["then"]["onlyCloserThan"].Num(CaseWithin);
		Lists["gun.close"] = Takes(Fm["art"]["gun"]["close"]);
		Lists["gun.far"] = Takes(Fm["art"]["gun"]["far"]);
		Lists["incoming"] = Takes(Fm["incoming"]["takes"]);
		Lists["grenade.throw"] = Takes(Fm["inf"]["grenade"]["takes"]);
		BoltWithin = Fm["inf"]["bolt"]["afterRifleWhenCloserThan"].Num(BoltWithin);
		BoltChance = Fm["inf"]["bolt"]["chance"].Num(BoltChance);
		const Json& Im = M["impacts"];
		NearWithin = Im["near"]["within"].Num(NearWithin);
		MiddleWithin = Im["middle"]["within"].Num(MiddleWithin);
		Lists["impact.near"] = Takes(Im["near"]["takes"]);
		Lists["impact.middle"] = Takes(Im["middle"]["takes"]);
		Lists["impact.far"] = Takes(Im["far"]["takes"]);
		const Json& Db = Im["debris"];
		Lists["impact.debris"] = Takes(Db["take"]);
		DebrisFrom = Db["from"].Num(DebrisFrom);
		DebrisTo = Db["to"].Num(DebrisTo);
		DebrisDb = Db["db"].Num(DebrisDb);
		DebrisFadeIn = Db["fadeIn"].Num(DebrisFadeIn);
		Debris0 = Db["after"].At(0).Num(Debris0);
		Debris1 = Db["after"].At(1).Num(Debris1);
		NearTwoCut = Db["cutAt"]["shell_impact_near_02"].Num(NearTwoCut);
		NearTwoCutFade = Db["cutFade"].Num(NearTwoCutFade);
		const Json& Sizes = Im["bySize"];
		for (size_t K = 0; K < Sizes.Keys.size(); ++K)
		{
			const Json& S = Sizes.Values[K];
			if (!S.IsObject()) continue;
			SizeDb[Sizes.Keys[K]] = S["db"].Num(0.0);
			SizeBig[Sizes.Keys[K]] = S["bigWithin"].Num(60.0);
			if (S["takes"].IsArray()) Lists["size." + Sizes.Keys[K]] = Takes(S["takes"]);
		}
		const Json& Gone = M["events"]["gone"];
		Lists["fall"] = Takes(Gone["close"]);
		Lists["cry"] = Takes(Gone["cry"]);
		CryChance = Gone["cryChance"].Num(CryChance);
		CryBelow = Gone["cryOnlyCameraBelow"].Num(CryBelow);
		Lists["whistle"] = Takes(M["events"]["contact"]["attacker"]);
		Lists["big"] = Takes(M["big"]["names"]);
		Lists["knocked_out"] = { "tank_knocked_out", "tank_knocked_out_02", "tank_knocked_out_03" };
		Lists["ricochet"] = { "ricochet_01" };
		const Json& V = M["variety"];
		LevelDb = V["levelDb"].Num(LevelDb);
		for (size_t K = 0; K < V["pitchPercent"].Keys.size(); ++K) PitchPercent[V["pitchPercent"].Keys[K]] = V["pitchPercent"].Values[K].Num(0.0);
		const Json& Sh = M["shellShock"];
		ShockWithin = Sh["within"].Num(ShockWithin);
		ShockBelow = Sh["cameraBelow"].Num(ShockBelow);
		ShockLpf = Sh["lpfHz"].Num(ShockLpf);
		ShockDown = Sh["downMs"].Num(ShockDown * 1000.0) / 1000.0;
		ShockHold = Sh["holdMs"].Num(ShockHold * 1000.0) / 1000.0;
		ShockBack = Sh["backMs"].Num(ShockBack * 1000.0) / 1000.0;
		ShockDb = Sh["worldDb"].Num(ShockDb);
		ShockEvery = Sh["notMoreThanOnceIn"].Num(ShockEvery);
		if (Lists["byNation.rifle.*"].empty())
		{
			OutError = "the mix has no rifle takes (byNation.rifle)";
			return false;
		}
		return true;
	}

	double Mixer::FlashToBang(double Far, double Speed) const
	{
		if (Far <= FtbFrom) return 0.0;
		return std::min(FtbMost, Far / std::max(1.0, FtbSpeed)) / std::max(0.05, Speed);
	}

	void Mixer::Count(double Now, const std::string& WeightKey, double Far)
	{
		auto It = Weights.find(WeightKey);
		const double W = It == Weights.end() ? 1.0 : It->second;
		const double Close = std::clamp(1.0 - Far / std::max(1.0, Radius), 0.0, 1.0);
		if (Close > 0.0) { HeardList.push_back({ Now, W * Close }); HeatBy[WeightKey] += W * Close; }
	}

	bool Mixer::Start(Take T, double Now, double Far, bool bOnScreen, std::vector<Take>& Out)
	{
		bLastTooFar = false;
		if (T.Name.empty()) return false;
		const Family& F = FamilyOf(T.Family);
		// Past the family's silent distance the mix's own law has it at nothing: not a drop, out of hearing.
		if (T.bSpatial && Far > F.Silent && !T.bBig)
		{
			bLastTooFar = true;
			return false;
		}
		// The limits: a family full drops the farthest and quietest first; never a take on the screen (the director).
		std::string Key = T.Family;
		if (T.Family == "small_arms") Key = Far > F.FarFrom ? "small_arms_far" : "small_arms_close";
		std::vector<Playing>& Now_ = PlayingNow[Key];
		Now_.erase(std::remove_if(Now_.begin(), Now_.end(), [&](const Playing& P) { return P.Ends <= Now; }), Now_.end());
		int Total = 0;
		for (auto& Pair : PlayingNow)
		{
			Pair.second.erase(std::remove_if(Pair.second.begin(), Pair.second.end(), [&](const Playing& P) { return P.Ends <= Now; }), Pair.second.end());
			Total += static_cast<int>(Pair.second.size());
		}
		auto Limit = Limits.find(Key);
		const bool bFull = (Limit != Limits.end() && static_cast<int>(Now_.size()) >= Limit->second) || Total >= WorldTotal;
		if (bFull && !bOnScreen && !T.bBig)
		{
			double Farthest = 0.0;
			for (const Playing& P : Now_) Farthest = std::max(Farthest, P.Far);
			if (Far >= Farthest) return false;
		}
		// The same take never starts twice within 0.15 s: another take of its list if it has one, else a moment later.
		double& Last = TakeStarted[T.Name];
		const double StartsAt = Now + T.Delay;
		if (StartsAt < Last + 0.15)
		{
			T.Delay += Last + 0.15 - StartsAt;
			++Delayed;
		}
		Last = Now + T.Delay;
		// Variety: a little pitch and level each time.
		auto P = PitchPercent.find(T.Family == "small_arms" || T.Family == "mg" ? T.Family : T.Family == "big_guns" ? "big_guns" : T.Family == "explosions" ? "explosions" : T.Family == "body" ? "body" : "none");
		const double Pitch = P == PitchPercent.end() ? 0.0 : P->second;
		T.Pitch *= 1.0 + (Roll() * 2.0 - 1.0) * Pitch * 0.01;
		T.Db += (Roll() * 2.0 - 1.0) * LevelDb * 0.5;
		const double Long = Length ? Length(T.Name) : 1.5;
		Now_.push_back({ Now + T.Delay + (T.Cut > 0.0 ? T.Cut : std::max(0.1, Long - T.From)), Far });
		++TakesByName[T.Name];
		Out.push_back(std::move(T));
		return true;
	}

	void Mixer::Crack(const Noise& N, double Now, const Listener& Ear, std::vector<Take>& Out)
	{
		// The crack of rounds passing near the ear: a rifle or machine-gun bullet is faster than sound, and its shock
		// wave cracks where it passes. Only rounds whose line comes within 12 m of the ear, from a shooter more than
		// 25 m off, at most one crack in a quarter second.
		const Vec Line = N.To - N.From;
		const double Long = Line.Length();
		if (Long < 1.0) return;
		const Vec Way = Line * (1.0 / Long);
		const double Along = std::clamp((Ear.At - N.From).Dot(Way), 0.0, Long);
		const Vec Nearest = N.From + Way * Along;
		const double Miss = Distance(Nearest, Ear.At);
		if (Miss > 12.0 || Distance(N.From, Ear.At) < 25.0 || Now - CrackAt < 0.25) return;
		CrackAt = Now;
		Take T;
		// (a burst's rounds pass as a run of cracks: the pack's incoming takes; a single round its own crack)
		T.Name = N.Weapon == "mg" ? Pick("incoming") : std::string("bullet_crack");
		T.At = Nearest;
		T.Family = "small_arms";
		T.Delay = Along / 760.0;
		T.Db = -2.0 - Miss * 0.3;
		if (Start(T, Now, Miss, true, Out)) ++Cracks;
	}

	void Mixer::Hear(const Noise& N, double Now, double Battle, double Speed, const Listener& Ear, std::vector<Take>& Out)
	{
		const double Far = Distance(N.At, Ear.At);
		const bool bOnScreen = OnScreen ? OnScreen(N.At) : true;
		// (a noise from earlier in this frame is at most a frame behind: started now)
		(void)Battle;
		const std::string Army = N.Army.empty() ? std::string("*") : N.Army;
		auto Nation = [&](const char* Kind) -> std::string
		{
			const std::string Key = std::string("byNation.") + Kind + "." + Army;
			return Lists.count(Key) ? Key : std::string("byNation.") + Kind + ".*";
		};
		Take T;
		T.At = N.At;
		T.Track = N.Track;
		switch (N.Kind)
		{
		case NoiseKind::Shot:
		{
			++ShotsHeard;
			bool bStarted = false;
			if (N.Weapon == "rifle" || N.Weapon == "mg")
			{
				const bool bMg = N.Weapon == "mg";
				T.Family = bMg ? "mg" : "small_arms";
				const Family& F = FamilyOf(T.Family);
				T.Name = Pick(Far > F.FarFrom ? (bMg ? "mg.far" : "rifle.far") : Nation(bMg ? "mg" : "rifle"));
				bStarted = Start(T, Now, Far, bOnScreen, Out);
				Count(Now, bMg ? "mg" : "small_arms", Far);
				if (!bMg && N.Track >= 0)
				{
					// The Garand's clip pings out after its eighth round; a bolt rifle's bolt is worked (close by).
					const int Shots = ++RifleCount[N.Track];
					if (Army == "USA" && Shots % 8 == 0 && Far < 60.0)
					{
						Take Ping = T;
						Ping.Name = "garand_clip_ping";
						Ping.Family = "body";
						Ping.Delay = 0.12;
						Start(Ping, Now, Far, bOnScreen, Out);
					}
					else if (Army != "USA" && Far < BoltWithin && Roll() < BoltChance)
					{
						Take Bolt = T;
						Bolt.Name = Pick(Nation("bolt"));
						Bolt.Family = "body";
						Bolt.Delay = 0.55 + Roll() * 0.3;
						Start(Bolt, Now, Far, bOnScreen, Out);
					}
				}
				Crack(N, Now, Ear, Out);
			}
			else if (N.Weapon == "tank" || N.Weapon == "at")
			{
				T.Family = "big_guns";
				T.Name = Pick(Far > FamilyOf("big_guns").FarFrom ? "tank.far" : "tank.close");
				if (N.Weapon == "at") T.Db = -8.0; // a launcher: no recording of its own yet, the tank gun well under
				T.Delay = FlashToBang(Far, Speed);
				bStarted = Start(T, Now, Far, bOnScreen, Out);
				Count(Now, "big_guns", Far);
				if (N.Weapon == "tank" && Far < CaseWithin)
				{
					Take Case = T;
					Case.Name = Pick("tank.case");
					Case.Family = "body";
					Case.Db = 0.0;
					Case.Delay = 0.6 + Roll() * 0.3;
					Start(Case, Now, Far, bOnScreen, Out);
				}
			}
			else if (N.Weapon == "gun")
			{
				T.Family = "big_guns";
				T.Name = Pick(Far > FamilyOf("big_guns").FarFrom ? "gun.far" : "gun.close");
				T.Delay = FlashToBang(Far, Speed);
				bStarted = Start(T, Now, Far, bOnScreen, Out);
				Count(Now, "big_guns", Far);
			}
			else if (N.Weapon == "mortar")
			{
				T.Family = "big_guns";
				T.Name = "mortar_fire";
				T.Db = -4.0;
				T.Delay = FlashToBang(Far, Speed);
				bStarted = Start(T, Now, Far, bOnScreen, Out);
				Count(Now, "big_guns", Far);
			}
			else if (N.Weapon == "grenade")
			{
				T.Family = "body";
				T.Name = Pick("grenade.throw");
				bStarted = Start(T, Now, Far, bOnScreen, Out);
			}
			if (bStarted) ++ShotTakes;
			else if (bLastTooFar) ++ShotsOutOfHearing;
			else
			{
				++ShotsDropped;
				if (bOnScreen) ++ShotsOnScreenDropped;
			}
			break;
		}
		case NoiseKind::Landing:
		{
			++Landings;
			if (N.Size == "ap")
			{
				// An armour-piercing round into the dirt: no blast. The pack has no recording of a round into earth
				// yet (the director's list of what is missing), so a near one ricochets off the ground, a far one is
				// silent.
				if (Far < 120.0)
				{
					T.Name = Pick("ricochet");
					T.Family = "small_arms";
					T.Db = -4.0;
					Start(T, Now, Far, bOnScreen, Out);
				}
				break;
			}
			const std::string Size = N.Size == "tank_he" ? std::string("field_gun") : N.Size;
			T.Family = "explosions";
			T.Db = SizeDb.count(Size) ? SizeDb[Size] : 0.0;
			T.Delay = FlashToBang(Far, Speed);
			const double BigWithin = SizeBig.count(Size) ? SizeBig[Size] : NearWithin;
			if (Lists.count("size." + Size))
			{
				T.Name = Pick("size." + Size);
				T.bBig = Far < BigWithin;
				T.Bus = T.bBig ? "big" : "close";
				Start(T, Now, Far, bOnScreen, Out);
			}
			else if (Far < NearWithin)
			{
				T.Name = Pick("impact.near");
				T.bBig = Far < BigWithin;
				T.Bus = T.bBig ? "big" : "close";
				if (T.Name == "shell_impact_near_02") { T.Cut = NearTwoCut; T.CutFade = NearTwoCutFade; }
				Start(T, Now, Far, bOnScreen, Out);
				// Earth coming down, as the picture's clods land.
				if (T.Name != "explosion_debris")
				{
					Take Earth = T;
					Earth.Name = Pick("impact.debris");
					Earth.From = DebrisFrom;
					Earth.Cut = DebrisTo - DebrisFrom;
					Earth.CutFade = 0.3;
					Earth.FadeIn = DebrisFadeIn;
					Earth.Db = T.Db + DebrisDb - 12.0;
					Earth.bBig = false;
					Earth.Bus = "close";
					const bool bBigGun = Size == "howitzer_105" || Size == "howitzer_150";
					Earth.Delay = T.Delay + (bBigGun ? 1.2 + Roll() * 0.3 : Debris0 + Roll() * (Debris1 - Debris0));
					Start(Earth, Now, Far, bOnScreen, Out);
				}
			}
			else
			{
				T.Name = Pick(Far < MiddleWithin ? "impact.middle" : "impact.far");
				Start(T, Now, Far, bOnScreen, Out);
			}
			Count(Now, T.bBig ? "big" : "explosions", Far);
			if (Far < ShockWithin && Ear.CameraHeight < ShockBelow && Now - ShockAt > ShockEvery)
			{
				ShockAt = Now;
				bShockDue = true;
			}
			break;
		}
		case NoiseKind::Glance:
		{
			T.Name = Pick("ricochet");
			T.Family = N.Weapon == "rifle" || N.Weapon == "mg" ? "small_arms" : "big_guns";
			T.Db = N.Weapon == "rifle" || N.Weapon == "mg" ? -6.0 : 0.0;
			T.Delay = T.Family == "big_guns" ? FlashToBang(Far, Speed) : 0.0;
			Start(T, Now, Far, bOnScreen, Out);
			break;
		}
		case NoiseKind::Holed:
		{
			// A round into a tank short of a kill: the crack of the strike (the pack has no clang of its own yet).
			T.Name = "shell_impact_near_03";
			T.Family = "explosions";
			T.Db = -6.0;
			T.Delay = FlashToBang(Far, Speed);
			Start(T, Now, Far, bOnScreen, Out);
			Count(Now, "explosions", Far);
			break;
		}
		case NoiseKind::Fall:
		{
			++Falls;
			T.Family = "body";
			if (!bThudFromClips)
			{
				T.Name = Pick("fall");
				T.Delay = 0.7;
				Start(T, Now, Far, bOnScreen, Out);
			}
			// Sometimes a cry (Brian chose "Sometimes"), only with the camera low.
			if (Ear.CameraHeight < CryBelow && Roll() < CryChance)
			{
				Take Cry = T;
				Cry.Name = Pick("cry");
				Cry.Family = "voices";
				Cry.Bus = "voices";
				Cry.Delay = 0.1 + Roll() * 0.3;
				Start(Cry, Now, Far, bOnScreen, Out);
			}
			break;
		}
		case NoiseKind::KnockedOut:
		{
			T.Name = Pick("knocked_out");
			T.Family = "explosions";
			T.Bus = "big";
			T.bBig = true;
			T.Delay = FlashToBang(Far, Speed);
			Start(T, Now, Far, true, Out);
			Count(Now, "big", Far);
			break;
		}
		case NoiseKind::Contact:
		{
			// The attacker's whistle, then one order in his own language where the pack has it.
			if (N.Side != 0) break;
			T.Name = Pick("whistle");
			T.Family = "voices";
			T.Bus = "voices";
			Start(T, Now, Far, bOnScreen, Out);
			const std::string Orders = Nation("orders");
			if (!ListOf(Orders).empty())
			{
				Take Order = T;
				Order.Name = Pick(Orders);
				Order.Delay = 1.4 + Roll() * 0.6;
				Start(Order, Now, Far, bOnScreen, Out);
			}
			break;
		}
		case NoiseKind::Broken:
		{
			T.Name = Pick(Nation("cheer"));
			T.Family = "voices";
			T.Bus = "voices";
			T.bSpatial = false;
			T.Db = -8.0;
			T.Delay = 1.0;
			Start(T, Now, 0.0, true, Out);
			break;
		}
		}
	}

	void Mixer::HearBody(const std::string& Cue, const std::string& Army, const Vec& At, double Now, const Listener& Ear, std::vector<Take>& Out)
	{
		const double Far = Distance(At, Ear.At);
		if (Far > FamilyOf("body").Silent) return;
		Take T;
		T.At = At;
		T.Family = "body";
		if (Cue.rfind("footstep", 0) == 0)
		{
			static const std::vector<std::string> Steps = { "footstep_01", "footstep_02", "footstep_03", "footstep_04", "footstep_05", "footstep_06", "footstep_07", "footstep_08" };
			if (!Lists.count("footstep")) Lists["footstep"] = Steps;
			T.Name = Pick("footstep");
			T.Db = -4.0;
		}
		else if (Cue.rfind("kit_rattle", 0) == 0)
		{
			if (!Lists.count("kit")) Lists["kit"] = { "kit_rattle_01", "kit_rattle_02", "kit_rattle_03" };
			T.Name = Pick("kit");
		}
		else if (Cue.rfind("body_fall", 0) == 0)
		{
			T.Name = Pick("fall");
		}
		else if (Cue.rfind("bolt", 0) == 0)
		{
			const std::string Key = "byNation.bolt." + (Army.empty() ? std::string("*") : Army);
			if (Army == "USA") return; // the Garand has no bolt to work
			T.Name = Pick(Lists.count(Key) ? Key : std::string("byNation.bolt.*"));
		}
		else if (Cue == "knee_down") T.Name = "knee_down";
		else if (Cue.rfind("grenade_throw", 0) == 0) T.Name = "grenade_throw";
		else if (Cue == "rifle_drop") T.Name = Army == "GER" ? "german_drop_weapon" : "";
		else T.Name = Cue;
		Start(T, Now, Far, OnScreen ? OnScreen(At) : true, Out);
	}

	void Mixer::Tick(double Now, double RealDt, const Listener& Ear, int Fighting, double FightingShare, bool bContact, const std::string& Cover, std::vector<Bed>& OutBeds)
	{
		HeardList.erase(std::remove_if(HeardList.begin(), HeardList.end(), [&](const Heard& H) { return H.At < Now - Window; }), HeardList.end());
		double Heat = PerBlock * Fighting;
		for (const Heard& H : HeardList) Heat += H.Weight;
		HeatNow = Heat;
		double Target = std::min(1.0, Heat / std::max(1.0, FullAt));
		if (Ear.CameraHeight > 400.0) Target = std::max(Target, FightingShare * 0.5);
		MeterNow = Ease(MeterNow, Target, Target > MeterNow ? Rise : Fall, RealDt);

		OutBeds.clear();
		for (const BedRow& B : Beds)
		{
			Bed Row;
			Row.Name = B.Name;
			double Share = 0.0;
			const double M = MeterNow;
			if (M >= B.Full && M <= B.Hold) Share = 1.0;
			else if (M > B.In && M < B.Full) Share = (M - B.In) / std::max(1e-6, B.Full - B.In);
			else if (M > B.Hold && M < B.Out) Share = 1.0 - (M - B.Hold) / std::max(1e-6, B.Out - B.Hold);
			if (!bContact)
			{
				// Before the armies meet: the war somewhere else (the far rifles low, the front's rumble).
				Share = B.Name == "rifles_far_loop" || B.Name == "front_rumble_loop" ? 1.0 : 0.0;
			}
			if (Share > 0.001)
			{
				Row.bSilent = false;
				const double Db = !bContact && B.Name == "rifles_far_loop" ? RiflesFarBeforeDb : B.Db;
				// Equal-power: the level of a share is its square root.
				Row.Db = Db + 20.0 * std::log10(std::sqrt(Share));
			}
			OutBeds.push_back(Row);
		}
		// The place's own air: loudest in the lulls.
		Bed Place;
		Place.Name = Cover == "woods" ? PlaceWoods : PlaceOpen;
		Place.bSilent = Cover == "water";
		Place.Db = PlaceDb + (PlaceFullDb - PlaceDb) * MeterNow + (Cover == "buildings" ? PlaceTownDb : 0.0);
		OutBeds.push_back(Place);
		(void)RumbleDb;
	}

	bool Mixer::TakeShellShock(double& OutLpfHz, double& OutDown, double& OutHold, double& OutBack, double& OutDb)
	{
		if (!bShockDue) return false;
		bShockDue = false;
		OutLpfHz = ShockLpf;
		OutDown = ShockDown;
		OutHold = ShockHold;
		OutBack = ShockBack;
		OutDb = ShockDb;
		return true;
	}
}
