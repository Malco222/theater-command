#include "Battle/SeaShow.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace SeaShow
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Gee = 9.81;
		constexpr double Deg = Pi / 180.0;
		/** B4: the farthest a big ship's helm is held hard over against a dive, off her course (a swing out and back). */
		constexpr double MostSwing = 50.0 * Deg;
		/** B4: the most a comb's heading lies off her course (as chosen, or as her course turns) before she gives it up. */
		constexpr double CombMost = 110.0 * Deg;
		/** L9: how often the ships' guns are laid (seconds). */
		constexpr double GunStep = 0.05;

		double Wrap(double A)
		{
			// (a course far out of range from an odd recording is brought in at once: taking a turn off a huge angle leaves
			// it as it was, and the loops below would turn for ever; not a number at all is read as north. Ordinary
			// angles keep the loops, so a recording plays to the same bit as before)
			if (!(std::fabs(A) <= 1.0e3)) A = std::isfinite(A) ? std::remainder(A, 2.0 * Pi) : 0.0;
			while (A > Pi) A -= 2.0 * Pi;
			while (A <= -Pi) A += 2.0 * Pi;
			return A;
		}
		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : V > Hi ? Hi : V; }
		double Lerp(double A, double B, double T) { return A + (B - A) * T; }
		double Flat(const Vec& V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }
		/** Along a heading (clockwise from north), level. */
		Vec Ahead(double Hdg) { return Vec(std::sin(Hdg), std::cos(Hdg), 0.0); }
		/** To the right of a heading, level. */
		Vec RightOf(double Hdg) { return Vec(std::cos(Hdg), -std::sin(Hdg), 0.0); }
		double Bearing(const Vec& From, const Vec& To) { return std::atan2(To.X - From.X, To.Y - From.Y); }
		/** A number from 0 to under 1 fixed by a whole number (a hash): the look's own, never the battle's dice. */
		double Mix(uint64_t N)
		{
			N ^= N >> 33;
			N *= 0xff51afd7ed558ccdULL;
			N ^= N >> 33;
			N *= 0xc4ceb9fe1a85ec53ULL;
			N ^= N >> 33;
			return static_cast<double>(N >> 11) / 9007199254740992.0;
		}
		bool Big(const std::string& Class) { return Class == "bb" || Class == "cv"; }

		/** Hits a ship of the class takes to be drawn going down: the battle's wear on her hull shared out as hits. */
		double HitsFor(const std::string& Class)
		{
			return Class == "bb" ? 12.0 : Class == "cv" ? 8.0 : Class == "ca" ? 8.0 : 3.0;
		}
		/** How hard a ship of the class heels in a turn (a share of the angle whose tangent is her turn's g). */
		struct Motion
		{
			double TurnHeel;
		};
		Motion MotionOf(const std::string& Class)
		{
			// (a destroyer at 40 knots hard over heeled 10 to 12 degrees, a battleship 4 to 6)
			if (Class == "dd") return { 0.55 };
			if (Class == "ca") return { 0.45 };
			if (Class == "cv") return { 0.4 };
			return { 0.35 }; // a battleship
		}
		/**
		 * A ship's ride on the sea by her length and the wind (the art director's fleet water targets, 21 November
		 * 23:44Z, section 3): her roll and pitch (degrees either way) at Beaufort 3, 4, 5 and 6, their periods (seconds),
		 * and how much of the waves' height she rises and falls by, either way. A destroyer or destroyer escort (110 m
		 * and under), a cruiser or escort carrier (180 m), a battleship or fleet carrier (250 m and over); between them,
		 * between their rows.
		 */
		struct Ride
		{
			double Roll[4], Pitch[4], RollT, PitchT, Heave;
		};
		Ride RideOf(double Long)
		{
			static const Ride Small = { { 1.5, 3.0, 5.5, 9.5 }, { 0.5, 1.25, 2.5, 4.0 }, 9.0, 5.5, 0.2 };
			static const Ride Middle = { { 0.75, 1.5, 3.0, 5.5 }, { 0.3, 0.75, 1.25, 2.0 }, 12.5, 7.5, 0.15 };
			static const Ride Big = { { 0.35, 0.75, 1.5, 3.0 }, { 0.2, 0.4, 0.75, 1.25 }, 16.0, 10.0, 0.075 };
			const double K = Clamp((Long - 110.0) / 140.0, 0.0, 1.0) * 2.0;
			const Ride& A = K < 1.0 ? Small : Middle;
			const Ride& B = K < 1.0 ? Middle : Big;
			const double T = K < 1.0 ? K : K - 1.0;
			Ride R;
			for (int I = 0; I < 4; ++I)
			{
				R.Roll[I] = A.Roll[I] + (B.Roll[I] - A.Roll[I]) * T;
				R.Pitch[I] = A.Pitch[I] + (B.Pitch[I] - A.Pitch[I]) * T;
			}
			R.RollT = A.RollT + (B.RollT - A.RollT) * T;
			R.PitchT = A.PitchT + (B.PitchT - A.PitchT) * T;
			R.Heave = A.Heave + (B.Heave - A.Heave) * T;
			return R;
		}
		/** One of a ride's rows at a Beaufort number that need not be whole: between 3 and 6 between the columns, under 3
		 * less to nothing in a calm, over 6 as at 6. */
		double AtForce(const double (&Row)[4], double Force)
		{
			if (Force <= 3.0) return Row[0] * Clamp(Force / 3.0, 0.0, 1.0);
			const double K = std::min(Force, 6.0) - 3.0;
			const int I = std::min(static_cast<int>(K), 2);
			return Row[I] + (Row[I + 1] - Row[I]) * (K - I);
		}
		/** Three swells summed, their periods within 15% of each other, so a ship's ride never repeats (and never bobs
		 * like a toy): about 1 at its most. */
		double Swells(double T, double Period, double Phase)
		{
			const double W = 2.0 * Pi / Period;
			return 0.6 * std::sin(W * T + Phase) + 0.28 * std::sin(W * 0.885 * T + 1.9 * Phase + 1.0) + 0.17 * std::sin(W * 1.14 * T + 2.7 * Phase + 2.0);
		}
		/** Where a worst one was, for the cloud test's report. */
		template <typename HullT> std::string Worst(const HullT& H, double T)
		{
			char Buf[160];
			std::snprintf(Buf, sizeof Buf, "ship %d (%s, piece %d, %s) at %.1f s, %.1f m/s", H.Key, H.Class.c_str(), H.Piece, H.Back > 0.0 ? "astern" : "leading", T, H.Spd);
			return Buf;
		}
		/** How fast she gathers and loses way, metres a second a second (a battleship slowest). */
		double AccelOf(const std::string& Class) { return Class == "dd" ? 0.5 : Class == "ca" ? 0.35 : 0.22; }
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The room for columns of water.

	int ColumnRoom::Standing(double T) const
	{
		int N = 0;
		for (const Lot& L : Lots) if (L.Until > T) ++N;
		return N;
	}

	bool ColumnRoom::Take(double T, double FromEye)
	{
		Lots.erase(std::remove_if(Lots.begin(), Lots.end(), [T](const Lot& L) { return L.Until <= T; }), Lots.end());
		const bool bFar = FromEye > Far;
		int FarNow = 0;
		for (const Lot& L : Lots) if (L.bFar) ++FarNow;
		if (static_cast<int>(Lots.size()) >= Most || (bFar && FarNow >= FarMost))
		{
			++Refused;
			return false;
		}
		Lots.push_back({ T + Life, bFar });
		++Given;
		Peak = std::max(Peak, static_cast<int>(Lots.size()));
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The field and the wind.

	Show::Show(uint64_t Seed) : Roll(Seed * 2654435761ULL + 239ULL), Gunners(Seed * 0x9E3779B97F4A7C15ULL + 1944ULL)
	{
		F.Seed = Seed;
		F.Classes["bb"] = { 240.0, 33.0, 11.0, 0.05, 5000.0 };
		F.Classes["cv"] = { 240.0, 32.0, 13.0, 0.05, 1500.0 };
		F.Classes["ca"] = { 180.0, 20.0, 13.0, 0.08, 5000.0 };
		F.Classes["dd"] = { 110.0, 11.0, 14.0, 0.12, 5000.0 };
		SetField(F);
	}

	void Show::SetField(const Field& InField)
	{
		const std::map<std::string, ClassSize> Kept = F.Classes;
		F = InField;
		for (const auto& Pair : Kept) if (!F.Classes.count(Pair.first)) F.Classes[Pair.first] = Pair.second;
		Roll = Dice(F.Seed * 2654435761ULL + 239ULL);
		// L6: one wind a battle, from its seed where the field names none (the recorder's hash is the same kind of thing).
		// (a speed of -2: the battle gave its bearing alone, SeaSim.wind; -1 or none: neither)
		double From = F.WindFrom, Speed = F.WindSpeed;
		if (Speed < 0.0)
		{
			if (Speed > -1.5) From = Mix(F.Seed * 31 + 7) * 2.0 * Pi;
			Speed = 4.0 + Mix(F.Seed * 31 + 11) * 7.0;
		}
		WindV = Ahead(From) * -Speed;
		WindU = Speed;
		WindFromR = From;
		const double Steps[] = { 0.3, 1.5, 3.3, 5.5, 7.9, 10.7, 13.8 };
		Beaufort = 7.0;
		for (int K = 0; K < 7; ++K)
		{
			if (Speed < Steps[K]) { Beaufort = K; break; }
		}
	}

	const ClassSize& Show::SizeOf(const std::string& Class) const
	{
		auto It = F.Classes.find(Class);
		return It != F.Classes.end() ? It->second : F.Classes.at("ca");
	}

	double Show::Calibre(const std::string& Class, const std::string& Nation) const
	{
		// The main guns of each navy's ships of the class in the war's middle years, millimetres.
		if (Class == "bb") return Nation == "USA" ? 406.0 : Nation == "JAP" ? 410.0 : Nation == "GER" ? 380.0 : Nation == "UK" ? 381.0 : Nation == "ITA" ? 381.0 : Nation == "FRA" ? 380.0 : 356.0;
		if (Class == "ca") return 203.0;
		if (Class == "dd") return Nation == "USA" || Nation == "JAP" || Nation == "GER" ? 127.0 : 120.0;
		return 127.0;
	}

	double Show::Cadence(const std::string& Class)
	{
		// Seconds between a ship's salvos in a gunnery action: a battleship's big guns about two a minute, a cruiser's
		// eight-inch about four, a destroyer's guns in rapid fire about eight (each a little ragged).
		const double Base = Class == "bb" ? 30.0 : Class == "ca" ? 16.0 : 7.5;
		return Base * Roll.U(0.85, 1.15);
	}

	const Piece* Show::PieceNow(int Id) const
	{
		auto It = Now.find(Id);
		return It == Now.end() ? nullptr : &It->second;
	}

	Show::Hull* Show::FindHull(int Key)
	{
		auto It = Hulls.find(Key);
		return It == Hulls.end() ? nullptr : &It->second;
	}

	const Show::Hull* Show::FindHull(int Key) const
	{
		auto It = Hulls.find(Key);
		return It == Hulls.end() ? nullptr : &It->second;
	}

	const ShipOut* Show::Ship(int Key) const
	{
		for (const ShipOut& S : OutShips) if (S.Key == Key) return &S;
		return nullptr;
	}

	Vec Show::Bow(const Hull& H) const { return Ahead(H.Hdg); }

	double Show::MostRateAt(const ClassSize& Sz, double Spd) const
	{
		return Sz.Turn * 1.25 * Clamp(Spd / std::max(1.0, Sz.Speed), 0.15, 1.45);
	}

	bool Show::OnHull(const Vec& At, double Margin, int Except) const
	{
		for (const auto& Pair : Hulls)
		{
			const Hull& H = Pair.second;
			if (H.Doing == Fate::Gone || H.Key == Except) continue;
			const ClassSize& Sz = SizeOf(H.Class);
			const Vec Off(At.X - H.P.X, At.Y - H.P.Y, 0.0);
			if (std::fabs(Off.X) > Sz.Long + Margin || std::fabs(Off.Y) > Sz.Long + Margin) continue;
			const double Along = std::max(0.0, std::fabs(Off.Dot(Ahead(H.Hdg))) - std::max(0.0, Sz.Long * 0.5 - Sz.Beam * 0.5));
			if (std::hypot(Along, Off.Dot(RightOf(H.Hdg))) < Sz.Beam * 0.5 + Margin) return true;
		}
		return false;
	}

	Vec Show::PlaceOn(const Hull& H, double Along, double Side, double Up) const
	{
		const ClassSize& Sz = SizeOf(H.Class);
		return H.P + Ahead(H.Hdg) * (Along * Sz.Long) + RightOf(H.Hdg) * Side + Vec(0.0, 0.0, Up);
	}

	Fx& Show::Emit(FxKind Kind, const Vec& At, const std::string& What, int Key)
	{
		Fx E;
		E.Kind = Kind;
		E.At = At;
		E.What = What;
		E.Key = Key;
		Effects.push_back(E);
		return Effects.back();
	}

	bool Show::PieceMiddle(int PieceId, Vec& OutAt, double& OutFace) const
	{
		auto It = Lines.find(PieceId);
		if (It == Lines.end() || It->second.Line.empty()) return false;
		Vec Sum;
		int N = 0;
		for (const int Key : It->second.Line)
		{
			if (const Hull* H = FindHull(Key)) { Sum += H->P; ++N; }
		}
		if (!N) return false;
		OutAt = Sum * (1.0 / N);
		const Hull* Lead = FindHull(It->second.Line.front());
		OutFace = Lead ? Lead->Hdg : 0.0;
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The views and the picture between them.

	void Show::Take(double T, const std::vector<Piece>& InPieces, const std::vector<Event>& InEvents, const std::vector<Zone>* InZones, double Contact)
	{
		View V;
		V.T = T;
		for (const Piece& P : InPieces) V.Pieces[P.Id] = P;
		// The hull each piece lost since the view before is owed as hits (the strikes', raids' and torpedoes' own aside).
		if (!Views.empty() && T > Views.back().T)
		{
			for (const Event& E : InEvents)
			{
				// (and a convoy's depth charge on a boat, ruling 241 §7)
				if ((E.What == "sea:strike" || E.What == "sea:air" || E.What == "sea:torpedoHit" || E.What == "sea:depthChargeHit") && E.Unit >= 0) AirBlowAt[E.Unit] = E.T;
			}
			Debts(Views.back(), V, InEvents);
		}
		if (!Views.empty() && T <= Views.back().T) Views.back() = V; // (the same moment again: the newer word stands)
		else Views.push_back(V);
		while (Views.size() > 12 && Views[1].T <= ShownT) Views.pop_front();
		for (const Event& E : InEvents) Queue.push_back(E);
		// (at one moment the blow before what it did: the torpedo that sank her strikes her before she goes down, and on
		// her, not on the empty place in her line)
		std::stable_sort(Queue.begin(), Queue.end(), [](const Event& A, const Event& B)
			{
				const bool bBlowA = A.What == "sea:torpedoHit" || A.What == "sea:depthChargeHit";
				const bool bBlowB = B.What == "sea:torpedoHit" || B.What == "sea:depthChargeHit";
				return A.T < B.T || (A.T == B.T && bBlowA && !bBlowB);
			});
		if (InZones) Zones = *InZones;
		if (Contact >= 0.0 && ContactT < 0.0) ContactT = Contact;
		if (!bStarted)
		{
			bStarted = true;
			ShownT = T;
			DrawT = T;
			Interpolate();
			StartColumns();
			// (in a hundred gentle passes: stations laid abreast nearer than their room spread out in order, none jumping past
			// its neighbour's, which would cross them the moment the line fans out)
			for (int K = 0; K < 100; ++K) KeepClear(1.0, true);
			Output();
		}
	}

	void Show::Debts(const View& Before, const View& After, const std::vector<Event>& Between)
	{
		for (const auto& Pair : After.Pieces)
		{
			auto Was = Before.Pieces.find(Pair.first);
			if (Was == Before.Pieces.end()) continue;
			const double Lost = Was->second.Str - Pair.second.Str;
			if (Lost <= 1e-6) continue;
			auto Blow = AirBlowAt.find(Pair.first);
			if (Blow != AirBlowAt.end() && Blow->second > Before.T - 1e-6) continue; // a strike, raid or torpedo drew it
			// A ship out of the fight takes her own hull with her: the piece's strength falls by it, but it is owed as no
			// hits on her sisters. One that falls out burning (the ship plan's R2, at half her hull or less) leaves with
			// her hull as the battle says it, else what she had before, at most that half. One that goes down shows the
			// blow that sank her in her going down: what she had before is not owed (the battle's hull for her is 0 when
			// she was worn through, which would put her last blow as shell hits on the ships still in her line).
			const int Ships0 = std::max(1, Pair.second.Ships0);
			double Left = 0.0;
			int Out = 0;
			for (const Event& E : Between)
			{
				if (E.Unit != Pair.first || (E.What != "sea:sunk" && E.What != "sea:burning")) continue;
				const double Own = Out == 0 ? Was->second.Str * Ships0 - (Was->second.Ships - 1) : 0.5;
				if (E.What == "sea:sunk") Left += Clamp(Own, 0.0, 1.0);
				else Left += E.Hull >= 0.0 ? E.Hull : Clamp(Own, 0.0, 0.5);
				++Out;
			}
			const double Struck = Lost * Ships0 - Left;
			if (Struck <= 1e-6) continue;
			const double Owe = Struck * HitsFor(Pair.second.Class);
			Debt[Pair.first].Hits += Owe;
			Check.OwedAll += Owe;
		}
	}

	void Show::Interpolate()
	{
		Now.clear();
		if (Views.empty()) return;
		const View* A = &Views.front();
		const View* B = nullptr;
		for (const View& V : Views)
		{
			if (V.T <= ShownT) A = &V;
			else { B = &V; break; }
		}
		const double Fr = B ? Clamp((ShownT - A->T) / std::max(1e-6, B->T - A->T), 0.0, 1.0) : 0.0;
		for (const auto& Pair : A->Pieces)
		{
			Piece P = Pair.second;
			if (B)
			{
				auto Next = B->Pieces.find(Pair.first);
				if (Next != B->Pieces.end())
				{
					P.X = Lerp(P.X, Next->second.X, Fr);
					P.Y = Lerp(P.Y, Next->second.Y, Fr);
					P.Face = Wrap(P.Face + Wrap(Next->second.Face - P.Face) * Fr);
					P.Speed = Lerp(P.Speed, Next->second.Speed, Fr);
					P.Str = Lerp(P.Str, Next->second.Str, Fr);
				}
			}
			Now[Pair.first] = P;
		}
	}

	void Show::StartColumns()
	{
		for (const auto& Pair : Now)
		{
			const Piece& P = Pair.second;
			if (Lines.count(P.Id)) continue;
			Column C;
			C.Id = P.Id;
			C.Side = P.Side;
			C.Class = P.Class;
			C.Nation = P.Nation;
			C.Size = SizeOf(P.Class);
			const int N = std::max(1, P.Ships0);
			C.Spacing = N > 1 && P.Depth > C.Size.Long ? (P.Depth - C.Size.Long) / (N - 1) : 3.0 * C.Size.Long;
			// The leader where the battle has the piece, the rest astern of him on his course, the track laid back that far.
			const Vec Lead(P.X, P.Y, 0.0);
			const double Back = C.Spacing * N + 400.0;
			for (double D = Back; D >= 0.0; D -= 10.0)
			{
				C.Track.push_back({ Lead - Ahead(P.Face) * D, Back - D, P.Face, 0.0 });
			}
			C.S = Back;
			C.bStarted = true;
			for (int K = 0; K < P.Ships; ++K)
			{
				Hull H;
				H.Key = P.Id * 64 + K;
				H.Piece = P.Id;
				H.Side = P.Side;
				H.Slot = K;
				H.Class = P.Class;
				H.Nation = P.Nation;
				H.Back = K * C.Spacing;
				H.SOn = Back - H.Back;
				H.P = Lead - Ahead(P.Face) * H.Back;
				H.Hdg = H.CourseHdg = P.Face;
				H.Spd = P.Speed;
				H.Roll0 = Mix(static_cast<uint64_t>(H.Key) * 97 + F.Seed) * 2.0 * Pi;
				const Ride R = RideOf(SizeOf(H.Class).Long);
				H.RollT = R.RollT * (0.9 + 0.2 * Mix(static_cast<uint64_t>(H.Key) * 13 + 5));
				H.PitchT = R.PitchT * (0.9 + 0.2 * Mix(static_cast<uint64_t>(H.Key) * 17 + 3));
				Hulls[H.Key] = H;
				C.Line.push_back(H.Key);
			}
			Lines[P.Id] = C;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The battle's events.

	Show::Hull* Show::NearestOf(int PieceId, double X, double Y)
	{
		auto It = Lines.find(PieceId);
		if (It == Lines.end()) return nullptr;
		Hull* Best = nullptr;
		double BestD = 1e18;
		for (const int Key : It->second.Line)
		{
			Hull* H = FindHull(Key);
			if (!H) continue;
			const double D = Flat(H->P - Vec(X, Y, 0.0));
			if (D < BestD) { BestD = D; Best = H; }
		}
		return Best;
	}

	Show::Hull* Show::ByZone(int PieceId, const Zone* Z)
	{
		auto It = Lines.find(PieceId);
		if (It == Lines.end() || It->second.Line.empty()) return nullptr;
		const Column& C = It->second;
		const Piece* P = PieceNow(PieceId);
		if (Z && P)
		{
			// The battle lays a lost ship's smoke at her place in her piece's line (sim.ts smoke): along its heading
			// from its middle, ahead positive, at most 0.4 of its depth. Read as a place in the line, front to back.
			const double Depth = std::max(P->Depth, 1.0);
			const double Along = (Z->X - P->X) * std::sin(P->Face) + (Z->Y - P->Y) * std::cos(P->Face);
			const double Share = Clamp(0.5 - Along / Depth, 0.0, 1.0);
			const int K = static_cast<int>(std::lround(Share * (static_cast<double>(C.Line.size()) - 1.0)));
			return FindHull(C.Line[static_cast<size_t>(Clamp(K, 0, static_cast<double>(C.Line.size()) - 1))]);
		}
		// No zone: the ship nearest the middle of the line, as near as a ship can be named.
		return FindHull(C.Line[C.Line.size() / 2]);
	}

	void Show::TakeOut(Hull& H)
	{
		auto It = Lines.find(H.Piece);
		if (It == Lines.end()) return;
		Column& C = It->second;
		auto At = std::find(C.Line.begin(), C.Line.end(), H.Key);
		if (At == C.Line.end()) return;
		const bool bLeader = At == C.Line.begin();
		C.Line.erase(At);
		if (bLeader && !C.Line.empty())
		{
			// The next astern takes the lead from where she is: the track ahead of her is his, not hers, and is dropped.
			Hull* Next = FindHull(C.Line.front());
			if (Next)
			{
				double Ahead0 = Next->Back;
				// (the head of the line: his ghost, where he was dodging, the column steaming its course)
				const Vec Head = C.bGhost && C.Ghost.Key == H.Key ? C.Ghost.P : H.P;
				const double SLead = C.Track.empty() ? C.S : C.S + Flat(Head - C.Track.back().P);
				// (B4: one dodging, off his track, leads from her place on it, not from where her dodge has taken her: the
				// column's course would step aside with her, and her ghost (SteamColumn) is laid there)
				Vec OnP = Next->P;
				double OnH = Next->Hdg, OnK = Next->Rate / std::max(0.5, Next->Spd);
				if (Next->DodgeJudge >= 0.0 && !C.Track.empty())
				{
					Along(C, Next->SOn, OnP, OnH, &OnK);
					Ahead0 = std::max(0.0, SLead - Next->SOn);
				}
				C.S = SLead - Ahead0;
				while (C.Track.size() > 1 && C.Track.back().S > C.S - 1.0) C.Track.pop_back();
				C.Track.push_back({ OnP, C.S, OnH, OnK });
				for (const int Key : C.Line)
				{
					if (Hull* Other = FindHull(Key)) Other->Back = std::max(0.0, Other->Back - Ahead0);
				}
				// (and the column's station goes back with the lead, to where she is: the battle's piece is a point, and a
				// column that has lost its van carries on from where its next ship steams, easing back to the battle's
				// place as it has room, rather than racing a piece already at its best speed)
				C.Keep += Vec(OnP.X - Head.X, OnP.Y - Head.Y, 0.0);
				if (Flat(C.Keep) > 2000.0) C.Keep = C.Keep * (2000.0 / Flat(C.Keep));
			}
		}
	}

	std::string Show::WayDown(const Hull& H)
	{
		// The ways a ship of her class went down in the war's gun and air actions, roughly as often as each (look only):
		// battleships capsized or sank more than they blew up (Hood, Barham, Arizona the famous few); destroyers broke in
		// two; carriers burned. A torpedo's flooding rolls her over or breaks her back; a heavy shell finds a magazine.
		struct Way { const char* How; double W; };
		std::vector<Way> Ways;
		if (H.Class == "bb") Ways = { { "explode", 0.15 }, { "capsize", 0.3 }, { "sink", 0.35 }, { "burn", 0.1 }, { "break", 0.1 } };
		else if (H.Class == "ca") Ways = { { "explode", 0.12 }, { "capsize", 0.3 }, { "sink", 0.33 }, { "burn", 0.08 }, { "break", 0.17 } };
		else if (H.Class == "cv") Ways = { { "burn", 0.35 }, { "sink", 0.25 }, { "capsize", 0.25 }, { "explode", 0.15 } };
		else if (H.Class == "ss")
		{
			// (a boat lost below is crushed: what comes up is oil, wreckage and air; on the surface she was more often
			// scuttled by her crew than sunk outright)
			if (H.Keel > SurfacedKeel(H.Class) + 3.0) return "crushed";
			Ways = { { "sink", 0.5 }, { "scuttled", 0.4 }, { "explode", 0.1 } };
		}
		// (a merchant torpedoed: most went down by the head or broke in two; tankers burned; ammunition ships blew up)
		else if (H.Class == "mv") Ways = { { "sink", 0.4 }, { "break", 0.25 }, { "capsize", 0.15 }, { "burn", 0.12 }, { "explode", 0.08 } };
		else Ways = { { "explode", 0.12 }, { "capsize", 0.15 }, { "sink", 0.33 }, { "break", 0.4 } };
		double Sum = 0.0;
		for (Way& W : Ways)
		{
			const std::string How = W.How;
			if (H.LastBlow == "torpedo" && (How == "capsize" || How == "break")) W.W *= 1.7;
			if (H.LastBlow == "shell_heavy" && How == "explode") W.W *= 1.6;
			if ((H.LastBlow == "dive" || H.LastBlow == "level") && How == "burn") W.W *= 1.6;
			Sum += W.W;
		}
		double R = Roll.U() * Sum;
		for (const Way& W : Ways)
		{
			R -= W.W;
			if (R < 0.0) return W.How;
		}
		return Ways.back().How;
	}

	void Show::Sink(Hull& H, const std::string& Words, const std::string& How)
	{
		TakeOut(H);
		// She sheers out of the line as she loses way, to the side away from the nearest enemy, so those astern pass her.
		{
			const Piece* Near = nullptr;
			double BestD = 1e18;
			for (const auto& Pair : Now)
			{
				const Piece& P = Pair.second;
				if (P.Side == H.Side) continue;
				const double D = Flat(Vec(P.X, P.Y, 0.0) - H.P);
				if (D < BestD) { BestD = D; Near = &P; }
			}
			const double Starboard = Near ? (Vec(Near->X, Near->Y, 0.0) - H.P).Dot(RightOf(H.Hdg)) : (Mix(static_cast<uint64_t>(H.Key)) - 0.5);
			H.Away = Wrap(H.Hdg + (Starboard > 0.0 ? -1.0 : 1.0) * 40.0 * Deg);
		}
		H.Doing = Fate::Sinking;
		// (the battle's own word where it gives one: a convoy's boat crushed, scuttled, rammed or shelled, ruling 241)
		static const char* const Words0[] = { "settle", "sink", "capsize", "explode", "burn", "break", "crushed", "scuttled", "rammed", "shelled" };
		bool bSaid = false;
		for (const char* W : Words0) bSaid = bSaid || How == W;
		H.How = bSaid ? How : WayDown(H);
		H.SinkAt = ShownT;
		const double For = H.How == "explode" ? 40.0 : H.How == "break" ? 70.0 : H.How == "capsize" ? 110.0 : H.How == "sink" ? 150.0 : H.How == "burn" ? 260.0
			: H.How == "crushed" ? 25.0 : H.How == "scuttled" ? 160.0 : H.How == "rammed" ? 60.0 : H.How == "shelled" ? 120.0 : 200.0;
		H.SinkFor = For * Roll.U(0.8, 1.25);
		H.ScreenUntil = -1.0;
		const bool bBig = H.How == "explode" && Big(H.Class) && !bBigBlast;
		if (bBig) bBigBlast = true;
		Fx& E = Emit(FxKind::Sink, H.P, H.How, H.Key);
		E.Size = bBig ? 1.0 : 0.0;
		E.Text = Words;
		E.Side = H.Side;
		++Check.Sinks;
		++Check.SinksBy[H.How];
	}

	void Show::LeaveLine(Hull& H, const std::string& Words)
	{
		TakeOut(H);
		H.Doing = Fate::Limping;
		H.LeftAt = ShownT;
		// Away from the nearest enemy still fighting, a little to one side or the other by her own number (a hash).
		const Piece* Near = nullptr;
		double BestD = 1e18;
		for (const auto& Pair : Now)
		{
			const Piece& P = Pair.second;
			if (P.Side == H.Side || P.State == "gone" || P.State == "withdrawn") continue;
			const double D = Flat(Vec(P.X, P.Y, 0.0) - H.P);
			if (D < BestD) { BestD = D; Near = &P; }
		}
		const double Off = (Mix(static_cast<uint64_t>(H.Key) * 7 + F.Seed) - 0.5) * 50.0 * Deg;
		H.Away = Near ? Wrap(Bearing(Vec(Near->X, Near->Y, 0.0), H.P) + Off) : Wrap(H.Hdg + Off);
		H.NextFire = ShownT;
		Fx& E = Emit(FxKind::LeaveLine, H.P, H.Class, H.Key);
		E.Text = Words;
		E.Side = H.Side;
		++Check.Leaves;
	}

	void Show::LightZone(const Zone& Z, const Hull* H)
	{
		if (ZonesLit.count(Z.Id)) return;
		ZonesLit.insert(Z.Id);
		// Where she went down, or where she left the line: her own place as drawn (the battle's spot is the middle of its
		// box; the column is drawn astern of the battle's place, so the drawn ship's own place keeps the oil under her).
		const Vec At = H ? Vec(H->P.X, H->P.Y, 0.0) : Vec(Z.X, Z.Y, 0.0);
		Fx& E = Emit(FxKind::OilFire, At, Z.Kind, Z.Id);
		E.Size = Z.R;
		E.Until = Z.Until > 0.0 ? Z.Until : F.Limit;
		E.Since = std::max(0.0, ShownT - Z.At);
		if (H) ZoneShip[Z.Id] = H->Key;
	}

	int Show::Launch(Strike& S, const Vec& From, double Hdg, int Slot)
	{
		Plane A;
		A.Key = NextKey++;
		A.Side = S.Side;
		A.Strike = S.Id;
		A.Nation = S.Side == 0 ? F.AttNat : F.DefNat;
		const bool bDive = Slot % 2 == 0;
		A.Role = S.bFromCoast ? "dive" : bDive ? "dive" : "torpedo";
		// The record's carrier aircraft by navy (the air scene's model names); any other navy's plain carrier models.
		if (A.Nation == "USA") A.Model = bDive ? "SBD" : "TBF";
		else if (A.Nation == "JAP") A.Model = bDive ? "D3A" : "B5N";
		else if (A.Nation == "UK") A.Model = bDive ? "Skua" : "Swordfish";
		else A.Model = std::string(bDive ? "Dive:" : "Torpedo:") + A.Nation;
		if (S.bFromCoast) A.Model = A.Nation == "GER" ? "Stuka" : A.Nation == "JAP" ? "D3A" : A.Nation == "USA" ? "SBD" : "Dive:" + A.Nation;
		A.P = From;
		A.Hdg = Hdg;
		A.Carrier = S.Carrier;
		A.Slot = Slot;
		A.Hand = Slot % 2 ? 1.0 : -1.0;
		A.Doing = S.bFromCoast ? Leg::Attack : Leg::Deck;
		A.Go = S.bFromCoast ? ShownT : ShownT + 3.0 + Slot * 4.0;
		A.LegT = 0.0;
		A.Spd = S.bFromCoast ? 95.0 : 0.0;
		Air[A.Key] = A;
		S.Planes.push_back(A.Key);
		return A.Key;
	}

	void Show::Play(const Event& E)
	{
		const Piece* P = E.Unit >= 0 ? PieceNow(E.Unit) : nullptr;
		const Vec At = P ? Vec(P->X, P->Y, 0.0) : Vec(E.X, E.Y, 0.0);
		if (!E.Cue.empty()) Emit(FxKind::Sound, At, E.Cue, E.Unit).Side = E.Side;
		if (!E.Text.empty())
		{
			Fx& D = Emit(FxKind::Dispatch, At, E.What, E.Unit);
			D.Text = E.Text;
			D.Side = E.Side;
		}
		if (E.What == "sea:line")
		{
			// (the R1 line: every piece of it steers the new course at once, so its rear leads after a turn together; the
			// columns follow their pieces, and the scene is told for its camera)
			const auto Guide = Lines.find(E.Unit);
			Fx& L = Emit(FxKind::Line, At, E.Why, Guide != Lines.end() && !Guide->second.Line.empty() ? Guide->second.Line.front() : -1);
			L.Side = E.Side;
			L.Size = static_cast<double>(E.Hand);
			L.Vel = Ahead(E.Course);
		}
		auto ZoneFor = [this, &E](const char* Kind) -> const Zone*
		{
			const Zone* Best = nullptr;
			for (const Zone& Z : Zones)
			{
				if (Z.Kind != Kind || Z.Unit != E.Unit || ZonesLit.count(Z.Id)) continue;
				if (std::fabs(Z.At - E.T) > 0.6) continue;
				if (!Best || Z.Id < Best->Id) Best = &Z;
			}
			return Best;
		};

		if (E.What == "sea:sunk" || E.What == "sea:burning")
		{
			const bool bSunk = E.What == "sea:sunk";
			bSunk ? ++Check.SunkEvents : ++Check.BurningEvents;
			const Zone* Z = ZoneFor(bSunk ? "sunk" : "burning");
			Hull* H = ByZone(E.Unit, Z);
			if (!H)
			{
				bSunk ? ++Check.SunkUnmatched : ++Check.BurningUnmatched;
				if (Z) LightZone(*Z, nullptr);
				return;
			}
			if (bSunk) Sink(*H, E.Text, E.How);
			else
			{
				LeaveLine(*H, E.Text);
				// (the battle's own course home and speed, where it gives them: B1's limp)
				if (E.bLimp) { H->Away = E.LimpCourse; H->LimpSpd = std::max(1.5, E.LimpSpeed); }
			}
			if (Z) LightZone(*Z, H);
			return;
		}
		if (E.What == "sea:torpedo")
		{
			++Check.TorpedoEvents;
			auto It = Lines.find(E.Unit);
			if (It == Lines.end()) return;
			const double Running = E.Running > 0.0 ? E.Running : F.TorpTravel;
			const Vec Aim = E.bAim ? Vec(E.AimX, E.AimY, 0.0) : At;
			// A boat's salvo (ruling 241): her own torpedoes (fish), fanned across the battle's spread, from her bow tubes, at
			// their own speed (an electric G7e about 15 m/s), its tracks none where they were wakeless.
			const bool bBoat = It->second.Class == "ss" || E.Fish > 0;
			// That share of the tracks the battle says the bank held (B1's share): one a ship at the least while it held
			// any, none at an empty bank; three a ship where the battle does not say.
			int PerShip = !E.bShare ? 3 : E.Share <= 0.0 ? 0 : std::max(1, static_cast<int>(std::lround(Clamp(E.Share, 0.0, 1.0) * 3.0)));
			if (bBoat && E.Fish > 0) PerShip = std::min(E.Fish, 6);
			// (no more than the room left, shared among her ships: a later salvo is never left without tracks while the
			// earlier ones still run, and every hit has a track to end)
			const int Firing = std::min(4, static_cast<int>(It->second.Line.size()));
			const int Room = TrackRoom - static_cast<int>(Torps.size());
			if (PerShip > 0 && Firing > 0 && PerShip * Firing > Room) PerShip = std::max(1, Room / Firing);
			int Fired = 0;
			for (const int Key : It->second.Line)
			{
				Hull* H = FindHull(Key);
				if (!H || Fired >= 4 || PerShip <= 0) continue;
				++Fired;
				// Three tracks from each ship of the piece (her tubes' spread, a few degrees apart), aimed at the battle's
				// aim point, at the speed that brings them there when the battle has them arrive; they run on past it.
				const double Base = Bearing(H->P, Aim), Dist = Flat(Aim - H->P);
				const double Speed = std::max(bBoat ? 8.0 : 18.0, Dist / Running);
				const double Fan = bBoat && E.Fan > 0.0 && PerShip > 1 ? Clamp(E.Fan, 0.0, 40.0) / (PerShip - 1) : 3.5;
				for (int Tube = 0; Tube < PerShip; ++Tube)
				{
					const double K = Tube - (PerShip - 1) * 0.5;
					if (static_cast<int>(Torps.size()) >= TrackRoom) break;
					Torp T;
					T.Key = NextKey++;
					T.Side = H->Side;
					T.Piece = E.Unit;
					const double Way = Base + (K * Fan + Roll.U(-0.8, 0.8)) * Deg;
					T.P = bBoat ? PlaceOn(*H, 0.45, K * 0.6, 0.0) : H->P + RightOf(H->Hdg) * (K * 4.0);
					T.From = T.P;
					T.Way = Ahead(Way);
					T.Speed = Speed;
					T.Launched = ShownT;
					T.Ends = ShownT + Running + 600.0 / Speed;
					T.Aim = Aim;
					Torps.push_back(T);
					Fx& S = Emit(FxKind::TrackStart, T.P, std::string(), T.Key);
					S.Vel = T.Way * T.Speed;
					S.Side = T.Side;
					S.Text = H->Nation; // (whose torpedo: the scene draws the Japanese ships' oxygen torpedoes' tracks faint)
					if (E.bWakeless) S.What = "wakeless";
					++Check.TracksRun;
				}
				// L5: she turns away under smoke, as destroyers did after their run (not a boat).
				if (!bBoat) H->ScreenUntil = ShownT + 60.0;
			}
			Check.MostTracks = std::max(Check.MostTracks, static_cast<int>(Torps.size()));
			return;
		}
		if (E.What == "sea:torpedoHit")
		{
			++Check.TorpedoHitEvents;
			AirBlowAt[E.Unit] = E.T;
			// The track of the launching piece that ends nearest a ship of the piece hit: it ends against her side. Where
			// the blow sinks or fires her, the battle's zone for it names the ship (the one that then goes down or burns).
			const Zone* Doom = ZoneFor("sunk");
			if (!Doom) Doom = ZoneFor("burning");
			Hull* Named = Doom ? ByZone(E.Unit, Doom) : nullptr;
			Torp* Best = nullptr;
			Hull* Struck = nullptr;
			double BestD = 1e18;
			for (Torp& T : Torps)
			{
				if (!T.bRunning || T.Piece != E.From) continue;
				Hull* H = Named ? Named : NearestOf(E.Unit, T.P.X, T.P.Y);
				if (!H) continue;
				const double D = Flat(H->P - T.P);
				if (D < BestD) { BestD = D; Best = &T; Struck = H; }
			}
			if (!Struck) Struck = Named ? Named : NearestOf(E.Unit, At.X, At.Y);
			if (!Struck) { ++Check.TorpedoHitUnmatched; return; }
			if (!Best) ++Check.TorpedoHitUnmatched;
			const double Side = Best ? (RightOf(Struck->Hdg).Dot(Best->P - Struck->P) >= 0.0 ? 1.0 : -1.0) : (Roll.U() < 0.5 ? -1.0 : 1.0);
			const ClassSize& Sz = SizeOf(Struck->Class);
			const Vec Hit = PlaceOn(*Struck, Roll.U(-0.3, 0.3), Side * (Sz.Beam * 0.5 + 2.0), 0.0);
			if (Best)
			{
				Best->bRunning = false;
				Best->P = Hit;
				Emit(FxKind::TrackStop, Hit, std::string(), Best->Key);
			}
			Fx& X = Emit(FxKind::TorpedoHit, Hit, "torpedo", Struck->Key);
			X.Side = Struck->Side;
			Struck->LastBlow = "torpedo";
			++Check.TorpedoHits;
			return;
		}
		if (E.What == "ability" && E.Key == "strike")
		{
			++Check.StrikeEvents;
			auto It = Lines.find(E.Unit);
			Hull* Deck = It != Lines.end() && !It->second.Line.empty() ? FindHull(It->second.Line.front()) : nullptr;
			if (!Deck) return;
			Strike S;
			S.Id = static_cast<int>(Strikes.size()) + 1;
			S.Side = Deck->Side;
			S.Carrier = Deck->Key;
			S.Target = E.Target;
			S.Launched = ShownT;
			S.Lands = E.T + F.StrikeWindup;
			S.Count = E.Count;
			// The aircraft seen: one for about every three that flew (four at least, twelve at most), half diving and
			// half with torpedoes; the rest of the strike is the battle's count.
			const int N = static_cast<int>(Clamp(std::round(std::max(E.Count, 12) / 3.0), 4.0, 12.0));
			for (int K = 0; K < N && static_cast<int>(Air.size()) < 60; ++K)
			{
				const Vec From = PlaceOn(*Deck, -0.3, 0.0, 18.0);
				Launch(S, From, Deck->Hdg, K);
			}
			Strikes.push_back(S);
			return;
		}
		if (E.What == "sea:cap" || E.What == "sea:flak")
		{
			// Fighters over the fleet and its guns meet the strike or raid now coming in on piece E.Unit: the share turned
			// back is shot down or scattered as the strike lands (its own event, next).
			for (Strike& S : Strikes)
			{
				if (S.bLanded || std::fabs(S.Lands - E.T) > 1.0) continue;
				// (the guns themselves are the ships' own, firing at every aircraft of the other side in their reach: L9,
				// FireGuns; the battle's word here is what the strike lost to them)
			}
			return;
		}
		if (E.What == "sea:strike")
		{
			++Check.StrikeLandEvents;
			// (the strike due now from that side, the one sent against this ship first)
			Strike* Found = nullptr;
			for (Strike& S : Strikes)
			{
				if (S.bLanded || std::fabs(S.Lands - E.T) > 1.0 || S.Side != E.Side) continue;
				if (!Found || (S.Target == E.Unit && Found->Target != E.Unit)) Found = &S;
			}
			if (!Found) return;
			Found->bLanded = true;
			Found->Target = E.Unit;
			const double Share = E.Flown > 0.0 ? Clamp(E.Landed / E.Flown, 0.0, 1.0) : 1.0;
			// Those the fighters and the guns turned back: the share not landed, some of them shot down.
			int N = static_cast<int>(Found->Planes.size());
			int Down = static_cast<int>(std::round(N * (1.0 - Share) * 0.5));
			int Hits = E.Landed > 0.0 ? std::max(1, static_cast<int>(std::round(N * Share * 0.4))) : 0;
			// (taken in an order of their own, so the dive bombers and the torpedo bombers each have their share of the
			// losses and the hits: B5.5's tall columns close aboard come with a dive bomber's hit; and those still to let
			// go first, as one that has let go already can no longer hit)
			std::vector<int> Order = Found->Planes;
			for (size_t K = Order.size(); K > 1; --K) std::swap(Order[K - 1], Order[std::min(K - 1, static_cast<size_t>(Roll.U() * static_cast<double>(K)))]);
			std::stable_partition(Order.begin(), Order.end(), [this](int Key)
			{
				auto It = Air.find(Key);
				return It != Air.end() && It->second.Doing != Leg::Home && It->second.Doing != Leg::Gone;
			});
			for (const int Key : Order)
			{
				auto A = Air.find(Key);
				if (A == Air.end()) continue;
				Plane& Pl = A->second;
				if (Down > 0) { --Down; Pl.Lost = ShownT + Roll.U(0.0, 5.0); continue; }
				// (the rest attack: Hits of them on the ship, the others near misses)
				Pl.bHit = Hits > 0;
				if (Hits > 0) --Hits;
			}
			AirBlowAt[E.Unit] = E.T;
			return;
		}
		if (E.What == "sea:recover")
		{
			// The strike lands on (or, its carrier gone, comes down in the sea): those still flying home do so as they
			// reach her (FlyStrikes); nothing more to start here.
			++Check.RecoverEvents;
			return;
		}
		if (E.What == "sea:air")
		{
			++Check.AirEvents;
			// Aircraft from the coast: a few seen already diving in from their own side, bombs falling a few seconds on.
			Strike S;
			S.Id = static_cast<int>(Strikes.size()) + 1;
			S.Side = E.Side >= 0 ? E.Side : 0;
			S.Target = E.Unit;
			S.Launched = ShownT;
			S.Lands = ShownT;
			S.bLanded = true;
			S.bFromCoast = true;
			const bool bNone = E.Text.find("none reach") != std::string::npos;
			const double Home = S.Side == 0 ? Pi : 0.0; // (the attacker's coast to the south, the defender's to the north)
			for (int K = 0; K < 4 && static_cast<int>(Air.size()) < 60; ++K)
			{
				const Vec From = At + Ahead(Home + (K - 1.5) * 12.0 * Deg) * (3200.0 + K * 150.0) + Vec(0.0, 0.0, 2600.0);
				const int Key = Launch(S, From, Wrap(Home + Pi), K);
				Air[Key].bHit = !bNone && K == 0;
				Air[Key].LegT = 0.0;
			}
			Strikes.push_back(S);
			AirBlowAt[E.Unit] = E.T;
			return;
		}

		// A convoy's night (ruling 241: form.md §5, field-side-read.md §3). Looks only: the battle has decided each of them.
		if (E.What == "sea:dive")
		{
			++Check.DiveEvents;
			Hull* H = NearestOf(E.Unit, At.X, At.Y);
			if (!H) return;
			// (her piece carries the state too, and her keel follows it, Trim; told here so the scene shows her going)
			const auto Rank = [](const std::string& D) { return D == "deep" ? 2 : D == "periscope" ? 1 : 0; };
			const bool bDown = E.Dive.empty() ? E.Keel > H->Keel : Rank(E.Dive) > Rank(H->Dive);
			if (!E.Dive.empty()) H->Dive = E.Dive;
			Fx& D = Emit(FxKind::Dive, Vec(H->P.X, H->P.Y, 0.0), bDown ? "dive" : "surface", H->Key);
			D.Text = E.Why;
			D.Side = H->Side;
			++Check.Dives;
			return;
		}
		if (E.What == "sea:sonar")
		{
			++Check.SonarEvents;
			// (an escort's asdic finding her: the ping heard; a contact lost, or radar, sight or a radio bearing, is silent)
			if (E.Lost.empty() && (E.Kind.empty() || E.Kind == "asdic"))
			{
				Hull* H = NearestOf(E.Unit, At.X, At.Y);
				Fx& S = Emit(FxKind::Sound, H ? Vec(H->P.X, H->P.Y, 0.0) : At, "sea_asdic_ping", H ? H->Key : E.Unit);
				S.Side = E.Side;
			}
			return;
		}
		if (E.What == "sea:report")
		{
			// (a boat's sighting report: its words, above, are all it shows)
			++Check.ReportEvents;
			return;
		}
		if (E.What == "sea:depthCharge")
		{
			++Check.ChargeEvents;
			const Vec Middle = E.bAim ? Vec(E.AimX, E.AimY, 0.0) : E.bAt ? Vec(E.X, E.Y, 0.0) : At;
			Hull* H = NearestOf(E.Unit, Middle.X, Middle.Y);
			if (!H) return;
			const bool bAhead = E.Kind == "ahead";
			const ClassSize& Sz = SizeOf(H->Class);
			const int N = std::clamp(E.N > 0 ? E.N : bAhead ? 24 : 10, 1, 30);
			const Hull* Boat = E.On >= 0 ? NearestOf(E.On, Middle.X, Middle.Y) : nullptr;
			const double Keel = Boat ? Boat->Keel : F.DeepKeel;
			// A pattern is set to burst at the depth the battle gives (else where the boat is), and sinks at the charge's own
			// rate; an ahead-thrown salvo's bombs (about 7 m/s) burst only against her.
			const double Set = E.Set >= 0.0 ? E.Set : Clamp(Keel, 10.0, F.CrushKeel);
			const double Sinks = E.Sinking >= 0.0 ? E.Sinking : Set / (bAhead ? 7.0 : std::max(0.5, F.ChargeSink));
			const double Spread = E.Spread > 0.0 ? E.Spread : bAhead ? 40.0 : 120.0;
			// A pattern goes in as she runs over the spot: rolled off her stern rails and thrown to either side, one after
			// another over the length of the pattern at her speed.
			const double Speed = std::max(3.0, H->Spd);
			const double Run = bAhead ? 0.0 : Spread / Speed;
			Fx& D = Emit(FxKind::ChargeDrop, bAhead ? PlaceOn(*H, 0.3, 0.0, 3.0) : PlaceOn(*H, -0.5, 0.0, 1.0), bAhead ? "ahead" : "pattern", H->Key);
			D.Aim = bAhead ? Middle : PlaceOn(*H, -0.5, 0.0, 0.0) + Ahead(H->Hdg) * (Spread * 0.5);
			D.Aim.Z = 0.0;
			D.Vel = Ahead(H->Hdg) * H->Spd;
			D.Size = N;
			D.Spread = Spread;
			D.Sinking = Sinks;
			D.Until = Run;
			D.Side = H->Side;
			++Check.ChargeDrops;
			if (bAhead)
			{
				AheadAt[E.Unit] = E.T;
				return;
			}
			const Vec Stern = PlaceOn(*H, -0.5, 0.0, 0.0);
			for (int K = 0; K < N; ++K)
			{
				const double Share = N > 1 ? static_cast<double>(K) / (N - 1) : 0.0;
				const bool bRail = K % 2 == 0;
				const double Side = bRail ? 0.0 : ((K / 2) % 2 == 0 ? 1.0 : -1.0) * (Spread * 0.5 - Sz.Beam * 0.5);
				Charge C;
				C.Escort = H->Key;
				C.Piece = E.Unit;
				C.Boat = E.On;
				C.Side = H->Side;
				C.Kind = "pattern";
				C.At = Stern + Ahead(H->Hdg) * (Share * Spread) + RightOf(H->Hdg) * Side + Vec(Roll.Bell() * 3.0, Roll.Bell() * 3.0, 0.0);
				C.At.Z = -Set;
				C.Bursts = E.T + Share * Run + Sinks;
				Charges.push_back(C);
			}
			return;
		}
		if (E.What == "sea:depthChargeHit")
		{
			++Check.ChargeHitEvents;
			AirBlowAt[E.Unit] = E.T;
			Hull* Boat = NearestOf(E.Unit, At.X, At.Y);
			if (Boat) Boat->LastBlow = "depthcharge";
			const double Keel = E.Keel >= 0.0 ? E.Keel : Boat ? Boat->Keel : F.DeepKeel;
			const Vec Near = Boat ? Boat->P + Vec(Roll.U(-1.0, 1.0), Roll.U(-1.0, 1.0), 0.0) * (F.ChargeLethal * 0.6) : At;
			// The blow: that escort's charge bursting nearest this moment, moved within its lethal reach of her and burst now
			// (the battle's moment); an ahead-thrown salvo's bombs burst only now, on her, two or three of them.
			Charge* Best = nullptr;
			for (Charge& C : Charges)
			{
				if (C.Piece != E.From || C.bHit || std::fabs(C.Bursts - E.T) > 15.0) continue;
				if (!Best || std::fabs(C.Bursts - E.T) < std::fabs(Best->Bursts - E.T)) Best = &C;
			}
			auto It = AheadAt.find(E.From);
			const bool bAhead = !Best && It != AheadAt.end() && E.T - It->second <= 60.0;
			if (!Best)
			{
				if (!bAhead) ++Check.ChargeHitUnmatched;
				const int Bombs = bAhead ? 2 + (Roll.U() < 0.4 ? 1 : 0) : 1;
				Hull* By = NearestOf(E.From, Near.X, Near.Y);
				for (int K = 0; K < Bombs; ++K)
				{
					Charge C;
					C.Escort = By ? By->Key : -1;
					C.Piece = E.From;
					C.Boat = E.Unit;
					C.Side = By ? By->Side : (E.Side >= 0 ? 1 - E.Side : -1);
					C.Kind = bAhead ? "ahead" : "pattern";
					C.At = Near + Vec(Roll.Bell() * 2.0, Roll.Bell() * 2.0, 0.0) * static_cast<double>(K);
					C.At.Z = -Keel;
					C.bHit = K == 0;
					C.Bursts = E.T + K * 0.4;
					Charges.push_back(C);
				}
				return;
			}
			Best->bHit = true;
			Best->At = Vec(Near.X, Near.Y, -Keel);
			Best->Bursts = E.T;
			return;
		}
		if (E.What == "sea:starshell")
		{
			++Check.StarshellEvents;
			// Bursting over where the battle says, about 450 m up, burning about 40 s as it falls under its parachute at about
			// 3 m/s (a destroyer's 4-inch star shell: stand-in figures for the historian to check).
			const Vec Over = E.bAim ? Vec(E.AimX, E.AimY, 0.0) : E.bAt ? Vec(E.X, E.Y, 0.0) : At;
			const int Key = NextKey++;
			Fx& S = Emit(FxKind::Starshell, Vec(Over.X, Over.Y, 420.0 + 60.0 * Mix(static_cast<uint64_t>(Key) * 41 + F.Seed)), std::string(), Key);
			S.Until = E.T + 40.0;
			S.Side = E.Side;
			++Check.Starshells;
			return;
		}
		if (E.What == "sea:rescue")
		{
			++Check.RescueEvents;
			// Where the ship lost went down (the latest of her piece's ships to go), and the ship of the rescuer's piece nearest.
			const Hull* Lost = nullptr;
			for (const auto& Pair : Hulls)
			{
				const Hull& L = Pair.second;
				if (L.Piece != E.On || (L.Doing != Fate::Sinking && L.Doing != Fate::Gone)) continue;
				if (!Lost || L.SinkAt > Lost->SinkAt) Lost = &L;
			}
			const Vec Where = Lost ? Vec(Lost->P.X, Lost->P.Y, 0.0) : E.bAim ? Vec(E.AimX, E.AimY, 0.0) : At;
			Hull* By = NearestOf(E.Unit, Where.X, Where.Y);
			if (!By) return;
			Fx& R = Emit(FxKind::Rescue, Vec(By->P.X, By->P.Y, 0.0), std::string(), By->Key);
			R.Aim = Where;
			R.Until = E.T + 600.0;
			R.Side = By->Side;
			++Check.Rescues;
			return;
		}
	}

	double Show::SurfacedKeel(const std::string& Class) const
	{
		// (her draught: a Type VIIC's 4.7 m at 67 m long, others in proportion)
		return SizeOf(Class).Long * 0.07;
	}

	double Show::Trim(Hull& H, const Piece* P, double Dt, double& OutSink)
	{
		OutSink = 0.0;
		const double Up = SurfacedKeel(H.Class);
		// (only in the fight: one going down keeps the depth she had, and her end is told by how)
		if (P && H.Doing == Fate::Line)
		{
			if (!P->Dive.empty()) H.Dive = P->Dive;
			H.bScope = P->bScope;
		}
		const double Want = P && P->Keel >= 0.0 && H.Doing == Fate::Line ? P->Keel : H.Dive == "deep" ? F.DeepKeel : H.Dive == "periscope" ? F.ScopeKeel : Up;
		const double To = Clamp(Want, Up, std::max(Up, F.CrushKeel));
		if (H.Keel < 0.0)
		{
			H.Keel = H.KeelTo = H.KeelFrom = To;
			H.KeelFor = 0.0;
		}
		if (H.Doing == Fate::Line && std::fabs(To - H.KeelTo) > 0.05)
		{
			// A new depth, from where she is: a crash dive puts her casing under and her at periscope depth in about 27 s,
			// then about 1 m/s deeper; she comes up at about 1.2 m/s (tanks blown), and from periscope depth to the surface in
			// about 20 s.
			const double Lo = std::min(To, H.Keel), Hi = std::max(To, H.Keel);
			const double Shallow = std::max(0.0, std::min(Hi, F.ScopeKeel) - Lo);
			const bool bDown = To > H.Keel;
			H.KeelFrom = H.Keel;
			H.KeelTo = To;
			H.KeelSince = ShownT;
			H.KeelFor = Shallow / std::max(1.0, F.ScopeKeel - Up) * (bDown ? 27.0 : 20.0) + (Hi - Lo - Shallow) / (bDown ? 1.0 : 1.2);
		}
		const double Was = H.Keel;
		const double K = H.KeelFor > 0.0 ? Clamp((ShownT - H.KeelSince) / H.KeelFor, 0.0, 1.0) : 1.0;
		H.Keel = H.KeelFrom + (H.KeelTo - H.KeelFrom) * K * K * (3.0 - 2.0 * K);
		OutSink = Dt > 0.0 ? (H.Keel - Was) / Dt : 0.0;
		Check.MostKeel = std::max(Check.MostKeel, H.Keel);
		// (the waves' motion fades with depth: about a third left at periscope depth, none deep)
		return std::exp(-std::max(0.0, H.Keel - Up) / 8.0);
	}

	void Show::BurstCharges()
	{
		for (size_t K = 0; K < Charges.size();)
		{
			if (Charges[K].Bursts > ShownT + 1e-9)
			{
				++K;
				continue;
			}
			const Charge C = Charges[K];
			Charges.erase(Charges.begin() + static_cast<long>(K));
			Fx& E = Emit(FxKind::ChargeBurst, C.At, C.Kind, C.Escort);
			E.Size = C.bHit ? 1.0 : 0.0;
			E.Side = C.Side;
			++Check.ChargeBursts;
			if (C.bHit) ++Check.ChargeBurstsBooked;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Steaming: each column's leader on the battle's course, the rest in his wake.

	void Show::Along(const Column& C, double S, Vec& OutP, double& OutHdg, double* OutK)
	{
		if (OutK) *OutK = 0.0;
		// Before his track's first point or past its last: straight on along its heading there.
		if (C.Track.empty()) return;
		if (S <= C.Track.front().S)
		{
			const Wake& W = C.Track.front();
			OutP = W.P - Ahead(W.H) * (W.S - S);
			OutHdg = W.H;
			return;
		}
		if (S >= C.Track.back().S)
		{
			const Wake& W = C.Track.back();
			OutP = W.P + Ahead(W.H) * (S - W.S);
			OutHdg = W.H;
			if (OutK) *OutK = W.K;
			return;
		}
		size_t L = 0, R = C.Track.size() - 1;
		while (R - L > 1)
		{
			const size_t M = (L + R) / 2;
			if (C.Track[M].S <= S) L = M; else R = M;
		}
		// A smooth curve between the two points he passed, leaving each on his heading there (Hermite), and the heading
		// along it smooth too, turning as sharply as he turned at each: so her rate of turn never jumps.
		const Wake& A = C.Track[L];
		const Wake& B = C.Track[R];
		const double D = std::max(1e-6, B.S - A.S);
		const double U = Clamp((S - A.S) / D, 0.0, 1.0);
		const double U2 = U * U, U3 = U2 * U;
		const double H00 = 2.0 * U3 - 3.0 * U2 + 1.0, H10 = U3 - 2.0 * U2 + U, H01 = -2.0 * U3 + 3.0 * U2, H11 = U3 - U2;
		const double Chord = Flat(B.P - A.P);
		OutP = A.P * H00 + Ahead(A.H) * (Chord * H10) + B.P * H01 + Ahead(B.H) * (Chord * H11);
		const double HB = A.H + Wrap(B.H - A.H);
		OutHdg = Wrap(A.H * H00 + A.K * D * H10 + HB * H01 + B.K * D * H11);
		if (OutK) *OutK = (A.H * (6.0 * U2 - 6.0 * U) + A.K * D * (3.0 * U2 - 4.0 * U + 1.0) + HB * (6.0 * U - 6.0 * U2) + B.K * D * (3.0 * U2 - 2.0 * U)) / D;
	}

	void Show::SteamColumn(Column& C, const Piece& P, double Dt)
	{
		if (C.Line.empty()) return;
		Hull* Lead = FindHull(C.Line.front());
		if (!Lead) return;
		// B4: a leader dodging, fallen back along her course level with the ship next astern of her (round to comb a drop
		// across it, or hard over from a dive), hands that ship the lead rather than come back up through her own line.
		if (C.bGhost && C.Ghost.Key == Lead->Key && Lead->Doing == Fate::Line && C.Line.size() >= 2 && !C.Track.empty())
		{
			Hull* Next = FindHull(C.Line[1]);
			if (Next && Next->Doing == Fate::Line)
			{
				const double SGhost = C.S + Flat(C.Ghost.P - C.Track.back().P);
				const double Lag = Vec(C.Ghost.P.X - Lead->P.X, C.Ghost.P.Y - Lead->P.Y, 0.0).Dot(Ahead(C.Ghost.Hdg));
				if (Lag >= SGhost - Next->SOn - C.Size.Long)
				{
					HandOver(C, *Lead, *Next);
					Lead = Next;
				}
			}
		}
		// L7: before the guns meet each side zig-zags together about its course, legs of about 75 s; from contact the
		// zig-zag fades out over 40 s. (Drawn only: the battle steams its own course.)
		{
			const double Period = 150.0, Width = 140.0;
			const double Phase = Mix(F.Seed * 5 + static_cast<uint64_t>(C.Side) * 3 + 1);
			const double Tri = (2.0 / Pi) * std::asin(std::sin(2.0 * Pi * (ShownT / Period + Phase)));
			double Fade = 1.0;
			if (ContactT >= 0.0) Fade = Clamp(1.0 - (ShownT - ContactT) / 40.0, 0.0, 1.0);
			if (P.State == "routing" || P.State == "withdrawn") Fade = 0.0;
			C.Zig = Lerp(C.Zig, Width * Tri * Fade, Clamp(Dt * 0.5, 0.0, 1.0));
			if (ContactT >= 0.0 && ShownT > ContactT + 60.0) Check.MostZigAfter = std::max(Check.MostZigAfter, std::fabs(C.Zig));
		}
		const Vec Want = Vec(P.X, P.Y, 0.0) + RightOf(P.Face) * C.Zig + C.Keep;
		const Vec WantV = Ahead(P.Face) * P.Speed;
		const ClassSize& Sz = C.Size;
		const double MostRudder = Sz.Turn * 1.25 / 3.0;
		const double Acc = AccelOf(C.Class) * 2.0;
		// The leader's helm (his own; while he dodges an air attack, his ghost's on the course, and then his own back to
		// it): the battle's course, altered toward his station by as much as the distance across to it asks over twenty
		// seconds' steaming (never more than 40 degrees off the course, as a column alters to regain station), or straight
		// for it when he is far off; his rate of turn no more than his class's at his speed, the rudder put on and taken
		// off over seconds; his speed a few knots over or under the battle's to close what he is astern or ahead.
		auto Helm = [&](Hull& L, const Vec& Goal, const Vec& GoalV, double Face, double Speed)
		{
			// (held to the way she will have at the end of the step where one step's loss of way is near a hundredth of
			// her rate of turn: a boat dived at a crawl, an escort stopping among survivors, ruling 241)
			const double Slowing = Acc * Dt;
			const double MostRate = MostRateAt(Sz, C.Class == "ss" || L.Spd < Slowing * 120.0 ? std::max(0.0, L.Spd - Slowing) : L.Spd);
			const Vec ToWant = Goal - L.P;
			const double Reach = std::max(Speed * 20.0, 2.5 * Sz.Long);
			const double Hope = Flat(ToWant) > 1500.0 ? Bearing(L.P, Goal + GoalV * 6.0)
				: Wrap(Face + Clamp(std::atan2(ToWant.Dot(RightOf(Face)), Reach), -40.0 * Deg, 40.0 * Deg));
			// Keeping clear: a ship that would pass nearer than her clearance within the next 40 s he turns away from,
			// astern of her where she crosses, to starboard where she comes on head on (the rule of the road), away from
			// her where she lies alongside; the harder the nearer.
			double Way = 0.0, Urgent = 0.0;
			{
				// (judged on the way he is going and on the way he means to go, whichever passes nearer: a leader turning
				// back across his own column's track sees the ship he would cut across before he is on her)
				const Vec Mine = Ahead(L.Hdg) * std::max(L.Spd, 1.0);
				const Vec Meant = Ahead(Hope) * std::max(L.Spd, 1.0);
				for (const auto& Pair : Hulls)
				{
					const Hull& O = Pair.second;
					if (O.Key == L.Key || O.Doing == Fate::Gone) continue;
					const Vec Rel(O.P.X - L.P.X, O.P.Y - L.P.Y, 0.0);
					if (std::fabs(Rel.X) > 1200.0 || std::fabs(Rel.Y) > 1200.0) continue;
					double Tc = 0.0;
					Vec Closest;
					for (int Pass = 0; Pass < 2; ++Pass)
					{
						const Vec RelV = Ahead(O.Hdg) * O.Spd - (Pass == 0 ? Mine : Meant);
						const double V2 = RelV.Dot(RelV);
						const double T0 = Clamp(V2 > 1e-6 ? -Rel.Dot(RelV) / V2 : 0.0, 0.0, 40.0);
						const Vec Near = Rel + RelV * T0;
						if (Pass == 0 || Flat(Near) < Flat(Closest))
						{
							Tc = T0;
							Closest = Near;
						}
					}
					// (his own line, coming back past it, by most of its spacing: his followers steam where he steams, so
					// the room he leaves his own ships is the room they will have from each other)
					const double Clear = O.Piece == L.Piece ? std::max(Sz.Long + 30.0, 0.8 * C.Spacing) : 0.5 * (Sz.Long + SizeOf(O.Class).Long) + 30.0;
					const double D = Flat(Closest);
					if (D >= Clear || (O.Piece == L.Piece && Rel.Dot(Ahead(L.Hdg)) < 0.0)) continue;
					const double Need = (1.0 - D / Clear) * (1.0 - Tc / 40.0);
					if (Need <= Urgent) continue;
					Urgent = Need;
					const double Side = Closest.Dot(RightOf(L.Hdg));
					if (std::fabs(Side) >= 0.2 * Clear) Way = Side > 0.0 ? -1.0 : 1.0;
					else if (std::cos(O.Hdg - L.Hdg) < -0.7) Way = 1.0;
					else
					{
						const double Now0 = Rel.Dot(RightOf(L.Hdg));
						Way = std::fabs(Now0) > 1.0 ? (Now0 > 0.0 ? -1.0 : 1.0) : (L.Key < O.Key ? -1.0 : 1.0);
					}
				}
			}
			const double Steer = Wrap(Hope + Way * Urgent * 70.0 * Deg);
			const double Behind = ToWant.Dot(Ahead(L.Hdg));
			const double WantRate = Clamp(Wrap(Steer - L.Hdg) * 0.6, -MostRate, MostRate);
			const double Bite = MostRudder * std::max(1.0, std::pow(L.Spd / std::max(1.0, Sz.Speed), 2.0));
			L.Rate += Clamp(WantRate - L.Rate, -Bite * Dt, Bite * Dt);
			L.Rate = Clamp(L.Rate, -MostRate, MostRate);
			L.Hdg = Wrap(L.Hdg + L.Rate * Dt);
			// (a boat dived makes her best speed on her batteries, ruling 241)
			const double Top = C.Class == "ss" && L.Keel > SurfacedKeel(C.Class) + 1.0 ? std::min(Sz.Speed * 1.45, F.DivedTop) : Sz.Speed * 1.45;
			const double WantSpd = Clamp(Speed + Clamp(Behind * 0.06, -0.4 * Speed, 4.0), 0.0, Top);
			L.Spd += Clamp(WantSpd - L.Spd, -Acc * Dt, Acc * Dt);
			L.P += Ahead(L.Hdg) * (L.Spd * Dt);
		};
		if (C.bGhost && (C.Ghost.Key != Lead->Key || Lead->Doing != Fate::Line)) C.bGhost = false; // (no longer the ship that dodged)
		if (!C.bGhost && Lead->DodgeJudge >= 0.0 && Lead->Doing == Fate::Line)
		{
			// (come to the head of the line while she dodges, or before she is back in her place: she leaves a ghost on the
			// course she was following, from here, and comes back to it as a leader does; else her dodge would end at once)
			C.bGhost = true;
			C.GhostSince = ShownT;
			C.Ghost = *Lead;
			C.Ghost.Hdg = Lead->CourseHdg;
			C.Ghost.Rate = 0.0;
			// (where the lead was given her: her place on the track, TakeOut's last point)
			if (!C.Track.empty()) C.Ghost.P = C.Track.back().P;
		}
		Hull& Course = C.bGhost ? C.Ghost : *Lead; // (the one steaming the column's course)
		Helm(Course, Want, WantV, P.Face, P.Speed);
		Lead->CourseHdg = Course.Hdg;
		if (C.bGhost)
		{
			// B4: the leader himself, hard over against the attack, or round to comb a drop, holding the battle's speed;
			// then back to his ghost (his own helm steering for it as for his station), and on it, his own course again.
			const double Most = MostRateAt(Sz, Lead->Spd);
			double Wish = 0.0;
			if (DodgeRate(*Lead, Most, Wish))
			{
				const double Bite = MostRudder * std::max(1.0, std::pow(Lead->Spd / std::max(1.0, Sz.Speed), 2.0));
				Lead->Rate += Clamp(Wish - Lead->Rate, -Bite * Dt, Bite * Dt);
				Lead->Rate = Clamp(Lead->Rate, -Most, Most);
				Lead->Hdg = Wrap(Lead->Hdg + Lead->Rate * Dt);
				Lead->Spd += Clamp(C.Ghost.Spd - Lead->Spd, -Acc * Dt, Acc * Dt);
				Lead->P += Ahead(Lead->Hdg) * (Lead->Spd * Dt);
			}
			else
			{
				Helm(*Lead, C.Ghost.P, Ahead(C.Ghost.Hdg) * C.Ghost.Spd, C.Ghost.Hdg, C.Ghost.Spd);
			}
			const double Off = Flat(Lead->P - C.Ghost.P);
			Check.MostStray = std::max(Check.MostStray, Off);
			Lead->OffTrack = Vec(C.Ghost.P.X - Lead->P.X, C.Ghost.P.Y - Lead->P.Y, 0.0).Dot(RightOf(C.Ghost.Hdg));
			const bool bOn = ShownT >= Lead->DodgeUntil && Off < 20.0 && std::fabs(Wrap(Lead->Hdg - C.Ghost.Hdg)) < 4.0 * Deg;
			if (Lead->DodgeJudge >= 0.0 && (bOn || ShownT >= Lead->DodgeJudge))
			{
				// (judged only where her helm bit: a dodge ended before it did, another ship too near, is none)
				if (!Lead->bDodgeTurned) ++Check.DodgesUnturned;
				else if (bOn) ++Check.DodgesBack;
				else Check.MostDodgeOff = std::max(Check.MostDodgeOff, Off);
				Lead->DodgeJudge = -1.0;
				Lead->bDodgeTurned = false;
			}
			// (the ghost kept until she is on it: dropped short of that, his track would jump to where she is)
			if (bOn) C.bGhost = false;
		}
		else
		{
			Lead->OffTrack = 0.0;
		}
		Lead->Back = 0.0;
		if (Flat(Course.P - Want) > Check.MostLead)
		{
			Check.MostLead = Flat(Course.P - Want);
			Check.LeadWorst = Worst(Course, ShownT);
		}
		Check.MostKeep = std::max(Check.MostKeep, Flat(C.Keep));
		// His track (his ghost's while he dodges): a point every 5 m he steams, with his heading and how sharply he
		// turned there.
		const double Moved = Flat(Course.P - C.Track.back().P);
		if (Moved >= 5.0)
		{
			C.S += Moved;
			C.Track.push_back({ Course.P, C.S, Course.Hdg, Course.Rate / std::max(0.5, Course.Spd) });
		}
		const double Keep = C.Spacing * static_cast<double>(C.Line.size() + 1) + 600.0;
		while (C.Track.size() > 2 && C.Track[1].S < C.S - Keep) C.Track.pop_front();
		// The rest in his wake: each at her place astern of him along his track, closing up after a loss ahead of her at
		// a few knots more than his speed.
		const double SLead = C.S + Flat(Course.P - C.Track.back().P);
		// (B4: a ship coming back from a comb who finds herself astern of the next in her line falls in astern of that one,
		// as a ship out of station takes the place she can reach, rather than run up through her)
		for (size_t K = 1; K + 1 < C.Line.size(); ++K)
		{
			Hull* Out = FindHull(C.Line[K]);
			Hull* Next = FindHull(C.Line[K + 1]);
			if (!Out || !Next || Out->DodgeJudge < 0.0 || ShownT < Out->DodgeUntil || std::fabs(Out->OffTrack) > 200.0) continue;
			if (Out->SOn >= Next->SOn - 0.5 * Sz.Long) continue;
			std::swap(C.Line[K], C.Line[K + 1]);
			std::swap(Out->Back, Next->Back);
			++Check.FellAstern;
		}
		double AheadS = SLead; // (where the ship next ahead of her is along his track)
		for (size_t K = 1; K < C.Line.size(); ++K)
		{
			Hull* H = FindHull(C.Line[K]);
			if (!H) continue;
			const double Want0 = static_cast<double>(K) * C.Spacing;
			// (closing up at up to five knots over his speed, easing as she comes to her place: her way never jumps)
			if (H->Back > Want0) H->Back = std::max(Want0, H->Back - std::min(2.5, 0.08 * (H->Back - Want0) + 0.05) * Dt);
			else H->Back = Want0;
			// (and never nearer than most of a spacing astern of the ship ahead of her, where that one has given way)
			const double STarget = std::min(SLead - H->Back, AheadS - 0.6 * C.Spacing);
			// Where she is along his track: her last place there, moved on by what she has made along it since.
			Vec OnP;
			double OnH = H->Hdg, OnK = 0.0;
			Along(C, H->SOn, OnP, OnH);
			// (far off it, after a comb, no faster than twice her way or his, whichever is more: there the nearest point of a
			// turning track can leap from one leg of his turn to another, and her place in the line with it)
			const double MostOn = std::fabs(H->OffTrack) > 100.0 ? 2.0 * std::max(H->Spd, Course.Spd) * Dt + 2.0 : 1e9;
			H->SOn += Clamp(Vec(H->P.X - OnP.X, H->P.Y - OnP.Y, 0.0).Dot(Ahead(OnH)), -MostOn, MostOn);
			Along(C, H->SOn, OnP, OnH, &OnK);
			{
				// (the turn she answers is his a second ahead of her: her rudder takes about that long to bite, so putting it
				// on there she turns where he turned, and comes out of one turn into the next on his track)
				Vec SoonP;
				double SoonH = OnH;
				Along(C, H->SOn + H->Spd * 1.0, SoonP, SoonH, &OnK);
			}
			// Her helm: turning as he turned just ahead of her (his rate there at her speed), with a touch more to come back
			// on to his track over a ship's length; her rate and rudder no more than her class's at her speed.
			const double Across = Vec(OnP.X - H->P.X, OnP.Y - H->P.Y, 0.0).Dot(RightOf(OnH));
			H->OffTrack = Across;
			// (far off it, after a comb, back to it at no more than 35 degrees: steeper, she makes too little way along it
			// to keep her place)
			const double WantHdg = Wrap(OnH + Clamp(std::atan2(Across, std::max(40.0, Sz.Long)), -35.0 * Deg, 35.0 * Deg));
			const double Most = MostRateAt(Sz, H->Spd);
			double Wish = Clamp(H->Spd * OnK + 0.5 * Wrap(WantHdg - H->Hdg), -Most, Most);
			H->CourseHdg = OnH;
			DodgeRate(*H, Most, Wish); // (B4: hard over against an air attack, or round to comb a drop, while she dodges)
			const double HerBite = MostRudder * std::max(1.0, std::pow(H->Spd / std::max(1.0, Sz.Speed), 2.0)); // (her rudder bites harder at more way)
			H->Rate += Clamp(Wish - H->Rate, -HerBite * Dt, HerBite * Dt);
			H->Rate = Clamp(H->Rate, -Most, Most);
			H->Hdg = Wrap(H->Hdg + H->Rate * Dt);
			// Her speed: his, and a little more or less to hold her place astern of him.
			const double Behind0 = STarget - H->SOn;
			double WishSpd = Clamp(Course.Spd + Clamp(0.1 * Behind0, -0.4 * Course.Spd, 3.0), 0.0, Sz.Speed * 1.45);
			// Giving way: where his track has come back across itself, a ship ahead of her in the line (far ahead along
			// the track, near across the water) may be about to cross her bow; she eases her way, or keeps it eased, while
			// stopping would pass her clearer than going on as she wishes, and takes up her place again once it has passed.
			const Vec Going = Ahead(H->Hdg) * std::max(H->Spd, WishSpd);
			bool bGave = false;
			for (size_t J = 0; J < K; ++J)
			{
				const Hull* Mate = FindHull(C.Line[J]);
				if (!Mate) continue;
				const Vec Off(Mate->P.X - H->P.X, Mate->P.Y - H->P.Y, 0.0);
				const double OnTrack = (J == 0 ? SLead : Mate->SOn) - H->SOn;
				if (OnTrack < 1.5 * Flat(Off) + Sz.Long) continue;
				const Vec Theirs = Ahead(Mate->Hdg) * Mate->Spd;
				auto Passing = [&Off, &Theirs](const Vec& Her) -> double
				{
					const Vec Closing = Theirs - Her;
					const double Sq = Closing.Dot(Closing);
					const double When = Sq > 1e-6 ? Clamp(-Off.Dot(Closing) / Sq, 0.0, 30.0) : 0.0;
					return Flat(Off + Closing * When);
				};
				const double AsShe = Passing(Going);
				if (AsShe < Sz.Long + 30.0 && Passing(Vec(0.0, 0.0, 0.0)) > AsShe + 1.0)
				{
					WishSpd = std::min(WishSpd, H->Spd * 0.25);
					bGave = true;
				}
			}
			if (bGave) Check.GaveWay += Dt;
			H->Spd += Clamp(WishSpd - H->Spd, -Acc * Dt, Acc * Dt);
			H->P += Ahead(H->Hdg) * (H->Spd * Dt);
			AheadS = H->SOn;
			if (H->DodgeJudge >= 0.0)
			{
				// (B4: back in her place in the line once the attacks on her are over: on his track and at her distance)
				const double Off = std::max(std::fabs(Across), std::fabs(STarget - H->SOn));
				Check.MostStray = std::max(Check.MostStray, Off);
				const double Gap = STarget - H->SOn;
				if (ShownT >= H->DodgeJudge && Gap > 60.0 && std::fabs(Across) <= 60.0 && H->Spd >= Sz.Speed * 1.45 - 0.2 &&
				    ShownT < H->DodgeUntil + 480.0 && (H->PutOffGap < 0.0 || H->PutOffGap - Gap >= 15.0))
				{
					// (on his track, still astern of her place with her best speed on, as when the line steams near its
					// top speed: closing, judged as she comes up; put off again only while she gains 15 m in each 30 s,
					// and for five minutes at the most)
					H->PutOffGap = Gap;
					H->DodgeJudge = ShownT + 30.0;
					++Check.DodgesClosing;
				}
				if (ShownT >= H->DodgeJudge)
				{
					if (!H->bDodgeTurned) ++Check.DodgesUnturned;
					else if (Off <= 60.0) ++Check.DodgesBack;
					else Check.MostDodgeOff = std::max(Check.MostDodgeOff, Off);
					H->DodgeJudge = -1.0;
					H->bDodgeTurned = false;
					H->PutOffGap = -1.0;
				}
			}
			else if (std::fabs(H->Back - Want0) < 1.0 && std::fabs(Across) > Check.MostGap)
			{
				Check.MostGap = std::fabs(Across);
				Check.GapWorst = Worst(*H, ShownT);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// L9: the ships' anti-aircraft guns (Battle/ShipGuns, shared with the air scene; Brian, 21 November 18:06Z: "for the
	// ships being attacked by planes, they can fire machine gun fire and flak guns at them just like real life"). Each
	// ship fires her navy's guns of the battle's month at every aircraft of the other side in their reach, holding on one
	// her own fighters are after; the battle decides every loss, the guns only show the fire they came from.

	std::vector<ShipGuns::Hull> Show::GunHulls() const
	{
		const double On = std::max(0.0, DrawT - ShownT);
		std::vector<ShipGuns::Hull> Out;
		Out.reserve(Hulls.size());
		for (const auto& Pair : Hulls)
		{
			const Hull& H = Pair.second;
			if (H.Doing == Fate::Gone) continue;
			if (H.Class == "ss" && H.Keel > SurfacedKeel(H.Class) + 1.0) continue; // (a boat dived: her guns are under)
			const ClassSize& Sz = SizeOf(H.Class);
			ShipGuns::Hull G;
			G.Id = H.Key;
			G.At = Vec(H.P.X, H.P.Y, 0.0) + Ahead(H.Hdg) * (H.Spd * On);
			G.Face = H.Hdg;
			G.Vel = Ahead(H.Hdg) * H.Spd;
			G.Nation = H.Nation;
			G.Class = H.Class;
			G.Side = H.Side;
			G.Long = Sz.Long;
			G.Beam = Sz.Beam;
			// (a ship burning out of the fight fights her fires with half her gun crews; one going down, none)
			G.Manned = H.Doing == Fate::Limping ? 0.5 : 1.0;
			G.bSunk = H.Doing == Fate::Sinking;
			Out.push_back(G);
		}
		return Out;
	}

	ShipGuns::Plane Show::GunPlane(const Plane& A) const
	{
		const double On = std::max(0.0, DrawT - ShownT);
		ShipGuns::Plane P;
		P.Key = A.Key;
		P.V = A.bFalling ? A.Fall : Ahead(A.Hdg) * A.Spd + Vec(0.0, 0.0, A.Climb);
		P.P = A.P + P.V * On;
		P.Hdg = A.Hdg;
		P.Gam = A.Pitch * Deg;
		P.Bank = A.Bank * Deg;
		// (its size, for where the rounds pass it: the war's carrier aircraft about 12 to 16 m across)
		const std::string& M = A.Model;
		const bool bBig = M == "TBF" || M == "B5N" || M == "Swordfish" || A.Role == "torpedo";
		P.Span = bBig ? 15.5 : 12.5;
		P.Long = bBig ? 11.5 : 10.0;
		P.Side = A.Side;
		P.bFighter = A.Over >= 0;
		P.bFalling = A.bFalling;
		return P;
	}

	void Show::FireGuns(double Dt)
	{
		std::vector<ShipGuns::Plane> Up;
		for (const auto& Pair : Air)
		{
			const Plane& A = Pair.second;
			if (A.Doing == Leg::Gone || A.Doing == Leg::Deck || !A.bShown) continue;
			Up.push_back(GunPlane(A));
		}
		// (nothing to do with no aircraft up, once the last streams are stopped: a battle of ships alone never calls it)
		if (Up.empty() && !bGunsUp) return;
		bGunsUp = !Up.empty();
		Gunners.Year = F.Year;
		Gunners.Month = F.Month;
		Gunners.Eye = Eye;
		std::vector<ShipGuns::Shot> Shots;
		Gunners.Fire(DrawT, Dt, GunHulls(), Up, Shots);
		GunShots(Shots);
	}

	void Show::GunShots(const std::vector<ShipGuns::Shot>& Shots)
	{
		for (const ShipGuns::Shot& S : Shots)
		{
			switch (S.Kind)
			{
			case ShipGuns::ShotKind::Stream:
			{
				const Hull* From = FindHull(S.Ship);
				Fx& E = Emit(FxKind::Gun, S.At, S.Kit, S.Key);
				E.Aim = S.Aim;
				E.Vel = S.Vel;
				E.Text = From ? From->Nation : std::string();
				E.Rate = S.Rate;
				E.Mv = S.Mv;
				break;
			}
			case ShipGuns::ShotKind::StreamStop:
				Emit(FxKind::GunStop, Vec(), std::string(), S.Key);
				break;
			case ShipGuns::ShotKind::Round:
			{
				Fx& E = Emit(FxKind::ShipGun, S.At, S.What, S.Ship);
				E.Aim = S.Aim;
				E.Size = S.Size;
				break;
			}
			case ShipGuns::ShotKind::Burst:
				Emit(FxKind::Flak, S.At, "heavy");
				++Check.FlakBursts;
				break;
			case ShipGuns::ShotKind::Splash:
			{
				// (a battleship's main battery thrown into the sea ahead of a torpedo plane is a column, and takes the
				// kit's room for one; a 5-inch's a shell's splash)
				const bool bHeavy = S.Size >= 200.0;
				const Vec At(S.At.X, S.At.Y, 0.0);
				if (OnHull(At, 6.0, -1)) break; // (thrown ahead of the plane, where a ship has steamed in meanwhile: not on her)
				if (bHeavy && !Columns.Take(ShownT, Flat(At - Eye))) break;
				Emit(FxKind::Splash, At, bHeavy ? "heavy" : "light").Size = S.Size;
				break;
			}
			case ShipGuns::ShotKind::Hit:
			{
				Fx& E = Emit(FxKind::PlaneHit, S.At, S.Kit, S.Key);
				E.Normal = S.Normal;
				E.Vel = S.Vel;
				E.Text = S.What;
				break;
			}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// B4 (ships under air attack, the historian's naval-battles/ship-air-defence.md): how each ship dodges. Drawn only:
	// the battle has booked the hits and misses already; this is how she fights for her life as they fall.

	void Show::Dodge(Hull& H, const Plane& A, bool bTorpedo, double Until)
	{
		if (H.Doing != Fate::Line) return;
		const ClassSize& Sz = SizeOf(H.Class);
		const bool bAlready = ShownT < H.DodgeUntil;
		if (bTorpedo)
		{
			// Bow on to the drop or stern on to it, whichever lies nearer her course (the torpedoes run along the aircraft's
			// heading): every ship combs, destroyers too. (From her course, not her bow: swung off it by a dive, the nearer
			// to her bow may be the way she came, and she would comb herself kilometres from her place.)
			if (bAlready && H.bComb) return;
			const double BowOn = Wrap(A.Hdg + Pi), SternOn = Wrap(A.Hdg);
			const bool bBow = std::fabs(Wrap(BowOn - H.CourseHdg)) <= std::fabs(Wrap(SternOn - H.CourseHdg));
			H.CombHdg = bBow ? BowOn : SternOn;
			// (but off her place across her course by more than her length, from a dodge before, and the two near abeam
			// (the other within 100 degrees of it), the one back toward her place: else comb on comb, each the same way,
			// carries her farther off by each)
			const double Other = bBow ? SternOn : BowOn;
			if (std::fabs(H.OffTrack) > Sz.Long && std::fabs(Wrap(Other - H.CourseHdg)) <= CombMost - 10.0 * Deg &&
			    Ahead(Other).Dot(RightOf(H.CourseHdg)) * H.OffTrack > 0.0)
			{
				H.CombHdg = Other;
			}
			H.bComb = true;
			H.bDodgeShown = false;
		}
		else
		{
			// A big ship's helm hard over as the dive begins, toward the side it comes from where she has the room, away
			// from it where she has not, not at all with neither; destroyers and smaller hold their course for their guns.
			if (bAlready || Sz.Long < 150.0) return;
			const double Spd = std::max(H.Spd, 1.0);
			const double Rate = std::max(1e-3, MostRateAt(Sz, Spd)), Radius = Spd / Rate;
			// (room: the first quarter of her turning circle clear of every other ship as each steams on meanwhile)
			auto Room = [&](double Way)
			{
				for (int Step = 1; Step <= 6; ++Step)
				{
					const double Turned = Step * (0.5 * Pi / 6.0), T = Turned / Rate;
					const Vec Her = H.P + Ahead(H.Hdg) * (Radius * std::sin(Turned)) + RightOf(H.Hdg) * (Way * Radius * (1.0 - std::cos(Turned)));
					for (const auto& Pair : Hulls)
					{
						const Hull& O = Pair.second;
						if (O.Key == H.Key || O.Doing == Fate::Gone) continue;
						if (std::fabs(O.P.X - H.P.X) > 2500.0 || std::fabs(O.P.Y - H.P.Y) > 2500.0) continue;
						const Vec There = O.P + Ahead(O.Hdg) * (O.Spd * T);
						if (Flat(Vec(There.X - Her.X, There.Y - Her.Y, 0.0)) < 0.5 * (Sz.Long + SizeOf(O.Class).Long) + 40.0) return false;
					}
				}
				return true;
			};
			double Way = Vec(A.P.X - H.P.X, A.P.Y - H.P.Y, 0.0).Dot(RightOf(H.Hdg)) >= 0.0 ? 1.0 : -1.0;
			if (!Room(Way)) Way = -Way;
			if (!Room(Way)) return;
			H.DodgeWay = Way;
			H.bComb = false;
			H.bDodgeShown = false;
		}
		H.DodgeUntil = std::max(bAlready ? H.DodgeUntil : ShownT, Until);
		H.DodgeJudge = H.DodgeUntil + 180.0;
		H.PutOffGap = -1.0;
		// (a column's leader leaves his ghost on the course: his line keeps it, and he comes back to it)
		auto It = Lines.find(H.Piece);
		if (It != Lines.end() && !It->second.Line.empty() && It->second.Line.front() == H.Key && !It->second.bGhost)
		{
			It->second.bGhost = true;
			It->second.GhostSince = ShownT;
			It->second.Ghost = H;
		}
	}

	int Show::OpenDodges() const
	{
		int Open = 0;
		for (const auto& Pair : Hulls) Open += Pair.second.DodgeJudge >= 0.0 && Pair.second.Doing == Fate::Line ? 1 : 0;
		return Open;
	}

	void Show::HandOver(Column& C, Hull& Old, Hull& New)
	{
		// As when a leader is lost (TakeOut): the next astern leads on from where she is, the track ahead of her (her old
		// leader's ghost's) is dropped, and the column's station goes back with the lead, easing to the battle's place as
		// it has room. The old leader falls in where she can reach, astern of every ship nearer the head than she is.
		const double SGhost = C.S + Flat(C.Ghost.P - C.Track.back().P);
		Vec OnP;
		double OnH = New.Hdg, OnK = 0.0;
		Along(C, New.SOn, OnP, OnH, &OnK);
		const double Ahead0 = std::max(0.0, SGhost - New.SOn);
		const double CourseSpd = C.Ghost.Spd;
		C.Keep += Vec(OnP.X - C.Ghost.P.X, OnP.Y - C.Ghost.P.Y, 0.0);
		if (Flat(C.Keep) > 2000.0) C.Keep = C.Keep * (2000.0 / Flat(C.Keep));
		while (C.Track.size() > 1 && C.Track.back().S > New.SOn - 1.0) C.Track.pop_back();
		C.S = New.SOn;
		C.Track.push_back({ OnP, C.S, OnH, OnK });
		for (const int Key : C.Line)
		{
			if (Hull* Other = FindHull(Key)) Other->Back = std::max(0.0, Other->Back - Ahead0);
		}
		C.Line.erase(C.Line.begin());
		// (her place: astern of each ship she lies astern of along that ship's way; along the track, astern of the ship
		// next ahead of her by as far, as a follower far off it is reckoned)
		size_t K = 1;
		while (K < C.Line.size())
		{
			const Hull* O = FindHull(C.Line[K]);
			if (O && Vec(Old.P.X - O->P.X, Old.P.Y - O->P.Y, 0.0).Dot(Ahead(O->Hdg)) > 0.0) break;
			++K;
		}
		const Hull* Mate = K == 1 ? &New : FindHull(C.Line[K - 1]);
		const double SMate = K == 1 ? C.S : (Mate ? Mate->SOn : C.S);
		const double Astern = Mate ? Vec(Old.P.X - Mate->P.X, Old.P.Y - Mate->P.Y, 0.0).Dot(Ahead(K == 1 ? OnH : Mate->Hdg)) : 0.0;
		C.Line.insert(C.Line.begin() + static_cast<long>(K), Old.Key);
		Old.SOn = SMate + std::min(-1.0, Astern);
		Old.Back = std::max(0.0, C.S - Old.SOn);
		Old.OffTrack = std::max(std::fabs(Old.OffTrack), 101.0) * (Old.OffTrack < 0.0 ? -1.0 : 1.0); // (reckoned as far off)
		// (the new leader: her own ghost on the track where she is while her own dodge is not done, else she steams it)
		if (New.DodgeJudge >= 0.0)
		{
			C.Ghost = New;
			C.Ghost.P = OnP;
			C.Ghost.Hdg = OnH;
			C.Ghost.Rate = 0.0;
			C.Ghost.Spd = CourseSpd;
			C.GhostSince = ShownT;
			C.bGhost = true;
		}
		else
		{
			C.bGhost = false;
		}
		++Check.HandedOver;
	}

	bool Show::DodgeRate(Hull& H, double Most, double& OutWish)
	{
		if (ShownT >= H.DodgeUntil) return false;
		// (she steadies where turning on would bring her nearer another ship than her clearance within half a minute: her
		// own helm, which keeps clear, has her from there)
		const ClassSize& Sz = SizeOf(H.Class);
		const Vec Mine = Ahead(H.Hdg) * std::max(H.Spd, 1.0);
		for (const auto& Pair : Hulls)
		{
			const Hull& O = Pair.second;
			if (O.Key == H.Key || O.Doing == Fate::Gone) continue;
			const Vec Rel(O.P.X - H.P.X, O.P.Y - H.P.Y, 0.0);
			if (std::fabs(Rel.X) > 1200.0 || std::fabs(Rel.Y) > 1200.0) continue;
			const Vec RelV = Ahead(O.Hdg) * O.Spd - Mine;
			const double V2 = RelV.Dot(RelV);
			const double T0 = Clamp(V2 > 1e-6 ? -Rel.Dot(RelV) / V2 : 0.0, 0.0, 30.0);
			if (Flat(Rel + RelV * T0) < 0.5 * (Sz.Long + SizeOf(O.Class).Long) + 40.0)
			{
				H.DodgeUntil = ShownT;
				return false;
			}
		}
		// (hard over no farther than MostSwing off her course: then her own helm brings her back, the dive's bombs fallen
		// by then or nearly; held on round, a battleship swings half a circle and is kilometres from her place)
		if (!H.bComb && Wrap(H.Hdg - H.CourseHdg) * H.DodgeWay >= MostSwing)
		{
			H.DodgeUntil = ShownT;
			return false;
		}
		// (and a comb her course has turned away from, past CombMost: held on, she would steam back down her own line)
		if (H.bComb && std::fabs(Wrap(H.CombHdg - H.CourseHdg)) > CombMost)
		{
			H.DodgeUntil = ShownT;
			return false;
		}
		if (!H.bDodgeShown)
		{
			H.bDodgeShown = true;
			H.bDodgeTurned = true;
			++(H.bComb ? Check.Combs : Check.Dodges);
		}
		OutWish = H.bComb ? Clamp(Wrap(H.CombHdg - H.Hdg) * 0.6, -Most, Most) : H.DodgeWay * Most;
		return true;
	}

	void Show::KeepClear(double Dt, bool bSettle)
	{
		// Each column's leader keeps clear of every other ship afloat and of every other leader's station, stepping off
		// square to his course (to the side the other lies away from, or by his number where it lies dead ahead, astern or
		// on the very spot), and back to the battle's place, slowly, once he has room. (The battle's pieces are points; it
		// may close two of one side on one spot as they steer for the same enemy. Drawn, each keeps its own water.) The
		// station is fixed on the sea, not on the course, so when the fleet turns its columns turn together.
		struct Station
		{
			int Id = 0;
			Vec W, Push;
			double Face = 0.0, Long = 0.0, Room = 1e18;
		};
		std::vector<Station> Stations;
		for (auto& Pair : Lines)
		{
			Column& C = Pair.second;
			if (C.Line.empty()) continue;
			const Piece* P = PieceNow(C.Id);
			if (!P) continue;
			Station St;
			St.Id = C.Id;
			St.W = Vec(P->X, P->Y, 0.0) + RightOf(P->Face) * C.Zig + C.Keep;
			St.Face = P->Face;
			St.Long = C.Size.Long;
			Stations.push_back(St);
		}
		for (Station& St : Stations)
		{
			const Vec Side = RightOf(St.Face);
			auto From = [&St, &Side](const Vec& There, double Clear, double Share, bool bRight)
			{
				const Vec Rel(St.W.X - There.X, St.W.Y - There.Y, 0.0);
				const double D = Flat(Rel);
				St.Room = std::min(St.Room, D - Clear);
				if (D >= Clear) return;
				const double Lat = Rel.Dot(Side);
				const double Way = std::fabs(Lat) > 0.25 * D + 1.0 ? (Lat > 0.0 ? 1.0 : -1.0) : (bRight ? 1.0 : -1.0);
				St.Push += Side * (Way * (Clear - D) * Share);
			};
			for (const Station& Other : Stations)
			{
				if (Other.Id == St.Id) continue;
				From(Other.W, 0.5 * (St.Long + Other.Long) + 200.0, 0.5, St.Id > Other.Id);
			}
			const Column& Own = Lines[St.Id];
			for (const auto& Pair : Hulls)
			{
				const Hull& H = Pair.second;
				if (H.Doing == Fate::Gone || H.Key == Own.Line.front()) continue;
				if (std::fabs(H.P.X - St.W.X) > 1500.0 || std::fabs(H.P.Y - St.W.Y) > 1500.0) continue;
				// (his own line too, where he comes back past it in a turn: only nearer than its ships' spacing)
				if (H.Piece == St.Id) From(H.P, St.Long + 60.0, 1.0, true);
				else From(H.P, 0.5 * (St.Long + SizeOf(H.Class).Long) + 120.0, 1.0, St.Id > H.Piece);
			}
		}
		for (const Station& St : Stations)
		{
			Column& C = Lines[St.Id];
			Vec Goal = C.Keep;
			// (laid out at the start, a third of his push a pass: two stations too near are pushed apart by each other's
			// station and each other's ships, three shares between them, so a third closes just the gap; the whole push
			// overshoots, and with neighbours on both sides sends stations past each other)
			if (Flat(St.Push) > 1e-6) Goal = C.Keep + St.Push * (bSettle ? 1.0 / 3.0 : 1.0);
			else if (St.Room > 120.0 && Flat(C.Keep) > 0.0) Goal = C.Keep * std::max(0.0, 1.0 - 0.6 * Dt / std::max(1.0, Flat(C.Keep)));
			const Vec Was = C.Keep;
			Vec Step = Goal - C.Keep;
			// (never on away from a leader who has already fallen 450 m behind his station: at his best speed he could
			// not close it, Samar on the rules head 41b3bc42, its battleship 501 m behind at flank; only across)
			if (!bSettle && !C.Line.empty())
			{
				const Hull* Lead = FindHull(C.Line.front());
				const Vec Steaming = C.bGhost ? C.Ghost.P : (Lead ? Lead->P : St.W);
				const double Lag = Flat(St.W - Steaming);
				if (Lag > 450.0)
				{
					const Vec Away = Vec(St.W.X - Steaming.X, St.W.Y - Steaming.Y, 0.0) * (1.0 / Lag);
					const double Out = Step.Dot(Away);
					if (Out > 0.0) Step = Step - Away * Out;
				}
			}
			const double Most = bSettle ? 60.0 : 3.0 * Dt;
			C.Keep += Flat(Step) > Most ? Step * (Most / Flat(Step)) : Step;
			if (Flat(C.Keep) > 2000.0) C.Keep = C.Keep * (2000.0 / Flat(C.Keep));
			if (bSettle)
			{
				// (at the start, each column is laid at its station whole: its track and its ships with it)
				const Vec Moved = C.Keep - Was;
				for (Wake& W : C.Track) W.P += Moved;
				for (const int Key : C.Line)
					if (Hull* H = FindHull(Key)) H->P += Moved;
			}
		}
	}

	void Show::Steam(double Dt)
	{
		for (auto& Pair : Lines)
		{
			const Piece* P = PieceNow(Pair.first);
			if (!P) continue;
			SteamColumn(Pair.second, *P, Dt);
		}
		// Ships out of the line: limping away burning, or losing way as they go down.
		for (auto& Pair : Hulls)
		{
			Hull& H = Pair.second;
			const ClassSize& Own = SizeOf(H.Class);
			const double Rudder = Own.Turn * 1.25 / 3.0;
			if (H.Doing == Fate::Limping)
			{
				// Out of the line and round for home, her helm still answering (more slowly, hurt as she is).
				const double Most = MostRateAt(Own, H.Spd) * 0.7;
				const double Rate = Clamp(Wrap(H.Away - H.Hdg) * 0.4, -Most, Most);
				H.Rate += Clamp(Rate - H.Rate, -Rudder * Dt, Rudder * Dt);
				H.Hdg = Wrap(H.Hdg + H.Rate * Dt);
				H.Spd += Clamp(H.LimpSpd - H.Spd, -0.15 * Dt, 0.15 * Dt);
				H.P += Ahead(H.Hdg) * (H.Spd * Dt);
				// L4: she burns as she goes: a fire on her deck now and then, where she is.
				if (ShownT >= H.NextFire)
				{
					H.NextFire = ShownT + Roll.U(18.0, 30.0);
					const ClassSize& Sz = SizeOf(H.Class);
					Fx& E = Emit(FxKind::Fire, PlaceOn(H, Roll.U(-0.3, 0.3), 0.0, 6.0), std::string(), H.Key);
					E.Size = Sz.Beam * 0.6;
					E.Until = 45.0;
				}
			}
			else if (H.Doing == Fate::Sinking)
			{
				H.Spd = std::max(0.0, H.Spd - 0.12 * Dt);
				const double Most = MostRateAt(Own, H.Spd);
				const double Rate = Clamp(Wrap(H.Away - H.Hdg) * 0.4, -Most, Most);
				H.Rate += Clamp(Rate - H.Rate, -Rudder * Dt, Rudder * Dt);
				H.Hdg = Wrap(H.Hdg + H.Rate * Dt);
				H.P += Ahead(H.Hdg) * (H.Spd * Dt);
				if (ShownT - H.SinkAt >= H.SinkFor) H.Doing = Fate::Gone;
			}
		}
		// Heel, pitch and heave: outward in a turn by her rate of turn and speed, and her own ride on the sea by her length
		// and the wind (the art director's table): her roll the most with the sea on her beam, her pitch the most head
		// or stern on, and she rises and falls by a share of the waves' height (about 0.021 U^2 in a sea the wind has
		// fully raised), up to the table's top, Beaufort 6, where her roll and pitch stop too (the pre-check, 22 November:
		// no top, and a gale's wind would lift her metres).
		const double Raised = std::min(WindU, 13.8);
		const double Force = std::pow(WindU / 0.836, 2.0 / 3.0), High = 0.021 * Raised * Raised;
		for (auto& Pair : Hulls)
		{
			Hull& H = Pair.second;
			if (H.Doing == Fate::Gone) continue;
			const Motion M = MotionOf(H.Class);
			const Ride R = RideOf(SizeOf(H.Class).Long);
			// (a boat, ruling 241: her keel eased to the battle's depth; the swell lets go of her as she goes down, and she
			// takes an angle down or up as she dives or surfaces, along her way through the water)
			double Sinking = 0.0;
			const double Calm = H.Class == "ss" ? Trim(H, PieceNow(H.Piece), Dt, Sinking) : 1.0;
			const double Beam = std::fabs(std::sin(H.Hdg - WindFromR));
			const double TurnHeel = -std::atan(H.Spd * H.Rate / Gee) / Deg * M.TurnHeel;
			const double Swell = AtForce(R.Roll, Force) * (0.35 + 0.65 * Beam) * Swells(ShownT, H.RollT, H.Roll0) * Calm;
			const double Want = TurnHeel + Swell;
			H.Heel += (Want - H.Heel) * Clamp(Dt / 2.0, 0.0, 1.0);
			H.Pitch = AtForce(R.Pitch, Force) * (0.35 + 0.65 * std::sqrt(std::max(0.0, 1.0 - Beam * Beam))) * Swells(ShownT, H.PitchT, H.Roll0 * 1.7 + 0.5) * Calm;
			H.Heave = R.Heave * High * Swells(ShownT, H.PitchT * 1.05, H.Roll0 * 2.3 + 1.3) * Calm;
			if (Sinking != 0.0) H.Pitch += Clamp(-std::atan(Sinking / std::max(1.5, H.Spd)) / Deg, -12.0, 12.0);
			if (H.Doing != Fate::Sinking && std::fabs(H.Heel) > Check.MostHeel)
			{
				Check.MostHeel = std::fabs(H.Heel);
				Check.HeelWorst = Worst(H, ShownT);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Gunnery: each ship's turrets on the ship she fires at, salvos on her class's cadence, their fall.

	void Show::Gunnery(double Dt)
	{
		for (auto& Pair : Lines)
		{
			const Piece* P = PieceNow(Pair.first);
			if (!P) continue;
			const Piece* Foe = P->Engaged >= 0 ? PieceNow(P->Engaged) : nullptr;
			for (const int Key : Pair.second.Line)
			{
				Hull* H = FindHull(Key);
				if (!H) continue;
				H->bFiring = false;
				const double Train = H->Class == "bb" ? 3.0 : H->Class == "ca" ? 6.0 : 12.0; // degrees a second
				Hull* On = nullptr;
				if (Foe && H->Class != "cv")
				{
					On = H->AimKey >= 0 ? FindHull(H->AimKey) : nullptr;
					if (!On || On->Piece != Foe->Id || On->Doing != Fate::Line || std::fmod(ShownT, 5.0) < Dt) On = NearestOf(Foe->Id, H->P.X, H->P.Y);
					// (a boat dived is no gun target: her hunters' weapon is the depth charge)
					if (On && On->Class == "ss" && On->Keel > SurfacedKeel(On->Class) + 1.0) On = nullptr;
				}
				if (!On)
				{
					// Nothing to fire on: turrets back fore and aft.
					H->AimKey = -1;
					H->NextSalvo = -1.0;
					H->Train += Clamp(-H->Train, -Train * 0.3 * Dt, Train * 0.3 * Dt);
					H->Elev += Clamp(-H->Elev, -2.0 * Dt, 2.0 * Dt);
					continue;
				}
				H->AimKey = On->Key;
				const double Range = Flat(On->P - H->P);
				const double Rel = Wrap(Bearing(H->P, On->P) - H->Hdg) / Deg;
				H->Train += Clamp(Rel - H->Train, -Train * Dt, Train * Dt);
				// Elevation for the range: a gun's flat-fire angle (about 800 m/s at the muzzle), raised a little for drag.
				const double Mv = H->Class == "dd" ? 760.0 : 820.0;
				const double WantElev = 0.5 * std::asin(Clamp(Gee * Range / (Mv * Mv), 0.0, 1.0)) / Deg * 1.6;
				H->Elev += Clamp(WantElev - H->Elev, -3.0 * Dt, 3.0 * Dt);
				if (H->NextSalvo < 0.0) H->NextSalvo = ShownT + Roll.U(1.5, Cadence(H->Class));
				if (ShownT < H->NextSalvo || std::fabs(Rel - H->Train) > 6.0) continue;
				H->NextSalvo = ShownT + Cadence(H->Class);
				H->bFiring = true;
				H->FiredAt = ShownT;
				const double Cal = Calibre(H->Class, H->Nation);
				Shells S;
				S.From = H->Key;
				S.Piece = H->Piece;
				S.OnPiece = On->Piece;
				S.Calibre = Cal;
				S.Class = H->Class;
				S.Count = H->Class == "bb" ? 3 : H->Class == "ca" ? 3 : 2;
				S.Fired = H->P;
				// Time of flight: about 650 m/s over the shell's arc (a battleship's at 5 km about 8 s).
				S.Lands = ShownT + Range / (H->Class == "dd" ? 560.0 : 650.0) + 0.5;
				Flying.push_back(S);
				const ClassSize& Sz = SizeOf(H->Class);
				Fx& E = Emit(FxKind::Salvo, PlaceOn(*H, 0.12, 0.0, Sz.Beam * 0.45), H->Class, H->Key);
				E.Aim = On->P;
				E.Size = Cal;
				E.Side = H->Side;
				++Check.Salvos;
			}
		}
	}

	void Show::LandShells()
	{
		for (size_t K = 0; K < Flying.size();)
		{
			Shells& S = Flying[K];
			if (S.Lands > ShownT) { ++K; continue; }
			const Shells Fell = S;
			Flying.erase(Flying.begin() + static_cast<long>(K));
			Hull* On = NearestOf(Fell.OnPiece, Fell.Fired.X, Fell.Fired.Y);
			if (!On) continue;
			const std::string Weapon = Fell.Calibre >= 300.0 ? "shell_heavy" : Fell.Calibre >= 180.0 ? "shell_medium" : "shell_light";
			const std::string Splash = Fell.Calibre >= 300.0 ? "heavy" : Fell.Calibre >= 180.0 ? "medium" : "light";
			const double Range = Flat(On->P - Fell.Fired);
			const Vec Down = (On->P - Fell.Fired) * (1.0 / std::max(1.0, Range));
			const Vec Across(Down.Y, -Down.X, 0.0);
			// The salvo's middle: over or short of her, a little to one side (a straddle now and then).
			const Vec Middle = On->P + Down * (Roll.Bell() * (0.012 * Range + 40.0)) + Across * (Roll.Bell() * (0.003 * Range + 10.0));
			Owed& D = Debt[Fell.OnPiece];
			int Hits = 0;
			if (D.Hits >= 0.5 && On->Doing == Fate::Line)
			{
				Hits = Fell.Class == "bb" && D.Hits >= 1.5 ? 2 : 1;
				D.Hits -= Hits;
			}
			const ClassSize& Sz = SizeOf(On->Class);
			for (int H = 0; H < Hits; ++H)
			{
				Fx& E = Emit(FxKind::ShellHit, PlaceOn(*On, Roll.U(-0.35, 0.35), Roll.U(-0.3, 0.3) * Sz.Beam, Roll.U(4.0, 10.0)), Weapon, On->Key);
				E.Size = Fell.Calibre;
				E.Side = On->Side;
				On->LastBlow = Weapon;
				++Check.ShellHits;
			}
			for (int C = Hits; C < Fell.Count; ++C)
			{
				const Vec At = Middle + Down * (Roll.Bell() * (0.006 * Range + 25.0)) + Across * (Roll.Bell() * (0.002 * Range + 8.0));
				// (never inside her hull: a shell that would fall there falls alongside)
				Vec Fall = At;
				const Vec Off = Fall - On->P;
				const double AlongOff = Off.Dot(Ahead(On->Hdg)), AcrossOff = Off.Dot(RightOf(On->Hdg));
				if (std::fabs(AlongOff) < Sz.Long * 0.55 && std::fabs(AcrossOff) < Sz.Beam * 0.5 + 6.0)
					Fall = Fall + RightOf(On->Hdg) * ((AcrossOff >= 0.0 ? 1.0 : -1.0) * (Sz.Beam * 0.5 + 12.0) - AcrossOff);
				Fall.Z = 0.0;
				// (nor on another ship near her: a shell there would be a hit on her, which the battle did not say)
				if (OnHull(Fall, 4.0, -1)) continue;
				if (!Columns.Take(ShownT, Flat(Fall - Eye))) continue;
				Fx& E = Emit(FxKind::Splash, Fall, Splash);
				E.Size = Fell.Calibre;
				++Check.Splashes;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Torpedoes and aircraft.

	void Show::RunTorpedoes(double Dt)
	{
		for (Torp& T : Torps)
		{
			if (!T.bRunning) continue;
			T.P += T.Way * (T.Speed * Dt);
			if (ShownT >= T.Ends)
			{
				T.bRunning = false;
				Emit(FxKind::TrackStop, T.P, std::string(), T.Key);
				continue;
			}
			Emit(FxKind::TrackMove, T.P, std::string(), T.Key).Vel = T.Way * T.Speed;
		}
		Torps.erase(std::remove_if(Torps.begin(), Torps.end(), [](const Torp& T) { return !T.bRunning; }), Torps.end());
	}

	void Show::FlyStrikes(double Dt)
	{
		for (Strike& S : Strikes)
		{
			const Piece* Target = PieceNow(S.Target);
			// (the ship of the target piece its aircraft go for, kept while she is in her line: one that turns away from
			// them is still theirs)
			Hull* On = Target ? FindHull(S.OnKey) : nullptr;
			if (Target && (!On || On->Piece != S.Target || On->Doing != Fate::Line))
			{
				On = NearestOf(S.Target, Target->X, Target->Y);
				S.OnKey = On ? On->Key : -1;
			}
			// B4: as the dive's leader tips over, about half a minute before the first bomb falls, the big ship it is aimed
			// at puts her helm hard over, and keeps turning through the whole group's dives.
			if (On && !S.bDodged && ShownT >= S.Lands - 32.0)
			{
				for (const int Key : S.Planes)
				{
					auto It = Air.find(Key);
					if (It == Air.end() || It->second.Role != "dive" || It->second.bFalling) continue;
					if (It->second.Doing != Leg::Out && It->second.Doing != Leg::Attack) continue;
					S.bDodged = true;
					Dodge(*On, It->second, false, S.Lands + static_cast<double>(S.Planes.size()) * 0.7 + 20.0);
					break;
				}
			}
			for (const int Key : S.Planes)
			{
				auto It = Air.find(Key);
				if (It == Air.end()) continue;
				Plane& A = It->second;
				if (A.Doing == Leg::Gone) continue;
				A.LegT += Dt;
				Vec Goal = A.P;
				double GoalAlt = A.P.Z, GoalSpd = 70.0, TurnMost = 12.0 * Deg;
				const Hull* Home = FindHull(A.Carrier);
				if (A.Lost >= 0.0 && ShownT >= A.Lost && !A.bFalling)
				{
					// Hit: down burning into the sea.
					A.bFalling = true;
					A.Fall = Ahead(A.Hdg) * A.Spd + Vec(0.0, 0.0, -10.0);
					Fx& E = Emit(FxKind::PlaneDown, A.P, A.Model, A.Key);
					E.Vel = A.Fall;
					E.Side = A.Side;
					++Check.PlanesDown;
					// (L9: under a ship's guns, her streams on it and the rounds seen striking home as it goes, or a burst
					// laid on it; past them, it falls as it did)
					std::vector<ShipGuns::Shot> Shots;
					if (Gunners.Kill(DrawT, GunPlane(A), GunHulls(), Shots)) GunShots(Shots);
				}
				if (A.bFalling)
				{
					A.Fall.Z -= Gee * 0.6 * Dt;
					A.P += A.Fall * Dt;
					A.Pitch = std::atan2(A.Fall.Z, Flat(A.Fall)) / Deg;
					A.Bank += 40.0 * Dt * A.Hand;
					if (A.P.Z <= 0.0)
					{
						A.P.Z = 0.0;
						A.Doing = Leg::Gone;
						Emit(FxKind::Ditch, A.P, A.Model, A.Key);
					}
					continue;
				}
				if (A.Doing == Leg::Deck)
				{
					if (ShownT < A.Go)
					{
						A.bShown = false;
						if (Home) { A.P = PlaceOn(*Home, -0.3, 0.0, 18.0); A.Hdg = Home->Hdg; A.Spd = Home->Spd; }
						continue;
					}
					if (!A.bShown && Home) { Fx& E = Emit(FxKind::Launch, A.P, A.Model, Home->Key); E.Side = A.Side; ++Check.Launches; }
					A.bShown = true;
					// Down the deck and off the bow, gathering speed, then climbing away.
					A.Spd = std::min(60.0, A.Spd + 9.0 * Dt);
					A.P += Ahead(A.Hdg) * (A.Spd * Dt);
					A.Climb = A.Spd >= 45.0 ? 6.0 : 0.0;
					A.P.Z += A.Climb * Dt;
					if (A.Spd >= 60.0 && A.P.Z > 60.0) { A.Doing = Leg::Form; A.LegT = 0.0; }
					continue;
				}
				const bool bDive = A.Role == "dive";
				const Vec Over = On ? On->P : Target ? Vec(Target->X, Target->Y, 0.0) : A.P;
				// (the dive bombers' moment eight seconds after the strike's: they come down fast from their point, and so let
				// go just after the battle has said which of them hit, not before it, when every one would be a miss)
				const double Attack = S.Lands + A.Slot * 0.7 + (bDive ? 8.0 : 0.0);
				if (A.Doing == Leg::Form)
				{
					// Circling up over the carrier until the deck has flown them all off, then away together.
					const Vec C = Home ? Home->P : A.P;
					Goal = C + Ahead(ShownT * 0.06 + A.Slot) * 900.0;
					GoalAlt = 500.0 + A.Slot * 30.0;
					const double Off = S.Launched + 3.0 + static_cast<double>(S.Planes.size()) * 4.0 + 6.0;
					if (ShownT >= Off) { A.Doing = Leg::Out; A.LegT = 0.0; }
				}
				else if (A.Doing == Leg::Out)
				{
					// Out to the target, timed to arrive as the strike is due: dive bombers high, to tip over about 1.3 km short
					// of her and half a minute before they let go; torpedo bombers low.
					const double Left = std::max(1.0, Attack - (bDive ? 27.0 : 12.0) - ShownT);
					const Vec Start = Over + Ahead(Bearing(Over, A.P)) * (bDive ? 1300.0 : 4500.0);
					Goal = Start;
					GoalAlt = bDive ? 3200.0 : 120.0;
					GoalSpd = Clamp(Flat(Start - A.P) / Left, 60.0, 150.0);
					if (Flat(Start - A.P) < 300.0 || ShownT >= Attack - (bDive ? 27.0 : 12.0)) { A.Doing = Leg::Attack; A.LegT = 0.0; }
				}
				else if (A.Doing == Leg::Attack)
				{
					if (bDive)
					{
						// Pushed over into a steep dive on her (about seventy degrees: little way over the sea, falling fast),
						// released low over her, pulled out over the sea beyond. One that has not the height for it (a strike
						// flown off close by, with no time to climb) runs in low and fast, and lets go over her.
						Goal = Over;
						GoalAlt = S.bFromCoast ? 300.0 : 450.0;
						GoalSpd = A.P.Z > GoalAlt + 300.0 ? 45.0 : 100.0;
						TurnMost = 20.0 * Deg;
					}
					else
					{
						Goal = Over;
						GoalAlt = 35.0;
						GoalSpd = 80.0;
					}
					const double Near = Flat(Over - A.P);
					if ((bDive && A.P.Z < 900.0 && (Near < 250.0 || (A.P.Z <= GoalAlt + 60.0 && Near < 600.0))) || (!bDive && Near < 900.0) || ShownT > Attack + 25.0)
					{
						// Released: a hit on her or a near miss (the battle's share, decided as the strike landed).
						if (On)
						{
							const ClassSize& Sz = SizeOf(On->Class);
							if (A.bHit)
							{
								const bool bTorp = !bDive;
								const Vec At = bTorp ? PlaceOn(*On, Roll.U(-0.3, 0.3), (Roll.U() < 0.5 ? -1.0 : 1.0) * (Sz.Beam * 0.5 + 2.0), 0.0)
									: PlaceOn(*On, Roll.U(-0.35, 0.35), Roll.U(-0.3, 0.3) * Sz.Beam, Roll.U(6.0, 12.0));
								Fx& E = Emit(FxKind::BombHit, At, bTorp ? "torpedo" : S.bFromCoast ? "level" : "dive", On->Key);
								E.Side = On->Side;
								On->LastBlow = bTorp ? "torpedo" : "dive";
								++Check.BombHits;
								// B5.4: beside a dive bomber's hit, its two small wing bombs close aboard (Repulse took one hit
								// and seven near misses): columns within about 50 m of her side.
								for (int Wing = 0; Wing < 2 && !bTorp; ++Wing)
								{
									const Vec Aboard = PlaceOn(*On, Roll.U(-0.45, 0.45), (Roll.U() < 0.5 ? -1.0 : 1.0) * Roll.U(Sz.Beam * 0.5 + 8.0, Sz.Beam * 0.5 + 45.0), 0.0);
									if (!OnHull(Aboard, 6.0, -1) && Columns.Take(ShownT, Flat(Aboard - Eye)))
									{
										Emit(FxKind::NearMiss, Vec(Aboard.X, Aboard.Y, 0.0), "bomb60");
										++Check.NearMisses;
										++Check.WingBombs;
									}
								}
							}
							else
							{
								// (B5.5: a dive bomb's miss a tall column close aboard, most within 50 m of her side, a few wider)
								const double Wide = bDive ? (Roll.U() < 0.75 ? Sz.Beam * 0.5 + 50.0 : 120.0) : 90.0;
								const Vec At = PlaceOn(*On, Roll.U(-0.8, 0.8), (Roll.U() < 0.5 ? -1.0 : 1.0) * Roll.U(Sz.Beam * 0.5 + 10.0, Wide), 0.0);
								if (!OnHull(At, 6.0, -1) && Columns.Take(ShownT, Flat(At - Eye)))
								{
									Emit(FxKind::NearMiss, Vec(At.X, At.Y, 0.0), bDive ? "bomb250" : "torpedo");
									++Check.NearMisses;
								}
							}
							// B4: every ship combs a torpedo plane's drop, bow or stern on to it, the torpedoes' run long.
							if (!bDive) Dodge(*On, A, true, ShownT + 45.0);
						}
						A.Doing = Leg::Home;
						A.LegT = 0.0;
					}
				}
				else if (A.Doing == Leg::Home)
				{
					if (S.bFromCoast || !Home || Home->Doing == Fate::Gone || Home->Doing == Fate::Sinking)
					{
						// Away to its own coast (or, its carrier lost, out of the picture to ditch by its own fleet).
						const double Away = S.Side == 0 ? Pi : 0.0;
						Goal = A.P + Ahead(Away) * 3000.0;
						GoalAlt = 1500.0;
						GoalSpd = 90.0;
						if (A.LegT > 70.0) { A.Doing = Leg::Gone; A.bShown = false; }
					}
					else
					{
						// Back to the carrier: round to a point astern of her, then in on her wake down to her deck, a little
						// faster than she steams.
						const Vec Astern = Home->P - Ahead(Home->Hdg) * 1500.0;
						if (!A.bFinal && Flat(A.P - Astern) < 400.0) A.bFinal = true;
						Goal = A.bFinal ? Home->P : Astern;
						GoalAlt = A.bFinal ? 20.0 : 300.0;
						GoalSpd = A.bFinal ? Home->Spd + 25.0 : 85.0;
						if (Flat(A.P - Home->P) < 120.0 && A.P.Z < 60.0)
						{
							Fx& E = Emit(FxKind::LandOn, A.P, A.Model, Home->Key);
							E.Side = A.Side;
							++Check.Landings;
							A.Doing = Leg::Gone;
							A.bShown = false;
						}
					}
				}
				// Flown as an aircraft flies: its nose round toward the goal no faster than it can turn, banked into the
				// turn, climbing or diving toward the goal's height, speed eased.
				const double Want = Flat(Goal - A.P) > 1.0 ? Bearing(A.P, Goal) : A.Hdg;
				const double Turn = Clamp(Wrap(Want - A.Hdg), -TurnMost * Dt, TurnMost * Dt);
				A.Hdg = Wrap(A.Hdg + Turn);
				A.Spd += Clamp(GoalSpd - A.Spd, -8.0 * Dt, 8.0 * Dt);
				const bool bDiving = A.Doing == Leg::Attack && bDive;
				// (pushed over into a climb or a dive, and pulled out of it, at no more than two g)
				const double Climb = Clamp((GoalAlt - A.P.Z) * 0.3, bDiving ? -100.0 : -20.0, 15.0);
				A.Climb += Clamp(Climb - A.Climb, -2.0 * Gee * Dt, 2.0 * Gee * Dt);
				A.P += Ahead(A.Hdg) * (A.Spd * Dt) + Vec(0.0, 0.0, A.Climb * Dt);
				if (A.P.Z < 15.0) { A.P.Z = 15.0; A.Climb = std::max(0.0, A.Climb); }
				A.Pitch = std::atan2(A.Climb, std::max(1.0, A.Spd)) / Deg;
				const double Rate = Dt > 0.0 ? Turn / Dt : 0.0;
				A.Bank += (std::atan(A.Spd * Rate / Gee) / Deg - A.Bank) * Clamp(Dt * 2.0, 0.0, 1.0);
			}
		}
		int Up = 0;
		for (const auto& Pair : Air) if (Pair.second.Doing != Leg::Gone && Pair.second.bShown) ++Up;
		Check.MostPlanes = std::max(Check.MostPlanes, Up);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// L2: the fighters a carrier holds over the fleet (B1's cover), circling high over her; those no longer held land on.

	void Show::CoverFleet(double Dt)
	{
		std::map<int, int> Want, Have;
		for (const auto& Pair : Now)
		{
			const Piece& P = Pair.second;
			if (P.Class != "cv" || !P.bLooks || P.Cover <= 0.0 || P.State == "gone") continue;
			Want[P.Id] = static_cast<int>(std::lround(Clamp(P.Cover, 0.0, 1.0) * 4.0));
		}
		for (auto& Pair : Air)
		{
			Plane& A = Pair.second;
			if (A.Over < 0 || A.Doing == Leg::Gone) continue;
			int& Count = Have[A.Over];
			if (A.Doing == Leg::Cover && Count >= Want[A.Over]) { A.Doing = Leg::Home; A.LegT = 0.0; }
			else if (A.Doing == Leg::Cover) ++Count;
		}
		for (const auto& Pair : Want)
		{
			auto It = Lines.find(Pair.first);
			const Hull* Deck = It != Lines.end() && !It->second.Line.empty() ? FindHull(It->second.Line.front()) : nullptr;
			if (!Deck) continue;
			for (int K = Have[Pair.first]; K < Pair.second && static_cast<int>(Air.size()) < 80; ++K)
			{
				Plane A;
				A.Key = NextKey++;
				A.Side = Deck->Side;
				A.Nation = Deck->Nation;
				A.Role = "cover";
				A.Model = A.Nation == "USA" ? (F.Year >= 1943 ? "Hellcat" : "Wildcat") : A.Nation == "JAP" ? "Zero" : A.Nation == "UK" ? "Seafire" : "Fighter:" + A.Nation;
				A.Over = Pair.first;
				A.Carrier = Deck->Key;
				A.Slot = K;
				A.Hand = K % 2 ? 1.0 : -1.0;
				A.Doing = Leg::Cover;
				A.P = Deck->P + Ahead(K * 1.6) * 1500.0 + Vec(0.0, 0.0, 1800.0 + K * 150.0);
				A.Hdg = Wrap(K * 1.6 + Pi * 0.5);
				A.Spd = 85.0;
				A.bShown = true;
				Air[A.Key] = A;
			}
		}
		int Up = 0;
		for (auto& Pair : Air)
		{
			Plane& A = Pair.second;
			if (A.Over < 0 || A.Doing == Leg::Gone) continue;
			const Hull* Deck = FindHull(A.Carrier);
			Vec Goal;
			double GoalAlt = 1800.0 + A.Slot * 150.0, GoalSpd = 85.0;
			if (A.Doing == Leg::Cover)
			{
				++Up;
				const Vec C = Deck ? Deck->P : A.P;
				Goal = C + Ahead(ShownT * 0.05 * A.Hand + A.Slot * 1.6) * 1500.0;
			}
			else
			{
				if (!Deck || Deck->Doing != Fate::Line) { A.Doing = Leg::Gone; A.bShown = false; continue; }
				const Vec Astern = Deck->P - Ahead(Deck->Hdg) * 1500.0;
				const bool bFinal = Flat(A.P - Astern) < 400.0 || A.LegT > 60.0;
				Goal = bFinal ? Deck->P : Astern;
				GoalAlt = bFinal ? 20.0 : 300.0;
				GoalSpd = bFinal ? 45.0 : 85.0;
				A.LegT += Dt;
				if (Flat(A.P - Deck->P) < 120.0 && A.P.Z < 60.0)
				{
					Fx& E = Emit(FxKind::LandOn, A.P, A.Model, Deck->Key);
					E.Side = A.Side;
					++Check.Landings;
					A.Doing = Leg::Gone;
					A.bShown = false;
					continue;
				}
			}
			const double Want0 = Bearing(A.P, Goal);
			const double Turn = Clamp(Wrap(Want0 - A.Hdg), -10.0 * Deg * Dt, 10.0 * Deg * Dt);
			A.Hdg = Wrap(A.Hdg + Turn);
			A.Spd += Clamp(GoalSpd - A.Spd, -8.0 * Dt, 8.0 * Dt);
			const double Climb = Clamp((GoalAlt - A.P.Z) * 0.3, -20.0, 15.0);
			A.Climb += Clamp(Climb - A.Climb, -2.0 * Gee * Dt, 2.0 * Gee * Dt);
			A.P += Ahead(A.Hdg) * (A.Spd * Dt) + Vec(0.0, 0.0, A.Climb * Dt);
			A.Pitch = std::atan2(A.Climb, std::max(1.0, A.Spd)) / Deg;
			const double Rate = Dt > 0.0 ? Turn / Dt : 0.0;
			A.Bank += (std::atan(A.Spd * Rate / Gee) / Deg - A.Bank) * Clamp(Dt * 2.0, 0.0, 1.0);
		}
		Check.CoverUp = std::max(Check.CoverUp, Up);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// L5: smoke screens.

	void Show::LayScreens()
	{
		// Who would lay smoke now: destroyers turning away from their run (set as they fire), light ships breaking off,
		// and the destroyers of a carrier's screen while enemy guns are on her.
		std::set<int> GunsOnCarrier;
		for (const auto& Pair : Now)
		{
			const Piece& P = Pair.second;
			if (P.Engaged < 0) continue;
			const Piece* On = PieceNow(P.Engaged);
			if (On && On->Class == "cv") GunsOnCarrier.insert(On->Id);
		}
		std::vector<std::pair<double, int>> Want;
		for (auto& Pair : Lines)
		{
			const Piece* P = PieceNow(Pair.first);
			if (!P) continue;
			const bool bLight = P->Class == "dd" || P->Class == "ca";
			bool bCover = false;
			if (P->Class == "dd")
			{
				for (const int Cv : GunsOnCarrier)
				{
					const Piece* C = PieceNow(Cv);
					if (C && C->Side == P->Side && Flat(Vec(C->X - P->X, C->Y - P->Y, 0.0)) < 4000.0) { bCover = true; break; }
				}
			}
			for (const int Key : Pair.second.Line)
			{
				Hull* H = FindHull(Key);
				if (!H) continue;
				if (P->bLooks)
				{
					// The battle says who lays smoke (B1's smoke): only that.
					if (P->bSmoke && bLight) H->ScreenUntil = std::max(H->ScreenUntil, ShownT + 1.0);
					else if (!P->bSmoke && H->ScreenUntil > ShownT + 1.0) H->ScreenUntil = ShownT;
				}
				else if ((bLight && P->State == "routing") || bCover) H->ScreenUntil = std::max(H->ScreenUntil, ShownT + 5.0);
				if (H->ScreenUntil > ShownT) Want.push_back({ Flat(H->P - Eye), H->Key });
			}
		}
		std::sort(Want.begin(), Want.end());
		std::set<int> Laying;
		for (size_t K = 0; K < Want.size() && K < 8; ++K) Laying.insert(Want[K].second);
		int Count = 0;
		for (auto& Pair : Hulls)
		{
			Hull& H = Pair.second;
			const bool bOn = Laying.count(H.Key) && (H.Doing == Fate::Line || H.Doing == Fate::Limping);
			const Vec Funnel = PlaceOn(H, 0.02, 0.0, SizeOf(H.Class).Beam * 0.9);
			if (bOn && !H.bScreen)
			{
				Fx& E = Emit(FxKind::ScreenStart, Funnel, H.Class, H.Key);
				E.Vel = Ahead(H.Hdg) * H.Spd;
				++Check.Screens;
			}
			else if (bOn) Emit(FxKind::ScreenMove, Funnel, H.Class, H.Key).Vel = Ahead(H.Hdg) * H.Spd;
			else if (H.bScreen) Emit(FxKind::ScreenStop, Funnel, H.Class, H.Key);
			H.bScreen = bOn;
			if (bOn) ++Count;
		}
		Check.MostScreens = std::max(Check.MostScreens, Count);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The clock.

	void Show::Advance(double T)
	{
		if (Views.empty()) return;
		T = std::min(T, Latest());
		if (T < DrawT) T = DrawT;
		const double Was = DrawT;
		DrawT = T;
		// The ships are steamed in steps of a fixed length whatever the frame rate, so the same battle is the same show on
		// any machine (and in the cloud test); what is drawn is carried on from the last step to the moment shown.
		while (ShownT + Fixed <= T + 1e-9)
		{
			for (auto& Pair : Hulls) { Pair.second.LastP = Pair.second.P; Pair.second.bHasLast = Pair.second.Doing != Fate::Gone; Pair.second.LastRate = Pair.second.Rate; }
			ShownT += Fixed;
			Interpolate();
			StartColumns();
			while (!Queue.empty() && Queue.front().T <= ShownT + 1e-9)
			{
				const Event E = Queue.front();
				Queue.pop_front();
				Play(E);
			}
			KeepClear(Fixed);
			Steam(Fixed);
			Gunnery(Fixed);
			LandShells();
			RunTorpedoes(Fixed);
			BurstCharges();
			FlyStrikes(Fixed);
			CoverFleet(Fixed);
			LayScreens();
			// The seamanship's checks over the step: no pop, no slide, no turn or rudder past the class's, for ships in a line.
			for (auto& Pair : Lines)
			{
				const ClassSize& Sz = Pair.second.Size;
				for (const int Key : Pair.second.Line)
				{
					Hull* H = FindHull(Key);
					if (!H || !H->bHasLast) continue;
					Check.MostJump = std::max(Check.MostJump, Flat(H->P - H->LastP) - (Sz.Speed * 1.5 + 6.0) * Fixed);
					if (Flat(H->P - H->LastP) > 0.2)
					{
						const double Way = Bearing(H->LastP, H->P);
						// (the bow against the way she made over the step, half a step's turn allowed for the chord)
						Check.MostSlip = std::max(Check.MostSlip, std::max(0.0, std::fabs(Wrap(Way - H->Hdg)) - std::fabs(H->Rate) * Fixed * 0.5) / Deg);
					}
					// (her limit at her speed: her turning circle the same at any speed, her rudder answering as fast)
					const double Pace = Clamp(H->Spd / std::max(1.0, Sz.Speed), 0.15, 1.45);
					const double Turn = std::fabs(H->Rate) / (Sz.Turn * 1.25 * Pace);
					if (Turn > Check.MostTurn) { Check.MostTurn = Turn; Check.TurnWorst = Worst(*H, ShownT); }
					const double Rudder = std::fabs(H->Rate - H->LastRate) / Fixed / (Sz.Turn * 1.25 / 3.0 * std::max(1.0, Pace * Pace));
					if (Rudder > Check.MostRudder) { Check.MostRudder = Rudder; Check.RudderWorst = Worst(*H, ShownT); }
				}
			}
		}
		// At the battle's last word, what is due is told though no step is left to take.
		if (T >= Latest() - 1e-9)
		{
			while (!Queue.empty() && Queue.front().T <= T + 1e-9)
			{
				const Event E = Queue.front();
				Queue.pop_front();
				Play(E);
			}
		}
		// (the guns laid twenty times a second, not every frame: the kit draws each stream's rounds between, from where its
		// muzzle was as each fired, and they cost a third as much)
		GunDue += T - Was;
		if (GunDue >= GunStep - 1e-9)
		{
			FireGuns(GunDue);
			GunDue = 0.0;
		}
		Output();
		double Left = 0.0;
		Check.OwedBy.clear();
		for (const auto& Pair : Debt)
		{
			Left += std::max(0.0, Pair.second.Hits);
			if (Pair.second.Hits >= 0.5) Check.OwedBy[Pair.first] = Pair.second.Hits;
		}
		Check.OwedLeft = Left;
	}

	void Show::Output()
	{
		const double On = std::max(0.0, DrawT - ShownT);
		OutShips.clear();
		for (const auto& Pair : Hulls)
		{
			const Hull& H = Pair.second;
			if (H.Doing == Fate::Gone) continue;
			if (H.Doing == Fate::Limping && (H.P.X < -3000.0 || H.P.Y < -3000.0 || H.P.X > F.Size + 3000.0 || H.P.Y > F.Size + 3000.0)) continue;
			const ClassSize& Sz = SizeOf(H.Class);
			ShipOut S;
			S.Key = H.Key;
			S.Piece = H.Piece;
			S.Side = H.Side;
			S.Slot = H.Slot;
			S.Class = H.Class;
			S.Nation = H.Nation;
			S.At = H.P + Ahead(H.Hdg) * (H.Spd * On);
			S.Heading = Wrap(H.Hdg + H.Rate * On) / Deg;
			S.Heel = H.Heel;
			S.Pitch = H.Pitch;
			S.Heave = H.Heave;
			S.Speed = H.Spd;
			S.Wake = Clamp(H.Spd / (Sz.Speed * 1.3), 0.0, 1.0);
			S.Funnel = H.Doing == Fate::Sinking ? 0.0 : Clamp(0.25 + 0.75 * H.Spd / (Sz.Speed * 1.3), 0.0, 1.0);
			S.Long = Sz.Long;
			S.Beam = Sz.Beam;
			S.Doing = H.Doing;
			S.How = H.How;
			S.DownFor = H.SinkFor;
			S.Down = H.Doing == Fate::Sinking ? Clamp((DrawT - H.SinkAt) / std::max(1.0, H.SinkFor), 0.0, 1.0) : 0.0;
			S.Train = H.Train;
			S.Elevation = H.Elev;
			S.Aim = H.AimKey;
			S.bFiring = H.bFiring;
			S.bBurning = H.Doing == Fate::Limping;
			S.bScreen = H.bScreen;
			if (H.Doing == Fate::Line)
			{
				if (const Piece* P = PieceNow(H.Piece)) S.LineAt = P->LineAt;
			}
			if (H.Class == "ss")
			{
				S.Dive = H.Dive.empty() ? "surfaced" : H.Dive;
				S.Keel = std::max(0.0, H.Keel);
				S.Dived = std::max(0.0, H.Keel - SurfacedKeel(H.Class));
				// (her periscope can be up only near periscope depth)
				S.bScope = H.bScope && H.Keel <= F.ScopeKeel + 2.0;
				// (under, she leaves no wake on the sea but her periscope's feather, the scene's own, and her diesels are stopped)
				if (S.Dived > 1.0)
				{
					S.Wake = 0.0;
					S.Funnel = 0.0;
				}
			}
			OutShips.push_back(S);
		}
		OutPlanes.clear();
		for (const auto& Pair : Air)
		{
			const Plane& A = Pair.second;
			if (A.Doing == Leg::Gone || !A.bShown) continue;
			PlaneOut O;
			O.Key = A.Key;
			O.Side = A.Side;
			O.Model = A.Model;
			O.Nation = A.Nation;
			O.Role = A.Role;
			O.At = A.bFalling ? A.P + A.Fall * On : A.P + Ahead(A.Hdg) * (A.Spd * On) + Vec(0.0, 0.0, A.Climb * On);
			O.Heading = A.Hdg / Deg;
			O.Pitch = A.Pitch;
			O.Bank = A.Bank;
			O.Speed = A.Spd;
			O.bFalling = A.bFalling;
			OutPlanes.push_back(O);
		}
		OutTorps.clear();
		for (const Torp& T : Torps)
		{
			TorpOut O;
			O.Key = T.Key;
			O.Side = T.Side;
			O.At = T.P + T.Way * (T.Speed * On);
			O.From = T.From;
			O.Heading = Bearing(Vec(), T.Way);
			O.Speed = T.Speed;
			OutTorps.push_back(O);
		}
	}

	std::string Show::Describe() const
	{
		std::string Out;
		char Line[256];
		for (const ShipOut& S : OutShips)
		{
			const char* Doing = S.Doing == Fate::Line ? "line" : S.Doing == Fate::Limping ? "limping" : S.Doing == Fate::Sinking ? "sinking" : "gone";
			std::snprintf(Line, sizeof(Line), "%5d %s %s p%d.%d %s at %.0f,%.0f hdg %.0f %.1f m/s heel %.1f train %.0f%s%s%s\n", S.Key, S.Side ? "def" : "att", S.Class.c_str(), S.Piece, S.Slot, Doing,
				S.At.X, S.At.Y, S.Heading, S.Speed, S.Heel, S.Train, S.bFiring ? " FIRE" : "", S.bScreen ? " smoke" : "", S.Doing == Fate::Sinking ? (" " + S.How).c_str() : "");
			Out += Line;
		}
		for (const PlaneOut& P : OutPlanes)
		{
			std::snprintf(Line, sizeof(Line), "  air %d %s %s at %.0f,%.0f,%.0f hdg %.0f%s\n", P.Key, P.Model.c_str(), P.Role.c_str(), P.At.X, P.At.Y, P.At.Z, P.Heading, P.bFalling ? " falling" : "");
			Out += Line;
		}
		return Out;
	}
}
