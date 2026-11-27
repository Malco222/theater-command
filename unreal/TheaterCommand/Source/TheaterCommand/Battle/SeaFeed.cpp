#include "Battle/SeaFeed.h"

#include <algorithm>
#include <cmath>

namespace SeaFeed
{
	int SideOf(const std::string& Side)
	{
		return Side == "att" ? 0 : Side == "def" ? 1 : -1;
	}

	double KnotOf(const Top& H)
	{
		if (!(H.LostAtt >= 0.0) || !(H.LostDef >= 0.0) || !std::isfinite(H.LostAtt) || !std::isfinite(H.LostDef)) return -1.0;
		const double La = std::min(H.LostAtt, 1.0), Ld = std::min(H.LostDef, 1.0);
		double Att = (Ld + 0.05) / (La + Ld + 0.1); // (x / (1 + x), with x the attacker's exchange)
		// (at the end the result's own winner stands, as the after card names him: the rules can give the sea to a side
		// that had broken off, when the last of its chasers is lost; before the end, the side that broke off is losing)
		const int Broke = SideOf(H.Broken), Named = H.bOver ? SideOf(H.Winner) : -1;
		const int Winner = Named >= 0 ? Named : Broke >= 0 ? 1 - Broke : -1;
		if (Winner == 0) Att = std::max(Att, 0.5 + SettleBy);
		else if (Winner == 1) Att = std::min(Att, 0.5 - SettleBy);
		return SideOf(H.Me) == 1 ? 1.0 - Att : Att;
	}


	bool ReadField(const Json& J, SeaShow::Field& Out)
	{
		if (!J.IsObject()) return false;
		Out.Size = J["size"].Num(Out.Size);
		Out.Edge = J["edge"].Num(Out.Edge);
		Out.ZoneName = J["zoneName"].Str("");
		Out.Title = J["title"].Str(Out.ZoneName.c_str());
		Out.Date = J["date"].Str("");
		Out.Lat = J["lat"].Num(Out.Lat);
		Out.Lon = J["lon"].Num(Out.Lon);
		if (J["when"].IsObject())
		{
			Out.Year = J["when"]["year"].Int(Out.Year);
			Out.Month = J["when"]["month"].Int(Out.Month);
			Out.bWhen = J["when"]["month"].IsNumber();
		}
		if (J["att"].IsObject()) { Out.AttNat = J["att"]["nat"].Str(""); Out.AttAdj = J["att"]["adj"].Str(""); }
		if (J["def"].IsObject()) { Out.DefNat = J["def"]["nat"].Str(""); Out.DefAdj = J["def"]["adj"].Str(""); }
		const Json& Classes = J["classes"];
		for (size_t K = 0; K < Classes.Keys.size(); ++K)
		{
			const Json& C = Classes.Values[K];
			SeaShow::ClassSize Size;
			Size.Long = C["len"].Num(Size.Long);
			Size.Beam = C["beam"].Num(Size.Beam);
			Size.Speed = C["speed"].Num(Size.Speed);
			Size.Turn = C["turn"].Num(Size.Turn);
			Size.Reach = C["reach"].Num(Size.Reach);
			Out.Classes[Classes.Keys[K]] = Size;
		}
		Out.TorpTravel = J["torp"]["travel"].Num(Out.TorpTravel);
		Out.TorpRadius = J["torp"]["radius"].Num(Out.TorpRadius);
		Out.StrikeWindup = J["strike"]["windup"].Num(Out.StrikeWindup);
		Out.StrikeRadius = J["strike"]["radius"].Num(Out.StrikeRadius);
		// A convoy's night (ruling 241, form.md §5): light{sun, moon, vis}, each a number or an object. sun: the sun's height
		// (degrees, below 0 at night), or {up}; moon: the share of her lit, or {lit, up, from} (her height and bearing,
		// degrees); vis: how far a ship is seen (metres). And the boats' and the depth charges' own: subs{scope, deep,
		// crush} (keel depths, metres) and dived (her best speed under, metres a second), dc{sink, lethal} (metres a second,
		// metres).
		const Json& Light = J["light"];
		Out.bLight = Light.IsObject();
		if (Out.bLight)
		{
			const Json& Sun = Light["sun"];
			Out.SunUp = Sun.IsObject() ? Sun["up"].Num(-20.0) : Sun.Num(-20.0);
			const Json& Moon = Light["moon"];
			Out.MoonLit = std::clamp(Moon.IsObject() ? Moon["lit"].Num(0.0) : Moon.Num(0.0), 0.0, 1.0);
			Out.MoonUp = Moon.IsObject() ? Moon["up"].Num(30.0) : 30.0;
			Out.MoonFrom = Moon.IsObject() ? Moon["from"].Num(180.0) : 180.0;
			Out.Seeing = std::max(0.0, Light["vis"].Num(0.0));
		}
		const Json& Subs = J["subs"];
		Out.ScopeKeel = std::max(1.0, Subs["scope"].Num(Out.ScopeKeel));
		Out.DeepKeel = std::max(Out.ScopeKeel, Subs["deep"].Num(Out.DeepKeel));
		Out.CrushKeel = std::max(Out.DeepKeel, Subs["crush"].Num(Out.CrushKeel));
		Out.DivedTop = std::clamp(Subs["dived"].Num(Out.DivedTop), 0.5, 15.0);
		Out.ChargeSink = std::max(0.5, J["dc"]["sink"].Num(Out.ChargeSink));
		Out.ChargeLethal = std::max(0.5, J["dc"]["lethal"].Num(Out.ChargeLethal));
		// The wind: { from, speed } (SeaSim.wind and windSpeed), or the bearing alone (the speed the show's own).
		const Json& Wind = J["wind"];
		if (Wind.IsObject())
		{
			Out.WindFrom = Wind["from"].Num(0.0);
			Out.WindSpeed = Wind["speed"].IsNumber() ? std::max(0.0, Wind["speed"].Num()) : -2.0;
		}
		else if (Wind.IsNumber())
		{
			Out.WindFrom = Wind.Num();
			Out.WindSpeed = -2.0; // (a bearing alone: the show keeps it and takes a speed of its own)
		}
		return true;
	}

	namespace
	{
		void ReadPiece(const Json& U, Memory& Kept, SeaShow::Piece& P)
		{
			P.Id = U["id"].Int();
			// What the view left out stands as the piece was last told.
			auto Was = Kept.Still.find(P.Id);
			if (Was != Kept.Still.end()) P = Was->second;
			P.Id = U["id"].Int();
			const Json& X = U["extra"];
			if (U["side"].IsString()) P.Side = SideOf(U["side"].Str()) == 1 ? 1 : 0;
			if (U["name"].IsString()) P.Name = U["name"].Str();
			if (U["nat"].IsString()) P.Nation = U["nat"].Str();
			if (X["cls"].IsString()) P.Class = X["cls"].Str();
			else if (U["kind"].IsString() && P.Class.empty()) P.Class = U["kind"].Str();
			if (X["fleet"].IsString()) P.Fleet = X["fleet"].Str();
			if (X["ships0"].IsNumber()) P.Ships0 = X["ships0"].Int();
			else if (U["figures0"].IsNumber() && P.Ships0 <= 1) P.Ships0 = U["figures0"].Int();
			P.X = U["x"].Num(P.X);
			P.Y = U["y"].Num(P.Y);
			P.Face = U["face"].Num(P.Face);
			P.Str = U["str"].Num(P.Str);
			P.Mor = U["mor"].Num(P.Mor);
			P.State = U["state"].Str(P.State.c_str());
			P.Act = U["act"].Str(P.Act.c_str());
			P.Speed = U["speed"].Num(P.Speed);
			P.Depth = U["d"].Num(P.Depth);
			P.Engaged = U["engaged"].IsNumber() ? U["engaged"].Int() : -1;
			P.Ships = X["ships"].IsNumber() ? X["ships"].Int() : U["figures"].Int(P.Ships);
			// (the show lays a hull for each ship and keys her piece x 64 + slot: never more than 64 in a piece; the
			// pre-check's odd recordings, 21 November: a piece claiming 2,000,000,000 ran memory out, 65 took the next
			// piece's first key)
			P.Ships = std::clamp(P.Ships, 0, 64);
			P.Ships0 = std::clamp(P.Ships0, 0, 64);
			P.Sunk = X["sunk"].Int(0);
			P.Burnt = X["burnt"].Int(0);
			P.SpeedOrder = X["speed"].Str(P.SpeedOrder.c_str());
			P.Run = X["run"].IsNumber() ? X["run"].Int() : -1;
			P.bScreen = X["screen"].Truth(false);
			P.Planes = X["planes"].Num(1.0);
			P.bStrikeWinding = false;
			const Json& Ab = U["abilities"];
			for (size_t K = 0; K < Ab.Count(); ++K)
			{
				if (Ab.At(K)["key"].Str("") == "strike") P.bStrikeWinding = Ab.At(K)["winding"].Truth(false);
			}
			// The battle's look fields (B1), where it gives them; its place in its side's battle line (R1) only while it is in one.
			P.LineAt = X["lineAt"].IsNumber() ? X["lineAt"].Int() : -1;
			P.bLooks = X["helm"].IsNumber();
			if (P.bLooks)
			{
				P.Helm = X["helm"].Num(0.0);
				P.Foe = X["foe"].IsNumber() ? X["foe"].Int() : -1;
				P.RunPhase = X["runPhase"].Str("");
				P.RunUntil = X["runUntil"].Num(-1.0);
				P.bSmoke = X["smoke"].Truth(false);
				P.Flight = X["flight"].Str("");
				P.Cover = X["cover"].Num(-1.0);
			}
			// A convoy's boat and merchant (ruling 241): her state as each view says it (a view without one leaves the show
			// her last, a view without a keel leaves her keel to her state); her column, as last told.
			P.Dive = X["dive"].Str("");
			P.Keel = X["keel"].IsNumber() ? std::clamp(X["keel"].Num(), 0.0, 1000.0) : -1.0;
			P.bScope = X["scope"].Truth(false);
			if (X["column"].IsNumber()) P.ConvoyColumn = X["column"].Int();
			// Whether the player's side has her (Show::SetMine): a boat's hunters as last told (the stager writes heard only as
			// it changes), her deck gun's mark and an escort's echo as each view says (written afresh while they last). The one
			// reading of heard: the show's hunt reads it, and the view under the water asks the show (U1 hook).
			const Json& Heard = X["heard"];
			if (Heard.IsArray())
			{
				P.bHeard = true;
				P.Heard.clear();
				for (size_t K = 0; K < Heard.Count(); ++K)
				{
					if (Heard.At(K).IsNumber()) P.Heard.push_back(Heard.At(K).Int());
				}
			}
			P.GunOn = X["gun"].IsNumber() ? X["gun"].Int() : -1;
			// (her gun's bore where the battle names it, as last told: a boat's gun is the same gun all night; a bore past any
			// ship's deck gun is not read)
			if (X["gunMm"].IsNumber() && X["gunMm"].Num() > 0.0 && X["gunMm"].Num() <= 200.0) P.GunMm = X["gunMm"].Num();
			const Json& Echo = X["echo"];
			P.EchoOn = Echo.IsObject() && Echo["on"].IsNumber() ? Echo["on"].Int() : -1;
			P.EchoKind = Echo.IsObject() ? Echo["kind"].Str("") : std::string();
			Kept.Still[P.Id] = P;
		}

		void ReadEvent(const Json& E, SeaShow::Event& Out)
		{
			Out.T = E["t"].Num();
			Out.What = E["what"].Str("");
			Out.Cue = E["cue"].Str("");
			Out.Text = E["text"].Str("");
			const Json& X = E["extra"];
			// (a convoy's events name the piece that acts as by, ruling 241, where the unit is not given)
			Out.Unit = E["unit"].IsNumber() ? E["unit"].Int() : X["by"].IsNumber() ? X["by"].Int() : -1;
			Out.Side = SideOf(E["side"].Str(""));
			Out.bAt = E["x"].IsNumber();
			Out.X = E["x"].Num();
			Out.Y = E["y"].Num();
			Out.Key = X["key"].Str("");
			Out.Target = X["target"].IsNumber() ? X["target"].Int() : -1;
			Out.Count = X["count"].Int(0);
			Out.From = X["from"].IsNumber() ? X["from"].Int() : -1;
			Out.bAim = X["x"].IsNumber();
			Out.AimX = X["x"].Num();
			Out.AimY = X["y"].Num();
			Out.Running = X["running"].Num(0.0);
			Out.Flown = X["flown"].Num(-1.0);
			Out.Landed = X["landed"].Num(-1.0);
			Out.Denied = X["denied"].Num(0.0);
			Out.bShare = X["share"].IsNumber();
			Out.Share = X["share"].Num(0.0);
			Out.Cls = X["cls"].Str("");
			Out.Why = X["why"].Str("");
			Out.Hand = X["hand"].Int(0);
			Out.Course = X["course"].Num(0.0);
			// (a negative hull is "not known", as absent: never read as a ship worn through)
			Out.Hull = X["hull"].IsNumber() && X["hull"].Num() >= 0.0 ? std::min(X["hull"].Num(), 1.0) : -1.0;
			const Json& Limp = X["limp"];
			Out.bLimp = Limp.IsObject() && Limp["course"].IsNumber();
			Out.LimpCourse = Limp["course"].Num(0.0);
			Out.LimpSpeed = Limp["speed"].Num(0.0);
			const Json& Ships = X["ships"];
			for (size_t K = 0; K < Ships.Count(); ++K) Out.Ships.push_back(Ships.At(K).Int());
			Out.bDitch = X["ditch"].Truth(false);
			// A convoy's night (ruling 241: form.md §5, field-side-read.md §3), from the extra (or the event itself).
			auto Field = [&E, &X](const char* Name) -> const Json& { return X[Name].IsNull() ? E[Name] : X[Name]; };
			Out.Dive = Field("dive").Str("");
			Out.Keel = Field("keel").IsNumber() ? std::clamp(Field("keel").Num(), 0.0, 1000.0) : -1.0;
			Out.Kind = Field("kind").Str("");
			Out.On = Field("on").IsNumber() ? Field("on").Int() : Field("at").IsNumber() ? Field("at").Int() : -1;
			Out.N = std::clamp(Field("n").Int(0), 0, 64);
			Out.Set = Field("set").IsNumber() ? std::clamp(Field("set").Num(), 0.0, 1000.0) : -1.0;
			Out.Sinking = Field("sinking").IsNumber() ? std::clamp(Field("sinking").Num(), 0.0, 600.0) : -1.0;
			// (spread: metres across for a pattern of depth charges, degrees across for a boat's salvo)
			Out.Spread = std::clamp(Field("spread").Num(0.0), 0.0, 2000.0);
			Out.Fan = Out.What == "sea:torpedo" ? std::clamp(Out.Spread, 0.0, 90.0) : 0.0;
			Out.Harm = std::clamp(Field("harm").Num(0.0), 0.0, 1.0);
			Out.How = Field("how").Str("");
			Out.Lost = Field("lost").IsString() ? Field("lost").Str("") : std::string();
			Out.Fish = std::clamp(Field("fish").Int(0), 0, 12);
			Out.bWakeless = Field("wakeless").Truth(false);
			// (sea:surrender: the piece she strikes to; her own piece is "unit", so "by" is read here as its own name)
			Out.By = Field("by").IsNumber() ? Field("by").Int() : -1;
			// The men in the water (the naval rules' sailor events): counts never below none, a crew at most a carrier's few
			// thousand (a count past it is not read); the oil's spreads to a few kilometres.
			auto Count = [&X](const char* Name) { return X[Name].IsNumber() && X[Name].Num() >= 0.0 && X[Name].Num() <= 10000.0 ? X[Name].Int() : -1; };
			Out.Ship = X["ship"].IsNumber() ? std::clamp(X["ship"].Int(), 0, 64) : 0;
			Out.Men = Count("men");
			Out.Dead = Count("dead");
			Out.Guests = std::max(0, Count("guests"));
			Out.OilR = X["r"].IsNumber() ? std::clamp(X["r"].Num(), 0.0, 5000.0) : -1.0;
			Out.LitR = X["litR"].IsNumber() ? std::clamp(X["litR"].Num(), 0.0, 5000.0) : -1.0;
			Out.OilUntil = X["until"].IsNumber() ? X["until"].Num() : -1.0;
			const Json& Wreck = X["wreck"];
			Out.WreckOf = Wreck.IsObject() && Wreck["unit"].IsNumber() ? Wreck["unit"].Int() : -1;
			Out.WreckShip = Wreck.IsObject() && Wreck["ship"].IsNumber() ? std::clamp(Wreck["ship"].Int(), 0, 64) : 0;
			Out.bDone = X["done"].Truth(false);
		}

		// ---- The shelling scene's own (the shore guns plan R1.8): read under -TheaterSeaShore=1 only ----------------------
		// (every name read here is one Rules/shore-scene.ts writes; tests/shore-scene.test.ts holds the two to each other)

		/** A finite number, else the default; and one held within [Lo, Hi]. */
		double Finite(const Json& J, double Default)
		{
			return J.IsNumber() && std::isfinite(J.Num()) ? J.Num() : Default;
		}
		double Within(const Json& J, double Default, double Lo, double Hi)
		{
			return std::clamp(Finite(J, Default), Lo, Hi);
		}
		/** Falls as [[x, y, kind], ...]: at most 64, each with a finite place. */
		void ReadFalls(const Json& J, std::vector<ShoreFall>& Out)
		{
			for (size_t K = 0; K < J.Count() && Out.size() < 64; ++K)
			{
				const Json& F = J.At(K);
				if (!F.IsArray() || F.Count() < 3 || !F.At(0).IsNumber() || !F.At(1).IsNumber() || !std::isfinite(F.At(0).Num()) || !std::isfinite(F.At(1).Num())) continue;
				Out.push_back(ShoreFall{ F.At(0).Num(), F.At(1).Num(), F.At(2).Str("") });
			}
		}
		/** sea:shore, sea:battery, sea:fire or sea:spot: its own fields from its extra; false for any other event. */
		bool ReadShoreEvent(const Json& E, ShoreEvent& Out)
		{
			Out.What = E["what"].Str("");
			if (Out.What != "sea:shore" && Out.What != "sea:battery" && Out.What != "sea:fire" && Out.What != "sea:spot") return false;
			const Json& X = E["extra"];
			Out.T = Finite(E["t"], 0.0);
			Out.Unit = E["unit"].IsNumber() ? E["unit"].Int() : -1;
			Out.Ship = X["ship"].IsNumber() ? std::clamp(X["ship"].Int(), 0, 64) : 0;
			Out.Lands = Finite(X["lands"], -1.0);
			Out.Mm = Within(X["mm"], 0.0, 0.0, 460.0);
			Out.N = std::clamp(X["n"].Int(0), 0, 64);
			Out.Train = Finite(X["train"], 0.0);
			const Json& Lay = X["lay"];
			Out.LayOn = std::clamp(Lay["on"].Int(0), 0, 64);
			Out.LaySea = std::clamp(Lay["sea"].Int(0), 0, 64);
			Out.LayBeach = std::clamp(Lay["beach"].Int(0), 0, 64);
			Out.LayOver = std::clamp(Lay["over"].Int(0), 0, 64);
			Out.Bat = X["bat"].IsNumber() ? std::clamp(X["bat"].Int(), 0, 64) : -1;
			Out.X = Finite(X["x"], 0.0);
			Out.Y = Finite(X["y"], 0.0);
			Out.On = X["on"].IsNumber() ? X["on"].Int() : -1;
			Out.Kind = X["kind"].Str("");
			Out.bDud = X["dud"].Truth(false);
			Out.R = Within(X["r"], 0.0, 0.0, 10000.0);
			Out.Until = Finite(X["until"], -1.0);
			Out.Nat = X["nat"].Str("");
			Out.Alt = Within(X["alt"], 0.0, 0.0, 10000.0);
			ReadFalls(X["falls"], Out.Falls);
			return true;
		}
		/** A window's shots: [{at, kind, subject?, spot?: [x, y]}], at most 16, in the window's order. */
		void ReadShots(const Json& J, std::vector<Shot>& Out)
		{
			for (size_t K = 0; K < J.Count() && Out.size() < 16; ++K)
			{
				const Json& S = J.At(K);
				if (!S.IsObject() || !S["at"].IsNumber() || !std::isfinite(S["at"].Num()) || !S["kind"].IsString()) continue;
				Shot One;
				One.At = S["at"].Num();
				One.Kind = S["kind"].Str("");
				One.Subject = S["subject"].IsNumber() ? S["subject"].Int() : -1;
				const Json& Spot = S["spot"];
				One.bSpot = Spot.IsArray() && Spot.Count() >= 2 && Spot.At(0).IsNumber() && Spot.At(1).IsNumber() && std::isfinite(Spot.At(0).Num()) && std::isfinite(Spot.At(1).Num());
				One.SpotX = One.bSpot ? Spot.At(0).Num() : 0.0;
				One.SpotY = One.bSpot ? Spot.At(1).Num() : 0.0;
				Out.push_back(One);
			}
		}
		// ---- (the shelling scene's own, ends) ----------------------------------------------------------------------
	}

	bool ReadCoast(const Json& J, Coast& Out)
	{
		const Json& C = J["coast"];
		Out = Coast();
		if (!C.IsObject() || !C["bearing"].IsNumber() || !std::isfinite(C["bearing"].Num())) return false;
		Out.bHas = true;
		Out.Bearing = std::fmod(std::fmod(C["bearing"].Num(), 360.0) + 360.0, 360.0);
		Out.AtX = Finite(C["at"]["x"], 0.0);
		Out.AtY = Finite(C["at"]["y"], 0.0);
		// (the land: closed rings of [x, y]; a ring of fewer than three finite points is not read, nor any past the caps)
		const Json& Shore = C["shore"];
		size_t Points = 0;
		for (size_t K = 0; K < Shore.Count() && Out.Shore.size() < ShoreRings; ++K)
		{
			const Json& Ring = Shore.At(K);
			std::vector<std::pair<double, double>> Read;
			for (size_t P = 0; P < Ring.Count() && Points + Read.size() < ShorePoints; ++P)
			{
				const Json& At = Ring.At(P);
				if (At.IsArray() && At.Count() >= 2 && At.At(0).IsNumber() && At.At(1).IsNumber() && std::isfinite(At.At(0).Num()) && std::isfinite(At.At(1).Num()))
				{
					Read.emplace_back(At.At(0).Num(), At.At(1).Num());
				}
			}
			if (Read.size() < 3) continue;
			Points += Read.size();
			Out.Shore.push_back(std::move(Read));
		}
		Out.Form = C["form"].Str("");
		Out.Relief = C["relief"].Str("");
		Out.Rise = Within(C["rise"], 0.0, 0.0, 3000.0);
		Out.Kit = C["kit"].Str("");
		Out.Land = C["land"].Str("");
		Out.Ground = C["ground"].Str("");
		Out.Shelf = Within(C["shelf"], 0.0, 0.0, 1000.0);
		const Json& Town = C["town"];
		Out.bTown = Town.IsObject();
		if (Out.bTown)
		{
			Out.TownName = Town["name"].Str("");
			Out.TownSize = Town["size"].Str("");
			Out.TownLook = Town["look"].Str("");
			Out.bPort = Town["port"].Truth(false);
			Out.TownX = Finite(Town["x"], 0.0);
			Out.TownY = Finite(Town["y"], 0.0);
			Out.TownR = Within(Town["r"], 0.0, 0.0, 10000.0);
			const Json& Quay = Town["quay"];
			Out.bQuay = Quay.IsObject();
			Out.QuayX = Finite(Quay["x"], 0.0);
			Out.QuayY = Finite(Quay["y"], 0.0);
			Out.QuayFace = Finite(Quay["face"], 0.0);
			Out.QuayLen = Within(Quay["len"], 0.0, 0.0, 5000.0);
			Out.Damage = Within(Town["damage"], 0.0, 0.0, 1.0);
			Out.bSkyline = Town["skyline"].Truth(false);
		}
		const Json& Target = C["target"];
		Out.TargetKind = Target["kind"].Str("");
		Out.TargetX = Finite(Target["x"], 0.0);
		Out.TargetY = Finite(Target["y"], 0.0);
		Out.TargetR = Within(Target["r"], 0.0, 0.0, 20000.0);
		const Json& Strips = Target["strips"];
		for (size_t K = 0; K < Strips.Count() && Out.Strips.size() < 16; ++K)
		{
			const Json& S = Strips.At(K);
			ShoreStrip One;
			One.X = Finite(S["x"], 0.0);
			One.Y = Finite(S["y"], 0.0);
			One.Len = Within(S["len"], 0.0, 0.0, 10000.0);
			One.Wide = Within(S["wide"], 0.0, 0.0, 10000.0);
			One.Face = Finite(S["face"], 0.0);
			One.Kind = S["kind"].Str("");
			Out.Strips.push_back(One);
		}
		const Json& Fort = C["fort"];
		Out.FortLevel = std::clamp(Fort["level"].Int(0), 0, 10);
		Out.bFortFired = Out.FortLevel > 0 && Fort["fired"].Truth(false);
		const Json& Batteries = Fort["batteries"];
		for (size_t K = 0; K < Batteries.Count() && Out.Batteries.size() < 16; ++K)
		{
			const Json& B = Batteries.At(K);
			Out.Batteries.push_back(ShoreBattery{ Finite(B["x"], 0.0), Finite(B["y"], 0.0), Finite(B["face"], 0.0), Within(B["mm"], 0.0, 0.0, 460.0) });
		}
		const Json& Air = C["air"];
		Out.AirSpot = Air["spot"].Str("");
		Out.AirNat = Air["nat"].Str("");
		Out.AirFrom = Air["from"].Str("");
		Out.Whose = C["whose"].Str("");
		return true;
	}

	bool ReadView(const Json& J, Memory& Kept, Frame& Out, bool bShore)
	{
		if (!J.IsObject()) return false;
		Top& H = Out.Head;
		H.T = J["t"].Num();
		H.Limit = J["limit"].Num(H.Limit);
		H.Speed = J["speed"].Num(1.0);
		H.BalanceAtt = J["balance"]["att"].Num(0.5);
		H.BreakingAtt = J["breaking"]["att"].Num(0.0);
		H.BreakingDef = J["breaking"]["def"].Num(0.0);
		H.Contact = J["contact"].IsNumber() ? J["contact"].Num() : -1.0;
		H.Me = J["me"].Str("att");
		{
			const Json& Lost = J["lost"];
			if (Lost["att"].IsNumber() && Lost["def"].IsNumber())
			{
				H.LostAtt = Lost["att"].Num();
				H.LostDef = Lost["def"].Num();
			}
			H.Broken = J["broken"].IsString() ? J["broken"].Str("") : std::string();
		}
		const Json& Over = J["over"];
		H.bOver = Over.IsObject();
		if (H.bOver)
		{
			H.Winner = Over["winner"].Str("");
			H.Level = Over["level"].Str("");
			const Json& Lines = Over["lines"];
			for (size_t K = 0; K < Lines.Count(); ++K) H.OverLines.push_back(Lines.At(K).Str(""));
		}
		const Json& Units = J["units"];
		for (size_t K = 0; K < Units.Count(); ++K)
		{
			SeaShow::Piece P;
			ReadPiece(Units.At(K), Kept, P);
			Out.Pieces.push_back(P);
			// (a shelling ship's main bore, 75 to 460 mm, and her guns, where her extra says them; the boats' gunMm and its
			// 200 mm cap above stay as they are)
			const Json& X = Units.At(K)["extra"];
			if (bShore && X["mainMm"].IsNumber() && X["mainMm"].Num() >= 75.0 && X["mainMm"].Num() <= 460.0)
			{
				Out.Guns.push_back(ShoreGuns{ P.Id, X["mainMm"].Num(), std::clamp(X["guns"].Int(0), 0, 32) });
			}
		}
		const Json& Events = J["events"];
		for (size_t K = 0; K < Events.Count(); ++K)
		{
			SeaShow::Event E;
			ReadEvent(Events.At(K), E);
			Out.Events.push_back(E);
			ShoreEvent Shore;
			if (bShore && ReadShoreEvent(Events.At(K), Shore)) Out.Shore.push_back(std::move(Shore));
		}
		const Json& Zones = J["zones"];
		Out.bZones = Zones.IsArray();
		for (size_t K = 0; K < Zones.Count(); ++K)
		{
			const Json& Z = Zones.At(K);
			SeaShow::Zone Zn;
			Zn.Id = Z["id"].Int();
			Zn.Kind = Z["kind"].Str("");
			Zn.X = Z["x"].Num();
			Zn.Y = Z["y"].Num();
			Zn.R = Z["r"].Num();
			Zn.Until = Z["until"].Num();
			Zn.At = Z["at"].Num();
			Zn.Side = SideOf(Z["side"].Str(""));
			Zn.Unit = Z["unit"].IsNumber() ? Z["unit"].Int() : -1;
			Out.Zones.push_back(Zn);
		}
		return true;
	}

	bool ReadAfter(const Json& J, After& Out)
	{
		if (!J.IsObject()) return false;
		const Json& Lines = J["lines"];
		for (size_t K = 0; K < Lines.Count(); ++K) Out.Lines.push_back(Lines.At(K).Str(""));
		Out.Level = J["level"].Str("");
		Out.Winner = J["winner"].Str("");
		Out.Seconds = J["seconds"].Num(0.0);
		return true;
	}

	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError, bool bShore)
	{
		Json J;
		if (!AirFeed::Parse(Source, J, OutError)) return false;
		if (!J.IsObject() || !J["views"].IsArray())
		{
			OutError = "not a recorded fleet action (no views)";
			return false;
		}
		Out.Key = J["key"].Str("");
		Out.Seed = static_cast<uint64_t>(J["seed"].Num(0.0));
		Out.Step = J["step"].Num(1.0);
		if (!ReadField(J["field"], Out.Board))
		{
			OutError = "no field";
			return false;
		}
		Out.Board.Seed = Out.Seed;
		Out.Board.Limit = 1500.0;
		Memory Kept;
		const Json& Views = J["views"];
		for (size_t K = 0; K < Views.Count(); ++K)
		{
			Frame F;
			if (!ReadView(Views.At(K), Kept, F, bShore)) continue;
			if (K == 0) Out.Board.Limit = F.Head.Limit;
			Out.Views.push_back(std::move(F));
		}
		ReadAfter(J["after"], Out.Card);
		// (a staged scene's windows of its battle time: the scene plays these and cuts between them)
		const Json& Beats = J["field"]["staged"]["beats"];
		double Last = -1e300;
		for (size_t K = 0; K < Beats.Count(); ++K)
		{
			const Json& B = Beats.At(K);
			Beat Read;
			Read.What = B["what"].Str("");
			Read.From = B["from"].Num(0.0);
			Read.To = B["to"].Num(0.0);
			Read.Screen = B["screen"].Num(0.0);
			if (!std::isfinite(Read.From) || !std::isfinite(Read.To) || !std::isfinite(Read.Screen) || Read.To - Read.From < 0.1 || Read.Screen < 0.1 || Read.From < Last) continue;
			Last = Read.To;
			if (bShore) ReadShots(B["shots"], Read.Shots);
			Out.Beats.push_back(Read);
		}
		// (a shelling's coast, under -TheaterSeaShore=1 only)
		if (bShore) ReadCoast(J["field"], Out.Shore);
		return !Out.Views.empty();
	}
}
