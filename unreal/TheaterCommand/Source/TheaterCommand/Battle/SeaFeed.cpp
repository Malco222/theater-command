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
		const int Broke = SideOf(H.Broken);
		const int Winner = Broke >= 0 ? 1 - Broke : H.bOver ? SideOf(H.Winner) : -1;
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
			Kept.Still[P.Id] = P;
		}

		void ReadEvent(const Json& E, SeaShow::Event& Out)
		{
			Out.T = E["t"].Num();
			Out.What = E["what"].Str("");
			Out.Cue = E["cue"].Str("");
			Out.Text = E["text"].Str("");
			Out.Unit = E["unit"].IsNumber() ? E["unit"].Int() : -1;
			Out.Side = SideOf(E["side"].Str(""));
			Out.bAt = E["x"].IsNumber();
			Out.X = E["x"].Num();
			Out.Y = E["y"].Num();
			const Json& X = E["extra"];
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
		}
	}

	bool ReadView(const Json& J, Memory& Kept, Frame& Out)
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
		}
		const Json& Events = J["events"];
		for (size_t K = 0; K < Events.Count(); ++K)
		{
			SeaShow::Event E;
			ReadEvent(Events.At(K), E);
			Out.Events.push_back(E);
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

	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError)
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
			if (!ReadView(Views.At(K), Kept, F)) continue;
			if (K == 0) Out.Board.Limit = F.Head.Limit;
			Out.Views.push_back(std::move(F));
		}
		ReadAfter(J["after"], Out.Card);
		return !Out.Views.empty();
	}
}
