#include "Battle/LandFeed.h"

#include "Battle/LandJson.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

		/** The night patrol's root (Patrol), where one is given (a recording, or the live delta that settles it): what N
		 * held is kept where none is. */
		void ReadPatrol(const Json& P, Patrol& N)
		{
			if (!P.IsObject()) return;
			N = Patrol();
			N.bOn = true;
			N.Side = P["side"].Int(0);
			N.Unit = P["unit"].Int(-1);
			N.T0 = P["t0"].Num(N.T0);
			N.T1 = std::max(N.T0, P["t1"].Num(N.T1));
			// (men: how many went, a count; a list, as an early draft wrote it, counted)
			N.Count = P["men"].IsNumber() ? std::max(0, P["men"].Int(0)) : static_cast<int>(P["men"].Items.size());
			const Json& F = P["found"];
			N.bFound = F.IsObject();
			if (N.bFound)
			{
				N.FoundX = F["x"].Num(0.0);
				N.FoundY = F["y"].Num(0.0);
				N.FoundKind = F["kind"].Str("");
			}
			N.Alerted = P["alerted"].IsNumber() ? P["alerted"].Num() : -1.0;
			const auto ReadPoints = [](const Json& List, std::vector<Patrol::Point>& Out)
			{
				Out.clear();
				for (const Json& Q : List.Items)
				{
					if (Q.Count() >= 3) Out.push_back({ Q.At(0).Num(0.0), Q.At(1).Num(0.0), Q.At(2).Num(0.0) });
				}
				std::stable_sort(Out.begin(), Out.end(), [](const Patrol::Point& A, const Patrol::Point& B) { return A.T < B.T; });
			};
			ReadPoints(P["route"], N.Route);
			ReadPoints(P["flares"], N.Flares);
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
			F.Look = J["look"].Str("");
			F.Snow = J["snow"].Num(-1.0);
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
				Row.bVague = U["vague"].Truth(false);
				Row.bCutter = U["cutter"].Truth(false);
				Row.Smoke = U["smoke"].Int(-1);
				const Json& Waits = U["waits"];
				if (Waits.IsObject())
				{
					Row.bWaits = true;
					Row.WaitDue = Waits["due"].Num(-1.0);
					Row.bWaitLate = Waits["late"].Truth(false);
					Row.bWaitFree = Waits["free"].Truth(false);
				}
				const Json& Search = U["search"];
				if (Search.IsArray() && Search.Count() >= 2)
				{
					Row.bSearch = true;
					Row.SearchX = Search.Items[0].Num(0.0);
					Row.SearchY = Search.Items[1].Num(0.0);
				}
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
			// (neighbours join the fight, release 2: the corps still coming up from next door, where and when)
			for (const Json& C : J["coming"].Items)
			{
				Arrival Row;
				Row.Id = C["id"].Int(0);
				Row.Side = SideOf(C["side"].Str());
				Row.Corps = C["corps"].Int(0);
				Row.Kind = C["kind"].Str("inf");
				Row.Edge = C["edge"].Str("");
				Row.Arrive = C["arrive"].Str("");
				Row.At = C["at"].Num(0.0);
				Row.X = C["x"].Num(0.0);
				Row.Y = C["y"].Num(0.0);
				V.Coming.push_back(std::move(Row));
			}
			// (the rider ahead, release 2: his own corps' despatch riders)
			for (const Json& R : J["riders"].Items)
			{
				Rider Row;
				Row.Corps = R["corps"].Int(0);
				Row.Side = SideOf(R["side"].Str());
				Row.X = R["x"].Num(0.0);
				Row.Y = R["y"].Num(0.0);
				Row.Face = R["face"].Num(0.0);
				Row.Speed = R["speed"].Num(0.0);
				Row.There = R["there"].Num(0.0);
				Row.Comes = R["comes"].Num(0.0);
				Row.Line = R["line"].IsNumber() ? R["line"].Num() : -1.0;
				Row.Edge = R["edge"].Str("");
				Row.Name = R["name"].Str("");
				Row.Kind = R["kind"].Str("");
				Row.Says = R["says"].Str("");
				Row.Men = R["men"].Int(0);
				Row.Str = R["str"].Num(1.0);
				V.Riders.push_back(std::move(Row));
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

		/** A man named by the rules: his track's index (a number), or who he is ([b, k, j], found once the men are read). */
		int ManOf(const Json& J, const Recording& R)
		{
			if (J.IsNumber()) return J.Int(-1) >= 0 && J.Int(-1) < static_cast<int>(R.Tracks.size()) ? J.Int(-1) : -1;
			if (J.IsArray() && J.Count() >= 2) return R.TrackIndexOf(J.At(0).Int(-1), J.At(1).Int(-1), std::max(0, J.At(2).Int(0)));
			return -1;
		}

		/** Release 2's bearers and linemen on a man's entry, a whole track's or a delta's: "ln", "br", "wd", "gone", "back"
		 * (the carries, "cy", once every man is read: ReadCarries). True when any was given. */
		bool ReadHurt(const Json& J, Track& T)
		{
			bool bAny = false;
			if (J.Has("ln")) { T.bLineman = J["ln"].Truth(); bAny = true; }
			if (J.Has("br")) { T.bBearer = J["br"].Truth(); bAny = true; }
			if (J["wd"].IsNumber()) { T.Wounded = J["wd"].Num(); bAny = true; }
			if (J["gone"].IsNumber()) { T.Gone = J["gone"].Num(); bAny = true; }
			if (J["back"].IsNumber()) { T.Back = J["back"].Num(); bAny = true; }
			return bAny;
		}

		/** A man's carries ("cy": [[from, to, by1, by2]]), added to his own, by time. True when any. */
		bool ReadCarries(const Json& J, Track& T, const Recording& R)
		{
			const Json& Cy = J["cy"];
			for (const Json& C : Cy.Items)
			{
				Track::Carry Row;
				Row.From = C.At(0).Num(0.0);
				Row.To = std::max(Row.From, C.At(1).Num(Row.From));
				Row.By1 = ManOf(C.At(2), R);
				Row.By2 = ManOf(C.At(3), R);
				T.Carried.push_back(Row);
			}
			std::stable_sort(T.Carried.begin(), T.Carried.end(), [](const Track::Carry& A, const Track::Carry& B) { return A.From < B.From; });
			return Cy.Count() > 0;
		}

		/** Release 2's field telephones, aid posts and night patrol at a recording's or a delta's root (the tactics
		 * thread's "wires", "wireCuts", "wireMends", "held", "aid", "patrol"), added to what R holds. */
		void ReadLines(const Json& Root, Recording& R)
		{
			for (const Json& W : Root["wires"].Items)
			{
				Wire Row;
				Row.Id = W["id"].Int(static_cast<int>(R.Wires.size()));
				Row.Side = W["side"].Int(1);
				Row.Hq = W["hq"].Int(-1);
				Row.Unit = W["unit"].Int(-1);
				for (const Json& P : W["pts"].Items) Row.Pts.push_back({ P.At(0).Num(0.0), P.At(1).Num(0.0) });
				if (Row.Pts.size() >= 2) R.Wires.push_back(std::move(Row));
			}
			for (const Json& C : Root["wireCuts"].Items)
			{
				WireCut Row;
				Row.T = C["t"].Num(0.0);
				Row.Wire = C["wire"].Int(0);
				Row.X = C["x"].Num(0.0);
				Row.Y = C["y"].Num(0.0);
				Row.By = C["by"].Str("shell");
				R.WireCuts.push_back(std::move(Row));
			}
			std::stable_sort(R.WireCuts.begin(), R.WireCuts.end(), [](const WireCut& A, const WireCut& B) { return A.T < B.T; });
			for (const Json& M : Root["wireMends"].Items)
			{
				WireMend Row;
				Row.T = M["t"].Num(0.0);
				Row.Wire = M["wire"].Int(0);
				Row.Man = ManOf(M["man"], R);
				Row.T0 = M["t0"].IsNumber() ? M["t0"].Num() : -1.0;
				Row.T1 = M["t1"].IsNumber() ? M["t1"].Num() : -1.0;
				Row.X = M["x"].Num(0.0);
				Row.Y = M["y"].Num(0.0);
				R.WireMends.push_back(Row);
			}
			std::stable_sort(R.WireMends.begin(), R.WireMends.end(), [](const WireMend& A, const WireMend& B) { return A.T < B.T; });
			for (const Json& H : Root["held"].Items)
			{
				HeldOrder Row;
				Row.T = H["t"].Num(0.0);
				Row.Due = H["due"].Num(Row.T);
				Row.Went = H["went"].IsNumber() ? H["went"].Num() : -1.0;
				Row.Unit = H["unit"].Int(-1);
				R.Held.push_back(Row);
			}
			for (const Json& A : Root["aid"].Items)
			{
				AidPost Row;
				Row.Side = A["side"].Int(0);
				Row.X = A["x"].Num(0.0);
				Row.Y = A["y"].Num(0.0);
				R.Aid.push_back(Row);
			}
			ReadPatrol(Root["patrol"], R.Night);
			if (Root["scars"].IsObject())
			{
				R.ScarCount = std::max(0, Root["scars"]["count"].Int(0));
				R.ScarMonths = std::max(0.0, Root["scars"]["months"].Num(0.0));
			}
			if (Root["oldRuts"].IsObject())
			{
				R.OldRutCount = std::max(0, Root["oldRuts"]["count"].Int(0));
				R.OldRutMonths = std::max(0.0, Root["oldRuts"]["months"].Num(0.0));
			}
		}

		/** The platoon cam's marks: ld [[from, to], ...], e [[from, to, "base" | "move"], ...], cl [t, ...] or
		 * [[t, x, y], ...]. */
		void ReadMarks(const Json& J, Track& T)
		{
			T.Lead.clear();
			T.Element.clear();
			T.Calls.clear();
			for (const Json& M : J["ld"].Items)
				T.Lead.push_back({ M.At(0).Num(0.0), M.At(1).Num(0.0), false });
			for (const Json& M : J["e"].Items)
				T.Element.push_back({ M.At(0).Num(0.0), M.At(1).Num(0.0), M.At(2).Str("base") == "move" });
			for (const Json& M : J["cl"].Items)
			{
				Track::Call C;
				if (M.IsNumber()) C.T = M.Num(0.0);
				else
				{
					C.T = M.At(0).Num(0.0);
					C.bSpot = M.At(1).IsNumber() && M.At(2).IsNumber();
					C.X = M.At(1).Num(0.0);
					C.Y = M.At(2).Num(0.0);
				}
				T.Calls.push_back(C);
			}
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
			ReadHurt(J, T);
			ReadMarks(J, T);
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

		/** Effects (R2): an event's note of the fighters on call (AirNote), if it is one. */
		void ReadAir(const Json& E, std::vector<AirNote>& Out)
		{
			const std::string& What = E["what"].Str();
			if (What != "land:air-in" && What != "land:strafe" && What != "land:bomb-drop" && What != "air:lost") return;
			const Json& X = E["extra"];
			AirNote N;
			N.T = E["t"].Num(0.0);
			N.What = What;
			N.Side = SideOf(E["side"].Str());
			N.X = E["x"].Num(0.0);
			N.Y = E["y"].Num(0.0);
			N.Flight = X["flight"].Int(-1);
			for (size_t K = 0; K < 4 && What == "land:air-in"; ++K) N.Run[K] = X["run"].At(K).Num(0.0);
			if (What == "land:strafe")
			{
				N.Run[0] = X["from"].At(0).Num(0.0); N.Run[1] = X["from"].At(1).Num(0.0);
				N.Run[2] = X["to"].At(0).Num(0.0); N.Run[3] = X["to"].At(1).Num(0.0);
			}
			N.Models = X["models"].Int(4);
			N.Alt = X["alt"].Num(0.0);
			N.Nat = X["nat"].Str("");
			N.Year = X["year"].Int(1944);
			N.Heading = X["heading"].Num(0.0);
			N.Secs = X["secs"].Num(0.0);
			N.Width = X["width"].Num(60.0);
			N.At = X["at"].Num(-1.0);
			N.To[0] = X["to"]["x"].Num(0.0);
			N.To[1] = X["to"]["y"].Num(0.0);
			N.Kind = X["kind"].Str("");
			N.Chutes = X["chutes"].Int(0);
			if (N.Flight >= 0) Out.push_back(std::move(N));
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
			Row.G = R["g"].Str("");
			Row.bCm = R["cm"].Int(0) != 0;
			Row.bMate = R["m"].Int(0) != 0;
			Row.Hang = R["hang"].IsNumber() ? R["hang"].Num() : -1.0;
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

	/** The hedge cutters' cuts in a recording or a delta ("cuts": { t0, t1, unit, x, y, yaw, width }), added in T1's order. */
	static void ReadCuts(const Json& List, std::vector<Cut>& Out)
	{
		for (const Json& C : List.Items)
		{
			Cut Row;
			Row.T1 = C["t1"].Num(C["t0"].Num(0.0));
			Row.T0 = std::min(C["t0"].Num(Row.T1), Row.T1);
			Row.X = C["x"].Num(0.0);
			Row.Y = C["y"].Num(0.0);
			Row.Yaw = C["yaw"].Num(0.0);
			Row.Width = std::clamp(C["width"].Num(4.0), 2.5, 8.0);
			Row.Unit = C["unit"].IsNumber() ? C["unit"].Int(-1) : -1;
			Out.push_back(Row);
		}
		std::stable_sort(Out.begin(), Out.end(), [](const Cut& A, const Cut& B) { return A.T1 < B.T1; });
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
		Out.WeatherWord = Root["weather"].Str("");
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
			Row.Corps = B["corps"].Int(0);
			Row.bJoins = B["joins"].Truth(false);
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
			ReadAir(E, Out.Air); // (Effects (R2): the fighters on call)
		}
		ReadCuts(Root["cuts"], Out.Cuts);
		for (const Json& R : Root["rounds"].Items)
		{
			Round Row;
			ReadRound(R, Row);
			Out.Rounds.push_back(std::move(Row));
		}
		std::sort(Out.Rounds.begin(), Out.Rounds.end(), RoundBefore);
		std::vector<const Json*> MenRead;
		for (const Json& M : Root["men"].Items)
		{
			Track Row;
			ReadTrack(M, Row);
			if (Row.At.empty()) continue;
			Out.Tracks.push_back(std::move(Row));
			MenRead.push_back(&M);
		}
		// (release 2's carries and lines name men by who they are: read once every man is)
		for (size_t I = 0; I < MenRead.size(); ++I) ReadCarries(*MenRead[I], Out.Tracks[I], Out);
		ReadLines(Root, Out);
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
			const bool bHurt = ReadHurt(M, T);
			if (ReadCarries(M, T, R) || bHurt) Note(Out.Hurt, N);
		}
		// Where each moving man stands now: [n, x, y, f] at t, his provisional last point. (Each man noted as moved once, by
		// a mark a track: a search of the moved so far for each tail, thousands of tails against thousands of moved men,
		// was 12 ms of a big battle's 31 a piece in the cloud. Release 1.5's Orel stall note, 23 November.)
		std::vector<uint8_t> MovedNow(R.Tracks.size(), 0);
		for (const int N : Out.Moved) MovedNow[static_cast<size_t>(N)] = 1;
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
			if (!MovedNow[static_cast<size_t>(N)])
			{
				MovedNow[static_cast<size_t>(N)] = 1;
				Out.Moved.push_back(N);
			}
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
			ReadAir(E, R.Air); // (Effects (R2): the fighters on call)
		}
		ReadCuts(Root["cuts"], R.Cuts);
		ReadLines(Root, R);
		std::map<int, Crater> Craters;
		for (const Json& V : Root["views"].Items)
		{
			View Row;
			ReadView(V, Row);
			R.Views.push_back(std::move(Row));
			ReadViewExtras(V, Craters, R.Blocks);
			if (V["setup"].IsNull() && R.bSetup) { R.bSetup = false; Out.bBegun = true; }
			// (after the break, release 2 `pursue`: who broke, and whether the winner is asked or fights on; a row with
			// none of it is a battle not yet broken, or one ended at the break)
			const Json& AfterRow = V["after"];
			if (AfterRow.IsObject())
			{
				R.After = AfterRow["phase"].Str("") == "ask" ? 1 : 2;
				R.AfterBroke = SideOf(AfterRow["side"].Str(""));
				R.AfterWinner = SideOf(AfterRow["winner"].Str(""));
				R.AfterAt = AfterRow["t"].Num(R.Views.back().T);
				R.AfterBrokeName = AfterRow["broke"].Str("");
				R.AfterWinnerName = AfterRow["by"].Str("");
			}
			else
			{
				R.After = 0;
			}
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

	// ---- The night patrol's men (release 2) -------------------------------------------------------------------------

	namespace
	{
		size_t SkipBlank(const std::string& S, size_t I)
		{
			while (I < S.size() && (S[I] == ' ' || S[I] == '\n' || S[I] == '\r' || S[I] == '\t')) ++I;
			return I;
		}

		/** Past a string whose opening quote is at I. */
		size_t SkipString(const std::string& S, size_t I)
		{
			for (++I; I < S.size(); ++I)
			{
				if (S[I] == '\\') ++I;
				else if (S[I] == '"') return I + 1;
			}
			return S.size();
		}

		/** Past the value that starts at I (after any blank). */
		size_t SkipValue(const std::string& S, size_t I)
		{
			I = SkipBlank(S, I);
			if (I >= S.size()) return I;
			if (S[I] == '"') return SkipString(S, I);
			if (S[I] == '{' || S[I] == '[')
			{
				int Depth = 0;
				while (I < S.size())
				{
					const char C = S[I];
					if (C == '"') { I = SkipString(S, I); continue; }
					if (C == '{' || C == '[') ++Depth;
					else if ((C == '}' || C == ']') && --Depth == 0) return I + 1;
					++I;
				}
				return S.size();
			}
			while (I < S.size() && S[I] != ',' && S[I] != '}' && S[I] != ']' && S[I] != ' ' && S[I] != '\n' && S[I] != '\r' && S[I] != '\t') ++I;
			return I;
		}

		/** Where a field of the text's outermost object holds its value: [Begin, End). False where it has none. */
		bool RootValue(const std::string& S, const char* Key, size_t& OutBegin, size_t& OutEnd)
		{
			size_t I = SkipBlank(S, 0);
			if (I >= S.size() || S[I] != '{') return false;
			++I;
			const std::string Want(Key);
			for (;;)
			{
				I = SkipBlank(S, I);
				if (I < S.size() && S[I] == ',') { I = SkipBlank(S, I + 1); }
				if (I >= S.size() || S[I] != '"') return false;
				const size_t NameEnd = SkipString(S, I);
				const bool bWanted = NameEnd - I == Want.size() + 2 && S.compare(I + 1, Want.size(), Want) == 0;
				I = SkipBlank(S, NameEnd);
				if (I >= S.size() || S[I] != ':') return false;
				I = SkipBlank(S, I + 1);
				const size_t ValueEnd = SkipValue(S, I);
				if (bWanted)
				{
					OutBegin = I;
					OutEnd = ValueEnd;
					return true;
				}
				I = ValueEnd;
			}
		}

		void Put(std::string& Out, double V)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.3f", std::fabs(V) < 5e-4 ? 0.0 : V);
			Out += Buffer;
		}
	}

	bool GunsOpen(const Recording& Rec, double& OutAt, int& OutTrack)
	{
		OutAt = -1.0;
		OutTrack = -1;
		const double From = Rec.Contact >= 0.0 ? Rec.Contact : 0.0;
		struct FLong
		{
			double T = 0.0;
			int Track = -1;
		};
		std::vector<FLong> Fire;
		for (const Round& R : Rec.Rounds)
		{
			if (R.W != "mg" || R.T < From || R.T > From + 60.0) continue;
			const int Ix = Rec.TrackIndexOf(R.By, R.K, R.J);
			if (Ix < 0) continue;
			const Spot S = SpotAt(Rec.Tracks[static_cast<size_t>(Ix)], R.T);
			if (std::hypot(R.X - S.X, R.Y - S.Y) < 150.0) continue;
			Fire.push_back({ R.T, Ix });
		}
		if (Fire.empty()) return false;
		std::stable_sort(Fire.begin(), Fire.end(), [](const FLong& A, const FLong& B) { return A.T < B.T; });
		size_t Lo = 0, BestLo = 0, BestHi = 0;
		for (size_t Hi = 0; Hi < Fire.size(); ++Hi)
		{
			while (Fire[Hi].T - Fire[Lo].T > 5.0) ++Lo;
			if (Hi - Lo > BestHi - BestLo) { BestLo = Lo; BestHi = Hi; }
		}
		OutAt = 0.5 * (Fire[BestLo].T + Fire[BestHi].T);
		std::map<int, int> ByGunner;
		for (size_t K = BestLo; K <= BestHi; ++K) ++ByGunner[Fire[K].Track];
		// (the attack's gunner who fired the most of it and stands through the shot, not down or gone within 3 s of it, as
		// the film's other stills keep to the attack's men out in the open; else the defence's, from his hedge or house)
		for (int Side = 0; Side < 2 && OutTrack < 0; ++Side)
		{
			int Most = 0;
			for (const std::pair<const int, int>& One : ByGunner)
			{
				const Track& T = Rec.Tracks[static_cast<size_t>(One.first)];
				const Block* B = Rec.BlockOf(T.Block);
				if (!B || B->Side != Side) continue;
				if ((T.Fell >= 0.0 && T.Fell < OutAt + 3.0) || (T.Out >= 0.0 && T.Out < OutAt + 3.0)) continue;
				if (One.second > Most) { Most = One.second; OutTrack = One.first; }
			}
		}
		return true;
	}

	int AddPatrolMen(std::string& Text)
	{
		// (most recordings have none: found at a glance, the whole text not walked)
		if (Text.find("\"patrol\"") == std::string::npos) return 0;
		size_t PatrolBegin = 0, PatrolEnd = 0;
		if (!RootValue(Text, "patrol", PatrolBegin, PatrolEnd)) return 0;
		Json Root;
		std::string Error;
		if (!LandJson::Parse(Text.substr(PatrolBegin, PatrolEnd - PatrolBegin), Root, Error)) return 0;
		Patrol N;
		ReadPatrol(Root, N);
		const std::vector<Patrol::Point>& R = N.Route;
		if (!N.bOn || N.Unit < 0 || N.Count <= 0 || R.size() < 2 || R.back().T <= R.front().T) return 0;
		const int Count = std::min(N.Count, 16);

		// The leader's way: how far along it he is at each of its points, and its pace (metres a battle second).
		std::vector<double> Along(R.size(), 0.0);
		for (size_t K = 1; K < R.size(); ++K) Along[K] = Along[K - 1] + std::hypot(R[K].X - R[K - 1].X, R[K].Y - R[K - 1].Y);
		const double Pace = std::max(0.3, Along.back() / (R.back().T - R.front().T));
		// (where on the way he is at route time U, and the way he faces: before the start back along its first leg, as
		// a man still coming out of his own line; after the end on along its last)
		const auto WayAt = [&R, Pace](double U, double& X, double& Y, double& F)
		{
			const size_t Last = R.size() - 1;
			if (U <= R.front().T || U >= R.back().T)
			{
				const bool bBefore = U <= R.front().T;
				const Patrol::Point& A = bBefore ? R[0] : R[Last - 1];
				const Patrol::Point& B = bBefore ? R[1] : R[Last];
				const double L = std::max(1e-6, std::hypot(B.X - A.X, B.Y - A.Y));
				const double Over = bBefore ? U - R.front().T : 0.0;
				const Patrol::Point& At = bBefore ? A : B;
				X = At.X + (B.X - A.X) / L * Pace * Over;
				Y = At.Y + (B.Y - A.Y) / L * Pace * Over;
				F = std::atan2(B.X - A.X, B.Y - A.Y);
				return;
			}
			size_t K = 0;
			while (K + 1 < Last && R[K + 1].T <= U) ++K;
			const Patrol::Point& A = R[K];
			const Patrol::Point& B = R[K + 1];
			const double Share = (U - A.T) / std::max(1e-9, B.T - A.T);
			X = A.X + (B.X - A.X) * Share;
			Y = A.Y + (B.Y - A.Y) * Share;
			// (where the way halts, the facing of the leg he came by)
			size_t Leg = K;
			while (Leg > 0 && std::hypot(R[Leg + 1].X - R[Leg].X, R[Leg + 1].Y - R[Leg].Y) < 0.05) --Leg;
			F = std::atan2(R[Leg + 1].X - R[Leg].X, R[Leg + 1].Y - R[Leg].Y);
		};
		// Each flare holds them flat while it burns: battle time T is route time U less the holds begun before it.
		std::vector<double> Holds;
		for (const Patrol::Point& Fl : N.Flares)
		{
			if (Fl.T > R.front().T && Fl.T < R.back().T + 30.0 && (Holds.empty() || Fl.T >= Holds.back() + PatrolFlareBurns)) Holds.push_back(Fl.T);
		}
		const auto HeldBy = [&Holds](double T)
		{
			double Held = 0.0;
			for (double H : Holds) Held += std::clamp(T - H, 0.0, PatrolFlareBurns);
			return Held;
		};
		const auto InHold = [&Holds](double T)
		{
			for (double H : Holds)
			{
				if (T >= H && T < H + PatrolFlareBurns) return true;
			}
			return false;
		};

		std::string Rows;
		for (int Man = 0; Man < Count; ++Man)
		{
			// In file: each the leader's way a little later, the odd to his right and the even to his left of it.
			const double Lag = Man * PatrolGap / Pace;
			const double Aside = Man == 0 ? 0.0 : (Man % 2 ? 0.6 : -0.6);
			// (the battle times kept: the start, each point of the way as he reaches it, each hold's two ends, home)
			std::vector<double> Times;
			Times.push_back(R.front().T);
			const auto TimeOfRoute = [&](double U)
			{
				double T = U + Lag;
				for (double H : Holds)
				{
					if (H < T) T += PatrolFlareBurns;
				}
				return T;
			};
			for (const Patrol::Point& P : R) Times.push_back(TimeOfRoute(P.T));
			for (double H : Holds)
			{
				Times.push_back(H);
				Times.push_back(H + PatrolFlareBurns);
			}
			std::sort(Times.begin(), Times.end());
			Times.erase(std::unique(Times.begin(), Times.end(), [](double A, double B) { return std::fabs(A - B) < 0.01; }), Times.end());
			const double Home = TimeOfRoute(R.back().T);
			while (!Times.empty() && Times.back() > Home + 1e-6) Times.pop_back();

			std::string At = "[", Ch = "[";
			std::string PoseWas;
			for (size_t K = 0; K < Times.size(); ++K)
			{
				const double T = Times[K];
				double X = 0.0, Y = 0.0, F = 0.0;
				WayAt(T - Lag - HeldBy(T), X, Y, F);
				X += std::cos(F) * Aside;
				Y -= std::sin(F) * Aside;
				if (K) At += ",";
				At += "[";
				Put(At, T); At += ","; Put(At, X); At += ","; Put(At, Y); At += ","; Put(At, F);
				At += "]";
				// His pose to the next point: flat under a flare (a moment late, man by man), else crawling near the post
				// found, creeping where he goes on, kneeling where the way halts.
				const double Next = K + 1 < Times.size() ? Times[K + 1] : T + 1.0;
				double NX = 0.0, NY = 0.0, NF = 0.0;
				WayAt(Next - Lag - HeldBy(Next), NX, NY, NF);
				NX += std::cos(NF) * Aside;
				NY -= std::sin(NF) * Aside;
				const bool bMoves = std::hypot(NX - X, NY - Y) > 0.05 * (Next - T);
				const double Mid = 0.5 * (T + Next);
				const bool bNear = N.bFound && std::hypot(0.5 * (X + NX) - N.FoundX, 0.5 * (Y + NY) - N.FoundY) < PatrolCrawlNear;
				const std::string Pose = InHold(Mid) ? "hide" : !bMoves ? "wait" : bNear ? "crawl" : "creep";
				if (Pose != PoseWas)
				{
					// (a flare: down a breath after it bursts, the leader first)
					const double When = Pose == "hide" ? T + 0.15 + 0.1 * Man : T;
					if (!PoseWas.empty()) Ch += ",";
					Ch += "[";
					Put(Ch, When);
					Ch += ",\"" + Pose + "\",\"none\",null,null,0,0]";
					PoseWas = Pose;
				}
			}
			At += "]";
			Ch += "]";
			if (Man) Rows += ",";
			Rows += "{\"b\":" + std::to_string(N.Unit) + ",\"k\":" + std::to_string(PatrolK + Man) + ",\"r\":\"" + (Man == 0 ? "leader" : "rifle")
				+ "\",\"at\":" + At + ",\"ch\":" + Ch + ",\"s\":[],\"out\":";
			// (out as the battle opens, home among their unit by then: a leaving time under 0 is read everywhere as never)
			Put(Rows, std::max(0.0, Home + 0.5));
			Rows += "}";
		}

		size_t MenBegin = 0, MenEnd = 0;
		if (RootValue(Text, "men", MenBegin, MenEnd) && MenEnd > MenBegin && Text[MenBegin] == '[' && Text[MenEnd - 1] == ']')
		{
			const bool bEmpty = SkipBlank(Text, MenBegin + 1) == MenEnd - 1;
			Text.insert(MenEnd - 1, bEmpty ? Rows : "," + Rows);
			return Count;
		}
		// (a live header without men yet: given its own)
		const size_t Close = Text.find_last_of('}');
		if (Close == std::string::npos) return 0;
		Text.insert(Close, ",\"men\":[" + Rows + "]");
		return Count;
	}
}
