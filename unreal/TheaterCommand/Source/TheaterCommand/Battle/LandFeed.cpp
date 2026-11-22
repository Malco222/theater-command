#include "Battle/LandFeed.h"

#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace LandFeed
{
	using LandJson::Json;

	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		const std::string& NoText()
		{
			static const std::string Empty;
			return Empty;
		}

		Result ResultOf(const std::string& R)
		{
			if (R == "glance") return Result::Glance;
			if (R == "worn") return Result::Worn;
			if (R == "kill") return Result::Kill;
			return Result::Miss;
		}

		void ReadBox(const Json& J, double Out[4])
		{
			for (int K = 0; K < 4; ++K) Out[K] = J.At(static_cast<size_t>(K)).Num(0.0);
		}

		void ReadField(const Json& J, Field& F)
		{
			F.W = J["w"].Num(0.0);
			F.H = J["h"].Num(0.0);
			F.Cell = J["cell"].Num(25.0);
			F.Cols = J["cols"].Int(0);
			F.Rows = J["rows"].Int(0);
			for (const Json& G : J["grounds"].Items) F.Grounds.push_back(G.Str());
			const size_t Cells = static_cast<size_t>(std::max(0, F.Cols * F.Rows));
			F.Ground.assign(Cells, 0);
			F.Height.assign(Cells, 0.0f);
			F.Storeys.assign(Cells, 0);
			const Json& Ground = J["ground"];
			const Json& Height = J["height"];
			const Json& Storeys = J["storeys"];
			for (size_t K = 0; K < Cells; ++K)
			{
				F.Ground[K] = static_cast<uint8_t>(std::clamp(Ground.At(K).Int(0), 0, 255));
				F.Height[K] = static_cast<float>(Height.At(K).Num(0.0));
				F.Storeys[K] = static_cast<uint8_t>(std::clamp(Storeys.At(K).Int(0), 0, 255));
			}
			const Json& Town = J["town"];
			F.TownX = Town["x"].Num(F.W * 0.5);
			F.TownY = Town["y"].Num(F.H * 0.5);
			F.TownR = Town["r"].Num(0.0);
			const Json& Place = J["place"];
			F.PlaceName = Place["name"].Str("");
			F.Terrain = Place["terrain"].Str("plains");
			F.TownName = Place["town"].Str("");
			F.TownSize = Place["townSize"].Str("");
			ReadBox(J["zones"]["att"], F.ZoneAtt);
			ReadBox(J["zones"]["def"], F.ZoneDef);
			const Json& S = J["steppe"];
			if (S.IsObject())
			{
				Field::SteppeLay& L = F.Steppe;
				const Json& Fields = S["fields"];
				L.Turn = Fields["turn"].Num(0.0);
				L.CX = Fields["cx"].Num(F.W * 0.5);
				L.CY = Fields["cy"].Num(F.H * 0.5);
				L.Xs = LandJson::Numbers(Fields["xs"]);
				L.Ys = LandJson::Numbers(Fields["ys"]);
				L.Shift = LandJson::Numbers(Fields["shift"]);
				L.Patch = Fields["patch"].Str("");
				L.Crop = S["crop"].Str("");
				auto Line = [](const Json& A, std::vector<std::pair<double, double>>& Out) {
					for (const Json& P : A.Items) Out.push_back({ P.At(0).Num(0.0), P.At(1).Num(0.0) });
				};
				Line(S["balka"]["line"], L.Balka);
				L.BalkaDeep = S["balka"]["deep"].Num(0.0);
				Line(S["rail"]["line"], L.Rail);
				L.RailBank = S["rail"]["bank"].Num(0.0);
				L.HillX = S["hill"]["x"].Num(0.0);
				L.HillY = S["hill"]["y"].Num(0.0);
				L.HillR = S["hill"]["r"].Num(0.0);
				L.HillRise = S["hill"]["rise"].Num(0.0);
				L.bOn = L.Xs.size() >= 2 && L.Ys.size() >= 2;
			}
		}

		void ReadView(const Json& J, View& V)
		{
			V.T = J["t"].Num(0.0);
			V.BalanceAtt = J["balance"]["att"].Num(0.5);
			V.BalanceDef = J["balance"]["def"].Num(0.5);
			V.BreakingAtt = J["breaking"]["att"].Num(0.0);
			V.BreakingDef = J["breaking"]["def"].Num(0.0);
			V.Contact = J["contact"].IsNumber() ? J["contact"].Num() : -1.0;
			for (const Json& U : J["units"].Items)
			{
				Unit Row;
				Row.Id = U["id"].Int(0);
				Row.Side = SideOf(U["side"].Str());
				Row.X = U["x"].Num(0.0);
				Row.Y = U["y"].Num(0.0);
				Row.Face = U["face"].Num(0.0);
				Row.Str = U["str"].Num(1.0);
				Row.Mor = U["mor"].Num(1.0);
				Row.Figures = U["figures"].Int(0);
				Row.State = U["state"].Str("steady");
				Row.Act = U["act"].Str("idle");
				Row.Cover = U["cover"].Str("open");
				Row.Engaged = U["engaged"].Int(-1);
				// (a live battle's: whether the player has met it, taken it in hand, an order on the air, its order)
				Row.bKnown = U["known"].Truth(true);
				Row.bTaken = U["taken"].Truth(false);
				Row.bHidden = U["hid"].Truth(false);
				Row.OrderIn = U["orderIn"].IsNumber() ? U["orderIn"].Num() : -1.0;
				Row.Men = U["men"].Int(-1);
				const Json& O = U["order"];
				if (O.IsObject())
				{
					Row.OrderKind = O["kind"].Str("");
					Row.OrderX = O["x"].Num(0.0);
					Row.OrderY = O["y"].Num(0.0);
					Row.OrderTarget = O["target"].Int(-1);
					for (const Json& P : O["path"].Items) Row.OrderPath.push_back({ P.At(0).Num(0.0), P.At(1).Num(0.0) });
				}
				V.Units.push_back(std::move(Row));
			}
		}

		void ReadWho(const Json& J, Track& T)
		{
			T.Block = J["b"].Int(0);
			T.K = J["k"].Int(0);
			T.J = std::max(0, J["j"].Int(0));
			T.Role = J["r"].Str("rifle");
			T.Fo = J["fo"].Int(0);
		}

		void ReadPoints(const Json& A, std::vector<Point>& Out)
		{
			Out.reserve(Out.size() + A.Count());
			for (const Json& P : A.Items)
			{
				Point Pt;
				Pt.T = P.At(0).Num(0.0);
				Pt.X = P.At(1).Num(0.0);
				Pt.Y = P.At(2).Num(0.0);
				Pt.F = P.At(3).Num(0.0);
				Out.push_back(Pt);
			}
		}

		void ReadChanges(const Json& A, std::vector<Change>& Out)
		{
			for (const Json& C : A.Items)
			{
				Change Row;
				Row.T = C.At(0).Num(0.0);
				Row.Pose = C.At(1).Str("idle");
				Row.Cover = C.At(2).Str("none");
				Row.bAim = C.At(3).IsNumber() && C.At(4).IsNumber();
				Row.AimX = C.At(3).Num(0.0);
				Row.AimY = C.At(4).Num(0.0);
				Row.bHidden = C.At(5).Truth(false);
				Row.bButtoned = C.At(6).Truth(false);
				Out.push_back(std::move(Row));
			}
		}

		void ReadScars(const Json& A, std::vector<Scar>& Out)
		{
			Out.clear();
			for (const Json& M : A.Items)
			{
				Scar S;
				S.Face = M.At(0).Str("front");
				S.U = M.At(1).Num(0.0);
				S.H = M.At(2).Num(0.0);
				S.Kind = M.At(3).Str("gouge");
				S.From = M.At(4).Num(0.0);
				Out.push_back(std::move(S));
			}
		}

		/** A vehicle knocked out stays where it was hit: the recording's last point jumps 14 to 22 m in a tenth of a
		 * second at the moment of its death (its formation place, not its own), and the wreck is laid where it was. */
		void PinWreck(Track& T)
		{
			if (!T.IsVehicle() || T.Fell < 0.0) return;
			for (size_t I = 1; I < T.At.size() && !T.bPinned; ++I)
			{
				const Point& A = T.At[I - 1];
				const Point& B = T.At[I];
				if (B.T < T.Fell - 0.15 || B.T - A.T > 0.6 || std::hypot(B.X - A.X, B.Y - A.Y) < 3.0) continue;
				T.bPinned = true;
				T.PinFrom = B.T;
				T.PinX = A.X;
				T.PinY = A.Y;
			}
			// (every point from the jump on, those a live battle adds later too)
			if (T.bPinned)
				for (Point& P : T.At)
					if (P.T >= T.PinFrom) { P.X = T.PinX; P.Y = T.PinY; }
		}

		void ReadTrack(const Json& J, Track& T)
		{
			ReadWho(J, T);
			ReadPoints(J["at"], T.At);
			ReadChanges(J["ch"], T.Ch);
			T.Shots = LandJson::Numbers(J["s"]);
			T.Fell = J["d"].IsNumber() ? J["d"].Num() : -1.0;
			T.Out = J["out"].IsNumber() ? J["out"].Num() : -1.0;
			PinWreck(T);
			ReadScars(J["mk"], T.Marks);
		}

		void ReadEvent(const Json& E, Event& Row)
		{
			Row.T = E["t"].Num(0.0);
			Row.What = E["what"].Str("");
			Row.Side = E["side"].Str("");
			Row.Text = E["text"].Str("");
			Row.Cue = E["cue"].Str("");
			Row.Cal = E["cal"].Str("");
			Row.Ground = E["ground"].Str("");
			Row.Unit = E["unit"].Int(-1);
			Row.On = E["on"].Int(-1);
			Row.By = E["by"].Int(-1);
			Row.bWhere = E["x"].IsNumber() && E["y"].IsNumber();
			Row.X = E["x"].Num(0.0);
			Row.Y = E["y"].Num(0.0);
			Row.Reach = E["reach"].Num(0.0);
			Row.bArmour = E["armour"].Truth(false);
		}

		void ReadRound(const Json& R, Round& Row)
		{
			Row.N = R["n"].Int(0);
			Row.T = R["t"].Num(0.0);
			Row.L = R["l"].Num(Row.T);
			Row.By = R["by"].Int(-1);
			Row.K = R["k"].Int(-1);
			Row.J = std::max(0, R["j"].Int(0));
			Row.W = R["w"].Str("rifle");
			Row.On = R["on"].Int(-1);
			Row.Ok = R["ok"].Int(-1);
			Row.Oj = std::max(0, R["oj"].Int(0));
			Row.X = R["x"].Num(0.0);
			Row.Y = R["y"].Num(0.0);
			Row.R = ResultOf(R["r"].Str("miss"));
			Row.Fc = R["fc"].Str("");
			Row.U = R["u"].Num(0.0);
			Row.H = R["h"].Num(0.0);
		}

		bool RoundBefore(const Round& A, const Round& B) { return A.T != B.T ? A.T < B.T : A.N < B.N; }

		/** A view's craters (zones of kind crater, by number) and the tank crews' names riding on its blocks. */
		void ReadViewExtras(const Json& V, std::map<int, Crater>& Craters, std::vector<Block>& Blocks)
		{
			for (const Json& Z : V["zones"].Items)
			{
				if (Z["kind"].Str() != "crater") continue;
				Crater C;
				C.X = Z["x"].Num(0.0);
				C.Y = Z["y"].Num(0.0);
				C.R = Z["r"].Num(0.0);
				C.At = Z["at"].Num(0.0);
				C.Cal = Z["cal"].Str("");
				Craters[Z["id"].Int(0)] = C;
			}
			// The crews' names ride on the tank blocks' views (the first view that has them is enough).
			for (const Json& U : V["units"].Items)
			{
				const Json& Names = U["crew"]["names"];
				if (!Names.IsArray()) continue;
				for (Block& B : Blocks)
				{
					if (B.Id != U["id"].Int(-1) || !B.CrewNames.empty()) continue;
					for (const Json& N : Names.Items) B.CrewNames.push_back(N.IsObject() ? N["en"].Str(N["name"].Str("").c_str()) : std::string());
				}
			}
		}

		void ReadOver(const Json& O, double End, Over& Out)
		{
			Out.Winner = O["winner"].Str("");
			Out.How = O["how"].Str("");
			Out.Level = O["level"].Str("");
			Out.Broke = O["broke"].Str("");
			Out.Seconds = O["seconds"].Num(End);
			Out.Lines.clear();
			for (const Json& L : O["lines"].Items) Out.Lines.push_back(L.Str());
			Out.LostAtt = O["lost"]["att"].Num(0.0);
			Out.LostDef = O["lost"]["def"].Num(0.0);
		}

		/** Two tracks drawn in one place for the whole battle (within half a metre at each of the first's points). */
		bool SameWalk(const Track& A, const Track& B)
		{
			if (A.At.empty() || B.At.empty() || A.Role != B.Role) return false;
			if (std::abs(A.Start() - B.Start()) > 1.0 || std::abs(A.End() - B.End()) > 1.0) return false;
			for (const Point& P : A.At)
			{
				const Spot S = SpotAt(B, P.T);
				if (std::hypot(S.X - P.X, S.Y - P.Y) > 0.5) return false;
			}
			return true;
		}
	}

	int SideOf(const std::string& Side)
	{
		if (Side == "att") return 0;
		if (Side == "def") return 1;
		return -1;
	}

	double Wrap(double F)
	{
		while (F > Pi) F -= 2.0 * Pi;
		while (F <= -Pi) F += 2.0 * Pi;
		return F;
	}

	double TurnToward(double A, double B, double Share)
	{
		return Wrap(A + Wrap(B - A) * Share);
	}

	double FacingTo(double FromX, double FromY, double ToX, double ToY)
	{
		return std::atan2(ToX - FromX, ToY - FromY);
	}

	const std::string& Field::GroundAt(double X, double Y) const
	{
		if (Cols <= 0 || Rows <= 0 || Grounds.empty()) return NoText();
		const int C = std::clamp(static_cast<int>(std::floor(X / Cell)), 0, Cols - 1);
		const int R = std::clamp(static_cast<int>(std::floor(Y / Cell)), 0, Rows - 1);
		const int G = Ground[static_cast<size_t>(R * Cols + C)];
		return G < static_cast<int>(Grounds.size()) ? Grounds[static_cast<size_t>(G)] : NoText();
	}

	double Field::HeightAt(double X, double Y) const
	{
		if (Cols <= 0 || Rows <= 0) return 0.0;
		// Heights are the cells' middles.
		const double Cx = std::clamp(X / Cell - 0.5, 0.0, static_cast<double>(Cols - 1));
		const double Cy = std::clamp(Y / Cell - 0.5, 0.0, static_cast<double>(Rows - 1));
		const int C0 = static_cast<int>(std::floor(Cx));
		const int R0 = static_cast<int>(std::floor(Cy));
		const int C1 = std::min(C0 + 1, Cols - 1);
		const int R1 = std::min(R0 + 1, Rows - 1);
		const double Fx = Cx - C0, Fy = Cy - R0;
		auto Up = [&](int C, int R) { return static_cast<double>(Height[static_cast<size_t>(R * Cols + C)]); };
		return (Up(C0, R0) * (1 - Fx) + Up(C1, R0) * Fx) * (1 - Fy) + (Up(C0, R1) * (1 - Fx) + Up(C1, R1) * Fx) * Fy;
	}

	const Block* Recording::BlockOf(int Id) const
	{
		for (const Block& B : Blocks) if (B.Id == Id) return &B;
		return nullptr;
	}

	int Recording::TrackIndexOf(int B, int K, int J) const
	{
		for (size_t I = 0; I < Tracks.size(); ++I)
			if (Tracks[I].Block == B && Tracks[I].K == K && Tracks[I].J == J) return static_cast<int>(I);
		return -1;
	}

	const Track* Recording::TrackOf(int B, int K, int J) const
	{
		const int I = TrackIndexOf(B, K, J);
		return I < 0 ? nullptr : &Tracks[static_cast<size_t>(I)];
	}

	bool Recording::HasSections() const
	{
		for (const Track& T : Tracks) if (T.J > 0) return true;
		return false;
	}

	const std::vector<Phase>* Recording::PhasesOf(int B) const
	{
		for (const auto& Pair : Phases) if (Pair.first == B) return &Pair.second;
		return nullptr;
	}

	const std::string& Recording::PhaseAt(int B, double T) const
	{
		const std::vector<Phase>* List = PhasesOf(B);
		if (!List) return NoText();
		const Phase* Found = nullptr;
		for (const Phase& P : *List)
		{
			if (P.T > T) break;
			Found = &P;
		}
		return Found ? Found->Ph : NoText();
	}

	const View* Recording::ViewAt(double T) const
	{
		if (Views.empty()) return nullptr;
		auto It = std::upper_bound(Views.begin(), Views.end(), T, [](double Time, const View& V) { return Time < V.T; });
		if (It == Views.begin()) return &Views.front();
		return &*(It - 1);
	}

	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError)
	{
		Json Root;
		if (!LandJson::Parse(Source, Root, OutError)) return false;
		if (!Root.IsObject() || !Root["men"].IsArray() || !Root["field"].IsObject())
		{
			OutError = "not a ground battle recording (no men or no field)";
			return false;
		}
		Out = Recording();
		Out.Key = Root["key"].Str("");
		Out.Title = Root["title"].Str("");
		Out.Date = Root["date"].Str("");
		Out.Seed = static_cast<uint64_t>(std::max(0.0, Root["seed"].Num(0.0)));
		Out.Dt = Root["dt"].Num(0.1);
		Out.Turn = Root["turn"].Int(0);
		const Json& Sky = Root["sky"];
		Out.Weather.Cover = Sky["cover"].Num(0.0);
		Out.Weather.Base = Sky["base"].Num(1000.0);
		Out.Weather.Kind = Sky["kind"].Str("cumulus");
		Out.Weather.Lat = Sky["latLon"].At(0).Num(49.0);
		Out.Weather.Lon = Sky["latLon"].At(1).Num(0.0);
		Out.Weather.Hour = Sky["hour"].Num(10.0);
		if (Sky["wind"].Count() >= 2)
		{
			Out.Weather.bWind = true;
			Out.Weather.WindX = Sky["wind"].At(0).Num(0.0);
			Out.Weather.WindY = Sky["wind"].At(1).Num(0.0);
		}
		if (Sky["grade"].Count() >= 4)
		{
			Out.Weather.bGrade = true;
			for (int K = 0; K < 4; ++K) Out.Weather.Grade[K] = Sky["grade"].At(static_cast<size_t>(K)).Num(Out.Weather.Grade[K]);
		}
		const char* SideKeys[2] = { "att", "def" };
		for (int S = 0; S < 2; ++S)
		{
			const Json& J = Root["sides"][SideKeys[S]];
			Out.Sides[S].Nat = J["nat"].Str("");
			Out.Sides[S].Adj = J["adj"].Str("");
			Out.Sides[S].Colour = J["color"].Str("");
			Out.Sides[S].Army = J["army"].Str("");
			Out.Sides[S].StandIn = J["standIn"].Str("");
		}
		ReadField(Root["field"], Out.Ground);
		Out.Limit = Root["limit"].Num(0.0);
		Out.Contact = Root["contact"].IsNumber() ? Root["contact"].Num() : -1.0;
		Out.End = Root["end"].Num(Out.Limit);

		for (const Json& B : Root["blocks"].Items)
		{
			Block Row;
			Row.Id = B["id"].Int(0);
			Row.Side = SideOf(B["side"].Str());
			Row.Nat = B["nat"].Str("");
			Row.Kind = B["kind"].Str("inf");
			Row.Name = B["name"].Str("");
			Row.Short = B["short"].Str(Row.Name.c_str());
			Row.Parent = B["parent"].Str("");
			Row.Model = B["model"].Str("");
			Row.Army = B["army"].Str(Row.Nat.c_str());
			Row.StandIn = B["standIn"].Str("");
			Row.Team = B["team"].Str("");
			Row.Cal = B["cal"].Str("");
			Row.Figures0 = B["figures0"].Int(0);
			Row.Men0 = B["men0"].Int(0);
			Row.W = B["w"].Num(0.0);
			Row.D = B["d"].Num(0.0);
			Out.Blocks.push_back(std::move(Row));
		}
		for (const Json& P : Root["phases"].Items)
		{
			std::vector<Phase> List;
			for (const Json& Ph : P["ph"].Items)
			{
				Phase Row;
				Row.T = Ph.At(0).Num(0.0);
				Row.Ph = Ph.At(1).Str("");
				List.push_back(std::move(Row));
			}
			Out.Phases.emplace_back(P["b"].Int(0), std::move(List));
		}
		std::map<int, Crater> Craters;
		for (const Json& V : Root["views"].Items)
		{
			View Row;
			ReadView(V, Row);
			Out.Views.push_back(std::move(Row));
			ReadViewExtras(V, Craters, Out.Blocks);
		}
		for (auto& Pair : Craters) { Out.Craters.push_back(Pair.second); Out.CraterIds.insert(Pair.first); }
		std::sort(Out.Craters.begin(), Out.Craters.end(), [](const Crater& A, const Crater& B) { return A.At < B.At; });
		for (const Json& E : Root["events"].Items)
		{
			Event Row;
			ReadEvent(E, Row);
			Out.Events.push_back(std::move(Row));
		}
		for (const Json& R : Root["rounds"].Items)
		{
			Round Row;
			ReadRound(R, Row);
			Out.Rounds.push_back(std::move(Row));
		}
		std::sort(Out.Rounds.begin(), Out.Rounds.end(), RoundBefore);
		for (const Json& M : Root["men"].Items)
		{
			Track Row;
			ReadTrack(M, Row);
			if (Row.At.empty()) continue;
			Out.Tracks.push_back(std::move(Row));
		}
		for (size_t I = 0; I < Out.Tracks.size(); ++I)
			for (size_t J = 0; J < I; ++J)
				if (!Out.Tracks[J].bTwin && Out.Tracks[I].Role == "fo" && SameWalk(Out.Tracks[J], Out.Tracks[I]))
				{
					Out.Tracks[I].bTwin = true;
					break;
				}
		ReadOver(Root["over"], Out.End, Out.Result);
		// A live battle's header (release 2): nothing fought yet; the deltas grow it (AppendDelta).
		Out.bLive = Root["live"].Truth(false);
		if (Out.bLive)
		{
			Out.Fought = 0.0;
			Out.End = 0.0;
			Out.bOver = false;
			Out.Me = Root["me"].IsString() ? SideOf(Root["me"].Str()) : -1;
			const Json& Setup = Root["setup"];
			Out.bSetup = Setup.IsObject() && Setup["room"].Count() >= 4;
			if (Out.bSetup) ReadBox(Setup["room"], Out.Room);
		}
		return true;
	}

	bool AppendDelta(const std::string& Source, Recording& R, Grown& Out, std::string& OutError)
	{
		Json Root;
		if (!LandJson::Parse(Source, Root, OutError)) return false;
		if (!Root.IsObject() || !Root["men"].IsArray() || !Root["t"].IsNumber())
		{
			OutError = "not a live ground battle's delta (no t or no men)";
			return false;
		}
		Out = Grown();
		Out.T = Root["t"].Num();
		Out.bReset = Root["reset"].Truth(false);
		if (Out.bReset)
		{
			// The men placed again before the clock ran: their tracks begin again (the rules' numbers with them).
			R.Tracks.clear();
			R.Views.clear();
			R.Phases.clear();
			R.Rounds.clear();
			R.Events.clear();
			R.Craters.clear();
			R.CraterIds.clear();
		}
		Out.NewTracksFrom = R.Tracks.size();
		Out.RoundsFrom = R.Rounds.size();
		Out.EventsFrom = R.Events.size();
		Out.ViewsFrom = R.Views.size();
		Out.CratersFrom = R.Craters.size();
		auto Note = [](std::vector<int>& List, int I) { if (List.empty() || List.back() != I) List.push_back(I); };

		// The tracks' news. A track's n is its place in the order first seen: the next new one is the next index.
		for (const Json& M : Root["men"].Items)
		{
			const int N = M["n"].Int(-1);
			if (N < 0 || N > static_cast<int>(R.Tracks.size()))
			{
				OutError = "a delta names track " + std::to_string(N) + " of " + std::to_string(R.Tracks.size());
				return false;
			}
			if (N == static_cast<int>(R.Tracks.size()))
			{
				if (!M.Has("b"))
				{
					OutError = "a new track " + std::to_string(N) + " without who it is";
					return false;
				}
				Track Row;
				ReadWho(M, Row);
				R.Tracks.push_back(std::move(Row));
			}
			Track& T = R.Tracks[static_cast<size_t>(N)];
			const Json& At = M["at"];
			if (At.Count() > 0)
			{
				// The provisional last point gives way to the kept ones; it stays only while it is still later than them.
				Point Tail;
				const bool bHadTail = T.bTail && !T.At.empty();
				if (bHadTail) { Tail = T.At.back(); T.At.pop_back(); T.bTail = false; }
				ReadPoints(At, T.At);
				if (bHadTail && Tail.T > T.At.back().T) { T.At.push_back(Tail); T.bTail = true; }
				Note(Out.Moved, N);
			}
			if (M["ch"].Count() > 0) { ReadChanges(M["ch"], T.Ch); Note(Out.Changed, N); }
			if (M["s"].Count() > 0)
			{
				for (const double S : LandJson::Numbers(M["s"])) T.Shots.push_back(S);
				Note(Out.Shot, N);
			}
			if (M["d"].IsNumber()) { T.Fell = M["d"].Num(); Note(Out.Fell, N); }
			if (M["out"].IsNumber())
			{
				// (-1: he is back on the list)
				const double O = M["out"].Num();
				T.Out = O < 0.0 ? -1.0 : O;
				Note(Out.Left, N);
			}
			if (M["mk"].IsArray()) { ReadScars(M["mk"], T.Marks); Note(Out.Marked, N); }
		}
		// Where each moving man stands now: [n, x, y, f] at t, his provisional last point.
		const Json& Tails = Root["tails"];
		for (size_t K = 0; K + 3 < Tails.Count(); K += 4)
		{
			const int N = Tails.At(K).Int(-1);
			if (N < 0 || N >= static_cast<int>(R.Tracks.size()))
			{
				OutError = "a delta's tail names track " + std::to_string(N);
				return false;
			}
			Track& T = R.Tracks[static_cast<size_t>(N)];
			if (T.bTail && !T.At.empty()) T.At.pop_back();
			Point P;
			P.T = Out.T;
			P.X = Tails.At(K + 1).Num(0.0);
			P.Y = Tails.At(K + 2).Num(0.0);
			P.F = Tails.At(K + 3).Num(0.0);
			T.At.push_back(P);
			T.bTail = true;
			if (Out.Moved.empty() || std::find(Out.Moved.begin(), Out.Moved.end(), N) == Out.Moved.end()) Out.Moved.push_back(N);
		}
		std::sort(Out.Moved.begin(), Out.Moved.end());
		// A wreck stays where it was hit; and the spotter pair drawn once (as ReadRecording, on what is fought so far).
		for (const int N : Out.Moved) PinWreck(R.Tracks[static_cast<size_t>(N)]);
		for (const int N : Out.Fell) PinWreck(R.Tracks[static_cast<size_t>(N)]);
		for (size_t I = Out.NewTracksFrom; I < R.Tracks.size(); ++I)
		{
			Track& T = R.Tracks[I];
			if (T.Role != "fo" || T.At.empty()) continue;
			for (size_t J = 0; J < I; ++J)
			{
				const Track& A = R.Tracks[J];
				if (A.bTwin || A.Role != "fo" || A.At.empty()) continue;
				if (std::hypot(A.At.front().X - T.At.front().X, A.At.front().Y - T.At.front().Y) <= 0.5 && std::abs(A.Start() - T.Start()) <= 1.0)
				{
					T.bTwin = true;
					break;
				}
			}
		}

		for (const Json& Rd : Root["rounds"].Items)
		{
			Round Row;
			ReadRound(Rd, Row);
			R.Rounds.push_back(std::move(Row));
		}
		// (each delta's own in order; kept in the order ReadRecording sorts them, should a round come out of turn)
		if (!std::is_sorted(R.Rounds.begin() + static_cast<std::ptrdiff_t>(Out.RoundsFrom > 0 ? Out.RoundsFrom - 1 : 0), R.Rounds.end(), RoundBefore))
		{
			std::stable_sort(R.Rounds.begin(), R.Rounds.end(), RoundBefore);
			Out.RoundsFrom = 0;
		}
		for (const Json& E : Root["events"].Items)
		{
			Event Row;
			ReadEvent(E, Row);
			R.Events.push_back(std::move(Row));
		}
		std::map<int, Crater> Craters;
		for (const Json& V : Root["views"].Items)
		{
			View Row;
			ReadView(V, Row);
			R.Views.push_back(std::move(Row));
			ReadViewExtras(V, Craters, R.Blocks);
			if (V["setup"].IsNull() && R.bSetup) { R.bSetup = false; Out.bBegun = true; }
		}
		for (auto& Pair : Craters)
		{
			if (!R.CraterIds.insert(Pair.first).second) continue;
			R.Craters.push_back(Pair.second);
		}
		std::stable_sort(R.Craters.begin() + static_cast<std::ptrdiff_t>(Out.CratersFrom), R.Craters.end(), [](const Crater& A, const Crater& B) { return A.At < B.At; });
		for (const Json& P : Root["phases"].Items)
		{
			const int B = P["b"].Int(0);
			std::vector<Phase>* List = nullptr;
			for (auto& Pair : R.Phases) if (Pair.first == B) List = &Pair.second;
			if (!List) { R.Phases.emplace_back(B, std::vector<Phase>()); List = &R.Phases.back().second; }
			for (const Json& Ph : P["ph"].Items)
			{
				Phase Row;
				Row.T = Ph.At(0).Num(0.0);
				Row.Ph = Ph.At(1).Str("");
				List->push_back(std::move(Row));
			}
			Out.Phased.push_back(B);
		}
		if (R.Contact < 0.0 && Root["contact"].IsNumber())
		{
			R.Contact = Root["contact"].Num();
			Out.bContact = true;
		}
		R.Fought = std::max(R.Fought, Out.T);
		if (!R.bOver) R.End = R.Fought;
		if (Root["over"].IsObject())
		{
			R.bOver = true;
			R.End = Root["end"].Num(Out.T);
			ReadOver(Root["over"], R.End, R.Result);
			Out.bOver = true;
		}
		return true;
	}

	Spot SpotAt(const Track& T, double Time)
	{
		Spot S;
		if (T.At.empty()) return S;
		const std::vector<Point>& P = T.At;
		if (Time <= P.front().T)
		{
			S.X = P.front().X; S.Y = P.front().Y; S.F = P.front().F; S.Way = S.F;
			return S;
		}
		if (Time >= P.back().T)
		{
			const Point& Last = P.back();
			S.X = Last.X; S.Y = Last.Y; S.F = Last.F; S.Way = S.F;
			if (P.size() >= 2)
			{
				const Point& Before = P[P.size() - 2];
				const double Dt = Last.T - Before.T;
				if (Dt > 1e-6)
				{
					const double Dx = Last.X - Before.X, Dy = Last.Y - Before.Y;
					S.V = std::hypot(Dx, Dy) / Dt;
					if (S.V > 0.05) S.Way = std::atan2(Dx, Dy);
					// A man who left with his block goes on at his last speed and facing.
					if (T.Out >= 0.0 && Time > Last.T)
					{
						const double Gone = Time - Last.T;
						S.X += Dx / Dt * Gone;
						S.Y += Dy / Dt * Gone;
					}
					else S.V = 0.0;
				}
			}
			return S;
		}
		auto It = std::upper_bound(P.begin(), P.end(), Time, [](double Tm, const Point& Pt) { return Tm < Pt.T; });
		const Point& B = *It;
		const Point& A = *(It - 1);
		const double Span = std::max(1e-6, B.T - A.T);
		const double U = (Time - A.T) / Span;
		S.X = A.X + (B.X - A.X) * U;
		S.Y = A.Y + (B.Y - A.Y) * U;
		S.F = TurnToward(A.F, B.F, U);
		const double Dx = B.X - A.X, Dy = B.Y - A.Y;
		S.V = std::hypot(Dx, Dy) / Span;
		S.Way = S.V > 0.05 ? std::atan2(Dx, Dy) : S.F;
		return S;
	}

	const Change* ChangeAt(const Track& T, double Time)
	{
		if (T.Ch.empty() || Time < T.Ch.front().T) return nullptr;
		auto It = std::upper_bound(T.Ch.begin(), T.Ch.end(), Time, [](double Tm, const Change& C) { return Tm < C.T; });
		return &*(It - 1);
	}
}
