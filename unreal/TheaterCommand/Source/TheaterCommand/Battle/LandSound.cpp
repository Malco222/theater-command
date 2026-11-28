#include "Battle/LandSound.h"

#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

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

	double Table::At(double X, double Default) const
	{
		if (Rows.empty()) return Default;
		if (X <= Rows.front().first) return Rows.front().second;
		for (size_t K = 1; K < Rows.size(); ++K)
		{
			if (X <= Rows[K].first)
			{
				const auto& A = Rows[K - 1];
				const auto& B = Rows[K];
				const double Span = std::max(1e-9, B.first - A.first);
				return A.second + (B.second - A.second) * (X - A.first) / Span;
			}
		}
		return Rows.back().second;
	}

	namespace
	{
		/** A table as the mix writes it, [[x, y], ...]; left as it was when the mix has none. */
		void ReadTable(const Json& J, Table& Out)
		{
			if (!J.IsArray()) return;
			Table Read;
			for (const Json& Row : J.Items)
			{
				if (Row.IsArray() && Row.At(0).IsNumber() && Row.At(1).IsNumber()) Read.Rows.emplace_back(Row.At(0).Num(), Row.At(1).Num());
			}
			if (!Read.Rows.empty()) Out = Read;
		}

		/** Pieces as the mix writes them: "take" (all of it), or ["take", from, cut]. */
		std::vector<Piece> Pieces(const Json& J)
		{
			std::vector<Piece> Out;
			for (const Json& P : J.Items)
			{
				Piece One;
				if (P.IsString()) One.Take = P.Text;
				else if (P.IsArray() && P.At(0).IsString())
				{
					One.Take = P.At(0).Text;
					One.From = P.At(1).Num(0.0);
					One.Cut = P.At(2).Num(0.0);
				}
				if (!One.Take.empty()) Out.push_back(One);
			}
			return Out;
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

	bool Mixer::InList(const std::string& ListName, const std::string& Name) const
	{
		auto It = Lists.find(ListName);
		return It != Lists.end() && std::find(It->second.begin(), It->second.end(), Name) != It->second.end();
	}

	bool Mixer::IsCloseRifle(const std::string& Name) const
	{
		for (const auto& L : Lists)
		{
			if (L.first.rfind("byNation.rifle.", 0) == 0 && std::find(L.second.begin(), L.second.end(), Name) != L.second.end()) return true;
		}
		return false;
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
		RainTake = Place["rain"]["take"].Str(RainTake.c_str());
		RainDb = Place["rain"]["gainDb"].Num(RainDb);
		RainFullDb = Place["rain"]["atFullDb"].Num(RainFullDb);
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
		// The ground battle's own rows (land): each left at its default where the mix has none.
		const Json& Land = M["land"];
		ReadTable(Land["closeView"]["dbByCameraHeight"], CloseView);
		const Json& Cr = Land["cracks"];
		ReadTable(Cr["withinByCameraHeight"], CrackWithin);
		ReadTable(Cr["gapByCameraHeight"], CrackGap);
		CrackFromShooter = Cr["fromShooterMoreThan"].Num(CrackFromShooter);
		CrackDb0 = Cr["dbAt0"].Num(CrackDb0);
		CrackDbPerMetre = Cr["dbPerMetre"].Num(CrackDbPerMetre);
		RunFade = Cr["runFade"].Num(RunFade);
		RunGap = Cr["runGap"].Num(RunGap);
		Lists["crack.single"] = Cr["single"].IsArray() ? Takes(Cr["single"]) : std::vector<std::string>{ "bullet_crack" };
		CrackRunParts = Pieces(Cr["run"]);
		if (!Cr["run"].IsArray()) CrackRunParts = { { "bullets_incoming_01", 0.03, 1.53 }, { "bullets_incoming_02", 0.02, 2.14 } };
		for (const Piece& P : CrackRunParts) Lists["crack.run"].push_back(P.Take);
		const Json& St = Land["groundStrikes"];
		StrikeWithin = St["within"].Num(StrikeWithin);
		StrikeBelow = St["cameraBelow"].Num(StrikeBelow);
		WhineChance = St["whineChance"].Num(WhineChance);
		WhineGap = St["whineGap"].Num(WhineGap);
		StrikeDb = St["db"].Num(StrikeDb);
		Lists["strike.dirt"] = Takes(St["dirt"]);
		WhinePieces = Pieces(St["whines"]);
		for (const std::string& R : (St["ricochet"].IsArray() ? Takes(St["ricochet"]) : std::vector<std::string>{ "ricochet_01" })) WhinePieces.push_back({ R, 0.0, 0.0 });
		// (a rifle or machine-gun round off a vehicle: the ring of the steel it struck, where a recording is listed;
		// none listed keeps the ricochet it had)
		const Json& Ar = Land["armourStrikes"];
		Lists["armour.smallArms"] = Takes(Ar["smallArms"]);
		ArmourDb = Ar["db"].Num(ArmourDb);
		ArmourWhineChance = Ar["whineChance"].Num(ArmourWhineChance);
		// (a burst off armour, World::bRicochet: the rattle, RattleOff)
		ArmourBurstMost = static_cast<int>(Ar["burstMost"].Num(ArmourBurstMost));
		ArmourBurstShare = Ar["burstShare"].Num(ArmourBurstShare);
		ArmourBurstPitch = Ar["burstPitchPercent"].Num(ArmourBurstPitch);
		ArmourBurstDbSpread = Ar["burstDbSpread"].Num(ArmourBurstDbSpread);
		ArmourBurstRing = Ar["burstRingS"].Num(ArmourBurstRing);
		ArmourBurstWhine = Ar["burstWhineChance"].Num(ArmourBurstWhine);
		const Json& Mg = Land["mgClose"];
		for (size_t K = 0; K < Mg.Keys.size(); ++K)
		{
			if (Mg.Values[K].IsArray() && Mg.Keys[K][0] != '_') Lists["byNation.mg." + Mg.Keys[K]] = Takes(Mg.Values[K]);
		}
		// (a tank's machine guns, by army, over the infantry's: land.tankMg; its roof row the guns on a commander's
		// hatch where they differ from her others, the Sherman's .50)
		const Json& TankMg = Land["tankMg"];
		for (size_t K = 0; K < TankMg.Keys.size(); ++K)
		{
			if (TankMg.Values[K].IsArray() && TankMg.Keys[K][0] != '_') Lists["tankMg." + TankMg.Keys[K]] = Takes(TankMg.Values[K]);
		}
		const Json& Roof = TankMg["roof"];
		for (size_t K = 0; K < Roof.Keys.size(); ++K)
		{
			if (Roof.Values[K].IsArray() && Roof.Keys[K][0] != '_') Lists["tankMg.roof." + Roof.Keys[K]] = Takes(Roof.Values[K]);
		}
		// (a support team's gun on its mount, release 2's "teamguns", by the gun drawn: land.teamMg, over its army's row, named
		// by Noise.Mount as the ground scene gives it: an MG 34 team before mid-1942 is not an MG 42's 1,200 a minute)
		const Json& TeamMg = Land["teamMg"];
		for (size_t K = 0; K < TeamMg.Keys.size(); ++K)
		{
			if (TeamMg.Values[K].IsArray() && TeamMg.Keys[K][0] != '_') Lists["teamMg." + TeamMg.Keys[K]] = Takes(TeamMg.Values[K]);
		}
		const Json& Parts = Mg["parts"];
		for (size_t K = 0; K < Parts.Keys.size(); ++K)
		{
			const Json& P = Parts.Values[K];
			if (P.IsArray() && P.At(0).IsNumber() && P.At(1).IsNumber()) FiringPart[Parts.Keys[K]] = { P.At(0).Num(), P.At(1).Num() };
		}
		const Json& More = Mg["moreParts"];
		for (size_t K = 0; K < More.Keys.size(); ++K)
		{
			const Json& Ps = More.Values[K];
			if (!Ps.IsArray() || More.Keys[K][0] == '_') continue;
			for (const Json& P : Ps.Items)
			{
				if (P.IsArray() && P.At(0).IsNumber() && P.At(1).IsNumber()) MoreParts[More.Keys[K]].push_back({ P.At(0).Num(), P.At(1).Num() });
			}
		}
		BurstEndFade = Mg["endFade"].Num(BurstEndFade);
		const Json& Vo = Land["voices"];
		bVoicesPooled = Vo["pool"].Truth(bVoicesPooled);
		VoicesWarm = std::max(0, Vo["warm"].Int(VoicesWarm));
		VoicesMost = std::max(1, Vo["most"].Int(VoicesMost));
		const Json& Ga = Land["garand"];
		ClipEvery = Ga["clipEvery"].Int(ClipEvery);
		PingTake = Ga["ping"].Str(PingTake.c_str());
		PingAfter = Ga["pingAfter"].Num(PingAfter);
		ReloadTake = Ga["reload"].Str(ReloadTake.c_str());
		ReloadAfterPing = Ga["reloadAfterPing"].Num(ReloadAfterPing);
		GarandWithin = Ga["within"].Num(GarandWithin);
		bReloadFromClips = Ga["reloadFromClips"].Truth(bReloadFromClips);
		if (Ga["reloadPieces"].IsArray())
		{
			ReloadPieces.clear();
			const Json& Rp = Ga["reloadPieces"];
			for (size_t K = 0; K < Rp.Count(); ++K) ReloadPieces.push_back({ { Rp.At(K).At(0).Num(), Rp.At(K).At(1).Num(), Rp.At(K).At(2).Num() } });
		}
		const Json& Sm = Land["smgCue"];
		SmgCue = Sm["cue"].Str(SmgCue.c_str());
		SmgBurst = Sm["rounds"].Int(SmgBurst);
		if (Sm["leaderRoles"].IsArray()) SmgRoles = Takes(Sm["leaderRoles"]);
		const Json& On = Sm["onsets"];
		if (On.Keys.size()) SmgOnsets.clear();
		for (size_t K = 0; K < On.Keys.size(); ++K)
		{
			SmgOnsets[On.Keys[K]] = LandJson::Numbers(On.Values[K]);
		}
		const Json& Sh2 = Land["shouts"];
		ShoutWithin = Sh2["within"].Num(ShoutWithin);
		ShoutMeterFrom = Sh2["meterFrom"].Num(ShoutMeterFrom);
		ShoutGapLow0 = Sh2["gapAtLow"].At(0).Num(ShoutGapLow0);
		ShoutGapLow1 = Sh2["gapAtLow"].At(1).Num(ShoutGapLow1);
		ShoutGapFull0 = Sh2["gapAtFull"].At(0).Num(ShoutGapFull0);
		ShoutGapFull1 = Sh2["gapAtFull"].At(1).Num(ShoutGapFull1);
		ShoutFewLines = Sh2["fewLines"].Int(ShoutFewLines);
		ShoutFewGap = Sh2["fewLinesGap"].Num(ShoutFewGap);
		ShoutOnFallWithin = Sh2["onFallWithin"].Num(ShoutOnFallWithin);
		ShoutOnFallChance = Sh2["onFallChance"].Num(ShoutOnFallChance);
		ShoutDb = Sh2["db"].Num(ShoutDb);
		ShoutExclude = Sh2["exclude"].IsArray() ? Takes(Sh2["exclude"]) : std::vector<std::string>{ "order_charge" };
		const Json& In = Land["incoming"];
		IncomingWithin = In["within"].Num(IncomingWithin);
		Lists["incoming.mortar"] = Takes(In["mortar"]);
		Lists["incoming.artillery"] = Takes(In["artillery"]);
		MostHold = Land["sameTake"]["mostHold"].Num(MostHold);
		ClearWithin = Land["sameTake"]["clearWithin"].Num(ClearWithin);
		Repitch = Land["sameTake"]["repitchPercent"].Num(Repitch * 100.0) * 0.01;
		const Json& Bc = Land["bodyCues"];
		for (size_t K = 0; K < Bc.Keys.size(); ++K)
		{
			if (Bc.Keys[K][0] == '_') continue;
			BodyCue[Bc.Keys[K]] = Bc.Values[K].IsString() ? Bc.Values[K].Text : std::string();
		}
		// (the field gun's breech has no recording, and its case already clangs out after the shot: silent unless the
		// mix names a take for it)
		if (!BodyCue.count("field_gun_breech")) BodyCue["field_gun_breech"] = std::string();
		// (a vehicle's engine by her model: the Panzer IV's own Maybach for the cue her clip plays)
		const Json& En = Land["engines"];
		for (size_t K = 0; K < En.Keys.size(); ++K)
		{
			const Json& Model = En.Values[K];
			if (En.Keys[K][0] == '_' || !Model.IsObject()) continue;
			for (size_t C = 0; C < Model.Keys.size(); ++C)
			{
				if (Model.Keys[C][0] != '_' && Model.Values[C].IsString()) EngineByModel[En.Keys[K]][Model.Keys[C]] = Model.Values[C].Text;
			}
		}
		// (a size's own landings past the near band, where it has recordings of its own: a mortar bomb's)
		const Json& Ld = Land["landings"];
		for (size_t K = 0; K < Ld.Keys.size(); ++K)
		{
			if (Ld.Keys[K][0] == '_' || !Ld.Values[K].IsObject()) continue;
			for (const char* Band : { "middle", "far" })
			{
				if (Ld.Values[K][Band].IsArray()) Lists["landing." + Ld.Keys[K] + "." + Band] = Takes(Ld.Values[K][Band]);
			}
		}
		// Release 2's battle acoustics (land.acoustics): read whatever the switches say, used only where one is on.
		const Json& Ac = Land["acoustics"];
		const Json& Tr = Ac["travel"];
		if (Tr["families"].IsArray()) TravelFamilies = Takes(Tr["families"]);
		TravelFrom = Tr["from"].Num(TravelFrom);
		TravelMost = Tr["maxSeconds"].Num(TravelMost);
		BulletSpeed = std::max(100.0, Tr["bulletMetresPerSecond"].Num(BulletSpeed));
		const Json& Tl = Ac["tails"];
		TailFadeIn = Tl["fadeIn"].Num(TailFadeIn);
		TailFloorDb = Tl["floorDb"].Num(TailFloorDb);
		for (size_t K = 0; K < Tl["takes"].Keys.size(); ++K)
		{
			const Json& Row = Tl["takes"].Values[K];
			if (!Row.IsArray() || !Row.At(0).IsNumber()) continue;
			TailOf[Tl["takes"].Keys[K]] = TailPart{ Row.At(0).Num(0.0), Row.At(1).Num(0.0), Row.At(2).Num(0.0) };
		}
		const Json& Ec = Ac["echo"];
		EchoReach = Ec["reach"].Num(EchoReach);
		EchoStep = std::max(1.0, Ec["step"].Num(EchoStep));
		EchoRays = std::clamp(static_cast<int>(Ec["rays"].Num(EchoRays)), 4, 72);
		EchoBelow = Ec["cameraBelow"].Num(EchoBelow);
		EchoLagLeast = Ec["lagLeast"].Num(EchoLagLeast);
		EchoUnder = std::max(0.0, Ec["leastUnderDb"].Num(EchoUnder));
		EchoLagMost = Ec["lagMost"].Num(EchoLagMost);
		EchoFloorDb = Ec["floorDb"].Num(EchoFloorDb);
		EchoFloorBusyDb = std::max(EchoFloorDb, Ec["floorDbBusy"].Num(EchoFloorBusyDb));
		EchoGap = Ec["smallArmsGap"].Num(EchoGap);
		EchoMost = std::max(0, static_cast<int>(Ec["most"].Num(EchoMost)));
		EchoOfBig = std::max(0, static_cast<int>(Ec["ofBigGuns"].Num(EchoOfBig)));
		EchoApart = Ec["apartDegrees"].Num(EchoApart);
		EchoLookMoved = Ec["lookAgainMoved"].Num(EchoLookMoved);
		EchoLookEvery = Ec["lookAgainEvery"].Num(EchoLookEvery);
		auto ReadFace = [](const Json& J, Face& F)
		{
			F.Db = J["db"].Num(F.Db);
			F.LpfHz = J["lpfHz"].Num(F.LpfHz);
			F.HpfHz = J["hpfHz"].Num(F.HpfHz);
		};
		ReadFace(Ec["woods"], EchoWoods);
		ReadFace(Ec["buildings"], EchoWalls);
		ReadFace(Ec["hedges"], EchoHedges);
		EchoMostSmall = std::clamp(static_cast<int>(Ec["mostSmallArms"].Num(EchoMostSmall)), 0, EchoMost);
		EchoDice ^= Seed * 0x9FB21C651E98DF25ull;
		if (Lists["byNation.rifle.*"].empty())
		{
			OutError = "the mix has no rifle takes (byNation.rifle)";
			return false;
		}
		return true;
	}

	double Mixer::FlashToBang(double Far, double Speed) const
	{
		// (Far / 343 is life's seconds: a real second holds Speed x Life of them)
		if (Far <= FtbFrom) return 0.0;
		return std::min(FtbMost, Far / std::max(1.0, FtbSpeed)) / std::max(0.05, Speed * std::max(0.01, Life));
	}

	double Mixer::Travel(const std::string& FamilyName, double Far, double Speed) const
	{
		// Release 2 (bTravel): a rifle's or a machine gun's report crosses the ground as a big gun's does, Far / 343 life
		// seconds (never more than the travel's most), a real second holding Speed x Life of them. From the first metre:
		// 20 m is six hundredths of a second, which no eye catches, and a line of rifles is heard as it stands, the near
		// men first.
		if (!bTravel || Far <= TravelFrom) return 0.0;
		if (std::find(TravelFamilies.begin(), TravelFamilies.end(), FamilyName) == TravelFamilies.end()) return 0.0;
		return std::min(TravelMost, Far / std::max(1.0, FtbSpeed)) / std::max(0.05, Speed * std::max(0.01, Life));
	}

	double Mixer::LossDb(const std::string& FamilyName, double Far) const
	{
		// (as the scene's players lose it, ALandBattle::FamilyOf: Unreal's "natural sound" law, 60 dB down at `silent`)
		const Family& F = FamilyOf(FamilyName);
		if (Far <= F.Full) return 0.0;
		if (Far >= F.Silent) return -120.0;
		return -60.0 * (Far - F.Full) / std::max(1.0, F.Silent - F.Full);
	}

	double Mixer::EchoRoll()
	{
		EchoDice += 0x9E3779B97F4A7C15ull;
		uint64_t Z = EchoDice;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		Z ^= Z >> 31;
		return static_cast<double>(Z >> 11) / 9007199254740992.0;
	}

	void Mixer::TailAfter(const Take& Cut, double Far, double Now, std::vector<Take>& Out)
	{
		// Release 2 (bTails): a burst cut to its rounds ended in a fade of four hundredths of a second, the gun shut off
		// like a tap, because the recording fires on past the burst's last round. Its dying echo is in the recording all
		// the same, after the firing's end (land.acoustics.tails: where each take's lies, measured): that stretch is laid
		// on as the cut fades, at the burst's own place, pitch and level, so the burst rings out as the gun's own
		// recording does. A take with no echo recorded after its firing (the town's MG 42s) ends as before.
		if (!bTails || Cut.Cut <= 0.0) return;
		auto Found = TailOf.find(Cut.Name);
		if (Found == TailOf.end()) return;
		const TailPart& Part = Found->second;
		// (only where it would be heard: the stretch lies some 22 dB under its burst's rounds, so under a burst that
		// reaches the ear fainter than tails.floorDb it is a player spent on nothing)
		if (Cut.bSpatial && Cut.Db + LossDb(Cut.Family, Far) < TailFloorDb) { ++TailsFaint; return; }
		const double Whole = Length ? Length(Cut.Name) : 0.0;
		const double Long = Part.Long > 0.0 ? Part.Long : Whole - Part.From;
		if (Long < 0.1) return;
		Take T = Cut;
		T.From = Part.From;
		T.Cut = Part.Long > 0.0 && Whole > Part.From + Part.Long + 0.02 ? Part.Long : 0.0;
		T.CutFade = T.Cut > 0.0 ? std::min(0.15, Part.Long * 0.3) : 0.0;
		T.FadeIn = std::max(0.005, std::min(TailFadeIn, Cut.CutFade > 0.0 ? Cut.CutFade : TailFadeIn));
		T.Delay = Cut.Delay + Cut.Cut;
		T.Db = Cut.Db + Part.Db;
		T.bBig = false;
		// (the same-take rule, for the tails among themselves: two guns on one recording whose bursts end together
		// would ring out as two copies of the one stretch; within the rule's 0.15 s of a tail already laid at a pitch
		// under its repitch apart, that tail is this burst's too)
		std::vector<std::pair<double, double>>& Laid = TailsLaid[T.Name];
		const double Starts = Now + T.Delay;
		Laid.erase(std::remove_if(Laid.begin(), Laid.end(), [Now](const std::pair<double, double>& One) { return One.first < Now - 0.15; }), Laid.end());
		for (const std::pair<double, double>& One : Laid)
		{
			if (std::abs(Starts - One.first) < 0.15 && std::abs(T.Pitch / One.second - 1.0) < Repitch)
			{
				++TailsShared;
				return;
			}
		}
		Laid.push_back({ Starts, T.Pitch });
		++Tails;
		Out.push_back(std::move(T));
	}

	int Mixer::CoverAt(double X, double Y, double& OutGroundZ)
	{
		// (each cell of the look's own step asked of the field once: a wood does not move)
		OutGroundZ = 0.0;
		if (!CoverOf) return 0;
		const std::pair<int, int> Cell(static_cast<int>(std::floor(X / EchoStep)), static_cast<int>(std::floor(Y / EchoStep)));
		auto Seen = CoverSeen.find(Cell);
		if (Seen == CoverSeen.end())
		{
			CoverCell Asked;
			Asked.What = CoverOf((Cell.first + 0.5) * EchoStep, (Cell.second + 0.5) * EchoStep, Asked.Z);
			Seen = CoverSeen.emplace(Cell, Asked).first;
			++EchoCellsAsked;
		}
		OutGroundZ = Seen->second.Z;
		return Seen->second.What;
	}

	void Mixer::LookForWalls(const Listener& Ear, double Now)
	{
		// The faces that throw a sound back to the ear: along each bearing round it, the first place where open ground
		// gives way to a wood's edge or to a house. Looked for again when the ear has moved or the look is old; the
		// field's cover is asked once a cell, so a look costs a few hundred small steps.
		const double Moved = std::hypot(Ear.At.X - WallsFrom.X, Ear.At.Y - WallsFrom.Y);
		if (bWallsLooked && Moved < EchoLookMoved && Now - WallsAt < EchoLookEvery) return;
		bWallsLooked = true;
		WallsFrom = Ear.At;
		WallsAt = Now;
		Walls.clear();
		if (!CoverOf) return;
		++EchoLooks;
		double GroundZ = 0.0;
		const int Here = CoverAt(Ear.At.X, Ear.At.Y, GroundZ);
		for (int R = 0; R < EchoRays; ++R)
		{
			const double Bearing = 6.283185307179586 * R / EchoRays;
			const double WX = std::cos(Bearing), WY = std::sin(Bearing);
			int Before = Here;
			for (double Out = EchoStep; Out <= EchoReach; Out += EchoStep)
			{
				const double X = Ear.At.X + WX * Out, Y = Ear.At.Y + WY * Out;
				const int What = CoverAt(X, Y, GroundZ);
				// (a face is where the way out over open ground, a garden or water runs into trees, a house or a hedgerow;
				// standing in the wood itself, the trees all round are no one face: the first is past the wood's far edge.
				// A hedgerow is the least of them: the way runs on past it to a wood or a house within three steps more)
				if (FaceOf(What) && What != Before)
				{
					int Kind = What;
					double FaceOut = Out, FaceZ = GroundZ;
					for (int More = 1; Kind == 4 && More <= 3 && Out + More * EchoStep <= EchoReach; ++More)
					{
						double Z2 = 0.0;
						const int Next = CoverAt(Ear.At.X + WX * (Out + More * EchoStep), Ear.At.Y + WY * (Out + More * EchoStep), Z2);
						if (Next == 1 || Next == 2) { Kind = Next; FaceOut = Out + More * EchoStep; FaceZ = Z2; }
					}
					// (half a step back from the trees or the wall and a little up it: where its sound comes from)
					Walls.push_back({ Vec(Ear.At.X + WX * (FaceOut - EchoStep * 0.5), Ear.At.Y + WY * (FaceOut - EchoStep * 0.5), FaceZ + (Kind == 1 ? 5.0 : 3.0)), Kind, Bearing });
					break;
				}
				Before = What;
			}
		}
		EchoFaces = static_cast<int>(Walls.size());
	}

	void Mixer::EchoOf(const Take& Sounded, double Far, double Now, double Speed, const Listener& Ear, int Most, bool bSmall, std::vector<Take>& Out)
	{
		// Release 2 (bEcho): open country is not a padded room. A shot goes out all round, and what stands round the ear,
		// a wood's edge, a farm, a street's houses, throws a part of it back: the same sound again, later by the longer
		// way round (the gun to the face to the ear, at 343 m/s), softer by that longer way and by what the face keeps
		// (trees scatter most of it and send back a thin, dull clatter; a wall sends back more, and harder), from where
		// the face stands. It is the recording itself played again, never a made sound. Only for an ear near the ground,
		// only faces that give an echo clear of its own sound (lagLeast) and loud enough to hear, a few at once.
		if (!bEcho || !CoverOf || Most <= 0 || !Sounded.bSpatial || Ear.CameraHeight > EchoBelow) return;
		LookForWalls(Ear, Now);
		if (Walls.empty()) return;
		if (bSmall && Now - EchoLast < EchoGap) return;
		// (the share this sound's echoes take their places in: the small arms' own, or the guns' and blasts')
		std::vector<double>& Share = bSmall ? EchoEndsSmall : EchoEnds;
		const int Places = bSmall ? EchoMostSmall : EchoMost - EchoMostSmall;
		if (Places <= 0) return;
		Share.erase(std::remove_if(Share.begin(), Share.end(), [Now](double Ends) { return Ends <= Now; }), Share.end());
		const double PerLife = std::max(0.05, Speed * std::max(0.01, Life));
		const double Direct = std::max(1.0, Far);
		const double DirectDb = LossDb(Sounded.Family, Direct);
		struct Found { double Db; double Lag; const Wall* From; double FromEar; };
		std::vector<Found> Good;
		bool bFaint = false;
		for (const Wall& W : Walls)
		{
			const double FromGun = (W.At - Sounded.At).Length();
			const double FromEar = (W.At - Ear.At).Length();
			const double Way = FromGun + FromEar;
			const double Lag = (Way - Direct) / std::max(1.0, FtbSpeed);
			// (on the real clock no sooner after its own sound than the same-take rule lets a recording follow itself,
			// whatever the battle's pace; on life's clock no later than an echo is still heard as that sound's)
			if (Lag / PerLife < EchoLagLeast || Lag > EchoLagMost) continue;
			const Face& F = *FaceOf(W.Kind);
			// (what reaches the ear: the family's loss over the whole way round, and the face's own)
			const double AtEar = LossDb(Sounded.Family, Way) + F.Db;
			if (AtEar < EchoFloorDb || AtEar - DirectDb > -EchoUnder) { bFaint = bFaint || AtEar < EchoFloorDb; continue; }
			Good.push_back({ AtEar, Lag, &W, FromEar });
		}
		if (Good.empty())
		{
			if (bFaint) ++EchoesFaint;
			return;
		}
		std::stable_sort(Good.begin(), Good.end(), [](const Found& A, const Found& B) { return A.Db > B.Db; });
		std::vector<double> Taken;
		int Started = 0;
		for (const Found& G : Good)
		{
			if (Started >= Most) break;
			// (a second echo of a gun comes from another quarter, not the next tree along)
			bool bApart = true;
			for (const double Bearing : Taken)
			{
				double Turn = std::abs(G.From->Bearing - Bearing);
				if (Turn > 3.141592653589793) Turn = 6.283185307179586 - Turn;
				bApart = bApart && Turn * 57.29577951308232 >= EchoApart;
			}
			if (!bApart) continue;
			// (at most so many at once in its share, and the fuller the share the louder it must be at the ear: the
			// loudest was tried first, so the rest of this sound's are under it too)
			const int Busy = static_cast<int>(Share.size());
			if (Busy >= Places)
			{
				++EchoesFull;
				break;
			}
			if (G.Db < EchoFloorDb + (EchoFloorBusyDb - EchoFloorDb) * Busy / Places)
			{
				++EchoesUnder;
				break;
			}
			const Face& F = *FaceOf(G.From->Kind);
			Take E = Sounded;
			E.At = G.From->At;
			E.Track = -1;
			E.bBig = false;
			E.bEcho = true;
			E.Bus = "close";
			E.Delay = Sounded.Delay + G.Lag / PerLife;
			// (the player at the face loses only the face's own distance to the ear: the rest of the way round, and what
			// the face keeps, are taken off here; a little of the dice's level so a volley's echoes are no one copy)
			E.Db = Sounded.Db + G.Db - LossDb(Sounded.Family, G.FromEar) + (EchoRoll() * 2.0 - 1.0) * 1.0;
			E.LpfHz = F.LpfHz;
			E.HpfHz = F.HpfHz;
			E.FadeIn = std::max(E.FadeIn, 0.004);
			const double Long = Length ? Length(E.Name) : 1.5;
			const double Plays = E.Cut > 0.0 ? E.Cut + E.CutFade : std::max(0.05, Long - E.From);
			Share.push_back(Now + E.Delay + Plays);
			Taken.push_back(G.From->Bearing);
			++Echoes;
			++(G.From->Kind == 1 ? EchoesWoods : G.From->Kind == 2 ? EchoesWalls : EchoesHedges);
			++Started;
			Out.push_back(std::move(E));
		}
		if (Started > 0)
		{
			if (bSmall) EchoLast = Now;
			EchoStarts.erase(std::remove_if(EchoStarts.begin(), EchoStarts.end(), [Now](double At) { return At <= Now - 1.0; }), EchoStarts.end());
			for (int K = 0; K < Started; ++K) EchoStarts.push_back(Now);
			EchoesMostASecond = std::max(EchoesMostASecond, static_cast<int>(EchoStarts.size()));
		}
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
		bLastMerged = false;
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
		// (voices are never cut: the shouts keep to one a side themselves, limits.voicesPerSide)
		if (bFull && !bOnScreen && !T.bBig && T.Family != "voices")
		{
			double Farthest = 0.0;
			for (const Playing& P : Now_) Farthest = std::max(Farthest, P.Far);
			if (Far >= Farthest) return false;
		}
		// Variety: a little pitch and level each time.
		auto P = PitchPercent.find(T.Family == "small_arms" || T.Family == "mg" ? T.Family : T.Family == "big_guns" ? "big_guns" : T.Family == "explosions" ? "explosions" : T.Family == "body" ? "body" : "none");
		const double Pitch = P == PitchPercent.end() ? 0.0 : P->second;
		T.Pitch *= 1.0 + (Roll() * 2.0 - 1.0) * Pitch * 0.01;
		T.Db += (Roll() * 2.0 - 1.0) * LevelDb * 0.5;
		// The same piece of a take never laid over itself (a copy a few milliseconds behind itself flanges): never
		// again within 0.15 s, or within its own length when shorter (a crack of 80 ms cannot overlap a copy of itself
		// 80 ms later). A moment later when that is a frame or two; when it would be longer (a battery's mortars all
		// firing in one tenth of a second) on time instead, at a pitch at least land.sameTake.repitchPercent from the
		// last copy, so it is no copy: a shot is never heard seconds after its flash.
		const double Long0 = Length ? Length(T.Name) : 1.5;
		const double Plays = T.Cut > 0.0 ? T.Cut : std::max(0.02, Long0 - T.From);
		const double Apart = std::min(0.15, Plays);
		char Key2[32];
		std::snprintf(Key2, sizeof(Key2), "@%.3f", T.From);
		const std::string PieceKey = T.Name + Key2;
		const double StartsAt = Now + T.Delay;
		// (once in a while forget pieces long quiet, so the map does not grow through a long battle)
		if (Now - CopiesSweptAt > 10.0)
		{
			CopiesSweptAt = Now;
			for (auto It = Copies.begin(); It != Copies.end();)
			{
				bool bRecent = false;
				for (const auto& Copy : It->second) bRecent = bRecent || Copy.first > Now - 1.0;
				It = bRecent ? std::next(It) : Copies.erase(It);
			}
		}
		std::vector<std::pair<double, double>>& Seen = Copies[PieceKey];
		// (copies forgotten by the clock now, not by this take's start: a far shell's take starts a second or more after
		// it is heard, and forgetting by it lost copies a nearer, sooner take could still land on)
		Seen.erase(std::remove_if(Seen.begin(), Seen.end(), [Now](const std::pair<double, double>& Copy) { return Copy.first < Now - 1.0; }), Seen.end());
		// The copies near this one, either side of it (a copy already set to start a moment later flanges just the
		// same; one a second later does not).
		auto Near = [Apart](double At, const std::pair<double, double>& Copy) { return std::abs(At - Copy.first) < Apart; };
		double Last = -1e9;
		bool bNear = false, bLater = false;
		for (const auto& Copy : Seen)
		{
			if (!Near(StartsAt, Copy)) continue;
			bNear = true;
			if (Copy.first <= StartsAt) Last = std::max(Last, Copy.first);
			else bLater = true;
		}
		if (bNear)
		{
			// A moment later when that is a frame or two and lands clear of every copy; else on time, repitched.
			const double Held = bLater ? 1e9 : Last + Apart - StartsAt;
			bool bHold = Held <= MostHold;
			for (const auto& Copy : Seen) bHold = bHold && !(Near(StartsAt + Held, Copy) && Copy.first > Last);
			if (bHold)
			{
				T.Delay += Held;
				MostDelay = std::max(MostDelay, Held);
				++Delayed;
			}
			else
			{
				// On time, at a pitch at least Repitch from EVERY copy near it (not only the last one: a third copy
				// could land back on the first's pitch).
				auto Gap = [&Seen, &Near, StartsAt](double Try)
				{
					double Least = 1e9;
					for (const auto& Copy : Seen)
					{
						if (Near(StartsAt, Copy) && Copy.second > 0.0) Least = std::min(Least, std::abs(Try / Copy.second - 1.0));
					}
					return Least;
				};
				if (Gap(T.Pitch) < Repitch)
				{
					const double Base = T.Pitch, Step = Repitch + 0.005;
					const double Way = Roll() < 0.5 ? -1.0 : 1.0;
					double Best = Base, BestGap = -1.0;
					for (int K = 1; K <= 4 && BestGap < Repitch; ++K)
					{
						for (const double Sign : { Way, -Way })
						{
							const double Try = Base * (1.0 + Sign * K * Step);
							const double G = Gap(Try);
							if (G > BestGap) { BestGap = G; Best = Try; }
							if (G >= Repitch) break;
						}
					}
					// (every pitch near it taken, a crowd's rifles all at once: off the screen it adds nothing a copy already
					// there does not, so it is left out, as the limits leave a far take)
					if (BestGap < Repitch && !bOnScreen && !T.bBig && T.Family != "voices")
					{
						++SameLeftOut;
						return false;
					}
					// (on the screen it is kept: at the first moment clear of every copy, when that is within the 0.15 s a
					// shot 50 m off is heard after its flash anyway; else at the pitch farthest from them)
					if (BestGap < Repitch)
					{
						double Clear = StartsAt;
						for (int Pass = 0; Pass < 16; ++Pass)
						{
							bool bMoved = false;
							for (const auto& Copy : Seen)
								if (Near(Clear, Copy)) { Clear = Copy.first + Apart; bMoved = true; }
							if (!bMoved) break;
						}
						bool bClear = Clear - StartsAt <= ClearWithin;
						for (const auto& Copy : Seen) bClear = bClear && !Near(Clear, Copy);
						if (bClear)
						{
							T.Delay += Clear - StartsAt;
							MostDelay = std::max(MostDelay, Clear - StartsAt);
							++Delayed;
							++SameHeldClear;
							Best = T.Pitch;
						}
						else if (T.Family == "small_arms" && T.From <= 0.0 && T.Cut <= 0.0 && !ListOf("rifle.far").empty()
							&& (IsCloseRifle(T.Name) || InList("rifle.far", T.Name)))
						{
							// (else a rifle's single shot gives way to another of the far ones, a different recording: so many
							// rifles at once, in the picture, are heard as the crowd's crackle, no copy laid on itself. Each far
							// recording is tried at the pitch farthest from its own copies near this moment, and the one with
							// the most room taken: with 2,000 men a single random pick could land on a recording as crowded as
							// the first. Only a whole rifle shot: a crack, a whine, a ring or a Thompson's cut burst keeps its own
							// take)
							const std::vector<std::string>& FarTakes = ListOf("rifle.far");
							const bool bWasClose = !InList("rifle.far", T.Name);
							const size_t N = FarTakes.size(), O = std::min(N - 1, static_cast<size_t>(Roll() * static_cast<double>(N)));
							std::string Other;
							double OtherPitch = Best, OtherGap = bWasClose ? -1.0 : BestGap;
							// (a close shot tries its own recording first, in the half steps the whole steps above skipped: it
							// keeps its own crack where a pitch is free. Then the far ones)
							std::vector<std::string> Tries;
							Tries.reserve(N + 1);
							if (bWasClose) Tries.push_back(T.Name);
							for (size_t I = 0; I < N; ++I) Tries.push_back(FarTakes[(O + I) % N]);
							for (size_t I = 0; I < Tries.size() && OtherGap < Repitch; ++I)
							{
								const std::string& Name = Tries[I];
								if (Name == T.Name && (!bWasClose || I > 0)) continue;
								const double ApartC = std::min(0.15, std::max(0.02, Length ? Length(Name) : 1.5));
								char KeyC[32];
								std::snprintf(KeyC, sizeof(KeyC), "@%.3f", 0.0);
								auto CIt = Copies.find(Name + KeyC);
								auto GapC = [&](double Try)
								{
									double Least = 1e9;
									if (CIt == Copies.end()) return Least;
									for (const auto& Copy : CIt->second)
									{
										if (Copy.first >= Now - 1.0 && std::abs(StartsAt - Copy.first) < ApartC && Copy.second > 0.0)
											Least = std::min(Least, std::abs(Try / Copy.second - 1.0));
									}
									return Least;
								};
								// (in half steps over the same reach as above: in a volley the copies' pitches lie at random, and
								// the room between two of them is often off the whole steps)
								double PitchC = Base, G = GapC(Base);
								for (int K = 1; K <= 8 && G < Repitch; ++K)
								{
									for (const double Sign : { Way, -Way })
									{
										const double Try = Base * (1.0 + Sign * K * Step * 0.5);
										const double GT = GapC(Try);
										if (GT > G) { G = GT; PitchC = Try; }
										if (G >= Repitch) break;
									}
								}
								if (G > OtherGap) { OtherGap = G; OtherPitch = PitchC; Other = Name; }
							}
							// (and when every far recording is as crowded, a volley in the picture with every pitch near this one
							// already sounding a copy of the same recording, this shot is heard in that copy: one more copy
							// would only flange it. Not a drop: the flash has its sound)
							if (OtherGap < Repitch)
							{
								++SameMerged;
								if (Far <= F.FarFrom && F.FarFrom < 1e8) { ++SameMergedNear; ++MergedNearBy[T.Name]; }  // (a family with far takes, nearer than them)
								bLastMerged = true;
								return false;
							}
							if (!Other.empty())
							{
								++(Other == T.Name ? SameCloseHalf : bWasClose ? SameToFar : SameToOtherFar);
								T.Name = Other;
								Best = OtherPitch;
								BestGap = OtherGap;
							}
						}
						else if (!bClear)
						{
							// (nearer than its family's far takes it is heard on its own: its own recording is tried in the
							// half steps between the whole ones above, as a close rifle's is, where the copies' pitches leave
							// room off the whole steps. A Maxim's burst of 8 has only its part to play from: Prokhorovka with
							// the night patrol's men, 22 November)
							if (Far <= F.FarFrom && F.FarFrom < 1e8)
							{
								double Half = Best, HalfGap = BestGap;
								for (int K = 1; K <= 7 && HalfGap < Repitch; K += 2)
								{
									for (const double Sign : { Way, -Way })
									{
										const double Try = Base * (1.0 + Sign * K * Step * 0.5);
										const double G = Gap(Try);
										if (G > HalfGap) { HalfGap = G; Half = Try; }
										if (HalfGap >= Repitch) break;
									}
								}
								if (HalfGap >= Repitch)
								{
									++SameNearHalf;
									Best = Half;
									BestGap = HalfGap;
								}
							}
							if (BestGap < Repitch)
							{
								// (any other take on the screen, every pitch near it sounding and no clear moment within reach: a
								// machine gun's burst as another of the same recording starts the same instant. It is heard in
								// that copy, as a crowd's rifles are: one more copy would only flange it, the 0.15 s rule's
								// fault. Found by the camera's new path through Prokhorovka, 22 November)
								++SameMerged;
								if (Far <= F.FarFrom && F.FarFrom < 1e8) { ++SameMergedNear; ++MergedNearBy[T.Name]; }  // (a family with far takes, nearer than them)
								bLastMerged = true;
								return false;
							}
						}
						if (!bClear && BestGap < Repitch) ++SameCrowded;
					}
					T.Pitch = Best;
				}
				++Repitched;
			}
		}
		// (a shot moved to another recording is a copy of that one)
		char KeyNow[32];
		std::snprintf(KeyNow, sizeof(KeyNow), "@%.3f", T.From);
		(T.Name + KeyNow == PieceKey ? Seen : Copies[T.Name + KeyNow]).push_back({ Now + T.Delay, T.Pitch });
		// Down among the men the whole battle is louder (land.closeView).
		T.Db += CloseView.At(EarHeight, 0.0);
		const double Long = Length ? Length(T.Name) : 1.5;
		Now_.push_back({ Now + T.Delay + (T.Cut > 0.0 ? T.Cut : std::max(0.1, Long - T.From)), Far });
		++TakesByName[T.Name];
		Out.push_back(std::move(T));
		return true;
	}

	void Mixer::Crack(const Noise& N, double Now, const Listener& Ear, std::vector<Take>& Out, double Speed, double ReportDelay)
	{
		// The crack of rounds passing near the ear: a rifle or machine-gun bullet is faster than sound, and its shock
		// wave cracks where it passes, before the shot itself is heard from the muzzle. Rounds whose line comes within
		// land.cracks' distance of the ear (farther down among the men), from a shooter more than 25 m off, no closer
		// together than its gap. A machine-gun burst passing near is a run of cracks: a stretch of a real recording of
		// rounds coming in, as long as the burst.
		const Vec Line = N.To - N.From;
		const double Long = Line.Length();
		if (Long < 1.0) return;
		const Vec Way = Line * (1.0 / Long);
		const double Along = std::clamp((Ear.At - N.From).Dot(Way), 0.0, Long);
		const Vec Nearest = N.From + Way * Along;
		const double Miss = Distance(Nearest, Ear.At);
		const double Within = CrackWithin.At(Ear.CameraHeight, 12.0);
		const double Gap = CrackGap.At(Ear.CameraHeight, 0.25);
		if (Miss > Within || Distance(N.From, Ear.At) < CrackFromShooter || Now - CrackAt < Gap) return;
		CrackAt = Now;
		Take T;
		T.At = Nearest;
		T.Family = "small_arms";
		T.Delay = Along / 760.0;
		// (release 2, bTravel: the round's flight at the battle clock's speed, as its gun's report is held, so the crack
		// keeps its place ahead of the thump whatever the clock; and the two counted, on or off, for the log)
		if (bTravel) T.Delay = Along / BulletSpeed / std::max(0.05, Speed * std::max(0.01, Life));
		const bool bAhead = ReportDelay >= 0.0 && T.Delay < ReportDelay;
		T.Db = CrackDb0 + CrackDbPerMetre * Miss;
		const bool bRun = N.Weapon == "mg" && N.Rounds > 1 && N.PerSecond > 0.0 && !CrackRunParts.empty() && Now - RunAt >= RunGap;
		if (bRun)
		{
			const Piece& P = CrackRunParts[static_cast<size_t>(Roll() * static_cast<double>(CrackRunParts.size())) % CrackRunParts.size()];
			const double Burst = N.Rounds / N.PerSecond;
			const double Part = P.Cut > 0.0 ? P.Cut : 1.0;
			T.Name = P.Take;
			T.From = P.From + Roll() * std::max(0.0, Part - Burst);
			// (gone as the burst ends: the recording runs on with more rounds passing, so no tail is cut from it)
			T.CutFade = std::min(RunFade, 0.5 / N.PerSecond);
			T.Cut = std::max(0.02, std::min(Burst, Part) - T.CutFade);
			T.FadeIn = 0.01;
			RunAt = Now;
			if (Start(T, Now, Miss, true, Out)) { ++Cracks; ++CrackRuns; if (ReportDelay >= 0.0) ++(bAhead ? CracksBeforeThump : CracksAfterThump); }
			return;
		}
		T.Name = Pick("crack.single");
		if (Start(T, Now, Miss, true, Out)) { ++Cracks; if (ReportDelay >= 0.0) ++(bAhead ? CracksBeforeThump : CracksAfterThump); }
	}

	void Mixer::Strike(const Noise& N, double Now, const Listener& Ear, std::vector<Take>& Out)
	{
		// A round that missed and struck the ground near the ear (down among the men): the strike in the earth, where a
		// real recording of one is listed, and now and then a ricochet whining off. One a burst: the rest are drawn.
		if (!N.bMiss || Ear.CameraHeight > StrikeBelow) return;
		const double Far = Distance(N.To, Ear.At);
		if (Far > StrikeWithin) return;
		++GroundStrikes;
		// (heard as it lands: the round's flight in real seconds)
		const double Flight = std::clamp(Distance(N.From, N.To) / 760.0, 0.0, 2.0);
		const std::vector<std::string> Dirt = ListOf("strike.dirt");
		if (!Dirt.empty())
		{
			Take D;
			D.Name = Pick("strike.dirt");
			D.At = N.To;
			D.Family = "small_arms";
			D.Db = StrikeDb;
			D.Delay = Flight;
			Start(D, Now, Far, true, Out);
		}
		WhineOff(N.To + Vec(0.0, 0.0, 0.5), Far, Now, Flight, StrikeDb, WhineChance, Out);
	}

	bool Mixer::WhineOff(const Vec& At, double Far, double Now, double Delay, double Db, double Chance, std::vector<Take>& Out)
	{
		if (WhinePieces.empty() || Now - WhineAt < WhineGap || Roll() >= Chance) return false;
		WhineAt = Now;
		const Piece& P = WhinePieces[static_cast<size_t>(Roll() * static_cast<double>(WhinePieces.size())) % WhinePieces.size()];
		Take W;
		W.Name = P.Take;
		W.From = P.From;
		W.Cut = P.Cut;
		W.CutFade = P.Cut > 0.0 ? std::min(0.12, P.Cut * 0.4) : 0.0;
		W.FadeIn = P.From > 0.0 ? 0.008 : 0.0;
		W.At = At;
		W.Family = "small_arms";
		W.Db = Db;
		W.Delay = Delay;
		if (!Start(W, Now, Far, true, Out)) return false;
		++Whines;
		return true;
	}

	int Mixer::PingsOf(int Rounds) const
	{
		return std::clamp(static_cast<int>(std::lround(Rounds * ArmourBurstShare)), 1, std::max(1, ArmourBurstMost));
	}

	void Mixer::RattleOff(const Noise& N, const Take& Base, double Far, double Now, bool bOnScreen, std::vector<Take>& Out)
	{
		// Effects (R2), small arms off armour: a machine gun raking a tank is a rattle of steel, not one ring. A ping for
		// about burstShare of its rounds, never more than burstMost, spread over the burst (rounds / rate) each in its own
		// stretch of it, a take of the list each (Pick: never the same twice running), a little pitch and level of its own
		// over Start's, every one but the last cut to burstRingS with a 0.05 s fade (a sharp clang; the steel rings out
		// only at the end, so eight pings are a rattle, not one wash); and one whine off now and then (the ground's gap
		// and burstWhineChance, somewhere in the burst). The mix's limits and same-take rule as ever (Start). One roll a
		// statement.
		const double Long = N.Rounds / N.PerSecond;
		const int Pings = PingsOf(N.Rounds);
		for (int K = 0; K < Pings; ++K)
		{
			Take P = Base;
			P.Name = Pick("armour.smallArms");
			P.Family = "small_arms";
			const double When = Roll();
			P.Delay = Base.Delay + Long * (static_cast<double>(K) + 0.1 + 0.8 * When) / static_cast<double>(Pings);
			const double Tone = Roll();
			P.Pitch *= 1.0 + (Tone * 2.0 - 1.0) * ArmourBurstPitch * 0.01;
			const double Level = Roll();
			P.Db = ArmourDb - Level * ArmourBurstDbSpread;
			if (K + 1 < Pings && ArmourBurstRing > 0.0)
			{
				P.Cut = ArmourBurstRing;
				P.CutFade = std::min(0.05, ArmourBurstRing);
			}
			if (Start(P, Now, Far, bOnScreen, Out)) ++ArmourRings;
		}
		const double WhineWhen = Roll();
		WhineOff(N.At + Vec(0.0, 0.0, 0.3), Far, Now, Base.Delay + 0.02 + Long * WhineWhen, ArmourDb - 2.0, ArmourBurstWhine, Out);
	}

	void Mixer::CutBurst(Take& T, const Noise& N)
	{
		// Every round fired is heard and no more: a burst of 8 from an MG 42 lasts 0.4 s, not the 1.2 s of firing in
		// its recording. From the take's first shot, silent by the time the next round would come: the recording keeps
		// firing after the burst's last round, so a "tail" cut from it is only more rounds (the review's find).
		auto It = FiringPart.find(T.Name);
		if (It != FiringPart.end()) CutBurstAt(T, N, It->second.first, It->second.second);
	}

	void Mixer::CutBurstAt(Take& T, const Noise& N, double A, double B)
	{
		// (from the stretch of firing A to B in the take's recording: its part, or one of its moreParts)
		if (N.Rounds <= 1 || N.PerSecond <= 0.0) return;
		const double Want = N.Rounds / N.PerSecond;
		if (Want >= B - A) return; // the whole recording, with its own tail
		T.From = std::max(0.0, A - 0.01);
		T.CutFade = std::min(BurstEndFade, 0.5 / N.PerSecond);
		T.Cut = std::max(0.02, (A - T.From) + Want - T.CutFade);
		// (what plays past the burst's last gap, faded out included: never above 0)
		MostBurstOver = std::max(MostBurstOver, T.From + T.Cut + T.CutFade - A - Want);
		++BurstsCut;
	}

	bool Mixer::Shout(int Side, const std::string& Army, const Vec& Where, double Now, double Delay, const Listener& Ear, std::vector<Take>& Out)
	{
		// One voice a side at a time, never cut off (limits.voicesPerSide), in the shouter's own tongue: an army with no
		// real voices of its own stays silent rather than speak someone else's language.
		if (Side < 0 || Now + Delay < ShoutFree[Side]) return false;
		const std::string Key = "shout." + Army;
		if (!Lists.count(Key))
		{
			std::vector<std::string> Lines;
			for (const std::string& Line : ListOf("byNation.orders." + Army))
			{
				if (std::find(ShoutExclude.begin(), ShoutExclude.end(), Line) == ShoutExclude.end()) Lines.push_back(Line);
			}
			Lists[Key] = Lines;
		}
		if (Lists[Key].empty()) return false;
		Take T;
		T.Name = Pick(Key);
		T.At = Where + Vec(0.0, 0.0, 1.5);
		T.Family = "voices";
		T.Bus = "voices";
		T.Db = ShoutDb;
		T.Delay = Delay;
		const double Far = Distance(T.At, Ear.At);
		if (!Start(T, Now, Far, OnScreen ? OnScreen(Where) : true, Out)) return false;
		ShoutFree[Side] = Now + T.Delay + (Length ? Length(T.Name) : 1.5) + 0.3;
		++Shouts;
		return true;
	}

	void Mixer::HearIncoming(const Noise& N, double Now, double Battle, double Speed, const Listener& Ear, std::vector<Take>& Out)
	{
		// A shell or a mortar bomb heard in its last moment, the take ending as it lands. Only a real recording of one
		// coming in (land.incoming); none is listed yet, so the moment stays the flash and the blast.
		const std::string Key = N.Weapon == "mortar" ? "incoming.mortar" : N.Weapon == "gun" ? "incoming.artillery" : std::string();
		if (Key.empty() || ListOf(Key).empty() || N.LandsAt < 0.0) return;
		const double Far = Distance(N.To, Ear.At);
		if (Far > IncomingWithin) return;
		Take T;
		T.Name = Pick(Key);
		T.At = N.To + Vec(0.0, 0.0, 20.0);
		T.Family = "explosions";
		const double Long = Length ? Length(T.Name) : 1.5;
		const double Until = (N.LandsAt - Battle) / std::max(0.05, Speed);
		T.Delay = Until - Long;
		if (T.Delay < 0.0)
		{
			T.From = -T.Delay;
			T.Delay = 0.0;
			T.FadeIn = 0.05;
		}
		if (T.From >= Long - 0.05) return;
		if (Start(T, Now, Far, true, Out)) ++Incoming;
	}

	void Mixer::Hear(const Noise& N, double Now, double Battle, double Speed, const Listener& Ear, std::vector<Take>& Out)
	{
		EarHeight = Ear.CameraHeight;
		const double Far = Distance(N.At, Ear.At);
		const bool bOnScreen = OnScreen ? OnScreen(N.At) : true;
		// (a noise from earlier in this frame is at most a frame behind: started now)
		const std::string Army = N.Army.empty() ? std::string("*") : N.Army;
		auto Nation = [&](const char* Kind) -> std::string
		{
			const std::string Key = std::string("byNation.") + Kind + "." + Army;
			return Lists.count(Key) ? Key : std::string("byNation.") + Kind + ".*";
		};
		Take T;
		T.At = N.At;
		T.Track = N.Track;
		// (release 2, bEcho: the gun or the blast just started, thrown back by the faces round the ear)
		auto Thrown = [&](double From, int Most)
		{
			if (!bEcho || Out.empty()) return;
			const Take Was = Out.back();
			EchoOf(Was, From, Now, Speed, Ear, Most, false, Out);
		};
		switch (N.Kind)
		{
		case NoiseKind::Shot:
		{
			++ShotsHeard;
			// (whether the shot itself was past hearing, taken as its own take is started: the crack, the strike, a bolt
			// or a case started after it set bLastTooFar again, and a far rifle under a near round's crack was counted
			// as a shot on the screen dropped)
			bool bStarted = false, bTooFar = false, bMerged = false;
			if (N.Weapon == "rifle" || N.Weapon == "mg")
			{
				const bool bMg = N.Weapon == "mg";
				// (a sniper's one aimed round, the rules' sn: his own family where the mix has one, heard across the field and
				// on the far takes, with their echo, sooner than a section's rifles; while he lies hidden, hd, heard where his
				// round arrives and never from his spot, the design director's ruling, whatever the mix: no bolt, no shout of
				// his, nothing gives his place away)
				const bool bSniper = !bMg && N.bSniper && Families.count("sniper") > 0;
				const bool bUnseen = !bMg && N.bSniper && N.bHidden;
				T.Family = bMg ? "mg" : bSniper ? "sniper" : "small_arms";
				if (bUnseen) T.At = N.To + Vec(0.0, 0.0, 1.5);
				const double ShotFar = bUnseen ? Distance(T.At, Ear.At) : Far;
				const bool bShotOnScreen = bUnseen ? (OnScreen ? OnScreen(T.At) : true) : bOnScreen;
				if (!bMg && N.bSniper) ++(bUnseen ? SniperUnseen : SniperSeen);
				const Family& F = FamilyOf(T.Family);
				// (a tank's machine guns are her own row, by army: a German tank's MG 34 is not her infantry's MG 42, the
				// Sherman's roof .50 not her coaxial .30; an army with no row of its own takes the infantry's)
				std::string MgList = Nation("mg");
				if (bMg && N.Role == "tank")
				{
					const std::string Roof = "tankMg.roof." + Army, Own = "tankMg." + Army;
					MgList = N.Mount == "roof_mg" && Lists.count(Roof) ? Roof : Lists.count(Own) ? Own : Lists.count("tankMg.*") ? std::string("tankMg.*") : MgList;
				}
				// (a support team's gun, her own row by the gun drawn where the mix has one: land.teamMg)
				const bool bTeamMg = bMg && N.Role == "hmg" && !N.Mount.empty() && Lists.count("teamMg." + N.Mount) > 0;
				if (bTeamMg) MgList = "teamMg." + N.Mount;
				const bool bFarMg = bMg && Far > F.FarFrom;
				T.Name = Pick(ShotFar > F.FarFrom ? (bMg ? "mg.far" : "rifle.far") : bMg ? MgList : Nation("rifle"));
				// (release 2, bTravel: the report crosses the ground to the ear; 0 with it off, as before)
				T.Delay = Travel(T.Family, ShotFar, Speed);
				const double Want = bMg && N.PerSecond > 0.0 ? N.Rounds / N.PerSecond : 0.0;
				Take Uncut;
				const int CutsWere = BurstsCut;
				if (bMg)
				{
					// (a take whose firing is shorter than the burst gives way to one long enough, where the list has one)
					for (int Try = 0; Try < 3; ++Try)
					{
						auto Part = FiringPart.find(T.Name);
						if (Part == FiringPart.end() || Part->second.second - Part->second.first >= Want) break;
						T.Name = Pick(bFarMg ? "mg.far" : MgList);
					}
					if (N.Role == "tank" && !bFarMg)
					{
						++TankMgTakes[(N.Mount.empty() ? std::string("?") : N.Mount) + " " + Army + " " + T.Name];
						// (a take on a tank's row shorter than her burst plays whole: more rounds than she fired)
						auto Part = FiringPart.find(T.Name);
						if (Part != FiringPart.end() && Part->second.second - Part->second.first < Want) ++TankMgWhole;
					}
					if (bTeamMg && !bFarMg)
					{
						++TeamMgTakes[N.Mount + " " + T.Name];
						auto Part = FiringPart.find(T.Name);
						if (Part != FiringPart.end() && Part->second.second - Part->second.first < Want) ++TeamMgWhole;
					}
					Uncut = T;
					CutBurst(T, N);
				}
				// (a squad leader's round once his Thompson clip is in: his clip's thompson_burst cue carries the sound)
				const bool bByClip = !bMg && SmgArmySeen.count(Army) && std::find(SmgRoles.begin(), SmgRoles.end(), N.Role) != SmgRoles.end();
				if (bByClip) ++ShotsByClip;
				else
				{
					bStarted = Start(T, Now, ShotFar, bShotOnScreen, Out);
					bTooFar = bLastTooFar;
					bMerged = bLastMerged;
					// (a machine gun's burst crowded out of its recording, every pitch near it sounding, gives way to another
					// recording on its list whose firing outlasts the burst, then to another stretch of firing in any of
					// them (moreParts: a Maxim's take fires three bursts), before it is heard in that copy: an MG at 400 m
					// is still its own rip, the mix's distance note)
					if (bMg && bMerged)
					{
						std::string Merged = T.Name;
						auto Retry = [&](Take Other)
						{
							--SameMerged;
							if (Far <= F.FarFrom && F.FarFrom < 1e8) { --SameMergedNear; --MergedNearBy[Merged]; }
							bStarted = Start(Other, Now, Far, bOnScreen, Out);
							bTooFar = bLastTooFar;
							bMerged = bLastMerged;
							Merged = Other.Name;
							return !bMerged;
						};
						// (one burst, however many places it tries: the count of bursts cut is the one it plays from)
						auto CutAgain = [&](Take& Other, const std::pair<double, double>* Stretch)
						{
							const int Before = BurstsCut;
							if (Stretch) CutBurstAt(Other, N, Stretch->first, Stretch->second);
							else CutBurst(Other, N);
							BurstsCut = CutsWere + (BurstsCut - Before);
						};
						const std::vector<std::string> Own = ListOf(bFarMg ? "mg.far" : MgList);
						for (const std::string& Name : Own)
						{
							auto Part = FiringPart.find(Name);
							if (Name == T.Name || (Part != FiringPart.end() && Part->second.second - Part->second.first < Want)) continue;
							Take Other = Uncut;
							Other.Name = Name;
							CutAgain(Other, nullptr);
							if (Retry(Other)) { ++MgToOther; break; }
						}
						for (size_t I = 0; I < Own.size() && bMerged && N.Rounds > 1 && Want > 0.0; ++I)
						{
							auto More = MoreParts.find(Own[I]);
							if (More == MoreParts.end()) continue;
							for (const auto& Stretch : More->second)
							{
								if (Stretch.second - Stretch.first <= Want) continue; // (a stretch the burst outlasts: its silence would play)
								Take Other = Uncut;
								Other.Name = Own[I];
								CutAgain(Other, &Stretch);
								if (Retry(Other)) { ++MgToOtherPart; break; }
							}
						}
						// (and nearer than the far takes, with every stretch long enough crowded too, the burst plays the
						// longest stretch of firing it fits, cut at that stretch's end: a round or two fewer heard than
						// fired, among the crowd's bursts, rather than none of its own. Six Maxims firing the same instant at
						// 390 m filled every pitch of the take's part: Prokhorovka with the night patrol, 22 November)
						if (bMerged && N.Rounds > 1 && Want > 0.0 && N.PerSecond > 0.0 && Far <= F.FarFrom && F.FarFrom < 1e8)
						{
							std::vector<std::pair<double, std::pair<std::string, std::pair<double, double>>>> Shorter;
							for (const std::string& Name : Own)
							{
								auto More = MoreParts.find(Name);
								if (More == MoreParts.end()) continue;
								for (const auto& Stretch : More->second)
									if (Stretch.second - Stretch.first <= Want) Shorter.push_back({ Stretch.second - Stretch.first, { Name, Stretch } });
							}
							std::stable_sort(Shorter.begin(), Shorter.end(), [](const auto& X, const auto& Y) { return X.first > Y.first; });
							for (const auto& Try : Shorter)
							{
								Take Other = Uncut;
								Other.Name = Try.second.first;
								const double A = Try.second.second.first, B = Try.second.second.second;
								Other.From = std::max(0.0, A - 0.01);
								Other.CutFade = std::min(BurstEndFade, 0.5 / N.PerSecond);
								Other.Cut = std::max(0.02, B - Other.From - Other.CutFade);
								BurstsCut = CutsWere + 1;
								if (Retry(Other)) { ++MgShortPart; break; }
							}
						}
					}
				}
				// (release 2's acoustics: the report as it was started, held for its crossing; a cut burst's own tail; the
				// faces round the ear throwing it back. Each only with its switch: off, nothing here starts a take)
				double ReportDelay = -1.0;
				if (bStarted && !Out.empty())
				{
					const Take Was = Out.back();
					ReportDelay = Was.Delay;
					if (bTravel && T.Delay > 0.0) { ++Travelled; MostTravel = std::max(MostTravel, T.Delay); }
					TailAfter(Was, ShotFar, Now, Out);
					EchoOf(Was, ShotFar, Now, Speed, Ear, 1, true, Out);
				}
				Count(Now, bMg ? "mg" : "small_arms", ShotFar);
				if (!bMg && !bUnseen && N.Track >= 0)
				{
					// The Garand's clip pings out after its eighth round and a new one is pressed in, the bolt slamming
					// home (close by); a bolt rifle's bolt is worked.
					const int Shots = ++RifleCount[N.Track];
					if (!bSniper && Army == "USA" && ClipEvery > 0 && Shots % ClipEvery == 0 && Far < GarandWithin)
					{
						Take Ping = T;
						Ping.Name = PingTake;
						Ping.Family = "body";
						Ping.Delay = T.Delay + PingAfter;
						Ping.From = Ping.Cut = Ping.CutFade = 0.0;
						if (Start(Ping, Now, Far, bOnScreen, Out)) ++Clips;
						// (the reload itself comes from the GI's reload clip, in step with his hands, unless the mix says
						// the soldier files have none)
						if (!ReloadTake.empty() && !(bReloadFromClips && bClipReloadSeen))
						{
							Take Reload = Ping;
							Reload.Name = ReloadTake;
							Reload.Delay = T.Delay + PingAfter + ReloadAfterPing;
							if (Start(Reload, Now, Far, bOnScreen, Out)) ++Reloads;
						}
					}
					// (a sniper works his bolt after every shot, the American's 1903 too)
					else if ((bSniper || Army != "USA") && Far < BoltWithin && (bSniper || Roll() < BoltChance))
					{
						Take Bolt = T;
						Bolt.Name = Pick(Nation("bolt"));
						Bolt.Family = "body";
						Bolt.Delay = T.Delay + (0.55 + Roll() * 0.3);
						Start(Bolt, Now, Far, bOnScreen, Out);
					}
				}
				Crack(N, Now, Ear, Out, Speed, ReportDelay);
				Strike(N, Now, Ear, Out);
				// Shouts in the thick of it: a man of the shooter's side near the ear calls out now and then, more often
				// as the fight thickens (land.shouts).
				if (!bUnseen && N.Side >= 0 && Far < ShoutWithin)
				{
					LastShooter[N.Side] = { Now, N.At, Army };
					if (MeterNow >= ShoutMeterFrom && Now >= ShoutNext[N.Side] && Shout(N.Side, Army, N.At, Now, 0.0, Ear, Out))
					{
						const double Hot = std::clamp((MeterNow - ShoutMeterFrom) / std::max(0.05, 1.0 - ShoutMeterFrom), 0.0, 1.0);
						const double Lo = ShoutGapLow0 + (ShoutGapFull0 - ShoutGapLow0) * Hot;
						const double Hi = ShoutGapLow1 + (ShoutGapFull1 - ShoutGapLow1) * Hot;
						double Gap = Lo + Roll() * std::max(0.0, Hi - Lo);
						if (static_cast<int>(ListOf("shout." + Army).size()) < ShoutFewLines) Gap = std::max(Gap, ShoutFewGap);
						ShoutNext[N.Side] = Now + Gap;
					}
				}
			}
			else if (N.Weapon == "tank" || N.Weapon == "at")
			{
				T.Family = "big_guns";
				T.Name = Pick(Far > FamilyOf("big_guns").FarFrom ? "tank.far" : "tank.close");
				if (N.Weapon == "at") T.Db = -8.0; // a launcher: no recording of its own yet, the tank gun well under
				T.Delay = FlashToBang(Far, Speed);
				bStarted = Start(T, Now, Far, bOnScreen, Out);
				bTooFar = bLastTooFar;
				if (bStarted) Thrown(Far, EchoOfBig);
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
				bTooFar = bLastTooFar;
				if (bStarted) Thrown(Far, EchoOfBig);
				Count(Now, "big_guns", Far);
				HearIncoming(N, Now, Battle, Speed, Ear, Out);
			}
			else if (N.Weapon == "mortar")
			{
				T.Family = "big_guns";
				T.Name = "mortar_fire";
				T.Db = -4.0;
				T.Delay = FlashToBang(Far, Speed);
				bStarted = Start(T, Now, Far, bOnScreen, Out);
				bTooFar = bLastTooFar;
				if (bStarted) Thrown(Far, 1);
				Count(Now, "big_guns", Far);
				HearIncoming(N, Now, Battle, Speed, Ear, Out);
			}
			else if (N.Weapon == "grenade")
			{
				T.Family = "body";
				T.Name = Pick("grenade.throw");
				bStarted = Start(T, Now, Far, bOnScreen, Out);
				bTooFar = bLastTooFar;
			}
			if (bStarted) ++ShotTakes;
			else if (bTooFar) ++ShotsOutOfHearing;
			else if (bMerged) ++ShotsMerged;
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
				if (Start(T, Now, Far, bOnScreen, Out)) Thrown(Far, EchoOfBig);
			}
			else if (Far < NearWithin)
			{
				T.Name = Pick("impact.near");
				T.bBig = Far < BigWithin;
				T.Bus = T.bBig ? "big" : "close";
				if (T.Name == "shell_impact_near_02") { T.Cut = NearTwoCut; T.CutFade = NearTwoCutFade; }
				if (Start(T, Now, Far, bOnScreen, Out)) Thrown(Far, EchoOfBig);
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
				// (the size's own recordings for the band where it has them: land.landings; else the shells')
				const std::string Band = Far < MiddleWithin ? "middle" : "far";
				const std::string Own = "landing." + Size + "." + Band;
				T.Name = Pick(ListOf(Own).empty() ? "impact." + Band : Own);
				if (Start(T, Now, Far, bOnScreen, Out)) Thrown(Far, EchoOfBig);
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
			const bool bSmallArms = N.Weapon == "rifle" || N.Weapon == "mg";
			if (bSmallArms && !ListOf("armour.smallArms").empty())
			{
				// A rifle or machine-gun round off a tank: the ring of the steel it struck, and now and then a whine off it.
				if (N.Rounds > 1 && N.PerSecond > 0.0)
				{
					RattleOff(N, T, Far, Now, bOnScreen, Out); // (a burst's, World::bRicochet: a rattle of pings)
					break;
				}
				T.Name = Pick("armour.smallArms");
				T.Family = "small_arms";
				T.Db = ArmourDb;
				if (Start(T, Now, Far, bOnScreen, Out)) ++ArmourRings;
				WhineOff(N.At + Vec(0.0, 0.0, 0.3), Far, Now, 0.02, ArmourDb - 2.0, ArmourWhineChance, Out);
				break;
			}
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
			// A man down near the ear: sometimes a comrade calls out (from the last of his side heard firing close by,
			// else beside him).
			if (Far < ShoutOnFallWithin && N.Side >= 0 && Roll() < ShoutOnFallChance)
			{
				const Shooter& Near = LastShooter[N.Side];
				const bool bRecent = Now - Near.At < 5.0 && Distance(Near.Where, Ear.At) < ShoutWithin;
				Shout(N.Side, N.Army, bRecent ? Near.Where : N.At + Vec(3.0, 2.0, 0.0), Now, 0.6 + Roll() * 0.6, Ear, Out);
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
			if (Start(T, Now, Far, true, Out)) Thrown(Far, EchoOfBig);
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
		EarHeight = Ear.CameraHeight;
		const double Far = Distance(At, Ear.At);
		if (Far > FamilyOf("body").Silent) return;
		Take T;
		T.At = At;
		T.Family = "body";
		// (land.bodyCues names a cue's take, or none: a cue the mix silences plays nothing)
		auto Named = BodyCue.find(Cue);
		if (Named != BodyCue.end())
		{
			if (Named->second.empty()) return;
			T.Name = Named->second;
			Start(T, Now, Far, OnScreen ? OnScreen(At) : true, Out);
			return;
		}
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
		else if (Cue == "garand_reload")
		{
			// The new clip pressed in now, the operating rod slamming home a third of a second later (two pieces of
			// the one recording, which has them 2.5 s apart).
			if (ReloadTake.empty()) return;
			bClipReloadSeen = true;
			const bool bSeen = OnScreen ? OnScreen(At) : true;
			for (const std::array<double, 3>& P : ReloadPieces)
			{
				Take R = T;
				R.Name = ReloadTake;
				R.From = P[0];
				R.Cut = P[1];
				R.CutFade = std::min(0.08, P[1] * 0.3);
				R.FadeIn = P[0] > 0.0 ? 0.005 : 0.0;
				R.Delay = P[2];
				if (Start(R, Now, Far, bSeen, Out)) ++Reloads;
			}
			return;
		}
		else if (Cue == SmgCue)
		{
			// The squad leader's burst: every round he fires heard and no more, from the first.
			SmgArmySeen[Army] = true;
			const std::string Key = Lists.count("byNation.smg." + Army) ? "byNation.smg." + Army : std::string("byNation.smg.*");
			T.Name = Pick(Key);
			T.Family = "small_arms";
			auto Starts = SmgOnsets.find(T.Name);
			if (Starts != SmgOnsets.end() && static_cast<int>(Starts->second.size()) > SmgBurst && SmgBurst > 0)
			{
				const std::vector<double>& S = Starts->second;
				T.From = std::max(0.0, S[0] - 0.005);
				T.CutFade = std::min(0.03, (S[static_cast<size_t>(SmgBurst)] - S[static_cast<size_t>(SmgBurst) - 1]) * 0.5);
				T.Cut = std::max(0.02, S[static_cast<size_t>(SmgBurst)] - T.From - T.CutFade);
			}
			else if (Starts != SmgOnsets.end() && !Starts->second.empty()) T.From = std::max(0.0, Starts->second[0] - 0.005);
			++SmgBursts;
		}
		else if (Cue == "knee_down") T.Name = "knee_down";
		else if (Cue.rfind("grenade_throw", 0) == 0) T.Name = "grenade_throw";
		// (a man's rifle falling as he dies: no recording of it yet. german_drop_weapon is a German voice calling "Drop
		// the gun!", for a surrender, never a dying man's rifle)
		else if (Cue == "rifle_drop") return;
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
				// Equal-power: the level of a share is its square root. Down among the men, louder (land.closeView).
				Row.Db = Db + 20.0 * std::log10(std::sqrt(Share)) + CloseView.At(Ear.CameraHeight, 0.0);
			}
			OutBeds.push_back(Row);
		}
		// The place's own air: loudest in the lulls.
		Bed Place;
		Place.Name = Cover == "woods" ? PlaceWoods : PlaceOpen;
		Place.bSilent = Cover == "water";
		Place.Db = PlaceDb + (PlaceFullDb - PlaceDb) * MeterNow + (Cover == "buildings" ? PlaceTownDb : 0.0);
		OutBeds.push_back(Place);
		// Rain falling (RainShare): over the place's air, sinking a little as the fight swells but heard through it, and
		// softer the lighter it falls (a light rain a quarter of a downpour's power).
		if (RainShare > 0.0 && !RainTake.empty())
		{
			Bed Rain;
			Rain.Name = RainTake;
			Rain.bSilent = false;
			Rain.Db = RainDb + (RainFullDb - RainDb) * MeterNow + 10.0 * std::log10(std::clamp(RainShare, 0.05, 1.0));
			OutBeds.push_back(Rain);
		}
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
	void Voices::Size(int Warm, int MostVoices)
	{
		Most = std::max(1, MostVoices);
		WarmCount = std::clamp(Warm, 0, Most);
		All.assign(static_cast<size_t>(WarmCount), Voice());
		Idle.clear();
		for (int K = WarmCount - 1; K >= 0; --K) Idle.push_back(K);
		Taken = BusyNow = MostBusy = MadeLate = Stolen = StolenCuts = Faded = 0;
	}

	int Voices::Take(double Now, double Plays, double Pitch, double Cut, double Fade, bool& bMade)
	{
		bMade = false;
		int Slot = -1;
		if (!Idle.empty())
		{
			Slot = Idle.back();
			Idle.pop_back();
		}
		else if (static_cast<int>(All.size()) < Most)
		{
			Slot = static_cast<int>(All.size());
			All.push_back(Voice());
			bMade = true;
			++MadeLate;
		}
		else
		{
			// (every voice sounding: the one nearest its end gives way, the least of it lost)
			double Soonest = 1e18;
			for (size_t K = 0; K < All.size(); ++K)
			{
				if (All[K].Ends < Soonest) { Soonest = All[K].Ends; Slot = static_cast<int>(K); }
			}
			++Stolen;
			if (All[static_cast<size_t>(Slot)].FadeAt >= 0.0) ++StolenCuts; // (its cut never fades: it stops now)
			--BusyNow;
		}
		Voice& V = All[static_cast<size_t>(Slot)];
		++V.Use;
		V.bBusy = true;
		V.Began = Now;
		V.FadeAt = Cut > 0.0 ? Now + Cut : -1.0;
		V.Fade = Cut > 0.0 ? Fade : 0.0;
		V.Ends = Now + (Cut > 0.0 ? Cut + Fade : std::max(0.0, Plays) / std::max(0.01, Pitch));
		++Taken;
		MostBusy = std::max(MostBusy, ++BusyNow);
		return Slot;
	}

	void Voices::Free(int Slot)
	{
		if (Slot < 0 || Slot >= static_cast<int>(All.size()) || !All[static_cast<size_t>(Slot)].bBusy) return;
		Voice& V = All[static_cast<size_t>(Slot)];
		V.bBusy = false;
		V.FadeAt = -1.0;
		--BusyNow;
		Idle.push_back(Slot);
	}

	void Voices::Due(double Now, std::vector<int>& OutFade)
	{
		for (size_t K = 0; K < All.size(); ++K)
		{
			Voice& V = All[K];
			if (V.bBusy && V.FadeAt >= 0.0 && V.FadeAt <= Now + 1e-9)
			{
				V.FadeAt = -1.0;
				OutFade.push_back(static_cast<int>(K));
				++Faded;
			}
		}
	}

	int EngineNames::Of(const Mixer& Mix, int Vehicle, const std::string& ModelName, const std::string& Cue)
	{
		++Asked;
		if (Vehicle < 0) return -1;
		if (Vehicle >= static_cast<int>(ByVehicle.size())) ByVehicle.resize(static_cast<size_t>(Vehicle) + 1);
		Was& W = ByVehicle[static_cast<size_t>(Vehicle)];
		if (W.bSet && W.Cue == Cue && W.Model == ModelName) return W.Name;
		W.bSet = true;
		W.Model = ModelName;
		W.Cue = Cue;
		W.Name = -1;
		if (!Cue.empty())
		{
			++Built;
			const std::string Take = Mix.EngineTake(ModelName, Cue);
			auto It = Index.find(Take);
			if (It == Index.end())
			{
				It = Index.emplace(Take, static_cast<int>(Known.size())).first;
				Known.push_back(Take);
			}
			W.Name = It->second;
		}
		return W.Name;
	}
}
