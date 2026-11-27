#include "Battle/SeaShow.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
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
		/** A class's name as a number, for SizeOf: its two letters (every class the battles name has two), -1 for another. */
		int ClassCode(const std::string& Class)
		{
			return Class.size() == 2 ? (static_cast<unsigned char>(Class[0]) << 8) | static_cast<unsigned char>(Class[1]) : -1;
		}
		/** The least distance between two segments, level (0 where they cross). */
		double SegGap(const Vec& A0, const Vec& A1, const Vec& B0, const Vec& B1)
		{
			auto Cross = [](const Vec& O, const Vec& A, const Vec& B) { return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X); };
			const double D1 = Cross(B0, B1, A0), D2 = Cross(B0, B1, A1), D3 = Cross(A0, A1, B0), D4 = Cross(A0, A1, B1);
			if (((D1 > 0.0 && D2 < 0.0) || (D1 < 0.0 && D2 > 0.0)) && ((D3 > 0.0 && D4 < 0.0) || (D3 < 0.0 && D4 > 0.0))) return 0.0;
			// (the nearest of the four ends to the other segment, by the square of its gap, and that gap measured once: the
			// show asks this many times a step of every ship keeping clear, finding 9 of 22 November. The ends whose squares
			// tie with the least, to a millionth of a millionth, are each measured and the least taken, so the answer is the
			// four measures' least to the last bit, as before: two ships abeam on one course tie, the review's 1.1%.)
			double GX[4], GY[4], G2[4];
			int Ends = 0;
			auto ToSeg = [&](const Vec& P, const Vec& S0, double DX, double DY, double L2)
			{
				const double U = L2 > 1e-9 ? Clamp(((P.X - S0.X) * DX + (P.Y - S0.Y) * DY) / L2, 0.0, 1.0) : 0.0;
				GX[Ends] = P.X - S0.X - DX * U;
				GY[Ends] = P.Y - S0.Y - DY * U;
				G2[Ends] = GX[Ends] * GX[Ends] + GY[Ends] * GY[Ends];
				++Ends;
			};
			const double BX = B1.X - B0.X, BY = B1.Y - B0.Y, BL2 = BX * BX + BY * BY;
			const double AX = A1.X - A0.X, AY = A1.Y - A0.Y, AL2 = AX * AX + AY * AY;
			ToSeg(A0, B0, BX, BY, BL2);
			ToSeg(A1, B0, BX, BY, BL2);
			ToSeg(B0, A0, AX, AY, AL2);
			ToSeg(B1, A0, AX, AY, AL2);
			const double Least2 = std::min(std::min(G2[0], G2[1]), std::min(G2[2], G2[3]));
			const double Band = Least2 * (1.0 + 1e-12) + 1e-200;
			double Gap = -1.0;
			for (int K = 0; K < 4; ++K)
			{
				if (G2[K] <= Band)
				{
					const double H = std::hypot(GX[K], GY[K]);
					Gap = Gap < 0.0 ? H : std::min(Gap, H);
				}
			}
			return Gap < 0.0 ? std::hypot(GX[0], GY[0]) : Gap; // (no number in: none out, as before)
		}
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
		/** One of the show's ways down (Sink's words): settle, sink, capsize, explode, burn, break, crushed, scuttled,
		 * rammed, shelled. */
		bool IsWayDown(const std::string& How)
		{
			static const char* const Ways[] = { "settle", "sink", "capsize", "explode", "burn", "break", "crushed", "scuttled", "rammed", "shelled" };
			for (const char* W : Ways)
				if (How == W) return true;
			return false;
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
		ClassList.clear();
		for (const auto& Pair : F.Classes) ClassList.push_back({ ClassCode(Pair.first), Pair.first, Pair.second });
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
		const int Code = ClassCode(Class);
		for (const ClassRow& Row : ClassList)
		{
			if (Code >= 0 ? Row.Code == Code : Row.Name == Class) return Row.Size;
		}
		for (const ClassRow& Row : ClassList)
		{
			if (Row.Name == "ca") return Row.Size;
		}
		return F.Classes.at("ca");
	}

	double Show::Calibre(const std::string& Class, const std::string& Nation) const
	{
		// The main guns of each navy's ships of the class in the war's middle years, millimetres.
		if (Class == "bb") return Nation == "USA" ? 406.0 : Nation == "JAP" ? 410.0 : Nation == "GER" ? 380.0 : Nation == "UK" ? 381.0 : Nation == "ITA" ? 381.0 : Nation == "FRA" ? 380.0 : 356.0;
		if (Class == "ca") return 203.0;
		if (Class == "dd") return Nation == "USA" || Nation == "JAP" || Nation == "GER" ? 127.0 : 120.0;
		// A boat's deck gun until the battle names it (gunMm; estimates for the historian): a German's 88 mm (a Type IX's
		// 105 the battle names); a British boat's 4-inch (102 mm); an American's 3-inch to 1941, 4-inch 1942-43, 5-inch
		// from 1944 (by year, as the show knows no boat's class); a Japanese boat's 140 mm (the naval rules thread's historian:
		// most Japanese fleet boats carried the 14 cm); an Italian's, a French boat's and a Soviet's 100 mm.
		if (Class == "ss") return Nation == "GER" ? 88.0 : Nation == "UK" ? 102.0 : Nation == "USA" ? (F.Year >= 1944 ? 127.0 : F.Year >= 1942 ? 102.0 : 76.0) : Nation == "JAP" ? 140.0 : 100.0;
		// A merchant's gunners (the Royal Navy's DEMS men, the US Navy's Armed Guard): a 4-inch on her stern; others an old
		// 88 mm or the like.
		if (Class == "mv") return Nation == "UK" || Nation == "USA" ? 102.0 : 88.0;
		return 127.0;
	}

	double Show::Cadence(const std::string& Class)
	{
		// Seconds between a ship's salvos in a gunnery action: a battleship's big guns about two a minute, a cruiser's
		// eight-inch about four, a destroyer's guns in rapid fire about eight; a boat's deck gun a round about every 5 s
		// (her crew on a wet, rolling casing), a merchant's gunners slower (each a little ragged).
		const double Base = Class == "bb" ? 30.0 : Class == "ca" ? 16.0 : Class == "ss" ? 5.0 : Class == "mv" ? 8.0 : 7.5;
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
				// (and a convoy's depth charge on a boat, ruling 241 §7; and a deck gun's hits, told one by one, sea:gunHit: drawn
				// as told, not again as the hull they took)
				if ((E.What == "sea:strike" || E.What == "sea:air" || E.What == "sea:torpedoHit" || E.What == "sea:depthChargeHit" || E.What == "sea:gunHit") && E.Unit >= 0)
					AirBlowAt[E.Unit] = E.T;
			}
			Debts(Views.back(), V, InEvents);
		}
		if (!Views.empty() && T <= Views.back().T) Views.back() = V; // (the same moment again: the newer word stands)
		else Views.push_back(V);
		while (Views.size() > 12 && Views[1].T <= ShownT) Views.pop_front();
		for (const Event& E : InEvents) Queue.push_back(E);
		// (at one moment the blow before what it did: the torpedo that sank her strikes her before she goes down, and on
		// her, not on the empty place in her line, as a gun's last hits do as she is shelled under; and a depth-charge attack
		// before a hit told at its own moment, so the hit has its charges to burst)
		auto Rank = [](const Event& E) { return E.What == "sea:depthCharge" ? 0 : E.What == "sea:torpedoHit" || E.What == "sea:depthChargeHit" || E.What == "sea:gunHit" ? 1 : 2; };
		std::stable_sort(Queue.begin(), Queue.end(), [&Rank](const Event& A, const Event& B) { return A.T < B.T || (A.T == B.T && Rank(A) < Rank(B)); });
		if (InZones) Zones = *InZones;
		if (Contact >= 0.0 && ContactT < 0.0) ContactT = Contact;
		if (!bStarted)
		{
			bStarted = true;
			ShownT = T;
			ClockFrom = T;
			Elapsed = 0.0;
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
				// (and one abandoned, sea:abandon, or a boat that struck, sea:surrender: she leaves the fight with her hull, as one
				// sunk; her strength gone to nothing is not hits owed her by gunfire, which no shell of the show's would ever pay)
				if (E.Unit != Pair.first || (E.What != "sea:sunk" && E.What != "sea:burning" && E.What != "sea:abandon" && E.What != "sea:surrender")) continue;
				const double Own = Out == 0 ? Was->second.Str * Ships0 - (Was->second.Ships - 1) : 0.5;
				if (E.What != "sea:burning") Left += Clamp(Own, 0.0, 1.0);
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
		const bool bReached = A->T != NowFrom;
		NowFrom = A->T;
		for (const auto& Pair : A->Pieces)
		{
			Piece P = Pair.second;
			// (the torpedo words' phrases: the rules' name of the piece a view reached names, heldForName and clearFromName, as
			// it gives them; never read in deciding)
			if (bReached && P.HeldFor >= 0 && !P.HeldForName.empty()) RulesNamed[P.HeldFor] = Phrase{ A->T, P.HeldForName };
			if (bReached && P.ClearFrom >= 0 && !P.ClearFromName.empty()) RulesNamed[P.ClearFrom] = Phrase{ A->T, P.ClearFromName };
			// (the torpedo words' held fields as the view reached gives them, until the piece's salvo is told after it)
			const auto Told = SalvoToldAt.find(Pair.first);
			if (Told != SalvoToldAt.end() && Told->second > A->T + 1e-9)
			{
				if (P.HeldFor >= 0) P.bHeldLetGo = true;
				P.HeldFor = -1;
				P.HeldForName.clear();
			}
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
					// (a boat's keel told as it changes, a view at a time no faster than a boat goes, blown up 2 m/s or going
					// down 1 m/s or so: between the two, as her place is, and how fast; a told jump stays a jump, which she
					// makes at her own rates, Trim; and the next view's keel, where what is told so far may end)
					if (P.Keel >= 0.0 && Next->second.Keel >= 0.0 && B->T > A->T)
					{
						const double KeelPace = (Next->second.Keel - P.Keel) / (B->T - A->T);
						if (std::fabs(KeelPace) <= KeelRampMost)
						{
							P.Keel = Lerp(P.Keel, Next->second.Keel, Fr);
							P.KeelRate = KeelPace;
							P.KeelNext = Next->second.Keel;
						}
					}
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
				// (a boat at the depth the battle has her from the first, as Trim lays it on its first call: the start's settling
				// passes (KeepClear) come before it, and a boat held deep keeps no water at the surface there either, HeldDeep.
				// Laid at the surface for them, hunters' Boat 1, told deep at 110 m as the window opens with Escort 8 199 m
				// astern of her on her course and throwing ahead at her, was pushed 97.5 m across from her place and the escort
				// as far the other way: naval round 6a's recordings, 26 November)
				if (H.Class == "ss")
				{
					if (!P.Dive.empty()) H.Dive = P.Dive;
					H.bScope = P.bScope;
					const double Up = SurfacedKeel(H.Class);
					const double Want = P.Keel >= 0.0 ? P.Keel : H.Dive == "deep" ? F.DeepKeel : H.Dive == "periscope" ? F.ScopeKeel : Up;
					H.Keel = H.KeelTo = Clamp(Want, Up, std::max(Up, F.CrushKeel));
				}
				// (a boat's deck gun: an American fleet boat's aft of her tower, as the battle modeller's boat has it, lying
				// stowed facing aft from the first; any other's forward of it, facing forward)
				H.bAftGun = H.Class == "ss" && H.Nation == "USA";
				H.Train = H.bAftGun ? 180.0 : 0.0;
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

	bool Show::StillAfloat(const Hull& H)
	{
		return H.Doing == Fate::Line || H.Doing == Fate::Limping || (H.Doing == Fate::Sinking && H.bAbandoned);
	}

	Show::Hull* Show::NearestAfloatOf(int PieceId, double X, double Y)
	{
		// (every ship of her piece, in her line or out of it: a straggler fallen out of her column, or one her men have left)
		Hull* Best = nullptr;
		double BestD = 1e18;
		for (auto& Pair : Hulls)
		{
			Hull& H = Pair.second;
			if (H.Piece != PieceId || !StillAfloat(H)) continue;
			const double D = Flat(H.P - Vec(X, Y, 0.0));
			if (D < BestD) { BestD = D; Best = &H; }
		}
		return Best;
	}

	std::map<int, const Event*> Show::GunHitsDue() const
	{
		std::map<int, const Event*> Due;
		for (const Event& E : Queue)
		{
			if (E.What != "sea:gunHit" || E.From < 0 || E.Unit < 0 || E.T <= ShownT + 1e-9 || Due.count(E.From)) continue;
			// (one a round already carries is on its way: the next of hers is the one to lay for)
			bool bCarried = false;
			for (const Shells& S : Flying)
				bCarried = bCarried || (S.ToldAt == E.T && S.Piece == E.From && S.OnPiece == E.Unit);
			if (!bCarried) Due[E.From] = &E;
		}
		return Due;
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
		// (her men over the side, her rafts, wreckage and oil: RunWrecks)
		Abandon(H);
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
		// (an aircraft from the coast has no deck to leave: it is in the picture from its first moment)
		A.bShown = S.bFromCoast;
		Air[A.Key] = A;
		S.Planes.push_back(A.Key);
		return A.Key;
	}

	void Show::Play(const Event& E)
	{
		const Piece* P = E.Unit >= 0 ? PieceNow(E.Unit) : nullptr;
		const Vec At = P ? Vec(P->X, P->Y, 0.0) : Vec(E.X, E.Y, 0.0);
		// What it finds first (SetMine), so a cue that finds her is heard. A cue of her own doing from an enemy boat the player
		// has not found (her torpedoes away, her tanks flooding, her signal) goes unheard: he knows of her only what his own
		// ships find. Forced up she is found, her tanks blown heard. The words are told whatever.
		HuntEvent(E, P, At);
		const bool bHerOwn = E.What == "sea:torpedo" || (E.What == "sea:dive" && E.Why != "forced") || E.What == "sea:report";
		// (the hull that acts, as every handler below takes it: her sister of a piece of two, found, is heard)
		const Hull* Doer = bHerOwn && MySide >= 0 && E.Unit >= 0 ? NearestOf(E.Unit, At.X, At.Y) : nullptr;
		if (!E.Cue.empty() && !(Doer && !InSight(*Doer))) Emit(FxKind::Sound, At, E.Cue, E.Unit).Side = E.Side;
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
			// (one her crew have left already, sea:abandon: the ship the battle sends down is her, where it names her, where
			// her piece has no other, or where its smoke is laid nearer her than the one of her line it would name)
			Hull* Left = nullptr;
			if (bSunk)
			{
				const auto Named = E.Ship > 0 ? LostShips.find({ E.Unit, E.Ship }) : LostShips.end();
				for (auto& Pair : Hulls)
				{
					Hull& A = Pair.second;
					if (A.Piece != E.Unit || !A.bAbandoned || A.Doing != Fate::Sinking) continue;
					if (Named != LostShips.end() ? Named->second == A.Key : !Left || A.SinkAt < Left->SinkAt) Left = &A;
				}
				if (Left && Named == LostShips.end() && H && Z && Flat(Vec(Z->X, Z->Y, 0.0) - H->P) < Flat(Vec(Z->X, Z->Y, 0.0) - Left->P)) Left = nullptr;
				if (Left) H = Left;
			}
			if (!H)
			{
				bSunk ? ++Check.SunkUnmatched : ++Check.BurningUnmatched;
				if (Z) LightZone(*Z, nullptr);
				return;
			}
			if (Left)
			{
				// (her end as the battle says it, where it says one of the show's ways: a boat her crew left and scuttled goes
				// "scuttled", her vents open, by the stern, not as a ship settling; a merchant given up burning, breaking or
				// capsizing goes so; none said, she settles as she was)
				if (IsWayDown(E.How)) H->How = E.How;
				// (none said of one that struck, as the battle's own sea:sunk says none: her crew's own way, scuttled, the way
				// she struck to go; one taken and sent down after all goes so too)
				else if (H->bStruck) H->How = "scuttled";
				if (H->bStruck)
				{
					// (struck: every man still below or on her casing goes over now, the last within HurryFor of it, riding her
					// down on the stage's sinking until he does (HandAt), and her flag comes down with her; a prize the battle
					// sends down after all leaves no prize)
					H->SentDownAt = ShownT;
					for (Wreck& W : Wrecked)
						if (W.Ship == H->Key && W.bStruck) Hurry(W);
					if (H->FlagDownAt < 0.0) H->FlagDownAt = ShownT;
					H->bPrize = false;
				}
				// (her going down now: she settles the rest of the way from as far down as she is, as one that settles)
				const double Was = Clamp((ShownT - H->SinkAt) / std::max(1.0, H->SinkFor), 0.0, 0.9);
				const double Rest = 200.0 * Roll.U(0.8, 1.25);
				H->bAbandoned = false;
				H->SinkFor = Rest / (1.0 - Was);
				H->SinkAt = ShownT - Was * H->SinkFor;
				for (Wreck& W : Wrecked)
				{
					if (W.Ship != H->Key) continue;
					W.For = std::max(10.0, ShownT + Rest - W.T0);
					W.How = H->How;
				}
				Fx& Down = Emit(FxKind::Sink, H->P, H->How, H->Key);
				Down.Size = 0.0;
				Down.Text = E.Text;
				Down.Side = H->Side;
				++Check.Sinks;
				++Check.SinksBy[H->How];
			}
			else if (bSunk)
			{
				Sink(*H, E.Text, E.How);
				NoteLost(*H, E.Ship);
			}
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
			// (the rules have let the piece fire: told after the view reached, its held fields are taken off now, SalvoToldAt)
			SalvoToldAt[E.Unit] = E.T;
			// (the torpedo words' phrases: the log's line of its salvo, "Four destroyers of Force H fire torpedoes at ...")
			if (!E.Text.empty()) SalvoSaid[E.Unit] = Phrase{ E.T, E.Text };
			const auto HeldNow = E.T > NowFrom + 1e-9 ? Now.find(E.Unit) : Now.end();
			if (HeldNow != Now.end())
			{
				if (HeldNow->second.HeldFor >= 0) HeldNow->second.bHeldLetGo = true;
				HeldNow->second.HeldFor = -1;
				HeldNow->second.HeldForName.clear();
			}
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
			// earlier ones still run, and every hit has a track to end; the room as the tracks fired at the battle's moment
			// keep it, the shadows, whether any ship holds hers or not)
			const int Firing = std::min(4, static_cast<int>(It->second.Line.size()));
			const int Room = TrackRoom - static_cast<int>(Shadows.size());
			if (PerShip > 0 && Firing > 0 && PerShip * Firing > Room) PerShip = std::max(1, Room / Firing);
			// (its firing point: the piece's place as the battle tells the salvo, its leader's as the rules lay him, where the
			// rules' water starts: from there, its leader's half beam wide, to the run's radius either side of the aim)
			const Hull* Lead = It->second.Line.empty() ? nullptr : FindHull(It->second.Line.front());
			const Vec Point = E.bAt ? Vec(E.X, E.Y, 0.0) : At;
			const int Salvo = ++TorpSalvos;
			Check.SalvoTubes.push_back(0);
			Check.SalvoTracks.push_back(0);
			Check.SalvoPiece.push_back(E.Unit);
			Check.SalvoDropped.push_back(0);
			LastSalvo[E.Unit] = Salvo;
			const size_t First = Holding.size();
			int Fired = 0;
			for (const int Key : It->second.Line)
			{
				Hull* H = FindHull(Key);
				if (!H || Fired >= 4 || PerShip <= 0) continue;
				++Fired;
				// Three tracks from each ship of the piece (her tubes' spread, a few degrees apart), aimed at the battle's
				// aim point, at the speed that brings them there when the battle has them arrive; they run on past it. Each
				// as it would have run had she fired now is the shadow (the room, the ship a hit strikes); her tubes are
				// fired below, now or once her way is clear, each with its own scatter thrown now (the battle's dice, in
				// their order whenever she fires).
				const double Base = Bearing(H->P, Aim), Dist = Flat(Aim - H->P);
				const double Speed = std::max(bBoat ? 8.0 : 18.0, Dist / Running);
				const double Fan = bBoat && E.Fan > 0.0 && PerShip > 1 ? Clamp(E.Fan, 0.0, 40.0) / (PerShip - 1) : 3.5;
				Held R;
				R.Salvo = Salvo;
				R.Ship = H->Key;
				R.Piece = E.Unit;
				R.Side = H->Side;
				R.Class = H->Class;
				R.Nation = H->Nation;
				R.bBoat = bBoat;
				R.bWakeless = E.bWakeless;
				R.Aim = Aim;
				R.Told = ShownT;
				R.Arrive = ShownT + Running;
				R.Running = Running;
				R.Fan = Fan;
				R.WaterP = Point;
				R.WaterHalf = SizeOf(Lead ? Lead->Class : H->Class).Beam * 0.5;
				R.Radius = std::max(F.TorpRadius, R.WaterHalf);
				for (int Tube = 0; Tube < PerShip; ++Tube)
				{
					const double K = Tube - (PerShip - 1) * 0.5;
					if (static_cast<int>(Shadows.size()) >= TrackRoom) break;
					const double Scatter = Roll.U(-0.8, 0.8);
					Shadow S;
					S.Piece = E.Unit;
					S.Way = Ahead(Base + (K * Fan + Scatter) * Deg);
					S.P = bBoat ? PlaceOn(*H, 0.45, K * 0.6, 0.0) : H->P + RightOf(H->Hdg) * (K * 4.0);
					S.Speed = Speed;
					S.Ends = ShownT + Running + 600.0 / Speed;
					Shadows.push_back(S);
					R.Ks.push_back(K);
					R.Scatter.push_back(Scatter);
					R.Ends.push_back(S.Ends);
					R.RulesOnly.push_back(0);
					++Check.SalvoTubes.back();
				}
				if (R.Ks.empty())
				{
					// (no room left for a track of hers: she turns away under smoke as she would have fired, L5)
					if (!bBoat) H->ScreenUntil = ShownT + 60.0;
					continue;
				}
				Holding.push_back(R);
			}
			// Each tube goes now where its way is clear from her own place (or from the salvo's firing point, where her own
			// place's track would not come into the rules' water within the look ahead), the leader's first; the rest are
			// held, and RunTorpedoes looks again every quarter second, each ship a step after the one ahead.
			ForeseenAt = -1.0;
			for (size_t K = First; K < Holding.size(); ++K) Holding[K].NextLook = ShownT + 0.25 + Fixed * static_cast<double>((K - First) % 5);
			for (size_t K = First; K < Holding.size();)
			{
				if (!FireTubes(Holding[K])) Holding.erase(Holding.begin() + static_cast<std::ptrdiff_t>(K));
				else ++K;
			}
			Check.MostTracks = std::max(Check.MostTracks, static_cast<int>(Torps.size()));
			return;
		}
		if (E.What == "sea:torpedoHit")
		{
			++Check.TorpedoHitEvents;
			AirBlowAt[E.Unit] = E.T;
			// The ship of the piece hit that the track of the launching piece ending nearest one of its ships comes to, as
			// the tracks fired at the battle's moment had it (the shadows: the same ship whether any ship held her tubes or
			// not). Where the blow sinks or fires her, the battle's zone for it names the ship (the one that then goes down or
			// burns).
			const Zone* Doom = ZoneFor("sunk");
			if (!Doom) Doom = ZoneFor("burning");
			Hull* Named = Doom ? ByZone(E.Unit, Doom) : nullptr;
			Shadow* Was = nullptr;
			Hull* Struck = nullptr;
			double BestD = 1e18;
			for (Shadow& S : Shadows)
			{
				if (S.Ends < 0.0 || S.Piece != E.From) continue;
				Hull* H = Named ? Named : NearestOf(E.Unit, S.P.X, S.P.Y);
				if (!H) continue;
				const double D = Flat(H->P - S.P);
				if (D < BestD) { BestD = D; Was = &S; Struck = H; }
			}
			if (!Struck) Struck = Named ? Named : NearestOf(E.Unit, At.X, At.Y);
			if (Was) Was->Ends = -1.0; // (its shadow ends against her, as the track did)
			if (!Struck) { ++Check.TorpedoHitUnmatched; return; }
			// The drawn track of the launching piece that ends against her side: one that has run a third of its salvo's run,
			// its way to her clear of its own side, the nearest her (TrackForHit).
			Torp* Best = TrackForHit(E.From, *Struck);
			if (!Best) ++Check.TorpedoHitUnmatched;
			// (her side: the one the track comes in on; the dice for it only where no shadow ran, as before)
			const Vec* From = Best ? &Best->P : Was ? &Was->P : nullptr;
			const double Coin = Was ? 0.0 : Roll.U();
			const double Side = From ? (RightOf(Struck->Hdg).Dot(*From - Struck->P) >= 0.0 ? 1.0 : -1.0) : (Coin < 0.5 ? -1.0 : 1.0);
			const ClassSize& Sz = SizeOf(Struck->Class);
			const Vec Hit = PlaceOn(*Struck, Roll.U(-0.3, 0.3), Side * (Sz.Beam * 0.5 + 2.0), 0.0);
			// (how far the track as fired at the battle's moment lay from her side: the jump it made before any ship held hers)
			Check.HitShadowGap.push_back(Was ? Flat(Was->P - Hit) : -1.0);
			if (Best)
			{
				Best->bRunning = false;
				Best->P = Hit;
				Emit(FxKind::TrackStop, Hit, std::string(), Best->Key);
			}
			Fx& X = Emit(FxKind::TorpedoHit, Hit, "torpedo", Struck->Key);
			X.Side = Struck->Side;
			Struck->LastBlow = "torpedo";
			Struck->BlowAt = ShownT;
			SightHit(*Struck); // (a boat struck is found: SetMine)
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
			// A boat (ruling 241, D1: "an aircraft seen running in on a boat"): the aircraft told (n, one at least, of its
			// kind), low over the sea from as far out as its `running` seconds take it, letting go over her as they end: its
			// depth bombs, the blow the battle booked, while she is still on the surface; then away to its own coast.
			if (Hull* Boat = P && P->Class == "ss" ? NearestOf(E.Unit, At.X, At.Y) : nullptr)
			{
				const double Run = std::clamp(E.Running > 0.0 ? E.Running : 8.0, 4.0, 60.0), Low = 75.0;
				S.Lands = ShownT + Run;
				S.OnKey = Boat->Key;
				const int Count = std::clamp(E.N > 0 ? E.N : 1, 1, 4);
				for (int K = 0; K < Count && static_cast<int>(Air.size()) < 60; ++K)
				{
					const Vec From = Vec(Boat->P.X, Boat->P.Y, 0.0) + Ahead(Home + (K - 0.5 * (Count - 1)) * 10.0 * Deg) * (Run * Low + K * 90.0) + Vec(0.0, 0.0, 150.0);
					const int Key = Launch(S, From, Wrap(Home + Pi), K);
					Plane& A = Air[Key];
					A.Role = "level";
					A.Model = "Patrol:" + A.Nation;
					A.Spd = Low;
					A.bHit = !bNone && K == 0;
				}
				Strikes.push_back(S);
				AirBlowAt[E.Unit] = E.T;
				// (the aircraft have her: in sight over their run and FoundFor after, SetMine)
				Sight(*Boat, Run + FoundFor);
				++Check.AirOnBoats;
				return;
			}
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
		if (E.What == "sea:gun")
		{
			// (a gun action begun or ceased, a boat's deck gun or a merchant's: its words, above, are all it shows; its rounds
			// come of the pieces' own "gun", view by view, Gunnery)
			++Check.GunEvents;
			return;
		}
		if (E.What == "sea:gunHit")
		{
			// A gun's hits, told as they strike (n of them, one where not said): drawn on the ship struck at once, through the
			// round of the gun that fired them that carries them (fired its flight before, Gunnery: so the gun has fired as a hit
			// lands, and it lands as its words are told), each of that gun's bore, the last ones as she is shelled under too
			// (told at her end's moment, they are played before it, Take); the hull they took in the view is not owed again
			// (Take). On the ship struck where she lies afloat, in her line or out of it (a straggler, or one her men have left:
			// what a deck gun most often finished). Only a boat already under as she is told struck (her crash dive begun with
			// the round in the air) owes them, drawn when a round falls on her up again (LandShells), its own round falling
			// about where she went down: a shell's hit shows on the hull it struck, never under the sea.
			++Check.GunHitEvents;
			const int Struck = std::max(1, E.N);
			Check.GunHitsTold += Struck;
			if (E.Unit < 0) return;
			Check.OwedAll += Struck;
			Shells ItsRound;
			bool bCarried = false;
			for (size_t K = 0; K < Flying.size(); ++K)
			{
				if (Flying[K].ToldAt != E.T || Flying[K].Piece != E.From || Flying[K].OnPiece != E.Unit) continue;
				ItsRound = Flying[K];
				Flying.erase(Flying.begin() + static_cast<long>(K));
				bCarried = true;
				break;
			}
			Hull* On = bCarried ? FindHull(ItsRound.OnKey) : nullptr;
			if (!On || !StillAfloat(*On)) On = NearestAfloatOf(E.Unit, At.X, At.Y);
			if (!On || (On->Class == "ss" && On->Keel > SurfacedKeel(On->Class) + 1.0))
			{
				Debt[E.Unit].Hits += Struck;
				if (bCarried)
				{
					// (its round falls as any other now: about where she went down)
					ItsRound.ToldAt = -1.0;
					Flying.push_back(ItsRound);
				}
				return;
			}
			const Piece* By = E.From >= 0 ? PieceNow(E.From) : nullptr;
			const double Cal = bCarried ? ItsRound.Calibre : By && By->GunMm > 0.0 ? By->GunMm : By ? Calibre(By->Class, By->Nation) : 102.0;
			const std::string Weapon = Cal >= 300.0 ? "shell_heavy" : Cal >= 180.0 ? "shell_medium" : "shell_light";
			const ClassSize& Sz = SizeOf(On->Class);
			for (int K = 0; K < Struck; ++K)
			{
				Fx& X = Emit(FxKind::ShellHit, PlaceOn(*On, Roll.U(-0.35, 0.35), Roll.U(-0.3, 0.3) * Sz.Beam, Roll.U(4.0, 10.0)), Weapon, On->Key);
				X.Size = Cal;
				X.Side = On->Side;
				++Check.ShellHits;
			}
			On->LastBlow = Weapon;
			On->BlowAt = ShownT;
			On->ShakenAt = ShownT;
			SightHit(*On); // (a boat struck is found: SetMine)
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
			// A pattern: rolled off her stern rails and thrown to either side. Where the battle tells the spot, the pattern
			// lies over it, its middle there and its length along her course as the battle has it: the pattern the battle
			// laid, drawn where the battle laid it (its zone, Spread across, on the spot), whatever her drawn hull's place and
			// heading then. Laid from her drawn stern on her drawn heading instead, hunted's Escort 9, turned 144 degrees in
			// one 3 s step by the battle at 6,495 s, drawn 84 m off her place and 76 degrees off her heading as she dropped at
			// 6,504 s, drew nine of her pattern's ten charges 75-176 m off the spot it was laid on; and first-pack's Escort 4,
			// 38 m short of the spot as she dropped at 13,887 s, began it 83 m short (naval round 6a's recordings, 26 November).
			// It goes in all at once, at the moment the battle tells it, and bursts all at once at the battle's moment, as the
			// naval rules lay and burst it (sim.ts: chargePattern lays one pattern on the spot, due at the drop and its sinking
			// time rounded up to the whole second; burstCharges bursts it, and books any blow, at the first step of the battle
			// at or after that, the step being the spacing of its views): every charge at its set depth at the battle's blow.
			// A pattern told between the views (a staged scene telling its own moments, not stepping) bursts at its drop and
			// sinking. Run over its length at her speed instead, hunted's Escort 9's pattern of 6,504 s, at her drawn 4.2 m/s
			// a 28.6 s run, put the charge nearest Boat 5 in 6.35 s into it, due 4.35 s after the battle's blow of 6,540 s and
			// still sinking at 85.9 m of its 102 m a second before it; run at her battle speed of 7.0 m/s, a 17.1 s run, due
			// 1.8 s after the blow and at about 96.6 m at it (naval round 6b, 26 November). Where the battle tells no spot,
			// from her stern as she is, one after another over its length at her speed.
			const bool bOverSpot = !bAhead && (E.bAim || E.bAt);
			const double Speed = std::max(3.0, H->Spd);
			const double Run = bAhead || bOverSpot ? 0.0 : Spread / Speed;
			double Step = 0.0;
			for (size_t K = 1; K < Views.size(); ++K)
			{
				const double Gap = Views[K].T - Views[K - 1].T;
				if (Gap > 1e-6 && (Step <= 0.0 || Gap < Step)) Step = Gap;
			}
			bool bOnStep = false;
			for (const View& V : Views) bOnStep = bOnStep || std::fabs(V.T - E.T) < 1e-6;
			const double Whole = std::ceil(Sinks - 1e-9);
			const double Moment = !bOnStep ? E.T + Sinks : Step > 0.0 ? E.T + Step * std::ceil(Whole / Step - 1e-9) : E.T + Whole;
			const double RunHdg = bOverSpot && P ? P->Face : H->Hdg;
			const Vec RunFrom = bOverSpot ? Middle - Ahead(RunHdg) * (Spread * 0.5) : PlaceOn(*H, -0.5, 0.0, 0.0);
			Fx& D = Emit(FxKind::ChargeDrop, bAhead ? PlaceOn(*H, 0.3, 0.0, 3.0) : Vec(RunFrom.X, RunFrom.Y, 1.0), bAhead ? "ahead" : "pattern", H->Key);
			D.Aim = bAhead ? Middle : RunFrom + Ahead(RunHdg) * (Spread * 0.5);
			D.Aim.Z = 0.0;
			D.Vel = Ahead(bAhead ? H->Hdg : RunHdg) * H->Spd;
			D.Size = N;
			D.Spread = Spread;
			D.Sinking = bOverSpot ? Moment - E.T : Sinks;
			D.Until = Run;
			D.Side = H->Side;
			++Check.ChargeDrops;
			if (bAhead)
			{
				AheadAt[E.Unit] = E.T;
				return;
			}
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
				// (each charge a few metres off its place in the pattern, as they fell, never more than ChargeScatterMost: the
				// looks draw it going in at its place, and a burst far off that would be one where no charge went in. The two
				// draws named, the northing first, as the cloud build's compiler drew them as arguments: so its dice fall as
				// they always have, and alike on every compiler)
				const double ScatterY = Roll.Bell() * 3.0;
				const double ScatterX = Roll.Bell() * 3.0;
				Vec Scatter(ScatterX, ScatterY, 0.0);
				const double ScatterOff = Scatter.Length();
				if (ScatterOff > ChargeScatterMost) Scatter = Scatter * (ChargeScatterMost / ScatterOff);
				C.At = RunFrom + Ahead(RunHdg) * (Share * Spread) + RightOf(RunHdg) * Side + Scatter;
				C.At.Z = -Set;
				C.Bursts = bOverSpot ? Moment : E.T + Share * Run + Sinks;
				C.bAtOnce = bOverSpot;
				C.Dropped = E.T;
				C.Laid = C.At;
				C.Due = C.Bursts;
				Charges.push_back(C);
			}
			return;
		}
		if (E.What == "sea:depthChargeHit")
		{
			++Check.ChargeHitEvents;
			AirBlowAt[E.Unit] = E.T;
			Hull* Boat = NearestOf(E.Unit, At.X, At.Y);
			if (Boat)
			{
				Boat->LastBlow = "depthcharge";
				Boat->BlowAt = ShownT;
			}
			const double Keel = E.Keel >= 0.0 ? E.Keel : Boat ? Boat->Keel : F.DeepKeel;
			const Vec Near = Boat ? Boat->P + Vec(Roll.U(-1.0, 1.0), Roll.U(-1.0, 1.0), 0.0) * (F.ChargeLethal * 0.6) : At;
			// The blow: of that escort's charges due within 15 s, the one nearest her keel where it is now (going down at the
			// charge's rate to its burst: where the view under the water draws it), burst now (the battle's moment); an
			// ahead-thrown salvo's bombs burst only now, on her, two or three of them. U1 hook (under water): the one nearest
			// her, not the one nearest in time, so a charge seen sinking by her is the one that bursts there.
			const Vec Her = Boat ? Vec(Boat->P.X, Boat->P.Y, -Keel) : Vec(At.X, At.Y, -Keel);
			auto NowOf = [&E, this](const Charge& C) { return Vec(C.At.X, C.At.Y, std::min(0.0, C.At.Z + std::max(0.0, C.Bursts - E.T) * std::max(0.5, F.ChargeSink))); };
			Charge* Best = nullptr;
			double BestFar = 0.0;
			for (Charge& C : Charges)
			{
				if (C.Piece != E.From || C.bHit || std::fabs(C.Bursts - E.T) > 15.0) continue;
				const double Far = (NowOf(C) - Her).Length();
				if (!Best || Far < BestFar)
				{
					Best = &C;
					BestFar = Far;
				}
			}
			auto It = AheadAt.find(E.From);
			const double AheadT = It != AheadAt.end() ? It->second : -1e18;
			if (!Best)
			{
				// (told well before its charges reach their depth, as at the drop: the first to burst of that escort's newest
				// pattern, dropped within two minutes and since any ahead-thrown salvo of hers; it bursts at its own time)
				for (Charge& C : Charges)
				{
					if (C.Piece != E.From || C.bHit || C.Kind != "pattern" || E.T - C.Dropped > 120.0 || C.Dropped < AheadT) continue;
					if (!Best || C.Dropped > Best->Dropped || (C.Dropped == Best->Dropped && C.Bursts < Best->Bursts)) Best = &C;
				}
			}
			const bool bAhead = !Best && E.T - AheadT <= 60.0;
			if (!Best)
			{
				// (a blow told on her after the pattern this escort laid all at once on her, this boat, burst at the rules'
				// moment, 0 to LateHitWithin s before it, is that pattern's: drawn at her now, as below, one burst at her keel.
				// A pattern that names no boat is matched to none. No recording of the battle tells one, its blows at the rules'
				// moment; the hand-made nights do, their blows told 5-8 s after it: wolf-pack's Boat A 165 s, its pattern of
				// 140 s due at 160 s; sub-air's boats 141.6 s and 212.5 s, theirs due at 134.2 s and 205.1 s)
				const auto Burst = PatternBurstAt.find(E.From);
				const bool bAfterBurst = !bAhead && Burst != PatternBurstAt.end() && Burst->second.second >= 0 && Burst->second.second == E.Unit &&
				                         E.T >= Burst->second.first - 1e-9 && E.T - Burst->second.first <= LateHitWithin;
				if (bAfterBurst) ++Check.ChargeHitLate;
				else if (!bAhead) ++Check.ChargeHitUnmatched;
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
					// (a blow matched to its pattern's burst: its one burst at her keel, drawing no dice, so the night's dice fall
					// as they did when it was matched to a charge still in the water; an ahead salvo's bombs and a blow with no
					// attack before it draw theirs as ever)
					C.At = bAfterBurst ? Near : Near + Vec(Roll.Bell() * 2.0, Roll.Bell() * 2.0, 0.0) * static_cast<double>(K);
					C.At.Z = -Keel;
					C.bHit = K == 0;
					C.Bursts = E.T + K * 0.4;
					C.Dropped = E.T;
					Charges.push_back(C);
				}
				return;
			}
			Best->bHit = true;
			Best->Boat = E.Unit; // (the boat it is booked on: found where it bursts, SetMine)
			if (std::fabs(Best->Bursts - E.T) <= 15.0)
			{
				// (bursting now: where it is, within its lethal reach of her keel; else moved that reach at the most toward her
				// keel, where that brings it within it; only where none of hers lies so near is it put by her, a jump the
				// pattern's place makes, laid where her hunter ran rather than where she is)
				const Vec Going = NowOf(*Best);
				const double Reach = F.ChargeLethal, Far = (Going - Her).Length();
				Best->At = Far <= Reach ? Going : Far <= 2.0 * Reach ? Going + (Her - Going) * (Reach / Far) : Vec(Near.X, Near.Y, -Keel);
				Best->Bursts = E.T;
			}
			else
			{
				Best->At = Vec(Near.X, Near.Y, -Keel);
				if (Boat)
				{
					// (bursting later, at its own time: beside her where she is by then)
					Best->bOnBoat = true;
					Best->Off = Vec(Near.X - Boat->P.X, Near.Y - Boat->P.Y, 0.0);
				}
			}
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
		// The men in the water as the naval rules count them (22 November): a ship's crew ordered off her, the men of a ship
		// lost in the water and her oil, by her piece and her number in it.
		// A boat beaten on the surface strikes (sea:surrender, the naval rules behind their subSurrender switch): her flag up,
		// her crew out of her hatches onto her casing and over her side; the battle says what follows (sea:sunk how
		// "scuttled"; or nothing more, "captured": she is taken). The show adds no loss the battle did not book: her piece's
		// own view says she is out of the fight.
		if (E.What == "sea:surrender")
		{
			++Check.SurrenderEvents;
			Hull* H = nullptr;
			double BestD = 1e18;
			for (auto& Pair : Hulls)
			{
				Hull& A = Pair.second;
				if (A.Piece != E.Unit || A.Class != "ss" || A.bStruck || (A.Doing != Fate::Line && A.Doing != Fate::Limping)) continue;
				const double D = E.bAt ? Flat(A.P - Vec(E.X, E.Y, 0.0)) : A.Doing == Fate::Line ? 0.0 : 1.0;
				if (D < BestD) { BestD = D; H = &A; }
			}
			if (!H)
			{
				++Check.StruckUnmatched;
				return;
			}
			StrikeColours(*H, E);
			NoteLost(*H, E.Ship);
			++Check.Struck;
			return;
		}
		if (E.What == "sea:abandon")
		{
			++Check.AbandonEvents;
			// (the ship of her piece nearest where the battle has her: in its line, or out of it burning)
			Hull* H = nullptr;
			double BestD = 1e18;
			for (auto& Pair : Hulls)
			{
				Hull& A = Pair.second;
				if (A.Piece != E.Unit || (A.Doing != Fate::Line && A.Doing != Fate::Limping)) continue;
				const double D = E.bAt ? Flat(A.P - Vec(E.X, E.Y, 0.0)) : A.Doing == Fate::Line ? 0.0 : 1.0;
				if (D < BestD) { BestD = D; H = &A; }
			}
			if (!H)
			{
				// (a ship the battle's count gave up first with no word of how, Untold: her crew already leaving her, she is
				// the one this abandoning tells, and now has the battle's own number, which its sea:men and sea:sunk for her
				// will give; she is not abandoned again. The review of 24 November, the second)
				for (auto& Pair : Hulls)
				{
					Hull& A = Pair.second;
					if (A.Piece != E.Unit || !A.bGivenUp || !A.bAbandoned || A.Doing != Fate::Sinking) continue;
					A.bGivenUp = false;
					NoteLost(A, E.Ship);
					++Check.Abandons;
					return;
				}
				// (a boat that struck already, sea:surrender: her crew are leaving her as the battle now orders, the same
				// leaving, shown once from her casing; so counted as shown, once)
				for (auto& Pair : Hulls)
				{
					Hull& A = Pair.second;
					if (A.Piece == E.Unit && A.bStruck && !A.bAbandonTold) { H = &A; break; }
				}
				if (!H) return;
				H->bAbandonTold = true;
				++Check.Abandons;
				return;
			}
			AbandonShip(*H, E.Text);
			NoteLost(*H, E.Ship);
			// (a boat: told abandoned before her casing is clear, a forced-up boat's crew ordered out as she comes up, she comes
			// the rest of the way up first, as the battle's own dive has her; her men over once she is up, LeaveUp)
			LeaveUp(*H);
			++Check.Abandons;
			return;
		}
		if (E.What == "sea:men")
		{
			++Check.MenEvents;
			Wreck* W = WreckOf(E.Unit, E.Ship);
			if (!W || E.Men < 0) return;
			++Check.MenMatched;
			// Her men in the water as the battle counts them (alive, the men she had picked up back in the water with them,
			// and the dead afloat), and the share of them the show keeps, as Abandon reckons it: half the room left by the men
			// of other wrecks, and at least a dozen.
			const int Dead = std::max(0, E.Dead), Real = E.Men + E.Guests + Dead;
			int Taken = 0;
			for (const Man& M : Swimmers) Taken += M.bGoing || &Wrecked[static_cast<size_t>(M.W)] == W ? 0 : 1;
			for (const Wreck& O : Wrecked) Taken += &O == W ? 0 : O.Men - O.Out;
			const int Room = std::max(0, F.MenMost - Taken);
			Check.MenReal += Real - W->Real;
			Check.MenKept -= W->Men;
			W->Real = Real;
			W->DeadShare = Real > 0 ? static_cast<double>(Dead) / Real : 0.0;
			W->Men = std::max(W->Out, std::min(Real, std::max(std::min(Real, 12), Room / 2)));
			if (W->bStruck)
			{
				// (a boat that struck: her men on her casing are her men, as many as the battle now counts. More: the rest come
				// up after the last planned, by their own hashes; fewer: those still below stay in her, and none already out on
				// her casing is taken off it)
				const int WI = static_cast<int>(W - Wrecked.data());
				auto PlannedOf = [this, W, WI]()
				{
					int N = W->Out;
					for (const DeckHand& D : OnCasing) N += D.W == WI && !D.bParty ? 1 : 0;
					return N;
				};
				int Planned = PlannedOf();
				if (W->Men < Planned)
				{
					const int Keep = W->Men;
					OnCasing.erase(std::remove_if(OnCasing.begin(), OnCasing.end(), [WI, Keep](const DeckHand& D) { return D.W == WI && !D.bParty && !D.bOut && D.N >= Keep; }), OnCasing.end());
					Planned = PlannedOf();
					W->Men = std::max(W->Men, Planned);
				}
				const Hull* H = FindHull(W->Ship);
				if (W->Men > Planned && H && H->UpAt >= 0.0) Muster(*W, *H, Planned);
			}
			Check.MenKept += W->Men;
			return;
		}
		if (E.What == "sea:oil")
		{
			++Check.OilEvents;
			Wreck* W = WreckOf(E.Unit, E.Ship);
			if (!W) return;
			++Check.OilMatched;
			// (her slick spreads to the battle's spread across at the most; the share of it alight (by its area) burns until
			// the battle's hour, or none)
			if (E.OilR > 0.0) W->OilR = E.OilR;
			W->LitR = E.OilR > 0.0 && E.LitR > 0.0 ? std::min(E.LitR, E.OilR) : 0.0;
			W->Burn = W->LitR > 0.0 ? (W->LitR / E.OilR) * (W->LitR / E.OilR) : 0.0;
			W->BurnUntil = W->Burn > 0.0 ? (E.OilUntil > E.T ? E.OilUntil : F.Limit + 600.0) : -1.0;
			return;
		}
		if (E.What == "sea:rescue")
		{
			++Check.RescueEvents;
			// Where the ship lost went down: the wreck the battle names ({piece, ship}, the naval rules), else the latest of her
			// piece's ships to go (a convoy's night); and the ship of the rescuer's piece nearest.
			const Hull* Lost = nullptr;
			Wreck* Of = nullptr;
			if (E.WreckOf >= 0)
			{
				Of = WreckOf(E.WreckOf, E.WreckShip);
				Lost = Of ? FindHull(Of->Ship) : nullptr;
			}
			else
			{
				for (const auto& Pair : Hulls)
				{
					const Hull& L = Pair.second;
					if (L.Piece != E.On || (L.Doing != Fate::Sinking && L.Doing != Fate::Gone)) continue;
					if (!Lost || L.SinkAt > Lost->SinkAt) Lost = &L;
				}
			}
			const Vec Where = Lost ? Vec(Lost->P.X, Lost->P.Y, 0.0) : Of ? Of->OilEnd : E.bAim ? Vec(E.AimX, E.AimY, 0.0) : At;
			// (the battle's count of those picked up so far: the share of the men the show keeps that may be drawn aboard)
			auto Count = [&E](const Wreck& W) { return W.Real > 0 ? static_cast<int>(std::lround(E.Men * static_cast<double>(W.Men) / W.Real)) : 0; };
			// (a rescue under way from this wreck by this piece: the battle's count as it goes on, and her leaving when done)
			const Hull* Was = Of && Of->Rescuer >= 0 ? FindHull(Of->Rescuer) : nullptr;
			if (E.WreckOf >= 0 && Was && Was->Piece == E.Unit)
			{
				if (E.Men >= 0) Of->PickCap = std::max(Of->PickCap, Count(*Of));
				Of->RescueUntil = E.bDone ? std::min(Of->RescueUntil, E.T) : std::max(Of->RescueUntil, E.T + 600.0);
				++Check.Rescues;
				return;
			}
			// (the naval rules' order: of a line of more than one, the follower nearest her wreck goes in and lies off it, out
			// of the wake; a ship alone is drawn where the battle has her piece, which lies off it already)
			Hull* By = nullptr;
			bool bSend = false;
			if (E.WreckOf >= 0 && Of)
			{
				const auto Line = Lines.find(E.Unit);
				double BestD = 1e18;
				if (Line != Lines.end())
				{
					for (size_t K = 1; K < Line->second.Line.size(); ++K)
					{
						Hull* Mate = FindHull(Line->second.Line[K]);
						if (!Mate || Mate->bLyingOff || Mate->Doing != Fate::Line) continue;
						const double D = Flat(Mate->P - Where);
						if (D < BestD) { BestD = D; By = Mate; }
					}
				}
				bSend = By != nullptr;
			}
			if (!By) By = NearestOf(E.Unit, Where.X, Where.Y);
			if (!By) return;
			if (bSend)
			{
				By->bLyingOff = true;
				By->LieFor = static_cast<int>(Of - Wrecked.data());
			}
			Fx& R = Emit(FxKind::Rescue, Vec(By->P.X, By->P.Y, 0.0), std::string(), By->Key);
			R.Aim = Where;
			R.Until = E.bDone ? E.T : E.T + 600.0;
			R.Side = By->Side;
			++Check.Rescues;
			// (her men swim to the rescuer, and her rafts and boats are pulled to her, while she lies stopped: RunWrecks)
			if (E.WreckOf < 0)
			{
				for (Wreck& W : Wrecked)
				{
					if (Lost ? W.Ship == Lost->Key : Flat(W.OilEnd - Where) < 3000.0 && (!Of || Flat(W.OilEnd - Where) < Flat(Of->OilEnd - Where))) Of = &W;
				}
			}
			if (Of)
			{
				Of->Rescuer = By->Key;
				Of->RescueUntil = R.Until;
				if (E.WreckOf >= 0 && E.Men >= 0) Of->PickCap = Count(*Of);
			}
			return;
		}
	}

	double Show::SurfacedKeel(const std::string& Class) const
	{
		// (her draught: a Type VIIC's 4.7 m at 67 m long, others in proportion)
		return SizeOf(Class).Long * 0.07;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Whom the player sees (SetMine): an enemy boat only once his ships have found her. Looks only: no draw on the dice,
	// nothing the battle reads; what finds her is what the battle told.

	Show::Hunted* Show::Quarry(const Hull& Boat)
	{
		if (MySide < 0 || Boat.Class != "ss" || Boat.Side == MySide) return nullptr;
		return &Hunts[Boat.Key];
	}

	bool Show::BoatUnder(const Hull& Boat) const
	{
		// (on her way up, her state "surfaced", she is coming into sight: under again only once she has gone down)
		return Boat.Keel > SurfacedKeel(Boat.Class) + 1.0 && Boat.Dive != "surfaced";
	}

	bool Show::HeldDeep(const Hull& Boat) const
	{
		if (Boat.Class != "ss" || Boat.Doing != Fate::Line || Boat.Keel <= SurfacedKeel(Boat.Class) + 1.0) return false;
		const Piece* Her = PieceNow(Boat.Piece);
		if (!Her) return false;
		const double Told = Her->Keel >= 0.0 ? Her->Keel : Her->Dive == "deep" ? F.DeepKeel : Her->Dive == "periscope" ? F.ScopeKeel : 0.0;
		return Told > F.ScopeKeel + 0.5;
	}

	void Show::Sight(const Hull& Boat, double For)
	{
		if (Hunted* W = Quarry(Boat)) W->Until = std::max(W->Until, ShownT + For);
	}

	void Show::SightHit(const Hull& Boat)
	{
		Hunted* W = Quarry(Boat);
		if (!W) return;
		// (struck up, she is in plain sight of whoever struck her until she is under again; under, her hunters work the spot)
		if (!BoatUnder(Boat)) W->bUp = true;
		W->Until = std::max(W->Until, ShownT + FoundFor);
	}

	void Show::HuntEvent(const Event& E, const Piece* P, const Vec& At)
	{
		if (MySide < 0) return;
		if (E.What == "sea:sonar")
		{
			// (a radio bearing is a line on the plot, not her place: it finds nothing)
			if (E.Kind == "bearing")
			{
				++Check.Bearings;
				return;
			}
			Hull* Boat = P && E.On >= 0 ? NearestOf(E.On, At.X, At.Y) : nullptr;
			Hunted* W = Boat && Boat->Side != P->Side ? Quarry(*Boat) : nullptr;
			if (!W) return;
			// Where her view says who is running in on her (heard), that is her hold, Hunt; a contact gained finds her for
			// FoundFor, and one lost keeps her FoundFor more if she was in sight. Else the escorts' events hold her, each
			// escort's by itself whatever the kind (radar may gain her up and asdic lose her under), each until its loss is
			// told, it is gone or she is: the battle closes every contact so, and one it still holds is her hunters' however
			// long they go without an attack (Hunt lets go only of one FoundFor past an ahead-thrown run, which sends none).
			const Piece* Her = PieceNow(Boat->Piece);
			const bool bByHeard = Her && Her->bHeard;
			if (E.Lost.empty())
			{
				++Check.Contacts;
				if (bByHeard) W->Until = std::max(W->Until, ShownT + FoundFor);
				else
				{
					// (gained, or gained again: its word on her is now, and no ahead-thrown run has been made on it since)
					Hunted::Hunter& Holds = W->HeldBy[E.Unit];
					Holds.Word = ShownT;
					Holds.bAhead = false;
				}
				W->Lapsed.erase(E.Unit);
				return;
			}
			// (the loss lets go of what this escort's events held, whichever way she is held now: a recording whose views
			// begin saying heard after an escort's gain leaves no stale holder)
			const bool bWasHolding = W->HeldBy.erase(E.Unit) > 0;
			const bool bLapsed = W->Lapsed.erase(E.Unit) > 0;
			if (bByHeard)
			{
				++Check.ContactsLost;
				if (W->bSeen || bWasHolding) W->Until = std::max(W->Until, ShownT + FoundFor);
				return;
			}
			if (!bWasHolding)
			{
				// (one whose hold ran out after its ahead-thrown run is let go already: its loss, told late, is its own, and
				// she is in sight FoundFor from it as from any loss, the battle having held her until now; one never held is
				// odd)
				if (bLapsed)
				{
					++Check.ContactsLost;
					W->Until = std::max(W->Until, ShownT + FoundFor);
				}
				else ++Check.ContactsOdd;
				return;
			}
			++Check.ContactsLost;
			if (W->HeldBy.empty()) W->Until = std::max(W->Until, ShownT + FoundFor);
			return;
		}
		if (E.What == "sea:depthCharge")
		{
			// (an escort's attack on her, a pattern or an ahead-thrown salvo, is made on her contact: she is the hunt's, and
			// an escort holding her by its events still has word of her; an ahead-thrown run is followed by no loss, so its
			// hold on her runs out FoundFor after the run unless it tells more, Hunt)
			Hull* Boat = P && E.On >= 0 ? NearestOf(E.On, At.X, At.Y) : nullptr;
			if (!Boat || Boat->Side == P->Side) return;
			Sight(*Boat);
			if (Hunted* W = Quarry(*Boat))
			{
				const auto It = W->HeldBy.find(E.Unit);
				if (It != W->HeldBy.end())
				{
					It->second.Word = ShownT;
					if (E.Kind == "ahead") It->second.bAhead = true;
				}
			}
			return;
		}
		if (E.What == "sea:dive" && E.Why == "forced")
		{
			// (forced up by her hunters: in their sight while she is up, and FoundFor once she is under again)
			if (Hull* Boat = NearestOf(E.Unit, At.X, At.Y))
			{
				if (Hunted* W = Quarry(*Boat)) W->bUp = true;
			}
			return;
		}
		if (E.What == "sea:surrender")
		{
			// (her crew giving her up, scuttling her or taken: out of the fight, in her takers' sight to her end)
			if (Hull* Boat = NearestOf(E.Unit, At.X, At.Y))
			{
				if (Hunted* W = Quarry(*Boat)) W->bForGood = true;
			}
			return;
		}
		if (E.What == "sea:gunHit")
		{
			// (a deck gun's hit: on her, she is struck; from her, her gun's flash gives her away)
			if (Hull* Boat = NearestOf(E.Unit, At.X, At.Y)) SightHit(*Boat);
			if (const Piece* By = E.From >= 0 ? PieceNow(E.From) : nullptr)
			{
				if (Hull* Firer = NearestOf(E.From, By->X, By->Y)) Sight(*Firer);
			}
		}
	}

	void Show::Hunt()
	{
		if (MySide < 0) return;
		// (a lookout's eye on a boat up: the night's seeing where the recording gives it, else a clear night's)
		const double Sees = F.Seeing > 0.0 ? F.Seeing : 4000.0;
		for (auto& Pair : Hulls)
		{
			Hull& Boat = Pair.second;
			if (Boat.Class != "ss" || Boat.Side == MySide || Boat.Doing != Fate::Line) continue;
			Hunted& W = Hunts[Boat.Key];
			const Piece* Her = PieceNow(Boat.Piece);
			// (her view saying who is running in on her now, what the escorts' events held lets go, as a contact lost: a
			// recording that begins to say heard part way through keeps no holder of the events' from before)
			if (Her && Her->bHeard && !W.HeldBy.empty())
			{
				W.HeldBy.clear();
				W.Until = std::max(W.Until, ShownT + FoundFor);
			}
			if (Her && Her->bHeard) W.Lapsed.clear();
			// The escorts her view says are running in on her (heard), each afloat and of the other side; one whose echo on
			// her is a radio bearing has her place only as a line on the plot.
			bool bHeld = false;
			if (Her && Her->bHeard)
			{
				for (const int Id : Her->Heard)
				{
					const Piece* By = PieceNow(Id);
					if (!By || By->Side == Boat.Side || By->Ships <= 0) continue;
					if (By->EchoOn == Boat.Piece && By->EchoKind == "bearing") continue;
					bHeld = true;
					break;
				}
			}
			if (W.bHeld && !bHeld) W.Until = std::max(W.Until, ShownT + FoundFor);
			W.bHeld = bHeld;
			// An escort holding her by its events holds her until it tells its loss (HuntEvent) or her end, however long it
			// goes without an attack, and lets her go when it is sunk or gone, as a contact lost (FoundFor after the last of
			// them). One that has made an ahead-thrown run on her, with no word since its last attack (the run sends no
			// loss), has held her its while: she is in sight to FoundFor after that attack, no longer.
			if (!W.HeldBy.empty())
			{
				bool bLetGo = false;
				for (auto It = W.HeldBy.begin(); It != W.HeldBy.end();)
				{
					const Piece* By = PieceNow(It->first);
					const bool bRunOut = It->second.bAhead && ShownT > It->second.Word + FoundFor + 1e-9;
					if (By && By->Ships > 0 && !bRunOut)
					{
						++It;
						continue;
					}
					if (bRunOut)
					{
						W.Until = std::max(W.Until, It->second.Word + FoundFor);
						W.Lapsed.insert(It->first);
					}
					else bLetGo = true;
					It = W.HeldBy.erase(It);
				}
				if (bLetGo && W.HeldBy.empty()) W.Until = std::max(W.Until, ShownT + FoundFor);
			}
			// (her deck gun laid on one of the player's ships while she is up: its flash finds her; and the guns of a ship
			// of the player's laid on her, an armed merchant's as an escort's, have her in their sight)
			if (!BoatUnder(Boat))
			{
				bool bGunned = Her && Her->GunOn >= 0;
				for (auto It = Now.begin(); !bGunned && It != Now.end(); ++It)
				{
					const Piece& Gunner = It->second;
					bGunned = Gunner.GunOn == Boat.Piece && Gunner.Side != Boat.Side && Gunner.Ships > 0;
				}
				if (bGunned) W.Until = std::max(W.Until, ShownT + FoundFor);
			}
			// (up in sight, struck or forced: once she is under again, her hunters work the spot FoundFor)
			if (W.bUp && BoatUnder(Boat))
			{
				W.bUp = false;
				W.Until = std::max(W.Until, ShownT + FoundFor);
			}
			// A lookout's eye: a boat in sight, up and within the night's seeing of a ship of the player's, stays in sight
			// until she is under or beyond it (never a pop on the surface in plain view); then FoundFor.
			bool bLookout = false;
			if (W.bSeen && !BoatUnder(Boat))
			{
				for (const auto& Other : Hulls)
				{
					const Hull& O = Other.second;
					if (O.Side != MySide || (O.Doing != Fate::Line && O.Doing != Fate::Limping)) continue;
					if (O.Class == "ss" && BoatUnder(O)) continue; // (a boat of his own dived has no lookout up)
					if (Flat(O.P - Boat.P) <= Sees)
					{
						bLookout = true;
						break;
					}
				}
			}
			if (W.bLookout && !bLookout) W.Until = std::max(W.Until, ShownT + FoundFor);
			W.bLookout = bLookout;
			W.bSeen = InSight(Boat);
		}
	}

	bool Show::InSight(const Hull& H) const
	{
		// (every ship but an enemy boat still in the fight: his own, the escorts, the merchants, and a boat going down,
		// limping away or given up are all drawn)
		if (MySide < 0 || H.Class != "ss" || H.Side == MySide || H.Doing != Fate::Line || H.bAbandoned) return true;
		const auto It = Hunts.find(H.Key);
		if (It == Hunts.end()) return false;
		const Hunted& W = It->second;
		return W.bForGood || W.bUp || W.bHeld || W.bLookout || !W.HeldBy.empty() || ShownT <= W.Until + 1e-9;
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
			H.Keel = H.KeelTo = To;
			H.KeelRate = 0.0;
		}
		if (H.Doing == Fate::Line) H.KeelTo = To;
		// A depth the battle tells as it changes, a view at a time (Interpolate's KeelRate: the convoy field blows a boat up
		// at 2 m/s, against her own 1.2 below), she follows at the battle's own rate, where it has her: behind it (as it
		// begins, or coming onto it from her own way) she takes up its rate and gains on it smoothly, never stepping onto it;
		// coming up she eases off over the last few metres to her trim. (The review of 24 November: forced up from 110 m, her
		// gun told firing for 45 s before her casing was clear here, and her shell hits owed.)
		// The show holds one view in hand, so a change told may end at the next view: she keeps where she could stop there,
		// braking at KeelBrake of her KeelTakeUp, and follows the battle's depth that far behind it (her stopping distance at
		// its rate: so on a change that goes on she never brakes, and a step in its rate moves her wish by a share of the
		// step, her rate easing onto the new one without turning back). Coming up she is on it until her own keel is at her
		// trim (the battle's depth below her trim is her trim). Where it ends, or she leaves her line in the middle of it (her
		// crew leave her, or given up, or she strikes), she stays on it, easing to a stop at the depth she was given, and
		// leaves it once stopped there: never stopped dead from her rate, which would snap her angle level in a frame. (The
		// review of 24 November, the second: her pitch snapped 12 degrees at the end of a rise and of a dive, and she passed
		// a steady depth by more than a metre.)
		const double Ramp = P && H.Doing == Fate::Line ? P->KeelRate : 0.0;
		const bool bRamp = Ramp > 0.0 || (Ramp < 0.0 && H.Keel > Up + 1e-6);
		if (!bRamp && H.bOnRamp && std::fabs(H.KeelRate) < 0.03 && std::fabs(H.KeelTo - H.Keel) < 0.05) H.bOnRamp = false;
		if (bRamp || H.bOnRamp)
		{
			H.bOnRamp = true;
			const double Brake = KeelBrake * KeelTakeUp;
			const double Deepest = std::max(Up, F.CrushKeel);
			// Blown up for the surface (her rise told "surfaced", or out of her line given her trim) her rise ends at her trim
			// and nowhere short of it: she keeps on the battle's depth itself, not her stopping distance behind it, and the sea
			// stops her as her casing breaks it (KeelTopBrake, until her angle is about to come off; her own braking from
			// there). (The review of 24 November, the second: 4.4 m behind the battle's rise and braking onto her trim from
			// 4.4 m under it, her casing came clear 2.5 s after the battle's, and the gun hit it told while she was 90 m down
			// was drawn a round later still.)
			const bool bForTop = Ramp < 0.0 ? H.Dive == "surfaced" : Ramp == 0.0 && H.KeelRate < 0.0 && H.KeelTo <= Up + 1e-6;
			// (where she aims: on a change, the battle's depth less her stopping distance at its rate, which she could stop
			// within at the next view's (blown up for the surface, the battle's depth, and her trim where she stops); ended,
			// or out of her line, the depth she was given. She stops KeelShort of where she could stop and leaves the last of
			// it to her own easing, below: a stop in the last step of a braking at her rate would still level her a degree or
			// two in one)
			const double Lag = bForTop ? 0.0 : Ramp * std::fabs(Ramp) / (2.0 * Brake) + (Ramp > 0.0 ? KeelShort : Ramp < 0.0 ? -KeelShort : 0.0);
			const double Aim = Ramp != 0.0 ? Clamp(To - Lag, Up, Deepest) : H.KeelTo;
			const double Stop = bForTop ? Up : Ramp != 0.0 && P->KeelNext >= 0.0 ? Clamp(P->KeelNext, Up, Deepest) : H.KeelTo;
			const double RampTop = std::max(1.2, KeelRampOver * std::fabs(Ramp));
			double Wish = Clamp(Ramp + Clamp((Aim - H.Keel) * KeelGain, -KeelGainMost, KeelGainMost), -RampTop, RampTop);
			// (ended, or out of her line: she keeps the way she has toward the depth she was given (her gain's most at
			// least, the ramp's at most) and brakes for it only as she must, not easing in on it by her gain, which left a boat
			// given up as she rose some ten seconds short of her casing clear, and her hits owed later still)
			if (Ramp == 0.0)
				Wish = (H.KeelTo >= H.Keel ? 1.0 : -1.0) * Clamp(std::fabs(H.KeelRate), KeelGainMost, KeelRampMost);
			// (for her trim, the sea brakes her while her angle is at its most, 12 degrees as Steam draws it from her rise
			// against her way (1.5 m/s at the least): down to KeelTopEase times the rise that gives it, Soft; she from there)
			const double Soft = KeelTopEase * std::tan(12.0 * Deg) * std::max(1.5, std::fabs(H.Spd));
			if (Wish > 0.0) Wish = std::min(Wish, std::sqrt(2.0 * Brake * std::max(0.0, Stop - KeelShort - H.Keel)));
			else if (Wish < 0.0)
			{
				// (rising: the most she may rise at to stop where she must)
				const double ToGo = std::max(0.0, H.Keel - Stop - KeelShort);
				const double SoftGo = Soft * Soft / (2.0 * Brake);
				const double Room = !bForTop || ToGo <= SoftGo ? std::sqrt(2.0 * Brake * ToGo) : std::sqrt(Soft * Soft + 2.0 * KeelTopBrake * (ToGo - SoftGo));
				Wish = -std::min(-Wish, Room);
			}
			// (her rate taken up and taken off at KeelTakeUp; rising for her trim faster than Soft, taken off as fast as the
			// sea brakes her, down to Soft)
			double Rate = H.KeelRate + Clamp(Wish - H.KeelRate, -KeelTakeUp * Dt, KeelTakeUp * Dt);
			if (bForTop && Wish > H.KeelRate && H.KeelRate < -Soft)
				Rate = std::max(Rate, std::min(std::min(Wish, -Soft), H.KeelRate + KeelTopBrake / KeelBrake * Dt));
			H.KeelRate = Rate;
			const double Before = H.Keel;
			H.Keel = Clamp(H.Keel + H.KeelRate * Dt, Up, Deepest);
			if (H.Keel <= Up && H.KeelRate < 0.0) H.KeelRate = 0.0;
			OutSink = Dt > 0.0 ? (H.Keel - Before) / Dt : 0.0;
			Check.MostKeel = std::max(Check.MostKeel, H.Keel);
			return std::exp(-std::max(0.0, H.Keel - Up) / 8.0);
		}
		// Her depth follows the battle's at her own rate, the rate itself eased (a depth told as it changes, a view at a
		// time, is followed without a stop at each; the pre-check, 22 November): a crash dive puts her casing under and her
		// at periscope depth in about 27 s, then about 1 m/s deeper; she comes up at about 1.2 m/s (tanks blown), and from
		// periscope depth to the surface in about 20 s; she eases off as she comes to the depth she wants (braking at
		// KeelBrake of her easing, so that her rate can follow it down and she comes onto it without a stop from her way).
		const double Left = H.KeelTo - H.Keel;
		const bool bDown = Left > 0.0;
		const bool bShallow = bDown ? H.Keel < F.ScopeKeel : H.Keel <= F.ScopeKeel;
		const double Band = std::max(1.0, F.ScopeKeel - Up);
		const double Most = bShallow ? Band / (bDown ? 27.0 : 20.0) * 1.3 : bDown ? 1.0 : 1.2;
		const double Ease = 0.08; // (metres a second, each second)
		const double WantRate = (bDown ? 1.0 : -1.0) * std::min(Most, std::sqrt(2.0 * KeelBrake * Ease * std::fabs(Left)));
		H.KeelRate += Clamp(WantRate - H.KeelRate, -Ease * Dt, Ease * Dt);
		const double Was = H.Keel;
		H.Keel += H.KeelRate * Dt;
		// (on the depth she wants, or past it: there she stops)
		if ((Left >= 0.0 && H.Keel >= H.KeelTo) || (Left <= 0.0 && H.Keel <= H.KeelTo))
		{
			H.Keel = H.KeelTo;
			H.KeelRate = 0.0;
		}
		H.Keel = Clamp(H.Keel, Up, std::max(Up, F.CrushKeel));
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
			Charge C = Charges[K];
			Charges.erase(Charges.begin() + static_cast<long>(K));
			if (C.bAtOnce) PatternBurstAt[C.Piece] = { C.Bursts, C.Boat };
			if (C.bOnBoat)
			{
				if (const Hull* Boat = NearestOf(C.Boat, C.At.X, C.At.Y))
				{
					C.At.X = Boat->P.X + C.Off.X;
					C.At.Y = Boat->P.Y + C.Off.Y;
					if (Boat->Keel > 0.0) C.At.Z = -Boat->Keel;
				}
			}
			Fx& E = Emit(FxKind::ChargeBurst, C.At, C.Kind, C.Escort);
			E.Size = C.bHit ? 1.0 : 0.0;
			E.Side = C.Side;
			E.Aim = C.Laid;
			E.Until = C.Due;
			++Check.ChargeBursts;
			if (C.bHit) ++Check.ChargeBurstsBooked;
			// (the battle's blow found her where it burst, which is where the picture shows her: SetMine)
			if (C.bHit)
			{
				if (const Hull* Boat = NearestOf(C.Boat, C.At.X, C.At.Y)) SightHit(*Boat);
			}
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
			const Vec ToWant = Goal - L.P;
			// (a boat deep, out of every ship's way at the surface (KeepClear), holds the battle's place itself, where its
			// hunters drop on her: she comes back across to it over her own length, not two and a half, at up to 60 degrees;
			// and where the battle has turned her round (the convoy field reverses a boat in a few seconds at 1.5 m/s), her
			// station fallen more than 60 degrees off her bow and the battle's course more than 90 off hers (not her station
			// astern on her own course, where she eases her way for it), she comes round for her station, steering for it, at
			// her best way dived, until it is off her bow or near: her turning circle is her class's at any way above a crawl,
			// and eased to a crawl as her station falls astern, her rate of turn is held at its least and she runs off in a
			// wide circle, 170 m from where she is attacked. The review of 24 November, the second)
			const bool bDeep = &L != &C.Ghost && HeldDeep(L);
			// (where it turns her round at a crawl as her hunter runs over her, and runs her back along her own track to his
			// next attack some 30 s on, she cannot come round inside her circle, 140 m across at any way: she goes astern on
			// her motors, her stern on the battle's course, at its speed, while an escort has her or has had her within the
			// minute (its echo on her piece); and ahead again once the battle's course is back before her beam, or she is
			// deep no longer. With no escort on her for a minute she comes round as above.)
			if (bDeep)
			{
				for (const auto& Pr : Now)
				{
					const Piece& By = Pr.second;
					if (By.Side != L.Side && By.Ships > 0 && By.EchoOn == L.Piece && By.EchoKind != "bearing")
					{
						L.HeldAt = ShownT;
						break;
					}
				}
			}
			const double Crawl = 2.0;
			const double OnBow = std::cos(Face - L.Hdg);
			const bool bHeld = L.HeldAt >= 0.0 && ShownT - L.HeldAt < 60.0;
			const bool bWasAstern = L.bAstern;
			if (!bDeep || Speed > Crawl || !bHeld || OnBow > 0.0) L.bAstern = false;
			else if (!L.bAstern && OnBow < -0.87 && L.Spd < Crawl + 0.5) L.bAstern = true;
			// (coming round only where the battle has turned her round, its course more than a right angle off her heading, and
			// then on round until her station is off her bow or near, or the course within 20 degrees of hers: where the course
			// is a few degrees off hers and her station has fallen a little astern of her, as a zig-zag's swing leaves it, she eases
			// her way for it as a ship does, never circling at her best way round a station inside her turning circle)
			const double Apart = Flat(ToWant);
			const bool bOffBow = Apart > 0.1 * Sz.Long && ToWant.Dot(Ahead(L.Hdg)) < 0.5 * Apart;
			if (!bDeep || L.bAstern || Speed <= 0.5 || !bOffBow || OnBow >= 0.94) L.bRounding = false;
			else if (OnBow < 0.0) L.bRounding = true;
			// (going astern no longer because the battle has swung its course across her beam, her station beside her and
			// drawing away at the battle's speed: she steers for it, for where it will be as she closes it at her way (CatchLead
			// s ahead at the most), her way the battle's and CatchGain more a metre it lies ahead of her, eased as it lies abaft
			// her beam so that she turns short, until she is within CatchNear m of it with the course within 60 degrees of her
			// heading; and from there she keeps it as above. Coming round for it in her circle at a crawl, 140 m across, she
			// was drawn 40 m off her place as her hunter threw his ahead salvo at it. The review of 24 November, the second)
			if (!bDeep || L.bAstern || Speed <= 0.5) L.bCatching = false;
			else if (bWasAstern) L.bCatching = true;
			else if (Apart < CatchNear && OnBow >= 0.5) L.bCatching = false;
			const bool bRound = L.bRounding && !L.bCatching;
			const double Reach = std::max(Speed * 20.0, (bDeep ? 1.0 : 2.5) * Sz.Long);
			const double Cut = (bDeep ? 60.0 : 40.0) * Deg;
			const double Hope = L.bCatching ? Bearing(L.P, Goal + GoalV * Clamp(Apart / std::max(1.0, std::fabs(L.Spd)), 0.0, CatchLead))
				: Flat(ToWant) > 1500.0 || bRound ? Bearing(L.P, Goal + GoalV * (bRound ? 10.0 : 6.0))
				: Wrap(Face + Clamp(std::atan2(ToWant.Dot(RightOf(Face)), Reach), -Cut, Cut));
			// Keeping clear of his own line: a ship of it that would pass nearer than most of its spacing within the next
			// 40 s he turns away from, astern of her where she crosses, to starboard where she comes on head on, away from
			// her where she lies alongside; the harder the nearer. (Every other ship, his and every ship's alike: ClearWay,
			// below. Judged here by centres a ship's length apart, alongside another column he was held off far wider than
			// she held off him, and fell 100 m off his ghost coming back from a dodge.)
			double Way = 0.0, Urgent = 0.0;
			{
				// (judged on the way he is going and on the way he means to go, whichever passes nearer: a leader turning
				// back across his own column's track sees the ship he would cut across before he is on her)
				const Vec Mine = Ahead(L.Hdg) * std::max(L.Spd, 1.0);
				const Vec Meant = Ahead(Hope) * std::max(L.Spd, 1.0);
				for (const auto& Pair : Hulls)
				{
					const Hull& O = Pair.second;
					if (O.Key == L.Key || O.Doing == Fate::Gone || O.Piece != L.Piece) continue;
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
					const double Clear = std::max(Sz.Long + 30.0, 0.8 * C.Spacing);
					const double D = Flat(Closest);
					if (D >= Clear || Rel.Dot(Ahead(L.Hdg)) < 0.0) continue;
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
			double Steer = Wrap(Hope + Way * Urgent * 70.0 * Deg);
			const double Behind = ToWant.Dot(Ahead(L.Hdg));
			// (a boat dived makes her best speed on her batteries, ruling 241)
			const double Top = C.Class == "ss" && L.Keel > SurfacedKeel(C.Class) + 1.0 ? std::min(Sz.Speed * 1.45, F.DivedTop) : Sz.Speed * 1.45;
			double WantSpd = bRound ? Top : Clamp(Speed + Clamp(Behind * (L.bCatching ? CatchGain : 0.06), -0.4 * Speed, 4.0), 0.0, Top);
			if (L.bAstern)
			{
				// (going astern: her stern on the battle's course, bent toward her station as her bow would be going ahead;
				// her way astern the battle's speed, a little more or less to close what she lies short of it or past it)
				const double Sternward = -ToWant.Dot(Ahead(L.Hdg));
				Steer = Wrap(Face + Clamp(std::atan2(ToWant.Dot(RightOf(Face)), Reach), -Cut, Cut) + Pi);
				WantSpd = -Clamp(Speed + Clamp(Sternward * 0.06, -0.4 * Speed, 1.0), 0.0, Crawl + 0.5);
			}
			// (the ship herself keeps clear of every other: not his ghost, which only marks the column's course)
			if (&L != &C.Ghost) ClearWay(L, Sz, Acc, Dt, Steer, WantSpd);
			// (her rate of turn held to the way she will have at the end of the step, where she is losing way: a boat dived
			// at a crawl, an escort stopping among survivors (ruling 241), a ship easing her way to keep clear)
			const double MostRate = MostRateAt(Sz, std::min(L.Spd, L.Spd + Clamp(WantSpd - L.Spd, -Acc * Dt, Acc * Dt)));
			const double WantRate = Clamp(Wrap(Steer - L.Hdg) * 0.6, -MostRate, MostRate);
			const double Bite = MostRudder * std::max(1.0, std::pow(L.Spd / std::max(1.0, Sz.Speed), 2.0));
			L.Rate += Clamp(WantRate - L.Rate, -Bite * Dt, Bite * Dt);
			L.Rate = Clamp(L.Rate, -MostRate, MostRate);
			L.Hdg = Wrap(L.Hdg + L.Rate * Dt);
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
				// (hard over or combing, she still keeps clear of the ships about her)
				double ClearHdg = Wrap(Lead->Hdg + 2.0 * Wish), ClearSpd = C.Ghost.Spd;
				ClearWay(*Lead, Sz, Acc, Dt, ClearHdg, ClearSpd);
				if (Lead->bClearing) Wish = Clamp(0.5 * Wrap(ClearHdg - Lead->Hdg), -Most, Most);
				const double Bite = MostRudder * std::max(1.0, std::pow(Lead->Spd / std::max(1.0, Sz.Speed), 2.0));
				Lead->Rate += Clamp(Wish - Lead->Rate, -Bite * Dt, Bite * Dt);
				Lead->Rate = Clamp(Lead->Rate, -Most, Most);
				Lead->Hdg = Wrap(Lead->Hdg + Lead->Rate * Dt);
				Lead->Spd += Clamp(ClearSpd - Lead->Spd, -Acc * Dt, Acc * Dt);
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
			if (Lead->DodgeJudge >= 0.0 && !bOn && ShownT >= Lead->DodgeJudge && ShownT - Lead->ClearLast < 30.0 && ShownT < Lead->DodgeUntil + 480.0)
			{
				// (keeping clear of another ship on her way back: judged once she has room, within eight minutes of the attack)
				Lead->DodgeJudge = ShownT + 30.0;
				++Check.DodgesClearing;
			}
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
		// (held off his station keeping clear of other ships, as in a crowd of destroyers running in together: his own
		// count, and the station's once he has had two minutes clear to come back to it)
		Check.MostLeadClearing = std::max(Check.MostLeadClearing, Flat(Course.P - Want));
		if (ShownT > Lead->ClearLast + 120.0 && Flat(Course.P - Want) > Check.MostLead)
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
			if (H->bLyingOff)
			{
				// Sent to pick men out of the water (the battle's sea:rescue): out of the wake, she makes for a spot off the
				// edge of the men and their rafts on her side of them, slowing to lie stopped there; when done, back to her
				// place astern of the ship ahead of her, and into the line once there. (Her place is the battle's piece's:
				// she is still of it, and the ships astern keep theirs.)
				const ClassSize& Own = SizeOf(H->Class);
				const Wreck* W = H->LieFor >= 0 && H->LieFor < static_cast<int>(Wrecked.size()) ? &Wrecked[static_cast<size_t>(H->LieFor)] : nullptr;
				const bool bServing = W && W->Rescuer == H->Key && ShownT < W->RescueUntil;
				const Hull* Ahead1 = K == 1 ? Lead : FindHull(C.Line[K - 1]);
				Vec Spot;
				double Hd = H->Hdg, Sp = 0.0;
				bool bBack = false;
				if (bServing)
				{
					const Vec Mid = W->Spread > 0.0 ? W->Middle : W->OilEnd;
					const double Off = std::max(Clamp(0.5 * W->Spread, 60.0, 200.0), W->bGone ? 0.0 : 0.5 * W->Long + 0.5 * Own.Beam + 40.0);
					Spot = Mid + Vec(H->P.X - Mid.X, H->P.Y - Mid.Y, 0.0).Unit(Ahead(H->Hdg + Pi)) * Off;
					const double D = Flat(Spot - H->P);
					Sp = Clamp((D - 10.0) / 40.0, 0.0, std::min(10.0, Own.Speed));
					if (D > 20.0) Hd = Bearing(H->P, Spot);
				}
				else if (Ahead1)
				{
					Spot = Ahead1->P - Ahead(Ahead1->Hdg) * C.Spacing;
					const double D = Flat(Spot - H->P);
					Sp = Clamp(Ahead1->Spd + D / 30.0, 0.0, Own.Speed * 1.05);
					Hd = D > 30.0 ? Bearing(H->P, Spot) : Ahead1->Hdg;
					bBack = D < 25.0 && std::fabs(Wrap(H->Hdg - Ahead1->Hdg)) < 10.0 * Deg;
				}
				const double Rudder = Own.Turn * 1.25 / 3.0;
				ClearWay(*H, Own, 0.15, Dt, Hd, Sp);
				const double Most = MostRateAt(Own, H->Spd);
				const double Rate = Clamp(Wrap(Hd - H->Hdg) * 0.4, -Most, Most);
				H->Rate += Clamp(Rate - H->Rate, -Rudder * Dt, Rudder * Dt);
				H->Hdg = Wrap(H->Hdg + H->Rate * Dt);
				H->Spd += Clamp(Sp - H->Spd, -AbandonedSlowing * Dt, 0.15 * Dt);
				H->P += Ahead(H->Hdg) * (H->Spd * Dt);
				if (bBack)
				{
					// (in her place again: on his track there, closed up, her judging of her gap put off as after keeping clear)
					H->bLyingOff = false;
					H->LieFor = -1;
					H->Back = Want0;
					H->SOn = SLead - Want0;
					H->OffTrack = 0.0;
					H->ClearLast = ShownT;
				}
				continue;
			}
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
			// Keeping clear of every other ship (her next ahead and astern by the line's own rule, above): the heading her
			// helm is steering for, altered as she must, and her way eased.
			double Most2 = Most;
			{
				double ClearHdg = Wrap(H->Hdg + 2.0 * Wish);
				ClearWay(*H, Sz, Acc, Dt, ClearHdg, WishSpd);
				// (her rate held to the way she will have at the end of the step, where she is losing way)
				Most2 = MostRateAt(Sz, std::min(H->Spd, H->Spd + Clamp(WishSpd - H->Spd, -Acc * Dt, Acc * Dt)));
				Wish = H->bClearing ? Clamp(0.5 * Wrap(ClearHdg - H->Hdg), -Most2, Most2) : Clamp(Wish, -Most2, Most2);
			}
			const double HerBite = MostRudder * std::max(1.0, std::pow(H->Spd / std::max(1.0, Sz.Speed), 2.0)); // (her rudder bites harder at more way)
			H->Rate += Clamp(Wish - H->Rate, -HerBite * Dt, HerBite * Dt);
			H->Rate = Clamp(H->Rate, -Most2, Most2);
			H->Hdg = Wrap(H->Hdg + H->Rate * Dt);
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
				if (ShownT >= H->DodgeJudge && Off > 60.0 && ShownT - H->ClearLast < 30.0 && ShownT < H->DodgeUntil + 480.0)
				{
					// (keeping clear of another ship on her way back: judged once she has room, within eight minutes of the attack)
					H->DodgeJudge = ShownT + 30.0;
					++Check.DodgesClearing;
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
			else if (std::fabs(H->Back - Want0) < 1.0 && ShownT > H->ClearLast + 120.0 && std::fabs(Across) > Check.MostGap)
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
			if (!InSight(H)) continue; // (an enemy boat not found keeps her guns quiet: her tracer would give her away, SetMine)
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

	// ---------------------------------------------------------------------------------------------------------------
	// Keeping clear (Brian, 22 November 05:31Z: "the ships are turning into each other and going over each other"; 05:32Z:
	// "make it realistic"). The battle's pieces are points, and it may lay two of one side within half a ship's length of
	// each other (Samar: 2,429 piece-views, as near as 5 m); drawn, every ship keeps her own water. Each looks 45 s ahead
	// along the course and way she wishes, her helm and way followed through as they would answer; where she would come
	// within her clearance of a ship she gives way to, she takes the least alteration that keeps her clear (a turn, to
	// starboard before port, her way eased, or both), and comes back to her place once clear. A ship that stands on holds
	// her course and way, and acts herself only at the last (12 s ahead). A collision is the battle's to decide
	// (sea:collision); the show never makes one.

	bool Show::GivesWay(const Hull& H, const Hull& O) const
	{
		// (one who cannot keep clear is kept clear of: going down, or hurt and out of the line, her helm slow)
		if (O.Doing == Fate::Sinking) return true;
		if (H.Doing == Fate::Sinking) return false;
		if ((O.Doing == Fate::Limping) != (H.Doing == Fate::Limping)) return O.Doing == Fate::Limping;
		// (in one line, the ship astern keeps clear of those ahead of her: they steam the line's course)
		if (O.Piece == H.Piece && H.Doing == Fate::Line && O.Doing == Fate::Line) return H.Back > O.Back || (H.Back == O.Back && H.Key > O.Key);
		// (the lighter ship keeps clear of the heavier: a destroyer works round a battle line, never through it)
		const double LH = SizeOf(H.Class).Long, LO = SizeOf(O.Class).Long;
		if (std::fabs(LH - LO) > 20.0) return LH < LO;
		// Between ships alike, the rule of the road: the ship overtaking keeps clear; meeting end on, both alter to
		// starboard; crossing, the ship that has the other on her own starboard side.
		const double Hers = Wrap(Bearing(O.P, H.P) - O.Hdg), His = Wrap(Bearing(H.P, O.P) - H.Hdg);
		if (std::fabs(Hers) > 112.5 * Deg && H.Spd > O.Spd) return true;
		if (std::fabs(His) > 112.5 * Deg && O.Spd > H.Spd) return false;
		if (std::cos(O.Hdg - H.Hdg) < -0.94 && std::fabs(His) < 20.0 * Deg) return true;
		return His > 0.0 || (His == 0.0 && H.Key > O.Key);
	}

	void Show::ClearWay(Hull& H, const ClassSize& Sz, double Acc, double Dt, double& Hdg, double& Spd)
	{
		// (a boat dived passes under, and is passed over: her hunters run over her to drop, ruling 241)
		auto Under = [this](const Hull& S) { return S.Class == "ss" && S.Keel > SurfacedKeel(S.Class) + 1.0; };
		if (H.Doing == Fate::Gone || H.Doing == Fate::Sinking || Under(H))
		{
			H.bClearing = false;
			return;
		}
		// (what she holds: her course to keep clear where she alters it, her way eased)
		auto Hold = [&]()
		{
			if (!H.bClearing) return;
			if (H.ClearOff != 0.0) Hdg = H.ClearAbs;
			Spd *= H.ClearKeep;
			H.ClearLast = ShownT;
			Check.KeptClear += Dt;
			if (!H.bEverCleared)
			{
				H.bEverCleared = true;
				++Check.ClearShips;
			}
		};
		// (with nothing to keep clear of, she looks about her ten times a second, each ship on her own beat; keeping clear,
		// every step)
		if (!H.bClearing && (std::llround(ShownT / Fixed) + H.Key) % 2 != 0)
		{
			Hold();
			return;
		}
		// (in her own line, her next astern keeps clear of her, not she of her; her next ahead she keeps clear of by a
		// little, the line's own rule holding her most of a spacing astern of her)
		int Mine = -1;
		const Column* Own = nullptr;
		{
			auto It = Lines.find(H.Piece);
			if (It != Lines.end())
			{
				Own = &It->second;
				for (size_t K = 0; K < Own->Line.size(); ++K)
					if (Own->Line[K] == H.Key) Mine = static_cast<int>(K);
			}
		}
		Nears.clear();
		for (const auto& Pair : Hulls)
		{
			const Hull& O = Pair.second;
			if (O.Key == H.Key || O.Doing == Fate::Gone || Under(O)) continue;
			if (std::fabs(O.P.X - H.P.X) > 1800.0 || std::fabs(O.P.Y - H.P.Y) > 1800.0) continue;
			bool bNextAhead = false, bFolded = false;
			if (Own && Mine >= 0 && O.Piece == H.Piece)
			{
				const int Last = static_cast<int>(Own->Line.size()) - 1;
				if (Mine < Last && Own->Line[Mine + 1] == O.Key)
				{
					// (her next astern keeps clear of her, unless the line has folded back on itself and he lies before her
					// beam, his track ahead of her: then she keeps clear of him as of her next ahead)
					if (std::fabs(Wrap(Bearing(H.P, O.P) - H.Hdg)) >= 90.0 * Deg) continue;
					bFolded = true;
				}
				bNextAhead = Mine > 0 && Own->Line[Mine - 1] == O.Key;
			}
			const ClassSize& OS = SizeOf(O.Class);
			bNextAhead = bNextAhead || bFolded;
			const bool bGive = bNextAhead || GivesWay(H, O);
			NearShip N;
			N.O = &O;
			N.Half = std::max(0.0, 0.5 * (OS.Long - OS.Beam));
			N.R = 0.5 * OS.Beam;
			N.V = Ahead(O.Hdg) * O.Spd;
			N.Fore = Ahead(O.Hdg) * N.Half;
			N.Horizon = bGive ? 45.0 : 12.0;
			N.Margin = bNextAhead ? 15.0 : bGive ? 35.0 : 12.0;
			// (none that could not come within reach in the time she looks ahead)
			const double Reach = (H.Spd * 1.45 + O.Spd + 2.0) * N.Horizon + 0.5 * (Sz.Long + OS.Long) + N.Margin + Sz.Beam;
			if (std::fabs(O.P.X - H.P.X) > Reach || std::fabs(O.P.Y - H.P.Y) > Reach) continue;
			Nears.push_back(N);
		}
		if (Nears.empty())
		{
			H.bClearing = false;
			return;
		}
		// How far inside her clearance of the others she would come on a course and way: her helm and way followed through
		// at her class's rate of turn and way gained or lost, a look every 3 s, the others held on their own; the nearer in
		// time, the more it counts. (Between looks two ships close by up to half a look's closing: counted in.)
		const double MyHalf = std::max(0.0, 0.5 * (Sz.Long - Sz.Beam)), MyR = 0.5 * Sz.Beam;
		const double Look = 3.0;
		auto Inside = [&](double ToHdg, double ToSpd, double Cap) -> double
		{
			double Sum = 0.0;
			Vec P = H.P;
			double A = H.Hdg, V = H.Spd;
			for (int I = 0; I <= 15; ++I)
			{
				const double T = I * Look;
				if (I > 0)
				{
					const double Most = MostRateAt(Sz, V) * 0.8 * Look;
					const double Turn = Clamp(Wrap(ToHdg - A), -Most, Most);
					const double V1 = V + Clamp(ToSpd - V, -Acc * Look, Acc * Look);
					P += Ahead(Wrap(A + 0.5 * Turn)) * (0.5 * (V + V1) * Look);
					A = Wrap(A + Turn);
					V = V1;
				}
				const Vec Fore = Ahead(A) * MyHalf, Mv = Ahead(A) * V;
				for (const NearShip& N : Nears)
				{
					if (T > N.Horizon) continue;
					const Vec OP = N.O->P + N.V * T;
					const double Need = MyR + N.R + N.Margin + 0.5 * Look * Flat(N.V - Mv);
					if (std::fabs(OP.X - P.X) > MyHalf + N.Half + Need || std::fabs(OP.Y - P.Y) > MyHalf + N.Half + Need) continue;
					const double Gap = SegGap(P + Fore, P - Fore, OP + N.Fore, OP - N.Fore);
					if (Gap < Need) Sum += (Need - Gap) * (1.0 - 0.7 * T / N.Horizon);
				}
				if (Sum > Cap) return Sum; // (already worse than a way in hand)
			}
			return Sum;
		};
		const double Wished = Inside(Hdg, Spd, 1e18);
		// (while she must keep clear, the alteration she holds is eased to a lesser one only once she has held it HoldFor
		// seconds; a greater one she takes at once: the pre-check, 22 November, found destroyers in a crowd swinging
		// between two alterations every five seconds, each taking her clear enough to choose the other)
		const double HoldFor = 15.0;
		if (Wished <= 0.0)
		{
			// (clear on the way she wishes: she takes it again, once she has been clear on it a few seconds)
			if (H.bClearing && ShownT >= H.ClearAt + 4.0) H.bClearing = false;
		}
		else if (!H.bClearing || (ShownT >= H.ClearAt + 1.0 && Rechosen < 2))
		{
			// (a ship holding an alteration chooses again each second, but no more than two in a step: the rest a step or two
			// later, so a crowd's choices do not all fall in one frame)
			if (H.bClearing) ++Rechosen;
			// The least alteration that keeps her clear: a degree of turn counts one, a turn to port ten more (the rule of
			// the road alters to starboard), easing her way to half twenty; being inside her clearance ten a metre-look.
			// (chosen again each second while she must; holding her last choice counts five less, and putting her helm
			// over the other way fifteen more, so she does not hunt)
			// (the alterations in order of what they cost before any nearness: once one in hand costs less than the rest
			// would before it, they are not looked at)
			struct Way
			{
				double Off, Keep, Price;
			};
			static const std::vector<Way> Ways = []()
			{
				std::vector<Way> W;
				for (const double Off : { 0.0, 10.0, -10.0, 20.0, -20.0, 30.0, -30.0, 45.0, -45.0, 60.0, -60.0, 90.0, -90.0 })
					for (const double Keep : { 1.0, 0.7, 0.4, 0.15 })
						W.push_back({ Off, Keep, std::fabs(Off) + (Off < 0.0 ? 10.0 : 0.0) + 40.0 * (1.0 - Keep) });
				std::stable_sort(W.begin(), W.end(), [](const Way& A, const Way& B) { return A.Price < B.Price; });
				return W;
			}();
			auto PriceOf = [](double Off, double Keep) { return std::fabs(Off) / Deg + (Off < 0.0 ? 10.0 : 0.0) + 40.0 * (1.0 - Keep); };
			const bool bHolding = H.bClearing;
			// (an alteration of course is held as a course, steady, not as so many degrees off a wish that swings as she
			// turns and as her line turns: what it costs now is how far it lies off her wish now)
			const bool bSteady = bHolding && H.ClearOff != 0.0;
			const double HeldOff = bSteady ? Wrap(H.ClearAbs - Hdg) : H.ClearOff;
			const double HeldPrice = bHolding ? PriceOf(H.ClearOff, H.ClearKeep) : 0.0;
			double Best = 1e18, BestOff = 0.0, BestKeep = 1.0, BestIn = Wished;
			bool bKeep = false;
			if (bHolding)
			{
				// (what she holds, first: the rest must do better than it)
				BestIn = Inside(Wrap(Hdg + HeldOff), Spd * H.ClearKeep, 1e18);
				Best = PriceOf(HeldOff, H.ClearKeep) - 5.0 + 10.0 * BestIn;
				BestOff = H.ClearOff;
				BestKeep = H.ClearKeep;
				bKeep = true;
			}
			const bool bMayEase = !bHolding || ShownT >= H.ClearSince + HoldFor;
			for (const Way& W : Ways)
			{
				const bool bOver = bHolding && W.Off * HeldOff < 0.0;
				const double Price = W.Price + (bOver ? 15.0 : 0.0);
				if (Price >= Best) continue;
				// (a lesser alteration than the one she holds: not before she has held it long; her own alteration has turned
				// her, and the lesser one looks clear from there only because the greater one is still taking her clear)
				if (!bMayEase && bHolding && W.Price < HeldPrice) continue;
				const double In = (W.Off == 0.0 && W.Keep == 1.0) ? Wished : Inside(Wrap(Hdg + W.Off * Deg), Spd * W.Keep, (Best - Price) / 10.0);
				const double Cost = Price + 10.0 * In;
				if (Cost < Best)
				{
					Best = Cost;
					BestOff = W.Off * Deg;
					BestKeep = W.Keep;
					BestIn = In;
					bKeep = false;
				}
			}
			if (!bKeep)
			{
				H.ClearSince = ShownT;
				H.ClearOff = BestOff;
				H.ClearKeep = BestKeep;
				H.ClearAbs = Wrap(Hdg + BestOff);
			}
			H.ClearAt = ShownT;
			H.bClearing = H.ClearOff != 0.0 || H.ClearKeep != 1.0;
		}
		Hold();
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
			bool bDeep = false;
		};
		// (a boat the battle has below periscope depth, and under as drawn, keeps no water at the surface: every ship passes
		// over her, as her hunters run over her to drop, ruling 241, and ClearWay has them. Her station is the battle's place
		// itself, pushed off nothing and pushing nothing off: so she is drawn where the battle attacks her, its drops laid
		// within 5 m of her, and not 120 m and more away (the review of 24 November, the second): HeldDeep)
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
			const Hull* Lead = FindHull(C.Line.front());
			St.bDeep = Lead && HeldDeep(*Lead);
			Stations.push_back(St);
		}
		for (Station& St : Stations)
		{
			if (St.bDeep) continue;
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
				if (Other.Id == St.Id || Other.bDeep) continue;
				From(Other.W, 0.5 * (St.Long + Other.Long) + 200.0, 0.5, St.Id > Other.Id);
			}
			const Column& Own = Lines[St.Id];
			for (const auto& Pair : Hulls)
			{
				const Hull& H = Pair.second;
				if (H.Doing == Fate::Gone || H.Key == Own.Line.front() || HeldDeep(H)) continue;
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
			// (a boat deep comes back to the battle's place faster, at 1.5 m/s: nothing at the surface is in her way)
			if (Flat(St.Push) > 1e-6) Goal = C.Keep + St.Push * (bSettle ? 1.0 / 3.0 : 1.0);
			else if (St.Room > 120.0 && Flat(C.Keep) > 0.0) Goal = C.Keep * std::max(0.0, 1.0 - (St.bDeep ? 1.5 : 0.6) * Dt / std::max(1.0, Flat(C.Keep)));
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
			// (a leader held back keeping clear of other ships, as in a crowd of destroyers running in together at their
			// best speed, could never close his station again: it is held back with him, no more than 300 m off him, and
			// eases back to the battle's place as above once he has room)
			if (!bSettle && !C.Line.empty() && !C.bGhost)
			{
				const Hull* Lead = FindHull(C.Line.front());
				if (Lead && ShownT - Lead->ClearLast < 1.0)
				{
					const Vec Lag(Lead->P.X - (St.W.X - Was.X + C.Keep.X), Lead->P.Y - (St.W.Y - Was.Y + C.Keep.Y), 0.0);
					const double D = Flat(Lag);
					if (D > 300.0) C.Keep += Lag * ((D - 300.0) / D);
				}
			}
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
				double Hd = H.Away, Sp = H.LimpSpd;
				ClearWay(H, Own, 0.15, Dt, Hd, Sp);
				const double Rate = Clamp(Wrap(Hd - H.Hdg) * 0.4, -Most, Most);
				H.Rate += Clamp(Rate - H.Rate, -Rudder * Dt, Rudder * Dt);
				H.Hdg = Wrap(H.Hdg + H.Rate * Dt);
				H.Spd += Clamp(Sp - H.Spd, -0.15 * Dt, 0.15 * Dt);
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
				// (losing her way, ahead or astern: a boat given up going astern stops as one going ahead does)
				const double Losing = (H.bAbandoned ? AbandonedSlowing : 0.12) * Dt;
				H.Spd = H.Spd >= 0.0 ? std::max(0.0, H.Spd - Losing) : std::min(0.0, H.Spd + Losing);
				const double Most = MostRateAt(Own, H.Spd);
				const double Rate = Clamp(Wrap(H.Away - H.Hdg) * 0.4, -Most, Most);
				H.Rate += Clamp(Rate - H.Rate, -Rudder * Dt, Rudder * Dt);
				H.Hdg = Wrap(H.Hdg + H.Rate * Dt);
				H.P += Ahead(H.Hdg) * (H.Spd * Dt);
				// (left by her men: empty and stopped, she drifts down the wind faster than they do)
				if (H.bAbandoned) H.P += Vec(WindV.X, WindV.Y, 0.0) * (AbandonedLeeway * Dt);
				// (a prize, taken: her vents shut by her boarders, she is held afloat as she is; her going carried on with the
				// clock, so her Down stands and she never goes)
				if (H.bPrize) H.SinkAt += Dt;
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
			H.DiveAngle = Sinking != 0.0 ? Clamp(-std::atan(Sinking / std::max(1.5, std::fabs(H.Spd))) / Deg, -12.0, 12.0) : 0.0;
			H.Pitch += H.DiveAngle;
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
		// The hits the battle has told of a boat's deck gun or a merchant's gun still to come, each firer's next (GunHitsDue):
		// her gun laid on the ship it strikes from as soon as it is known, and its round fired its flight before it, so the
		// hit lands as it is told with the round that carries it (Play draws it through that round).
		const std::map<int, const Event*> Due = GunHitsDue();
		for (auto& Pair : Lines)
		{
			const Piece* P = PieceNow(Pair.first);
			if (!P) continue;
			// A boat's deck gun and a merchant's gun fire only where the battle says they do (its "gun", view by view, and the
			// hits it tells of them); a warship's line on the piece it is engaged with, or, engaged with none, the one its own
			// "gun" is told on (a convoy's escort at a boat forced up: the convoy field tells its guns so, never "engaged").
			// (A boat or a merchant told "engaged" alone never opens fire: the show fires no gun the battle did not.)
			const bool bOneGun = P->Class == "ss" || P->Class == "mv";
			const int FoeId = bOneGun ? P->GunOn : P->Engaged >= 0 ? P->Engaged : P->GunOn;
			const Piece* Foe = FoeId >= 0 ? PieceNow(FoeId) : nullptr;
			// (a gun told on a piece, a boat's, a merchant's or an escort's, is on her ships wherever they lie afloat, in her
			// line or out of it: a straggler, or one her men have left, as the convoy field keeps an escort's gun on a boat
			// forced up whose crew abandon her until she goes. A warship engaged, on her line)
			const bool bAnyAfloat = bOneGun || P->Engaged < 0;
			// (her next hit told: the ship it strikes, afloat, nearest her piece, and the ship of hers nearest that one, whose
			// gun fires it)
			const auto Next = bOneGun ? Due.find(P->Id) : Due.end();
			const Event* Told = Next != Due.end() ? Next->second : nullptr;
			Hull* Struck = Told ? NearestAfloatOf(Told->Unit, P->X, P->Y) : nullptr;
			const Hull* Firer = Struck ? NearestOf(P->Id, Struck->P.X, Struck->P.Y) : nullptr;
			for (const int Key : Pair.second.Line)
			{
				Hull* H = FindHull(Key);
				if (!H) continue;
				H->bFiring = false;
				// (degrees a second; a merchant's gun 15: her stern gun comes round from where it lies stowed to the farthest
				// bearing its mount takes, MerchantGunArc, within the battle's own 10 s gun cycle, so the round the battle tells her
				// gunners firing is drawn while it tells them, her gun's mark held a cycle and a view. At a boat's 8, hunters'
				// Merchant 10, told on Boat 14 up on the surface from 2,040 s to 2,049 s ("Merchant 10's gunners open fire"), her
				// bearing 104 degrees off the stern gun as it lay stowed, drew no round: naval round 6a's recordings, 26 November)
				const double Train = H->Class == "bb" ? 3.0 : H->Class == "ca" ? 6.0 : H->Class == "mv" ? 15.0 : bOneGun ? 8.0 : 12.0;
				const bool bTold = H == Firer;
				Hull* On = nullptr;
				if (bTold) On = Struck;
				else if (Foe && H->Class != "cv")
				{
					On = H->AimKey >= 0 ? FindHull(H->AimKey) : nullptr;
					// (a boat's or a merchant's gun on a ship out of her line too while she is afloat: a straggler, or one her
					// men have left, as a deck gun most often finished one)
					if (!On || On->Piece != Foe->Id || !(bAnyAfloat ? StillAfloat(*On) : On->Doing == Fate::Line) || std::fmod(ShownT, 5.0) < Dt)
						On = bAnyAfloat ? NearestAfloatOf(Foe->Id, H->P.X, H->P.Y) : NearestOf(Foe->Id, H->P.X, H->P.Y);
				}
				// (a boat dived is no gun target: her hunters' weapon is the depth charge. But a warship's gun the battle tells
				// on a boat it has forced up, her rise told "surfaced", is laid on her as she comes up, its fire held until her
				// casing is clear, as an escort waits for the boat she has blown up: bearing on her as she comes clear, not
				// trained round to her from fore and aft after it; never on one the player has not found, InSight, whom it
				// would give away. The review of 24 November, the second: her hunter's next round after her casing came clear
				// fell 15 s on, his gun swung round from aft)
				bool bHold = false;
				if (On && On->Class == "ss" && On->Keel > SurfacedKeel(On->Class) + 1.0)
				{
					bHold = !bOneGun && P->GunOn == On->Piece && On->Dive == "surfaced" && On->KeelRate < 0.0 && !On->bStruck && InSight(*On);
					if (!bHold) On = nullptr;
				}
				// (nor one that has struck, her white flag up: no gun is laid on her but for a hit the battle tells, which it
				// never does once she has)
				if (On && On->bStruck && !bTold) On = nullptr;
				// (nor does a boat dived fire: her deck gun is under)
				if (On && H->Class == "ss" && H->Keel > SurfacedKeel(H->Class) + 1.0) On = nullptr;
				// (nor one told down, her casing still awash: at the diving alarm her gun's crew are below before the hatch
				// shuts; nor one coming up until her casing is clear, above)
				if (On && H->Class == "ss" && !H->Dive.empty() && H->Dive != "surfaced") On = nullptr;
				// (her gun's crew at it while it is laid: told on a ship she can fire at, she up)
				H->bGunLaid = bOneGun && On;
				// A boat's deck gun or a merchant's gun: one gun, trained by hand the short way round from where it lies stowed
				// fore and aft (Stow, degrees off her bow) no further than its mount turns either side of that (Arc), and fired
				// only on a target it bears on. A boat's sits forward of her tower and never fires through it (an American fleet
				// boat's, as the battle modeller's boat has it, aft of her tower: DeckGunArc). Of a merchant's bow and stern
				// guns her stern gun, her main one, takes a target it bears on (from 30 degrees off her bow round her stern:
				// MerchantGunArc), her bow gun only one finer on her bow than that, each laid afresh from where it lies stowed
				// (the one at her target changed over 3 degrees past the line between them, so neither swings to and fro on a
				// target near it).
				auto Around = [](double A) { return Wrap(A * Deg) / Deg; };
				const double Rel = On ? Wrap(Bearing(H->P, On->P) - H->Hdg) / Deg : 0.0;
				const double Fine = 180.0 - MerchantGunArc;
				double Stow = 0.0, Arc = 180.0;
				bool bBears = true;
				if (bOneGun)
				{
					if (H->Class == "ss")
					{
						// (her gun forward of her tower or, an American fleet boat's, aft of it: as her hull was made)
						Arc = DeckGunArc;
					}
					else
					{
						const bool bAft = !On ? H->bAftGun : H->bAftGun ? std::fabs(Rel) > Fine - 3.0 : std::fabs(Rel) >= Fine + 3.0;
						if (On && (bAft != H->bAftGun || H->AimKey < 0)) H->Train = bAft ? 180.0 : 0.0;
						H->bAftGun = bAft;
						Arc = MerchantGunArc;
					}
					Stow = H->bAftGun ? 180.0 : 0.0;
					bBears = std::fabs(Around(Rel - Stow)) <= Arc && (H->Class != "mv" || (H->bAftGun ? std::fabs(Rel) >= Fine : std::fabs(Rel) < Fine));
				}
				if (!On)
				{
					// Nothing to fire on: turrets back fore and aft.
					H->AimKey = -1;
					H->NextSalvo = -1.0;
					if (bOneGun)
					{
						const double Off = Clamp(Around(H->Train - Stow), -Arc, Arc);
						H->Train = Around(Stow + Off - Clamp(Off, -Train * 0.3 * Dt, Train * 0.3 * Dt));
					}
					else H->Train += Clamp(-H->Train, -Train * 0.3 * Dt, Train * 0.3 * Dt);
					H->Elev += Clamp(-H->Elev, -2.0 * Dt, 2.0 * Dt);
					continue;
				}
				if (bOneGun)
				{
					const double Off = Clamp(Around(H->Train - Stow), -Arc, Arc);
					const double Want = Clamp(Around(Rel - Stow), -Arc, Arc);
					H->Train = Around(Stow + Off + Clamp(Want - Off, -Train * Dt, Train * Dt));
				}
				else H->Train += Clamp(Rel - H->Train, -Train * Dt, Train * Dt);
				H->AimKey = On->Key;
				const double Range = Flat(On->P - H->P);
				// Elevation for the range: a gun's flat-fire angle (about 800 m/s at the muzzle; a boat's or a merchant's
				// lighter gun about 700), raised a little for drag.
				const double Mv = H->Class == "dd" ? 760.0 : bOneGun ? 700.0 : 820.0;
				const double WantElev = 0.5 * std::asin(Clamp(Gee * Range / (Mv * Mv), 0.0, 1.0)) / Deg * 1.6;
				H->Elev += Clamp(WantElev - H->Elev, -3.0 * Dt, 3.0 * Dt);
				if (bHold)
				{
					H->NextSalvo = -1.0;
					continue;
				}
				// Her guns fire: the shells in the air until they fall (LandShells, or Play for a round that carries a hit told:
				// ToldAt), the flash at her guns.
				auto Fire = [&](double Lands, double ToldAt)
				{
					H->NextSalvo = ShownT + Cadence(H->Class);
					H->bFiring = true;
					H->FiredAt = ShownT;
					// (guns laid on a boat, or hers firing, have her in sight: the flash and her hull come together, SetMine; a
					// round that carries a hit told on a boat her gun is not told on finds her no sooner than the hit does)
					Sight(*H);
					if (ToldAt < 0.0 || P->GunOn == On->Piece) Sight(*On);
					// (a boat's or a merchant's gun: the bore the battle names, else the table's)
					const double Cal = bOneGun && P->GunMm > 0.0 ? P->GunMm : Calibre(H->Class, H->Nation);
					Shells S;
					S.From = H->Key;
					S.Piece = H->Piece;
					S.OnPiece = On->Piece;
					S.Calibre = Cal;
					S.Class = H->Class;
					S.Count = H->Class == "bb" ? 3 : H->Class == "ca" ? 3 : bOneGun ? 1 : 2; // (one gun: one round)
					S.Fired = H->P;
					S.Lands = Lands;
					S.ToldAt = ToldAt;
					S.OnKey = ToldAt >= 0.0 ? On->Key : -1;
					S.bLaidAfloat = bAnyAfloat;
					Flying.push_back(S);
					const ClassSize& Sz = SizeOf(H->Class);
					// (a boat's gun on her casing, forward of her tower or aft of it; a merchant's on her forecastle or her poop)
					const Vec GunAt = !bOneGun ? PlaceOn(*H, 0.12, 0.0, Sz.Beam * 0.45)
					                : H->Class == "ss" ? PlaceOn(*H, H->bAftGun ? -0.18 : 0.26, 0.0, Sz.Beam * 0.6)
					                                   : PlaceOn(*H, H->bAftGun ? -0.47 : 0.45, 0.0, Sz.Beam * 0.5);
					Fx& E = Emit(FxKind::Salvo, GunAt, H->Class, H->Key);
					E.Aim = On->P;
					E.Size = Cal;
					E.Side = H->Side;
					if (bOneGun) E.Until = Lands;
					++Check.Salvos;
					if (H->Class == "ss") ++Check.DeckRounds;
					else if (H->Class == "mv") ++Check.MerchantRounds;
				};
				// A hit told of her gun: its round goes its flight before it (a boat's or a merchant's lighter shell about 560 m/s
				// over its arc, below), whatever her cadence, so it lands as it is told; laid on her by then, or else as her gun
				// lies (told too late for it to come round to her, or her in the blind arc her tower makes: the battle says she
				// hit her, and the hit has its round). None of hers in the few seconds before it: one gun. A boat the player has
				// not found, her gun not told by the view, fires no round he sees (SetMine: nothing of hers is shown before
				// anything the battle told finds her): her hit lands with no flash of hers, and finds her (Play).
				if (bTold)
				{
					// (a round of hers in the air on her as the hit comes to be known is the one that strikes: it lands as the hit
					// is told, at most a second after its own flight would have it, and no second round goes after it)
					Shells* InAir = nullptr;
					for (Shells& S : Flying)
					{
						if (S.From == H->Key && S.ToldAt < 0.0 && S.OnPiece == Told->Unit && Told->T - S.Lands <= 1.0 && (!InAir || S.Lands > InAir->Lands)) InAir = &S;
					}
					if (InAir)
					{
						InAir->Lands = Told->T;
						InAir->ToldAt = Told->T;
						InAir->OnKey = On->Key;
						++Check.GunHitRounds;
						continue;
					}
					const double Flight = Range / 560.0 + 0.5;
					if (ShownT + 1e-9 >= Told->T - Flight)
					{
						if (P->GunOn < 0 && !InSight(*H)) continue;
						++Check.GunHitRounds;
						if (!bBears || std::fabs(Around(Rel - H->Train)) > 6.0) ++Check.GunHitRoundsHasty;
						Fire(Told->T, Told->T);
						continue;
					}
					if (Told->T - Flight - ShownT < 3.0) continue;
				}
				// (a gun laid on a ship for a hit told fires no other round on her the view does not tell)
				if (bTold && P->GunOn != On->Piece) continue;
				if (H->NextSalvo < 0.0) H->NextSalvo = ShownT + Roll.U(1.5, Cadence(H->Class));
				// (laid on her: a target out of a gun's arc is never fired at, its gun held at the arc's edge)
				if (ShownT < H->NextSalvo || !bBears || std::fabs(bOneGun ? Around(Rel - H->Train) : Rel - H->Train) > 6.0) continue;
				// Time of flight: about 650 m/s over the shell's arc (a battleship's at 5 km about 8 s; a destroyer's, a boat's
				// and a merchant's lighter shells slower).
				Fire(ShownT + Range / (H->Class == "dd" || bOneGun ? 560.0 : 650.0) + 0.5, -1.0);
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
			// (a boat's deck gun's or a merchant's gun's round, or a warship's told on her piece, falls on the ship it was laid
			// on in her line or out of it, a straggler or one her men have left, as Gunnery lays it: NearestAfloatOf)
			const bool bOneGun = Fell.Class == "ss" || Fell.Class == "mv";
			const bool bAfloatToo = bOneGun || Fell.bLaidAfloat;
			Hull* On = bAfloatToo ? NearestAfloatOf(Fell.OnPiece, Fell.Fired.X, Fell.Fired.Y) : NearestOf(Fell.OnPiece, Fell.Fired.X, Fell.Fired.Y);
			if (!On) continue;
			const std::string Weapon = Fell.Calibre >= 300.0 ? "shell_heavy" : Fell.Calibre >= 180.0 ? "shell_medium" : "shell_light";
			// (a boat's deck gun's or a merchant's gun's round, of any bore a deck gun has to a Japanese boat's 14 cm, throws a
			// smaller column than a warship's shell of a bore near it)
			const std::string Splash = bOneGun && Fell.Calibre <= 140.0 ? "small" : Fell.Calibre >= 300.0 ? "heavy" : Fell.Calibre >= 180.0 ? "medium" : "light";
			const double Range = Flat(On->P - Fell.Fired);
			const Vec Down = (On->P - Fell.Fired) * (1.0 / std::max(1.0, Range));
			const Vec Across(Down.Y, -Down.X, 0.0);
			// The salvo's middle: over or short of her, a little to one side (a straddle now and then).
			const Vec Middle = On->P + Down * (Roll.Bell() * (0.012 * Range + 40.0)) + Across * (Roll.Bell() * (0.003 * Range + 10.0));
			Owed& D = Debt[Fell.OnPiece];
			int Hits = 0;
			// (a boat gone under while the round was in the air takes no hit: what she is owed waits until she is up again; a
			// boat's or a merchant's round, or a warship's told on her piece, draws what a straggler or a ship abandoned is owed
			// too, where she lies afloat; a round that carried a hit told is drawn as told, Play, and one here is one whose hit
			// she was under for: a miss)
			const bool bUnder = On->Class == "ss" && On->Keel > SurfacedKeel(On->Class) + 1.0;
			if (Fell.ToldAt < 0.0 && !bUnder && D.Hits >= 0.5 && (On->Doing == Fate::Line || (bAfloatToo && StillAfloat(*On))))
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
				On->BlowAt = ShownT;
				++Check.ShellHits;
			}
			if (Hits > 0)
			{
				SightHit(*On); // (a boat struck is found: SetMine)
				On->ShakenAt = ShownT;
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
				// (a boat's or a merchant's one round throws a light column, which takes none of the kit's room for the big
				// ones: as a ship's anti-aircraft gun's splash, GunShots)
				if (!bOneGun && !Columns.Take(ShownT, Flat(Fall - Eye))) continue;
				Fx& E = Emit(FxKind::Splash, Fall, Splash);
				E.Size = Fell.Calibre;
				++Check.Splashes;
				// (close aboard her: it shakes her, her gun's crew ducking)
				if (Flat(Fall - On->P) < Sz.Long * 0.5 + 30.0) On->ShakenAt = ShownT;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Torpedoes and aircraft.

	namespace
	{
		/** How far along a track's way (from Line, along Dir) it first lies Gap metres clear of a hull (a capsule: her middle
		 * at P heading Hdg, a half beam short of each end, her half beam round it) and only draws away from it after: the gap
		 * along a straight way to a capsule falls to its least and then only grows (its least, then the first Gap past it;
		 * none where the way never comes within Gap of her). */
		double ClearOf(const Vec& Line, const Vec& Dir, const Vec& P, double Hdg, double Long, double Beam, double Gap)
		{
			const Vec Half = Ahead(Hdg) * std::max(0.0, Long * 0.5 - Beam * 0.5);
			auto HerGap = [&](double Along) {
				const Vec Q = Line + Dir * Along;
				return SegGap(Q, Q, P - Half, P + Half) - Beam * 0.5;
			};
			double Lo = 0.0, Hi = Long + Flat(Line - P) + 40.0;
			const double Far = Hi;
			for (int Third = 0; Third < 40; ++Third)
			{
				const double M1 = Lo + (Hi - Lo) / 3.0, M2 = Hi - (Hi - Lo) / 3.0;
				if (HerGap(M1) <= HerGap(M2)) Hi = M2;
				else Lo = M1;
			}
			double In = Lo, Exit = Far;
			if (HerGap(In) >= Gap) return 0.0;
			for (int Halve = 0; Halve < 24; ++Halve)
			{
				const double Mid = (In + Exit) * 0.5;
				if (HerGap(Mid) < Gap) In = Mid;
				else Exit = Mid;
			}
			return Exit;
		}
	}

	bool Show::WayClear(const Vec& A, const Vec& B, int Side, int Except, double* Least, int OnlyPiece, std::vector<int>* Foul, int* First) const
	{
		bool bClear = true;
		const double LoX = std::min(A.X, B.X), HiX = std::max(A.X, B.X), LoY = std::min(A.Y, B.Y), HiY = std::max(A.Y, B.Y);
		for (const auto& Pair : Hulls)
		{
			const Hull& O = Pair.second;
			if (O.Side != Side || O.Key == Except || O.Doing == Fate::Gone || (OnlyPiece >= 0 && O.Piece != OnlyPiece)) continue;
			const ClassSize& Sz = SizeOf(O.Class);
			// (her hull a capsule: her middle line to a half beam short of each end, her half beam round it)
			const double Reach = Sz.Long * 0.5 + TrackClear + (Least ? 1000.0 : 0.0);
			if (O.P.X < LoX - Reach || O.P.X > HiX + Reach || O.P.Y < LoY - Reach || O.P.Y > HiY + Reach) continue;
			const Vec Half = Ahead(O.Hdg) * std::max(0.0, Sz.Long * 0.5 - Sz.Beam * 0.5);
			const double Gap = SegGap(A, B, O.P - Half, O.P + Half) - Sz.Beam * 0.5;
			if (Least) *Least = std::min(*Least, Gap);
			if (Gap < TrackClear)
			{
				bClear = false;
				if (Foul) Foul->push_back(O.Key);
				else if (!Least)
				{
					if (First && *First < 0) *First = O.Key;
					return false;
				}
			}
		}
		return bClear;
	}

	bool Show::InWater(const Held& R, const Vec& Q, double Pad, double* S) const
	{
		// The rules' water (the naval rules' own-water switch): the fan from the piece's place as the salvo was told, her half
		// beam wide there, to the run's radius either side of the aim; across it at right angles to the way from one to the
		// other.
		const Vec Axis = R.Aim - R.WaterP;
		const double Long = Flat(Axis);
		const Vec D = Q - R.WaterP;
		if (Long < 1.0)
		{
			if (S) *S = 0.0;
			return Flat(D) <= R.Radius + Pad;
		}
		const double UX = Axis.X / Long, UY = Axis.Y / Long;
		const double Along = D.X * UX + D.Y * UY, Across = std::fabs(D.X * UY - D.Y * UX);
		if (S) *S = Along;
		if (Along < -Pad || Along > Long + Pad) return false;
		return Across <= R.WaterHalf + (R.Radius - R.WaterHalf) * Clamp(Along / Long, 0.0, 1.0) + Pad;
	}

	bool Show::InWaterAhead(const Held& R, const Hull& O) const
	{
		// (her bow, middle and stern, her half beam round each, as the rules lay her: in the water, and not yet passed by its
		// heads as the battle's moment has them, a front across the water running from the piece's place to the aim in the
		// run's time)
		const ClassSize& Sz = SizeOf(O.Class);
		const double Front = Flat(R.Aim - R.WaterP) * Clamp((ShownT - R.Told) / std::max(1.0, R.Running), 0.0, 1.0);
		const Vec Half = Ahead(O.Hdg) * (Sz.Long * 0.5);
		const Vec Points[3] = { O.P - Half, O.P, O.P + Half };
		for (const Vec& Q : Points)
		{
			double S = 0.0;
			if (InWater(R, Q, Sz.Beam * 0.5, &S) && S >= Front - Sz.Beam * 0.5) return true;
		}
		return false;
	}

	void Show::PlacesOf(const Held& R, std::vector<FirePlace>& Out) const
	{
		Out.clear();
		const Hull* H = FindHull(R.Ship);
		const bool bHers = H && H->Doing == Fate::Line;
		// Her own place while she is in her line, until a third of the run is gone (her tubes: her own hull left out of the
		// tracks' way, they starting at her side); a boat's, at her bow tubes, for as long as she holds any (one boat, no
		// column: she fires from nowhere else).
		const bool bEarly = ShownT < R.Told + R.Running / 3.0 - 1e-9;
		if (bHers && (bEarly || R.bBoat)) Out.push_back({ H->P, H->Hdg, H->Key, false });
		// Instead, from then (or once she is out of her line), the salvo's firing point: the place the battle told it from,
		// where the rules' water starts; before then too for a tube whose track from her own place would not come into that
		// water within the look ahead (she lying too far off it for her own place ever to serve: no use waiting). Where she
		// lies at it her own hull is left out of the tracks' way too, they starting clear of her side.
		if (R.bBoat) return;
		int Except = -1;
		if (H && H->Doing != Fate::Gone)
		{
			const ClassSize& Sz = SizeOf(H->Class);
			const Vec Half = Ahead(H->Hdg) * std::max(0.0, Sz.Long * 0.5 - Sz.Beam * 0.5);
			if (SegGap(R.WaterP, R.WaterP, H->P - Half, H->P + Half) - Sz.Beam * 0.5 < TrackClear + 10.0) Except = H->Key;
		}
		Out.push_back({ R.WaterP, Bearing(R.WaterP, R.Aim), Except, true, bHers && bEarly });
	}

	bool Show::PlanTube(const Held& R, size_t Tube, const FirePlace& From, int OnlyPiece, TubeWay& W, bool bAll) const
	{
		W = TubeWay();
		const ClassSize& Sz = SizeOf(R.Class);
		const double Base = Bearing(From.P, R.Aim), Dist = Flat(R.Aim - From.P);
		const double Off = R.Ks[Tube];
		const double Way = Base + (Off * R.Fan + R.Scatter[Tube]) * Deg;
		W.Dir = Ahead(Way);
		// (a ship's tracks, her tubes' spread 4 m apart across their way: from her own place each starting where its way from
		// her middle is 4 m clear of her hull and only draws away from it after, off her side where it runs abeam, ahead of
		// her bow where it runs ahead, so none runs along inside her; from the salvo's firing point there, or clear of her
		// side as of her own where she lies at it; a boat's from her bow tubes, as before)
		const Hull* Her = From.Except >= 0 ? FindHull(From.Except) : nullptr;
		double Out = 0.0;
		if (R.bBoat) W.P = From.P + Ahead(From.Hdg) * (0.45 * Sz.Long) + RightOf(From.Hdg) * (Off * 0.6);
		else
		{
			// (spread across her way from her middle; across the water's own way at the firing point, none of them behind it)
			const Vec Line = From.P + RightOf(From.bPoint ? Base : Way) * (Off * 4.0);
			if (!From.bPoint) Out = ClearOf(Line, W.Dir, From.P, From.Hdg, Sz.Long, Sz.Beam, 4.0);
			else if (Her)
			{
				const ClassSize& HerSz = SizeOf(Her->Class);
				Out = ClearOf(Line, W.Dir, Her->P, Her->Hdg, HerSz.Long, HerSz.Beam, 4.0);
			}
			W.P = Line + W.Dir * Out;
		}
		// At the speed that brings it to the aim when the battle has it arrive (no slower than the floor; never slower than
		// the ship it leaves steams its way, so she never overruns her own tracks).
		const double ToAim = std::max(1.0, Dist - Out);
		double Speed = std::max(R.bBoat ? 8.0 : 18.0, ToAim / std::max(1.0, R.Arrive - ShownT));
		if (Her && !R.bBoat) Speed = std::max(Speed, Her->Spd * std::max(0.0, std::cos(Way - Her->Hdg)) + 3.0);
		W.Speed = Speed;
		W.AimAt = ShownT + ToAim / Speed;
		// (its run on of 600 m past the aim, none where an own ship lies in it; no longer than its shadow's, so the room holds,
		// but never short of the aim)
		const Vec AimP = W.P + W.Dir * ToAim;
		W.bCut = !WayClear(AimP, AimP + W.Dir * 600.0, R.Side, From.Except, nullptr, OnlyPiece);
		W.Ends = std::max(W.AimAt, std::min(W.AimAt + (W.bCut ? 0.0 : 600.0 / Speed), R.Ends[Tube]));
		W.End = W.P + W.Dir * (Speed * (W.Ends - ShownT));
		// Clear: in the rules' water as it starts from the salvo's firing point, or into it within the look ahead from her own
		// place (the rules keeping other ships out of the water ahead of its heads, their switch; a boat's spread is the
		// battle's, as before, her own heads watched over their whole run as every track's are); its line from where it
		// starts to where its run ends clear of every own hull now; and its head clear of each as she steams on, turning as
		// she is, her way a little faster or slower, over its whole run to the aim and the battle's moment (past both it is
		// watched as it runs on: she herself never runs or swings across her own track; a boat's own hull apart, her tracks
		// leaving her bow tubes). The cheapest asked first, and (not bAll) no more once one fails.
		W.bClear = true;
		if (From.bPoint)
		{
			if (!InWater(R, W.P, 0.25))
			{
				W.bOutside = true;
				W.bClear = false;
			}
		}
		else if (!R.bBoat)
		{
			const double Step = std::max(5.0, Speed * 0.25);
			for (double Along = 0.0;; Along += Step)
			{
				if (Along > ToAim + Step || Along / Speed > LookAhead)
				{
					W.bOutside = true;
					W.bClear = false;
					break;
				}
				if (InWater(R, W.P + W.Dir * std::min(Along, ToAim), 0.0)) break;
			}
		}
		if (!W.bClear && !bAll) return false;
		if (!WayClear(W.P, W.End, R.Side, From.Except, nullptr, OnlyPiece, bAll ? &W.Foul : nullptr, bAll ? nullptr : &W.FirstFoul)) W.bClear = false;
		if (!W.bClear && !bAll) return false;
		const double Run = std::max(0.0, std::min(W.Ends, std::max(W.AimAt, R.Arrive)) - ShownT);
		if (!HeadsClear(W.P, W.Dir, Speed, Speed * Run, R.Side, R.bBoat ? -1 : From.Except, R.bBoat ? From.Except : -1, OnlyPiece, bAll ? &W.Foul : nullptr,
		                bAll ? nullptr : &W.FirstFoul))
			W.bClear = false;
		return W.bClear;
	}

	const std::vector<std::vector<std::pair<Vec, double>>>& Show::Foreseen(const Hull& O, double Upto) const
	{
		// Her way as she steams on, a quarter second apart (the K-th (K + 1) steps on): a follower in her line along her
		// leader's track ahead of her (she turns where he turned, her offset from it kept); any other turning as she is, or
		// easing her helm (her rate held, three quarters, half, a quarter of it, or none; held until she has come round half
		// a circle, then straight on), each a way. Kept for the step, longer as it is asked for.
		if (ForeseenAt != ShownT)
		{
			ForeseenWays.clear();
			ForeseenAt = ShownT;
		}
		std::vector<std::vector<std::pair<Vec, double>>>& Ways = ForeseenWays[O.Key];
		const double Step = 0.25;
		const int Steps = static_cast<int>(std::ceil(Upto / Step));
		if (!Ways.empty() && static_cast<int>(Ways.front().size()) >= Steps) return Ways;
		const ClassSize& Sz = SizeOf(O.Class);
		const auto Col = Lines.find(O.Piece);
		const bool bTrack = O.Doing == Fate::Line && Col != Lines.end() && !Col->second.Line.empty() && Col->second.Line.front() != O.Key &&
		                    std::fabs(O.OffTrack) < Sz.Long && O.DodgeUntil <= ShownT;
		static constexpr double Helm[5] = { 1.0, 0.75, 0.5, 0.25, 0.0 };
		const int Count = bTrack || std::fabs(O.Rate) < 0.2 * Deg ? 1 : 5;
		Ways.resize(static_cast<size_t>(Count));
		for (int Fi = 0; Fi < Count; ++Fi)
		{
			std::vector<std::pair<Vec, double>>& Way = Ways[static_cast<size_t>(Fi)];
			const double Rate = O.Rate * Helm[Fi];
			Vec P = Way.empty() ? O.P : Way.back().first;
			double Hdg = Way.empty() ? O.Hdg : Way.back().second;
			Way.reserve(static_cast<size_t>(Steps));
			for (int K = static_cast<int>(Way.size()) + 1; K <= Steps; ++K)
			{
				if (bTrack)
				{
					Vec OnP;
					double OnH = O.Hdg;
					Along(Col->second, O.SOn + O.Spd * (Step * K), OnP, OnH);
					P = OnP - RightOf(OnH) * O.OffTrack;
					Hdg = OnH;
				}
				else
				{
					if (std::fabs(Rate) * Step * K <= Pi) Hdg += Rate * Step;
					P += Ahead(Hdg) * (O.Spd * Step);
				}
				Way.push_back({ P, Hdg });
			}
		}
		return Ways;
	}

	bool Show::HeadsClear(const Vec& Start, const Vec& Dir, double Speed, double Length, int Side, int Her, int Skip, int OnlyPiece, std::vector<int>* Foul, int* First) const
	{
		// Each ship of her side near its way as she steams on over the head's run (Foreseen), as much as WayDoubt slower or
		// faster than now (a second either way at the least; the ship it leaves as she is, her tracks outrunning her). The
		// head never within 5 m of her hull (3 m of the ship it left, it starting 4 m out of her).
		const double Horizon = Length / std::max(1.0, Speed);
		const Vec End = Start + Dir * Length;
		const double Step = 0.25;
		const int Steps = static_cast<int>(Horizon / Step + 1e-9);
		auto Crosses = [&](const Hull& O) {
			const ClassSize& Sz = SizeOf(O.Class);
			const double Doubt = O.Key == Her ? 0.0 : WayDoubt, Slack = O.Key == Her ? 0.0 : 1.0;
			const double Upto = Horizon * (1.0 + Doubt) + Slack;
			if (SegGap(O.P, O.P, Start, End) > Sz.Long * 0.5 + std::fabs(O.Spd) * Upto + 10.0) return false;
			const double Margin = O.Key == Her ? 3.0 : 5.0, Reach = std::max(0.0, Sz.Long * 0.5 - Sz.Beam * 0.5);
			{
				// (nor where the head at each moment lies further from her place now than she can steam by her latest place
				// looked at for that moment, her length and 10 m: the least over the run of the head's distance less her
				// way, which only falls and then rises, found by thirds)
				const double Way = std::fabs(O.Spd) * (1.0 + Doubt);
				auto Gap = [&](double T) { return Flat(Start + Dir * (Speed * T) - O.P) - Way * T; };
				double Lo = 0.0, Hi = Horizon;
				for (int Third = 0; Third < 30; ++Third)
				{
					const double M1 = Lo + (Hi - Lo) / 3.0, M2 = Hi - (Hi - Lo) / 3.0;
					if (Gap(M1) <= Gap(M2)) Hi = M2;
					else Lo = M1;
				}
				if (Gap((Lo + Hi) * 0.5) > Sz.Long * 0.5 + Margin + 1.0 + std::fabs(O.Spd) * (Slack + 2.0 * Step) + 10.0) return false;
			}
			const double Near = Sz.Long * 0.5 + Margin + 1.0;
			for (const std::vector<std::pair<Vec, double>>& Way : Foreseen(O, Upto))
			{
				const int Last = static_cast<int>(Way.size()) - 1;
				// (her place J steps on, J -1 now)
				auto At = [&](int J) -> const Vec& { return J < 0 ? O.P : Way[static_cast<size_t>(J)].first; };
				auto HdgAt = [&](int J) { return J < 0 ? O.Hdg : Way[static_cast<size_t>(J)].second; };
				for (int K = 0; K < Steps; ++K)
				{
					const double Tk = Step * (K + 1);
					const Vec Head = Start + Dir * (Speed * Tk);
					// (her places from (1 - doubt) to (1 + doubt) of the head's time: the stretch's middle further from it than
					// half the stretch and her reach, none of them near)
					const int A = std::clamp(static_cast<int>(std::floor(std::max(0.0, std::min(Tk * (1.0 - Doubt), Tk - Slack)) / Step + 1e-9)) - 1, -1, Last);
					const int B = std::clamp(static_cast<int>(std::ceil(std::max(Tk * (1.0 + Doubt), Tk + Slack) / Step - 1e-9)) - 1, -1, Last);
					const Vec& Mid = At((A + B) / 2);
					const double Span = std::fabs(O.Spd) * Step * (B - A) * 0.5 + Near + 1.0;
					const double MX = Head.X - Mid.X, MY = Head.Y - Mid.Y;
					if (MX * MX + MY * MY > Span * Span)
					{
						// (and the steps after it that cannot close the gap: a step the head closes her stretch by its own way, the
						// stretch's middle by at most a little over two of hers (its ends rounded out), and the stretch widens by
						// little over one; ten metres a second more for her offset from a turning track)
						const double Close = Step * (Speed + 4.0 * std::fabs(O.Spd) + 10.0);
						const int Leap = static_cast<int>((std::sqrt(MX * MX + MY * MY) - Span) / Close);
						if (Leap > 1) K += Leap - 1;
						continue;
					}
					for (int J = A; J <= B; ++J)
					{
						const Vec& P = At(J);
						const double DX = Head.X - P.X, DY = Head.Y - P.Y;
						if (DX * DX + DY * DY > Near * Near) continue;
						const Vec Half = Ahead(HdgAt(J)) * Reach;
						if (SegGap(Head, Head, P - Half, P + Half) - Sz.Beam * 0.5 < Margin) return true;
					}
				}
			}
			return false;
		};
		bool bClear = true;
		for (const auto& Pair : Hulls)
		{
			const Hull& O = Pair.second;
			if (O.Side != Side || O.Key == Skip || O.Doing == Fate::Gone || (OnlyPiece >= 0 && O.Piece != OnlyPiece)) continue;
			if (!Crosses(O)) continue;
			if (!Foul)
			{
				if (First && *First < 0) *First = O.Key;
				return false;
			}
			bClear = false;
			if (std::find(Foul->begin(), Foul->end(), O.Key) == Foul->end()) Foul->push_back(O.Key);
		}
		return bClear;
	}

	void Show::LaunchTube(Held& R, size_t Tube, const FirePlace& From, const TubeWay& W)
	{
		Torp T;
		T.Key = NextKey++;
		T.Side = R.Side;
		T.Piece = R.Piece;
		T.P = W.P;
		T.From = T.P;
		T.Way = W.Dir;
		T.Speed = W.Speed;
		T.Launched = ShownT;
		T.Ends = W.Ends;
		T.Aim = R.Aim;
		T.Ship = From.Except;
		T.bFromPoint = From.bPoint;
		T.Salvo = R.Salvo;
		T.End = W.End;
		T.AimAt = W.AimAt;
		T.Arrive = R.Arrive;
		T.Running = R.Running;
		Torps.push_back(T);
		Fx& S = Emit(FxKind::TrackStart, T.P, std::string(), T.Key);
		S.Vel = T.Way * T.Speed;
		S.Side = T.Side;
		S.Text = R.Nation; // (whose torpedo: the scene draws the Japanese ships' oxygen torpedoes' tracks faint)
		if (R.bWakeless) S.What = "wakeless";
		S.Aim = T.End;
		S.Until = T.Ends;
		S.Size = static_cast<double>(T.Ship);
		++Check.TracksRun;
		if (R.Salvo >= 1 && R.Salvo <= static_cast<int>(Check.SalvoTracks.size())) ++Check.SalvoTracks[static_cast<size_t>(R.Salvo - 1)];
		if (ShownT > R.Told + 1e-9)
		{
			++Check.TracksHeld;
			Check.MostHold = std::max(Check.MostHold, ShownT - R.Told);
		}
		if (From.bPoint) ++Check.TracksMoved;
		if (W.bCut) ++Check.TracksCut;
		// (the show's own measure: how far its line cleared the nearest own hull as it was fired)
		double Least = 1e9;
		WayClear(W.P, W.End, R.Side, From.Except, &Least, -1);
		Check.LeastTrackGap = std::min(Check.LeastTrackGap, Least);
		Check.MostTracks = std::max(Check.MostTracks, static_cast<int>(Torps.size()));
		// L5: she turns away under smoke as she fires her first, as destroyers did after their run (not a boat).
		Hull* H = FindHull(R.Ship);
		if (!R.bFiredAny && !R.bBoat && H && H->Doing != Fate::Gone) H->ScreenUntil = ShownT + 60.0;
		R.bFiredAny = true;
		R.Ks.erase(R.Ks.begin() + static_cast<std::ptrdiff_t>(Tube));
		R.Scatter.erase(R.Scatter.begin() + static_cast<std::ptrdiff_t>(Tube));
		R.Ends.erase(R.Ends.begin() + static_cast<std::ptrdiff_t>(Tube));
		R.RulesOnly.erase(R.RulesOnly.begin() + static_cast<std::ptrdiff_t>(Tube));
		if (Tube < R.Across.size()) R.Across.erase(R.Across.begin() + static_cast<std::ptrdiff_t>(Tube)); // (the words' note kept in step)
	}

	int Show::WhyHeld(const Held& R, size_t Tube, const std::vector<FirePlace>& From) const
	{
		// At every place she may fire from now (her own, the salvo's firing point): her own piece's ships alone foul them all
		// (0, the scene's own); else, at a place her own piece leaves clear, the ships of other pieces across its way all in
		// her salvo's water ahead of its heads (1, the rules' side: their switch keeps them out of it); else 2, the scene's
		// own (one out of the water or behind its heads, or none: the look ahead).
		bool bOwnFoul = true, bOwnOut = false;
		for (const FirePlace& P : From)
		{
			if (P.bIfOut && !bOwnOut) continue;
			TubeWay Own;
			if (!PlanTube(R, Tube, P, R.Piece, Own))
			{
				if (!P.bPoint && Own.bOutside) bOwnOut = true;
				continue;
			}
			bOwnFoul = false;
			TubeWay All;
			PlanTube(R, Tube, P, -1, All, true);
			bool bOther = false, bAllWater = true;
			for (const int Key : All.Foul)
			{
				const Hull* O = FindHull(Key);
				if (!O || O->Piece == R.Piece) continue;
				bOther = true;
				if (!InWaterAhead(R, *O)) bAllWater = false;
			}
			if (bOther && bAllWater) return 1;
		}
		return bOwnFoul ? 0 : 2;
	}

	void Show::DropTube(Held& R, size_t Tube, bool bRoom)
	{
		++Check.TracksDropped;
		// Judged over its hold: the rules' side where at any look the ships of other pieces in her salvo's water ahead of its
		// heads were all that stood in its way (had their switch kept them out of it, it would have been fired then); else as
		// it is given up, for her own piece's ships alone or the scene's own otherwise (the room among it).
		int Why = 2;
		if (!bRoom)
		{
			std::vector<FirePlace> From;
			PlacesOf(R, From);
			Why = R.RulesOnly[Tube] ? 1 : WhyHeld(R, Tube, From);
		}
		if (Why == 0) ++Check.TracksDroppedOwn;
		else if (Why == 1) ++Check.TracksDroppedWater;
		if (R.Salvo >= 1 && R.Salvo <= static_cast<int>(Check.SalvoDropped.size())) ++Check.SalvoDropped[static_cast<size_t>(R.Salvo - 1)];
		R.Ks.erase(R.Ks.begin() + static_cast<std::ptrdiff_t>(Tube));
		R.Scatter.erase(R.Scatter.begin() + static_cast<std::ptrdiff_t>(Tube));
		R.Ends.erase(R.Ends.begin() + static_cast<std::ptrdiff_t>(Tube));
		R.RulesOnly.erase(R.RulesOnly.begin() + static_cast<std::ptrdiff_t>(Tube));
		if (Tube < R.Across.size()) R.Across.erase(R.Across.begin() + static_cast<std::ptrdiff_t>(Tube)); // (the words' note kept in step)
		// (none of hers ever drawn: she turns away under smoke as she gives them up, L5)
		Hull* H = FindHull(R.Ship);
		if (R.Ks.empty() && !R.bFiredAny && !R.bBoat && H && H->Doing != Fate::Gone) H->ScreenUntil = ShownT + 60.0;
	}

	bool Show::FireTubes(Held& R)
	{
		// Each tube where its way is clear now, from the first place that has it (her own, then the salvo's firing point);
		// the room as the shadows keep it holds the drawn tracks too, a track that would pass it never drawn. One still held
		// notes whether the rules' ships alone are in its way now (its judging, if it is given up): once a second, as she
		// first looks and every fourth look after (the judging asks twice what the look asks, of every ship of her side).
		std::vector<FirePlace> From;
		PlacesOf(R, From);
		const bool bJudge = ShownT >= R.NextJudge - 1e-9;
		if (bJudge) R.NextJudge = ShownT + 1.0 - 1e-6;
		// (for the torpedo words, each tube still held: the piece of the first own ship found across its way, at the first
		// place a ship holds it; noted as she looks, never read in deciding)
		std::vector<int> Across;
		for (size_t Tube = 0; Tube < R.Ks.size();)
		{
			bool bFired = false, bOwnOut = false;
			int By = -1;
			for (const FirePlace& P : From)
			{
				if (P.bIfOut && !bOwnOut) continue;
				TubeWay W;
				if (!PlanTube(R, Tube, P, -1, W))
				{
					if (!P.bPoint && W.bOutside) bOwnOut = true;
					if (By < 0 && W.FirstFoul >= 0)
					{
						const Hull* O = FindHull(W.FirstFoul);
						if (O) By = O->Piece;
					}
					continue;
				}
				if (static_cast<int>(Torps.size()) >= TrackRoom) DropTube(R, Tube, true);
				else LaunchTube(R, Tube, P, W);
				bFired = true;
				break;
			}
			if (bFired) continue;
			if (bJudge && !R.RulesOnly[Tube] && WhyHeld(R, Tube, From) == 1) R.RulesOnly[Tube] = 1;
			Across.push_back(By);
			++Tube;
		}
		R.Across.swap(Across);
		return !R.Ks.empty();
	}

	void Show::TubesNow(std::map<int, TubesOut>& Out) const
	{
		Out.clear();
		auto TracksOf = [this](int Salvo) { return Salvo >= 1 && Salvo <= static_cast<int>(Check.SalvoTracks.size()) ? Check.SalvoTracks[static_cast<size_t>(Salvo - 1)] : 0; };
		// Each piece's latest salvo and the tracks drawn of it.
		for (const auto& Pair : LastSalvo)
		{
			TubesOut& T = Out[Pair.first];
			T.Salvo = Pair.second;
			T.Fired = TracksOf(Pair.second);
		}
		// Its tubes still held, the salvos they are of, and who stands across them as each ship last looked.
		std::map<int, std::vector<int>> HeldSalvos;
		std::map<int, std::map<int, int>> Votes;
		for (const Held& R : Holding)
		{
			if (R.Ks.empty()) continue;
			TubesOut& T = Out[R.Piece];
			T.Held += static_cast<int>(R.Ks.size());
			std::vector<int>& Of = HeldSalvos[R.Piece];
			if (std::find(Of.begin(), Of.end(), R.Salvo) == Of.end()) Of.push_back(R.Salvo);
			for (size_t K = 0; K < R.Ks.size() && K < R.Across.size(); ++K)
				if (R.Across[K] >= 0) ++Votes[R.Piece][R.Across[K]];
		}
		for (const auto& Pair : HeldSalvos)
		{
			TubesOut& T = Out[Pair.first];
			for (const int Salvo : Pair.second) T.HeldFired += TracksOf(Salvo);
			// (the piece across the most of its held tubes, another piece's ships before its own, then the lowest)
			const auto Vote = Votes.find(Pair.first);
			if (Vote == Votes.end()) continue;
			int Best = -1, Most = 0;
			for (const auto& One : Vote->second)
			{
				if (One.first == Pair.first) continue;
				if (One.second > Most) { Best = One.first; Most = One.second; }
			}
			T.Across = Best >= 0 ? Best : Vote->second.count(Pair.first) ? Pair.first : -1;
		}
	}

	Show::Torp* Show::TrackForHit(int Piece, const Hull& Struck)
	{
		// Of the piece's tracks running, by preference: one that has run a third of its salvo's run (the least a hit's track
		// has run) with its way on to her side clear of every hull of its side; then one that has run so long; then any. The
		// nearest her of the first kind there is.
		Torp* Best[3] = { nullptr, nullptr, nullptr };
		double BestD[3] = { 1e18, 1e18, 1e18 };
		for (Torp& T : Torps)
		{
			if (!T.bRunning || T.Piece != Piece) continue;
			const double D = Flat(Struck.P - T.P);
			const bool bRun = ShownT - T.Launched >= T.Running / 3.0 - 1e-6;
			bool bClear = bRun;
			for (auto It = Hulls.begin(); bClear && It != Hulls.end(); ++It)
			{
				const Hull& O = It->second;
				if (O.Side != T.Side || O.Doing == Fate::Gone) continue;
				const ClassSize& Sz = SizeOf(O.Class);
				const Vec Half = Ahead(O.Hdg) * std::max(0.0, Sz.Long * 0.5 - Sz.Beam * 0.5);
				if (SegGap(T.P, Struck.P, O.P - Half, O.P + Half) - Sz.Beam * 0.5 < 2.0) bClear = false;
			}
			const int Kind = bClear ? 0 : bRun ? 1 : 2;
			if (D < BestD[Kind]) { BestD[Kind] = D; Best[Kind] = &T; }
		}
		for (Torp* T : Best)
			if (T) return T;
		// None running (a last resort, the test holds it to none): a held tube of the piece, or one as the battle's moment's,
		// fired at her now from the longest clear stretch short of her side on the way from its firing point, to end there.
		Held* R = nullptr;
		for (Held& O : Holding)
			if (O.Piece == Piece && !O.Ks.empty()) { R = &O; break; }
		int Side = -1;
		std::string Nation;
		Vec From = Struck.P;
		if (R)
		{
			Side = R->Side;
			Nation = R->Nation;
			From = R->WaterP;
		}
		else
		{
			const auto Col = Lines.find(Piece);
			const Hull* Lead = Col != Lines.end() && !Col->second.Line.empty() ? FindHull(Col->second.Line.front()) : nullptr;
			if (!Lead) return nullptr;
			Side = Lead->Side;
			Nation = Lead->Nation;
			From = Lead->P;
		}
		if (Flat(Struck.P - From) < 1.0) From = Struck.P - Ahead(Struck.Hdg) * 500.0;
		const Vec Dir = Ahead(Bearing(From, Struck.P));
		const ClassSize& Sz = SizeOf(Struck.Class);
		const double Short[] = { 400.0, 300.0, 200.0, 120.0, 60.0, 20.0 };
		for (const double Stretch : Short)
		{
			const Vec Start = Struck.P - Dir * (Sz.Beam * 0.5 + 2.0 + Stretch);
			if (!WayClear(Start, Struck.P - Dir * (Sz.Beam * 0.5 + 2.0), Side, -1, nullptr, -1) && Stretch > 20.0) continue;
			Torp T;
			T.Key = NextKey++;
			T.Side = Side;
			T.Piece = Piece;
			T.P = T.From = Start;
			T.Way = Dir;
			T.Speed = 18.0;
			T.Launched = ShownT;
			T.Ends = ShownT + 1.0;
			T.Aim = Struck.P;
			T.End = Struck.P;
			T.AimAt = T.Arrive = ShownT;
			T.Running = 0.0;
			if (R) T.Salvo = R->Salvo;
			Torps.push_back(T);
			Fx& S = Emit(FxKind::TrackStart, T.P, std::string(), T.Key);
			S.Vel = T.Way * T.Speed;
			S.Side = T.Side;
			S.Text = Nation;
			if (R && R->bWakeless) S.What = "wakeless";
			S.Aim = T.End;
			S.Until = T.Ends;
			S.Size = -1.0;
			++Check.TubesAtHit;
			if (R)
			{
				++Check.TracksRun;
				if (R->Salvo >= 1 && R->Salvo <= static_cast<int>(Check.SalvoTracks.size())) ++Check.SalvoTracks[static_cast<size_t>(R->Salvo - 1)];
				R->bFiredAny = true;
				R->Ks.pop_back();
				R->Scatter.pop_back();
				R->Ends.pop_back();
				R->RulesOnly.pop_back();
				if (R->Across.size() > R->Ks.size()) R->Across.pop_back(); // (the words' note kept in step)
			}
			return &Torps.back();
		}
		return nullptr;
	}

	void Show::RunTorpedoes(double Dt)
	{
		// Held tubes (Brian, 25 November 12:52Z) fired where their way is now clear (FireTubes); any still held at two thirds
		// of their salvo's run are never drawn, each judged over its hold (DropTube): fired later, a track would not have run
		// the third of the run a hit's track has run.
		// (each ship looks every quarter second, her salvo's a step apart; the ships as they have steamed this step)
		ForeseenAt = -1.0;
		for (size_t K = 0; K < Holding.size();)
		{
			Held& R = Holding[K];
			bool bLeft = true;
			if (ShownT > R.Arrive - R.Running / 3.0 + 1e-9)
			{
				while (!R.Ks.empty()) DropTube(R, 0);
				bLeft = false;
			}
			else if (ShownT >= R.NextLook - 1e-9)
			{
				R.NextLook = ShownT + 0.25;
				bLeft = FireTubes(R);
			}
			if (bLeft) ++K;
			else Holding.erase(Holding.begin() + static_cast<std::ptrdiff_t>(K));
		}
		// (the shadows run as the tracks fired at the battle's moment ran: the room, the ship a hit strikes)
		for (Shadow& S : Shadows) S.P += S.Way * (S.Speed * Dt);
		Shadows.erase(std::remove_if(Shadows.begin(), Shadows.end(), [this](const Shadow& S) { return ShownT >= S.Ends; }), Shadows.end());
		for (Torp& T : Torps)
		{
			if (!T.bRunning) continue;
			T.P += T.Way * (T.Speed * Dt);
			bool bStop = ShownT >= T.Ends;
			// Past the aim and the battle's moment it runs on in the scene's own water: it stops short of an own ship come
			// across its way (the next two seconds of its run, her hull and 5 m).
			if (!bStop && ShownT >= std::max(T.AimAt, T.Arrive))
			{
				const Vec Ahead2 = T.P + T.Way * (T.Speed * 2.0);
				for (const auto& Pair : Hulls)
				{
					const Hull& O = Pair.second;
					if (O.Side != T.Side || O.Doing == Fate::Gone) continue;
					const ClassSize& Sz = SizeOf(O.Class);
					if (Flat(O.P - T.P) > Sz.Long * 0.5 + T.Speed * 2.0 + 10.0) continue;
					const Vec Half = Ahead(O.Hdg) * std::max(0.0, Sz.Long * 0.5 - Sz.Beam * 0.5);
					if (SegGap(T.P, Ahead2, O.P - Half, O.P + Half) - Sz.Beam * 0.5 < 5.0)
					{
						bStop = true;
						++Check.TracksStopped;
						break;
					}
				}
			}
			if (bStop)
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
				const bool bLevel = A.Role == "level"; // (low over a boat, ruling 241 D1: Play's sea:air)
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
					if (bLevel)
					{
						// Low and straight for her, timed to be over her as the battle's moment comes, 60 m up.
						Goal = Over;
						GoalAlt = 60.0;
						GoalSpd = Clamp(Flat(Over - A.P) / std::max(1.0, Attack - ShownT), 50.0, 110.0);
					}
					else if (bDive)
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
					if ((bDive && A.P.Z < 900.0 && (Near < 250.0 || (A.P.Z <= GoalAlt + 60.0 && Near < 600.0))) || (bLevel && (Near < 60.0 || ShownT >= Attack + 3.0))
						|| (!bDive && !bLevel && Near < 900.0) || ShownT > Attack + 25.0)
					{
						// Released: a hit on her or a near miss (the battle's share, decided as the strike landed).
						if (On)
						{
							const ClassSize& Sz = SizeOf(On->Class);
							if (A.bHit)
							{
								const bool bTorp = !bDive && !bLevel;
								if (bLevel && On->Class == "ss") ++Check.AirBlowsOnBoats;
								const Vec At = bTorp ? PlaceOn(*On, Roll.U(-0.3, 0.3), (Roll.U() < 0.5 ? -1.0 : 1.0) * (Sz.Beam * 0.5 + 2.0), 0.0)
									: PlaceOn(*On, Roll.U(-0.35, 0.35), Roll.U(-0.3, 0.3) * Sz.Beam, Roll.U(6.0, 12.0));
								Fx& E = Emit(FxKind::BombHit, At, bTorp ? "torpedo" : S.bFromCoast ? "level" : "dive", On->Key);
								E.Side = On->Side;
								On->LastBlow = bTorp ? "torpedo" : "dive";
								On->BlowAt = ShownT;
								SightHit(*On); // (a boat struck is found: SetMine)
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
									Emit(FxKind::NearMiss, Vec(At.X, At.Y, 0.0), bDive ? "bomb250" : bLevel ? "bomb60" : "torpedo");
									++Check.NearMisses;
								}
							}
							// B4: every ship combs a torpedo plane's drop, bow or stern on to it, the torpedoes' run long.
							if (!bDive && !bLevel) Dodge(*On, A, true, ShownT + 45.0);
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
	// Where a ship went down (Brian, 22 November 05:32Z to 05:33Z: "sailors should jump in the water if the boat is going
	// down", "have them use the models we have for the soldiers for the sailors", "there should be debris floating in the
	// water. maybe oil pools?"): her men over the side, swimming, holding on to her wreckage, in her rafts and boats, and
	// picked up; her wreckage and her oil drifting down the wind. Looks only: the battle books every loss and every life.
	// Each man's and each piece's own hash, never the battle's dice, so the ships' tracks are the same with it or without.

	namespace
	{
		/** A ship's company and what floats off her, by class: her men (roughly: a battleship 1,800 to 2,700, a fleet carrier
		 * 2,200 to 3,000, a heavy cruiser 900 to 1,100, a light 850, a destroyer 300, an escort 200, a boat 50 to 60, a
		 * merchant 40 to 60); her Carley floats or life rafts; her whaleboats or lifeboats; pieces of her that float; her
		 * deck's height over the sea (metres); her oil, against a destroyer's. */
		struct Company
		{
			int Men, Rafts, Boats, Bits;
			double Free, Oil;
		};
		Company CompanyOf(const std::string& Class)
		{
			if (Class == "bb") return { 1800, 30, 2, 70, 8.0, 1.6 };
			if (Class == "cv") return { 2200, 36, 2, 80, 12.0, 1.6 };
			if (Class == "ca") return { 950, 16, 2, 50, 7.0, 1.3 };
			if (Class == "cl") return { 850, 14, 2, 45, 6.5, 1.25 };
			if (Class == "de") return { 200, 4, 1, 20, 4.5, 0.9 };
			if (Class == "ss") return { 55, 1, 0, 10, 2.5, 0.8 };
			if (Class == "mv") return { 50, 2, 4, 50, 6.0, 1.2 };
			return { 300, 6, 1, 30, 5.0, 1.0 }; // a destroyer
		}
		/** Her men by the way she went: the share of her company in the water (the rest went down with her), of them the
		 * share dead, and between what shares of her going down they go over the side. */
		struct Leaving
		{
			double Into, Dead, Start, End;
		};
		Leaving LeavingOf(const std::string& How)
		{
			// (a magazine's blast leaves few; a ship that settles slowly, or is scuttled, most; a boat crushed deep, none)
			if (How == "explode") return { 0.08, 0.5, 0.02, 0.3 };
			if (How == "break") return { 0.45, 0.25, 0.05, 0.6 };
			if (How == "capsize") return { 0.4, 0.25, 0.1, 0.7 };
			if (How == "sink") return { 0.75, 0.12, 0.1, 0.8 };
			if (How == "burn") return { 0.7, 0.2, 0.15, 0.85 };
			if (How == "scuttled") return { 0.95, 0.02, 0.02, 0.5 };
			if (How == "crushed") return { 0.0, 0.0, 0.0, 0.0 };
			if (How == "rammed") return { 0.5, 0.3, 0.05, 0.7 };
			if (How == "shelled") return { 0.6, 0.3, 0.05, 0.7 };
			return { 0.85, 0.08, 0.1, 0.85 }; // settle
		}
		/** What floats: its size (metres along and across), the men it takes aboard and lets hold on, the share of the wind
		 * it drifts at. */
		struct Kind
		{
			const char* Name;
			double Long, Beam;
			int Room, Hold;
			double Leeway;
		};
		const Kind& WreckageOf(double R)
		{
			static const Kind Kinds[] = { { "plank", 3.0, 0.3, 0, 1, 0.025 }, { "spar", 6.0, 0.4, 0, 3, 0.025 }, { "crate", 1.0, 0.8, 0, 1, 0.025 },
				{ "drum", 0.9, 0.6, 0, 1, 0.025 }, { "hatch", 2.0, 1.5, 0, 4, 0.025 }, { "ring", 0.8, 0.8, 0, 1, 0.03 } };
			static const double Odds[] = { 0.35, 0.12, 0.18, 0.12, 0.13, 0.10 };
			for (int I = 0; I < 6; ++I)
			{
				if (R < Odds[I]) return Kinds[I];
				R -= Odds[I];
			}
			return Kinds[0];
		}
		const Kind RaftBig = { "raft", 3.0, 1.8, 10, 12, 0.035 };  // a Carley float
		const Kind RaftSmall = { "raft", 2.4, 1.5, 6, 6, 0.035 }; // a boat's dinghy, a merchant's life raft
		const Kind Lifeboat = { "boat", 8.0, 2.3, 24, 4, 0.03 };  // a whaleboat or a lifeboat
		/** The deck's height over the sea of a ship of the class (a rescuer's side, for a man climbing it). */
		double FreeOf(const std::string& Class) { return CompanyOf(Class).Free; }
		/** A boat's casing over her waterline as the stage draws her now, the battle modeller's boat of her navy (AirStage's
		 * ShipFileOf; ConvoyLook::SculptedOf's Deck): a Type VIIC's 1.77 m, a fleet boat's 2.6, any other's (or a boat of no navy) 2.0.
		 * Her men go over her side from there. */
		double BoatFreeOf(const std::string& Nation) { return Nation == "USA" ? 2.6 : Nation == "GER" ? 1.8 : 2.0; }
		/** A man's place on a float, in its own frame (x along, y across): aboard, or round it in the water holding on. */
		Vec SlotOn(const FloatOut& O, int Slot, bool bIn, int Room, int Hold)
		{
			if (bIn)
			{
				if (O.Kind == "boat")
				{
					// (on the thwarts, two by two, bow to stern)
					const int Rows = std::max(1, (Room + 1) / 2);
					const double Along = Rows > 1 ? -0.38 + 0.76 * (Slot / 2) / (Rows - 1.0) : 0.0;
					return Vec(Along * O.Long, (Slot % 2 ? 0.25 : -0.25) * O.Beam, 0.0);
				}
				// (round its rim, inside)
				const double A = 2.0 * Pi * (Slot + 0.5) / std::max(1, Room);
				return Vec(std::cos(A) * (0.5 * O.Long - 0.35), std::sin(A) * (0.5 * O.Beam - 0.35), 0.0);
			}
			// (a boat's holders on her grab line at her sides, the art director, 22 November 13:47Z; others round its rim, out
			// from it)
			if (O.Kind == "boat") return GrabHold(O.Long, O.Beam, Slot, Hold);
			const double A = 2.0 * Pi * (Slot + 0.25) / std::max(1, Hold);
			return Vec(std::cos(A) * (0.5 * O.Long + 0.35), std::sin(A) * (0.5 * O.Beam + 0.35), 0.0);
		}
		/** A place in a thing's own frame (x along its heading, y across to its right) put on the sea. */
		Vec OnFrame(const Vec& At, double Heading, const Vec& Off)
		{
			return Vec(At.X, At.Y, 0.0) + Ahead(Heading) * Off.X + RightOf(Heading) * Off.Y;
		}
		/** Pushes a point out of a hull (her middle, heading and half her length less her beam) to Clear metres from her
		 * keel's line; true where it moved. */
		bool PushOut(Vec& P, const Vec& At, double Hdg, double Half, double Clear)
		{
			const Vec A = Ahead(Hdg);
			if (std::fabs(P.X - At.X) > Half + Clear || std::fabs(P.Y - At.Y) > Half + Clear) return false;
			const double U = Clamp((P.X - At.X) * A.X + (P.Y - At.Y) * A.Y, -Half, Half);
			const double CX = At.X + A.X * U, CY = At.Y + A.Y * U;
			double DX = P.X - CX, DY = P.Y - CY;
			const double L = std::hypot(DX, DY);
			if (L >= Clear) return false;
			if (L < 1e-6)
			{
				const Vec R = RightOf(Hdg);
				DX = R.X;
				DY = R.Y;
			}
			else
			{
				DX /= L;
				DY /= L;
			}
			P.X = CX + DX * Clear;
			P.Y = CY + DY * Clear;
			return true;
		}
		int FreeSlot(uint32_t Mask, int Of)
		{
			for (int I = 0; I < Of && I < 32; ++I)
				if (!(Mask & (1u << I))) return I;
			return -1;
		}
		/** How far a point lies from a segment, on the sea (metres). */
		double FlatToSegment(const Vec& P, const Vec& A, const Vec& B)
		{
			const double DX = B.X - A.X, DY = B.Y - A.Y, L2 = DX * DX + DY * DY;
			const double U = L2 > 0.0 ? Clamp(((P.X - A.X) * DX + (P.Y - A.Y) * DY) / L2, 0.0, 1.0) : 0.0;
			return std::hypot(P.X - (A.X + DX * U), P.Y - (A.Y + DY * U));
		}
		/** How near two segments come on the sea (0 where they cross). */
		double FlatGap(const Vec& A, const Vec& B, const Vec& C, const Vec& D)
		{
			auto Turn = [](const Vec& P, const Vec& Q, const Vec& R) { return (Q.X - P.X) * (R.Y - P.Y) - (Q.Y - P.Y) * (R.X - P.X); };
			const double T1 = Turn(A, B, C), T2 = Turn(A, B, D), T3 = Turn(C, D, A), T4 = Turn(C, D, B);
			if (((T1 > 0.0) != (T2 > 0.0)) && ((T3 > 0.0) != (T4 > 0.0))) return 0.0;
			return std::min(std::min(FlatToSegment(A, C, D), FlatToSegment(B, C, D)), std::min(FlatToSegment(C, A, B), FlatToSegment(D, A, B)));
		}
		/** The keys of the oil fires the show lights where a ship's oil burns (apart from the battle's zones' own). */
		constexpr int SlickFireKeys = 100000;
	}

	const BoatDeck& DeckOf(const std::string& Nation)
	{
		// (the stage's three boats, ConvoyLook::SculptedOf's rows; the casing's height fitted to the model files where her
		// men stand, within a few centimetres; hatches where her casing has their coamings)
		static const BoatDeck Ger = { 67.0, 6.79, 3.5, -6.5, 1.5, 4.1, 8.0, -12.2, -20.0, -7.5, 4.5, 10.0, 1.2, 1.58, 3.0, 0.0087, 0.051, 10.85, 15.55, 10.49 };
		static const BoatDeck Usa = { 95.0, 7.97, 6.5, -5.0, 1.6, 5.3, 13.5, -12.0, -24.0, -6.0, 10.0, 21.0, 1.9, 2.54, 9.0, 0.0202, 0.031, -17.05, -12.35, 11.79 };
		static const BoatDeck Other = { 80.0, 7.08, 4.5, -7.5, 1.5, 4.4, 10.0, -13.5, -24.0, -8.5, 5.5, 12.5, 1.5, 1.90, 5.0, 0.0122, 0.042, 13.35, 18.05, 10.99 };
		// (a boat of no navy is the generic boat, as the stage draws her: ShipFileOf, and BoatFreeOf beside this)
		return Nation == "USA" ? Usa : Nation == "GER" ? Ger : Other;
	}

	double CasingAt(const BoatDeck& D, double Along, double Across)
	{
		return D.Deck + D.Rise * std::max(0.0, Along - D.RiseFrom) - D.Fall * Across * Across;
	}

	void StageSink(double T, double High, bool bByStern, double& OutDepth, double& OutPitch)
	{
		// (the stage's own: Under = High + 40, Depth = Under * EaseIn(T / 70), Pitch = 14 * Down * Ease(T / 35), its Down -1
		// for "stern", where a plus pitch lifts her stern; here plus is her bow up, so by the stern is plus)
		const double In = Clamp(T / 70.0, 0.0, 1.0), Over = Clamp(T / 35.0, 0.0, 1.0);
		OutDepth = (High + 40.0) * In * In;
		OutPitch = bByStern ? 14.0 * Over * Over * (3.0 - 2.0 * Over) : 0.0;
	}

	std::string StageEndOf(const std::string& How, double Struck, double Lowered)
	{
		return How == "scuttled" && (Struck >= 0.0 || Lowered >= 0.0) ? std::string("stern") : std::string();
	}

	Show::Floater* Show::FloatByKey(int Key)
	{
		return Key >= 1 && Key <= static_cast<int>(Afloat.size()) ? &Afloat[Key - 1] : nullptr;
	}

	void Show::Abandon(const Hull& H)
	{
		for (const Wreck& Was : Wrecked)
			if (Was.Ship == H.Key) return;
		const Company C = CompanyOf(H.Class);
		const Leaving L = LeavingOf(H.How);
		const ClassSize& Sz = SizeOf(H.Class);
		Wreck W;
		W.Ship = H.Key;
		W.Side = H.Side;
		W.Class = H.Class;
		W.Nation = H.Nation;
		W.How = H.How;
		W.Long = Sz.Long;
		W.Beam = Sz.Beam;
		W.Free = H.Class == "ss" ? BoatFreeOf(H.Nation) : C.Free;
		W.T0 = H.SinkAt;
		W.For = std::max(10.0, H.SinkFor);
		W.Start = W.T0 + L.Start * W.For;
		W.End = W.T0 + std::max(L.Start + 0.05, L.End) * W.For;
		W.Seed = F.Seed * 0x9E3779B97F4A7C15ULL + static_cast<uint64_t>(H.Key) * 1000003ULL;
		W.Real = static_cast<int>(std::lround(C.Men * L.Into * (0.85 + 0.3 * Mix(W.Seed + 1))));
		W.DeadShare = L.Dead;
		// The room (Brian's standing rule, 22 November 04:14Z: the real numbers, as many as the Mac draws smoothly): the men
		// in the water and those still to go over from other ships against the most the show keeps; a ship's men take half
		// what is left, and at least a dozen (the dead of earlier ships going under to make it, RunWrecks).
		int Taken = 0;
		for (const Man& M : Swimmers) Taken += M.bGoing ? 0 : 1;
		for (const Wreck& O : Wrecked) Taken += O.Men - O.Out;
		const int Room = std::max(0, F.MenMost - Taken);
		W.Men = std::min(W.Real, std::max(std::min(W.Real, 12), Room / 2));
		// Her rafts float free or are thrown over; her boats are lowered only where she goes slowly enough; her wreckage.
		const bool bSlow = H.How == "sink" || H.How == "settle" || H.How == "burn" || H.How == "scuttled";
		W.Rafts = H.How == "crushed" ? 0 : std::max(H.Class == "ss" ? 1 : 0, static_cast<int>(std::lround(C.Rafts * (0.5 + 0.5 * L.Into))));
		W.Boats = bSlow ? C.Boats : H.How == "shelled" ? C.Boats / 2 : 0;
		W.Bits = static_cast<int>(std::lround(C.Bits * (H.How == "explode" ? 1.3 : H.How == "break" ? 1.2 : H.How == "crushed" ? 0.3 : 1.0)));
		// (her men go over on her windward side, most of them, so she does not drift down on them)
		W.Lee = RightOf(H.Hdg).Dot(Ahead(WindFromR)) >= 0.0 ? 1.0 : -1.0;
		W.Oil0 = Vec(H.P.X, H.P.Y, 0.0);
		W.OilEnd = W.Oil0;
		W.TrackHdg = H.Hdg;
		// (her oil alight: a ship that burned or blew up, and a merchant now and then, a tanker or her bunkers afire)
		W.Burn = H.How == "burn" ? 0.6 : H.How == "explode" ? 0.5 : (H.Class == "mv" && Mix(W.Seed + 2) < 0.3) ? 0.45 : 0.0;
		W.BurnUntil = W.Burn > 0.0 ? W.T0 + 0.6 * W.For + 180.0 + 120.0 * Mix(W.Seed + 3) : -1.0;
		W.Slick = static_cast<int>(Oil.size()) + 1;
		SlickOut S;
		S.Key = W.Slick;
		S.Ship = H.Key;
		S.At = W.Oil0;
		S.Heading = H.Hdg;
		Oil.push_back(S);
		W.Middle = W.Oil0;
		W.Spread = 0.5 * W.Long;
		// (the ships near her now; RunWrecks looks again each second)
		for (const auto& Pair : Hulls)
			if (Pair.second.Doing != Fate::Gone && Flat(Pair.second.P - H.P) < 1500.0) W.Near.push_back(Pair.first);
		Wrecked.push_back(W);
		++Check.Wrecks;
		// (her slick counted where it is laid, every wreck's: one given up, abandoned or struck may not go down by the
		// battle's last word, a prize never, and the cloud test allows those theirs, still settling or afloat at the end)
		++Check.Slicks;
		Check.MenReal += W.Real;
		Check.MenKept += W.Men;
	}

	void Show::AbandonShip(Hull& H, const std::string& Words)
	{
		(void)Words; // (her words are the battle's dispatch, told as every event's)
		TakeOut(H);
		// She sheers out of her line away from the nearest enemy, as one going down, her engines stopped (she loses her way
		// in a minute or so), settling a little while her men leave her; the battle says when she goes (sea:sunk).
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
		H.Doing = Fate::Sinking;
		H.bAbandoned = true;
		H.How = "settle";
		H.SinkAt = ShownT;
		H.SinkFor = AbandonedFor;
		H.ScreenUntil = -1.0;
		Abandon(H);
		// (her men over the side as she loses her way, over two minutes, so that the first of them string out along her track
		// behind her; her rafts and boats over in the first of it: the art director, 22 November 18:54Z)
		for (Wreck& W : Wrecked)
		{
			if (W.Ship != H.Key) continue;
			W.bLeft = true;
			W.Start = ShownT + std::min(20.0, 0.4 * std::fabs(H.Spd) / AbandonedSlowing);
			W.End = W.Start + 90.0;
			W.BoatsFrom = ShownT + std::min(40.0, 0.8 * std::fabs(H.Spd) / AbandonedSlowing);
		}
	}

	void Show::LeaveUp(Hull& H)
	{
		if (H.Class != "ss") return;
		// (as one that struck: her tanks blown, her engines stopped; out of her line Trim keeps the depth she is going to)
		H.Dive = "surfaced";
		H.bScope = false;
		H.KeelTo = SurfacedKeel(H.Class);
		if (H.Keel <= SurfacedKeel(H.Class) + 0.3) return;
		H.bLeftUnder = true;
		for (Wreck& W : Wrecked)
			if (W.Ship == H.Key) W.Start = W.End = W.BoatsFrom = 1e18;
	}

	void Show::Untold(double Upto)
	{
		// (the view the picture has reached, as each step's events are played: one at its own moment counts)
		const View* At = nullptr;
		for (const View& V : Views)
		{
			if (V.T > Upto + 1e-9) break;
			At = &V;
		}
		if (!At) return;
		for (const auto& Pair : At->Pieces)
		{
			const Piece& P = Pair.second;
			const auto Line = Lines.find(P.Id);
			if (Line == Lines.end()) continue;
			int InLine = 0;
			for (const int Key : Line->second.Line)
			{
				const Hull* H = FindHull(Key);
				InLine += H && H->Doing == Fate::Line ? 1 : 0;
			}
			while (InLine > std::max(0, P.Ships))
			{
				Hull* Given = nullptr;
				for (const int Key : Line->second.Line)
				{
					Hull* H = FindHull(Key);
					if (!H || H->Doing != Fate::Line) continue;
					if (!Given || H->BlowAt >= Given->BlowAt) Given = H;
				}
				if (!Given) break;
				AbandonShip(*Given, std::string());
				NoteLost(*Given, 0);
				Given->bGivenUp = true;
				LeaveUp(*Given);
				++Check.LeftUntold;
				--InLine;
			}
		}
	}

	void Show::StrikeColours(Hull& H, const Event& E)
	{
		// She is on the surface, or coming up to it (forced up, sea:dive why "forced"): her tanks blown and her engines
		// stopped, she leaves the fight as one her crew are leaving (AbandonShip), losing her way as such a ship does (no
		// lurch); her men come out on her casing once she is up (StepHands, Muster), not over her waist, and the battle says
		// how she ends. Her flag is the looks' (ShipOut::Struck), up from her crew's first coming out.
		H.bStruck = true;
		H.StruckAt = ShownT;
		// (the battle's "scuttle" and "scuttled" are one: her crew send her down; anything but "captured" is that)
		H.StruckHow = E.How == "captured" ? "captured" : "scuttled";
		H.StruckTo = E.By;
		H.Dive = "surfaced";
		H.bScope = false;
		AbandonShip(H, E.Text);
		// (out of her line Trim keeps the depth she was going to: she is coming up, whatever she was doing)
		H.KeelTo = SurfacedKeel(H.Class);
		for (Wreck& W : Wrecked)
		{
			if (W.Ship != H.Key) continue;
			W.bStruck = true;
			// (her men, her raft and her boats wait for her crew's plan, made once she is up)
			W.Start = W.End = W.BoatsFrom = 1e18;
		}
		const Hull* By = E.By >= 0 ? NearestOf(E.By, H.P.X, H.P.Y) : nullptr;
		Fx& S = Emit(FxKind::Surrender, H.P, H.StruckHow, H.Key);
		S.Side = H.Side;
		S.Size = E.By;
		S.Text = E.Text;
		if (By) S.Aim = Vec(By->P.X, By->P.Y, 0.0);
		if (H.StruckHow == "captured") SendBoarders(H, E.By);
	}

	void Show::Muster(Wreck& W, const Hull& H, int From)
	{
		// Her crew out on her casing, as the photographs of the boats taken in the war show them: about a third up through
		// her conning tower onto her bridge and down its after face, the rest out of her fore and after hatches, a man each
		// one and a half to two and a half seconds from each; each walks to his place on her casing, forward or aft of her
		// tower on the side he will go over, and stands there, his hands up where a ship of the other side is within a
		// kilometre of her; at his turn he runs out to the edge of her casing and over (RunWrecks). Each man by his own hash
		// (Mix: the show's Roll is not drawn, so every other show keeps its own), their turns a steady file from about
		// half a minute after the first is up, a man each 1.2 to 1.5 seconds.
		// (in her model's own frame: the stage draws the battle modeller's boat as made, AirStage's ShipFileOf)
		const BoatDeck& Row = DeckOf(W.Nation);
		const int WI = static_cast<int>(&W - Wrecked.data());
		double Nearest = 1e18;
		for (const auto& Pair : Hulls)
		{
			const Hull& A = Pair.second;
			if (A.Side != H.Side && A.Class != "ss" && A.Doing != Fate::Gone) Nearest = std::min(Nearest, Flat(A.P - H.P));
		}
		// Their places, one man to each, so that none ever stands in another: along each stretch of her casing a pace apart,
		// in ranks from its edge inboard, clear of her hatches, her tower and her deck gun. Her bridge's men take the after
		// stretch forward of her after hatch, that hatch's men the rest of it, her fore hatch's men the stretch forward of her
		// tower; on each, the edge's rank first and in it the place farthest from where they come up first, so that none walks
		// past one standing. A man's own hash moves him a few centimetres in his place, no more.
		constexpr double Pace = 0.6, RankApart = 0.6;
		struct Spot
		{
			double Along = 0.0, Across = 0.0;
			int Rank = 0;
		};
		const double Source[3] = { Row.TowerAft, Row.ForeHatch, Row.AftHatch };
		auto SpotsOf = [&Row, &Source](int Via, double Toward)
		{
			const double Lo = Via == 1 ? Row.ForeFrom : Row.AftFrom, Hi = Via == 1 ? Row.ForeTo : Row.AftTo;
			std::vector<Spot> Spots;
			for (int Rank = 0;; ++Rank)
			{
				const double Across = Row.Edge - 0.2 - Rank * RankApart;
				if (Across < 0.25) break;
				std::vector<Spot> Ranked;
				for (int K = 0;; ++K)
				{
					const double Along = Lo + 0.15 + K * Pace;
					if (Along > Hi - 0.15 + 1e-9) break;
					const bool bHers = Via == 1 || (Via == 0 ? Along > Row.AftHatch : Along <= Row.AftHatch);
					const bool bHatch = (std::fabs(Along - Row.ForeHatch) < 0.75 || std::fabs(Along - Row.AftHatch) < 0.75) && Across < 0.75;
					const bool bGun = Along > Row.GunFrom - 0.4 && Along < Row.GunTo + 0.4 && Across < 1.0;
					const bool bTower = Along > Row.TowerAft - 0.4 && Along < Row.TowerFore + 0.4 && Across < Row.TowerHalf + 0.4;
					if (!bHers || bHatch || bGun || bTower) continue;
					Ranked.push_back({ Along, Toward * Across, Rank });
				}
				std::stable_sort(Ranked.begin(), Ranked.end(),
				                 [&Source, Via](const Spot& L, const Spot& R) { return std::fabs(L.Along - Source[Via]) > std::fabs(R.Along - Source[Via]); });
				Spots.insert(Spots.end(), Ranked.begin(), Ranked.end());
			}
			return Spots;
		};
		auto Free = [this, WI](const Spot& Sp)
		{
			for (const DeckHand& D : OnCasing)
				if (D.W == WI && !D.bParty && std::fabs(D.PlaceAlong - Sp.Along) < 0.2 && std::fabs(D.PlaceAcross - Sp.Across) < 0.2) return false;
			return true;
		};
		double Next[3] = { ShownT, ShownT + 1.0, ShownT + 2.0 }; // her bridge, her fore hatch, her after hatch
		double LastTurn = ShownT + 28.8;
		for (const DeckHand& D : OnCasing)
		{
			if (D.W != WI || D.bParty) continue;
			LastTurn = std::max(LastTurn, D.TurnAt);
			for (double& N : Next) N = std::max(N, D.OutAt + 1.5);
		}
		for (int N = From; N < W.Men; ++N)
		{
			const uint64_t S = W.Seed + 7919ULL * static_cast<uint64_t>(N + 1);
			const int Via = Mix(S + 41) < 0.34 ? 0 : Mix(S + 43) < 0.5 ? 1 : 2;
			DeckHand D;
			D.W = WI;
			D.N = N;
			D.Via = Via;
			D.O.Key = NextMan++;
			D.O.Ship = W.Ship;
			D.O.Side = W.Side;
			D.O.Nation = W.Nation;
			D.O.Look = static_cast<uint32_t>(Mix(S + 3) * 4294967295.0); // (RunWrecks' own hash for her N'th man: the same man)
			D.O.Doing = Swim::Hatch;
			D.Side = Mix(S + 17) < 0.88 ? W.Lee : -W.Lee;
			// (his place: the first free one of his hatch's on his side; else on her other side, where he will go over; else,
			// one of her bridge's or her after hatch's men, the other's stretch aft)
			const int Order[4][2] = { { Via, 1 }, { Via, -1 }, { Via == 0 ? 2 : Via == 2 ? 0 : 1, 1 }, { Via == 0 ? 2 : Via == 2 ? 0 : 1, -1 } };
			bool bPlaced = false;
			for (const auto& Try : Order)
			{
				for (const Spot& Sp : SpotsOf(Try[0], D.Side * Try[1]))
				{
					if (!Free(Sp)) continue;
					D.PlaceAlong = Sp.Along + 0.04 * (2.0 * Mix(S + 13) - 1.0);
					D.PlaceAcross = Sp.Across + 0.03 * (2.0 * Mix(S + 19) - 1.0);
					D.Rank = Sp.Rank;
					D.Side *= Try[1];
					bPlaced = true;
					break;
				}
				if (bPlaced) break;
			}
			if (!bPlaced)
			{
				// (her casing full, which a boat's company never fills: by his hatch, at its edge)
				D.PlaceAlong = Source[Via];
				D.PlaceAcross = D.Side * (Row.Edge - 0.2);
			}
			// (out of his hatch from her hull below her casing; or onto her bridge's after rim from her tower's hatch, and down
			// its after face from its floor)
			const double Along0 = Via == 0 ? Row.TowerAft - 0.02 : Source[Via];
			D.Along = Along0;
			D.Across = 0.0;
			D.Z = Via == 0 ? Row.Bridge - CasingAt(Row, Along0, 0.0) : -1.8;
			D.OutAt = Next[Via];
			Next[Via] += 1.5 + 1.0 * Mix(S + 47);
			D.bUp = Nearest < 1000.0 && Mix(S + 53) < 0.7;
			// (never before he is at his place: up, across and along at a walk, a few seconds' grace)
			const double Placed = D.OutAt + std::fabs(D.Z) / 0.6 + (std::fabs(D.PlaceAcross) + std::fabs(D.PlaceAlong - D.Along)) / 1.25 + 3.0;
			D.TurnAt = std::max(LastTurn + 1.2 + 0.3 * Mix(S + 59), Placed);
			LastTurn = D.TurnAt;
			OnCasing.push_back(D);
		}
		if (From == 0)
		{
			// (her raft over with her first man, her men over between her first turn and her last)
			double First = 1e18;
			for (const DeckHand& D : OnCasing)
				if (D.W == WI && !D.bParty) First = std::min(First, D.TurnAt);
			W.Start = W.Men > 0 ? First : ShownT;
			W.End = std::max(W.Start + 1.0, LastTurn);
			W.BoatsFrom = W.Start;
		}
		// (the battle has sent her down already: no waiting for turns)
		if (!H.bAbandoned) Hurry(W);
	}

	void Show::Hurry(Wreck& W)
	{
		// The battle sends her down (sea:sunk, scuttled): her vents opened, every man still below comes up a hatch at a run,
		// a file at each, a man each HurriedApart close behind the one before, and straight out to the edge of her casing and
		// over: up his own, or, where its file is too long for him to be over within HurryFor of it, up the one that gets him
		// over soonest. One whom none would get over in time stays below, never drawn, and is in the water with the rest of
		// those below once she has gone (RunWrecks). Those at their places go now as well, her outer rank first, each within
		// a few seconds: the last of all over within HurryFor. Her captor's party, where she was taken, go back down her side
		// into their whaler (StepHands), never over it; those still on its thwarts stay there.
		const int WI = static_cast<int>(&W - Wrecked.data());
		const BoatDeck& Row = DeckOf(W.Nation);
		const double Due = ShownT + HurryFor;
		// (where each opening's men come up, as Muster has them: onto her bridge's after rim and down her tower's after face,
		// or up a hatch's ladder from below her casing)
		const double Source[3] = { Row.TowerAft - 0.02, Row.ForeHatch, Row.AftHatch };
		const double Below[3] = { Row.Bridge - CasingAt(Row, Source[0], 0.0), -1.8, -1.8 };
		// (when one starting up an opening then is over: up it at HurriedClimb, out to her edge at a run, a step's grace)
		auto OverAt = [&Row, &Below](int Via, double Out) { return Out + std::fabs(Below[Via]) / HurriedClimb + (Row.Edge + 0.3) / 3.0; };
		// (each file starts now, behind any man of it already on his way up, so that none climbs into the man above him)
		double Next[3] = { ShownT, ShownT, ShownT };
		for (const DeckHand& D : OnCasing)
		{
			if (D.W != WI || D.O.Key == 0 || D.bParty || !D.bOut || D.Leg != 0) continue;
			Next[D.Via] = std::max(Next[D.Via], ShownT + HurriedApart - std::fabs(D.Z - Below[D.Via]) / HurriedClimb);
		}
		int Party = 0;
		for (DeckHand& D : OnCasing)
		{
			if (D.W != WI || D.O.Key == 0 || D.bHurried) continue;
			D.bHurried = true;
			if (D.bParty)
			{
				// (one still on his thwart stays in it; those below up her after hatch first, close behind one another, a man
				// each 0.2 s; the rest from where they are: all in within HurryFor)
				if (!D.bOut)
				{
					D.O.Key = 0;
					continue;
				}
				if (D.bBelow)
				{
					D.bBelow = false;
					D.Along = Row.AftHatch;
					D.Across = 0.0;
					D.Z = -1.9;
					D.O.Doing = Swim::Hatch;
					D.O.Since = 0.0;
					D.OutAt = ShownT + 0.2 * Party++;
				}
				D.Leg = D.Leg == 0 ? 12 : D.Leg == 1 || D.Leg == 2 ? 11 : 10;
				continue;
			}
			const uint64_t S = W.Seed + 7919ULL * static_cast<uint64_t>(D.N + 1);
			if (!D.bOut)
			{
				int Via = D.Via;
				if (OverAt(Via, Next[Via]) > Due - 0.1)
					for (int V = 0; V < 3; ++V)
						if (OverAt(V, Next[V]) < OverAt(Via, Next[Via])) Via = V;
				if (OverAt(Via, Next[Via]) > Due - 0.1)
				{
					// (none would get him over in time: below as she goes)
					D.OutAt = 1e18;
					continue;
				}
				if (Via != D.Via)
				{
					D.Via = Via;
					D.Along = Source[Via];
					D.Across = 0.0;
					D.Z = Below[Via];
				}
				D.OutAt = Next[Via];
				Next[Via] += HurriedApart;
				D.TurnAt = D.OutAt;
			}
			else if (D.Leg == 0)
			{
				// (on his way up: on, at a run)
				D.TurnAt = ShownT;
			}
			else
			{
				// (at his place or walking to it: over now, her outer rank first)
				D.TurnAt = std::min(D.TurnAt, ShownT + 0.2 + 0.8 * D.Rank + 1.2 * Mix(S + 61));
			}
		}
		W.End = std::min(W.End, Due + 1.0);
	}

	void Show::SendBoarders(const Hull& H, int ByPiece)
	{
		// Taken (sea:surrender how "captured"): her captor, lying stopped near her, lowers his whaler; a boarding party pulls
		// across, climbs her casing and goes down her after hatch to shut her vents before she floods; three below for half
		// a minute and she is a prize (her flag down). Drawn only: the battle took her.
		const Hull* By = ByPiece >= 0 ? NearestOf(ByPiece, H.P.X, H.P.Y) : nullptr;
		if (!By)
		{
			// (no piece named, or none of it afloat: the ship of the other side nearest her, not a boat)
			double BestD = 1e18;
			for (const auto& Pair : Hulls)
			{
				const Hull& A = Pair.second;
				if (A.Side == H.Side || A.Class == "ss" || A.Doing == Fate::Gone) continue;
				const double D = Flat(A.P - H.P);
				if (D < BestD) { BestD = D; By = &A; }
			}
		}
		if (!By) return;
		Boarding B;
		B.Boat = H.Key;
		B.By = By->Key;
		// (keys of their own, far past the wrecks' floats)
		B.Whaler.Key = (1 << 20) + static_cast<int>(Boardings.size());
		B.Whaler.Ship = By->Key;
		B.Whaler.Kind = "boat";
		B.Whaler.Long = 8.0;
		B.Whaler.Beam = 2.3;
		// (her davits a metre over her deck's edge)
		B.Davit = CompanyOf(By->Class).Free + 1.0;
		Boardings.push_back(B);
	}

	Vec Show::RoundHull(const Vec& From, const Vec& To, const Hull& H, double Clear) const
	{
		// (her outline as both the show and the stage have her: a boat's the larger of her class's and her model's)
		const ClassSize& Sz = SizeOf(H.Class);
		double Long = Sz.Long, Beam = Sz.Beam;
		if (H.Class == "ss")
		{
			const BoatDeck& Row = DeckOf(H.Nation);
			Long = std::max(Long, Row.Long);
			Beam = std::max(Beam, Row.Wide);
		}
		const double Half = std::max(0.0, 0.5 * (Long - Beam)), R = 0.5 * Beam + Clear;
		const Vec Fore = Ahead(H.Hdg), Right = RightOf(H.Hdg), Mid(H.P.X, H.P.Y, 0.0);
		const Vec Bow = Mid + Fore * Half, Stern = Mid - Fore * Half;
		// (clear by its own width, to within a hair: a way along which the last step found it clear stays so)
		auto Clears = [&Bow, &Stern, R](const Vec& A, const Vec& B) { return FlatGap(A, B, Bow, Stern) >= R - 1e-6; };
		if (Clears(From, To)) return To;
		// (round the end nearer the way: off her shoulder on To's side, where it can be reached; else off that end, on her
		// middle line; else off her shoulder on its own side, to come to that end along her. Each a point half as far again
		// out as the way must keep, so that from one to the next it clears her too.)
		const double Ring = 1.5 * R;
		const double SideTo = (To - Mid).Dot(Right) >= 0.0 ? 1.0 : -1.0, SideFrom = (From - Mid).Dot(Right) >= 0.0 ? 1.0 : -1.0;
		const double Nearer = (From - Mid).Dot(Fore) + (To - Mid).Dot(Fore) >= 0.0 ? 1.0 : -1.0;
		for (const double End : { Nearer, -Nearer })
		{
			const Vec Corner = Mid + Fore * (End * Half);
			for (const Vec& Way : { Corner + Right * (SideTo * Ring), Corner + Fore * (End * Ring), Corner + Right * (SideFrom * Ring) })
				if (Clears(From, Way)) return Way;
		}
		// (inside the way it must keep, which a boat pulling never is: straight out from her middle line)
		const double U = Clamp((From - Mid).Dot(Fore), -Half, Half);
		return Mid + Fore * U + Right * (SideFrom * Ring);
	}

	Vec Show::HandAt(const DeckHand& D, const Hull& H, double On) const
	{
		const BoatDeck& Row = DeckOf(Wrecked[static_cast<size_t>(D.W)].Nation);
		// (a prize's sinking clock is held, so between steps she holds too)
		const double Down = H.Doing == Fate::Sinking ? Clamp((ShownT + (H.bPrize ? 0.0 : On) - H.SinkAt) / std::max(1.0, H.SinkFor), 0.0, 1.0) : 0.0;
		const double Dived = std::max(0.0, H.Keel - SurfacedKeel(H.Class));
		const double Hdg = H.Hdg + H.Rate * On;
		// (sent down, she goes as the stage takes her: down, and by the stern where her crew scuttled her; those still on her
		// ride her down until they go over)
		double Sunk = 0.0, Trim = 0.0;
		if (H.SentDownAt >= 0.0) StageSink(ShownT + On - H.SentDownAt, Row.Top, H.How == "scuttled", Sunk, Trim);
		Vec P = H.P + Ahead(H.Hdg) * (H.Spd * On) + Ahead(Hdg) * D.Along + RightOf(Hdg) * D.Across;
		// (on her casing as she rides: her heave, her pitch, bow up, and her heel, starboard down, as ShipOut carries them;
		// her casing as the stage draws the modeller's boat, as made; settling a little as she goes)
		P.Z = H.Heave + CasingAt(Row, D.Along, D.Across) * (1.0 - 0.8 * Down) + D.Z - Dived - Sunk + D.Along * std::sin((H.Pitch + Trim) * Deg) -
		      D.Across * std::sin(H.Heel * Deg);
		return P;
	}

	void Show::StepHands(double Dt)
	{
		// A boat abandoned coming up (LeaveUp): her men over her side, her raft and her boats, once her casing is clear, as
		// AbandonShip has them go (she has long lost her way); sent down first, every man of hers goes as she does (RunWrecks).
		for (auto& Pair : Hulls)
		{
			Hull& H = Pair.second;
			if (!H.bLeftUnder || H.Keel > SurfacedKeel(H.Class) + 0.3) continue;
			H.bLeftUnder = false;
			for (Wreck& W : Wrecked)
			{
				if (W.Ship != H.Key) continue;
				W.Start = ShownT + std::min(20.0, 0.4 * std::fabs(H.Spd) / AbandonedSlowing);
				W.End = W.Start + 90.0;
				W.BoatsFrom = ShownT + std::min(40.0, 0.8 * std::fabs(H.Spd) / AbandonedSlowing);
			}
		}
		// A struck boat coming up: her crew's plan once her casing is clear (her keel within 0.3 m of her trim surfaced).
		for (auto& Pair : Hulls)
		{
			Hull& H = Pair.second;
			if (!H.bStruck || H.UpAt >= 0.0 || H.Doing != Fate::Sinking || H.Keel > SurfacedKeel(H.Class) + 0.3) continue;
			H.UpAt = ShownT;
			for (Wreck& W : Wrecked)
				if (W.Ship == H.Key && W.bStruck) Muster(W, H, 0);
		}
		if (OnCasing.empty() && Boardings.empty()) return;
		// Her crew and her boarders, a step at a time at a man's pace (climbing 0.6 m/s, walking 1.25, running 3; hurried,
		// climbing HurriedClimb, 1.8, her captor's party, five fresh men going back up a ladder they have just come down, 1.5),
		// so that none is ever placed where he was not.
		auto Toward = [Dt](double& X, double To, double Pace)
		{
			X += Clamp(To - X, -Pace * Dt, Pace * Dt);
			return std::fabs(To - X) < 1e-9;
		};
		// (a whaler's places, in its own frame: its oarsmen at the scene's rows for oars, 2, 4 and 6 of its eleven from its
		// stern (SeaBattle's BoatRow), in pairs; its coxswain in its stern sheets, row 0; its party, going across and back,
		// forward of the oars, one to a thwart on the side that lies toward her alongside, so that each steps from his seat up
		// her side, never across another man; the foremost a little in from where it narrows)
		auto SeatOf = [](const Boarding& Crossing, int K)
		{
			static const double PartyBench[5] = { 7.0, 8.0, 9.0, 10.0, 11.0 }, PartyHand[5] = { 0.55, 0.55, 0.55, 0.55, 0.45 };
			const FloatOut& Whaler = Crossing.Whaler;
			const bool bCox = K == 6, bParty = K > 6;
			const int P = bParty ? std::clamp(K - 7, 0, 4) : 0;
			const double Bench = bParty ? PartyBench[P] : bCox ? 0.0 : static_cast<double>(2 + 2 * (K / 2));
			const double Seat = (Bench * 0.76 / 11.0 - 0.38) * Whaler.Long, Hand = bParty ? Crossing.Inboard * PartyHand[P] : bCox ? 0.0 : K % 2 ? -0.55 : 0.55;
			Vec At = Whaler.At + Ahead(Whaler.Heading) * Seat + RightOf(Whaler.Heading) * Hand;
			At.Z = Whaler.At.Z + 0.3;
			return At;
		};
		for (DeckHand& D : OnCasing)
		{
			if (D.O.Key == 0 || ShownT < D.OutAt || D.bBelow) continue;
			const Wreck& W = Wrecked[static_cast<size_t>(D.W)];
			Hull* H = FindHull(W.Ship);
			if (!H || H->Doing == Fate::Gone) continue;
			const BoatDeck& Row = DeckOf(W.Nation);
			const bool bFirst = !D.bOut;
			if (!D.bOut)
			{
				D.bOut = true;
				if (!D.bParty) ++Check.HandsOut;
			}
			const Swim Was = D.O.Doing;
			const double Climb = D.bHurried ? (D.bParty ? 1.5 : HurriedClimb) : 0.6;
			if (D.bParty)
			{
				Boarding* Back = nullptr;
				for (Boarding& B : Boardings)
					if (B.Boat == W.Ship) Back = &B;
				// (his seat in the whaler alongside, in her frame, on her casing's height there: where he goes up her side from, and
				// back to)
				const int SeatNo = 7 + std::clamp(D.N - 10000, 0, 4);
				const Vec Seat = Back ? SeatOf(*Back, SeatNo) : D.At;
				const Vec Off(Seat.X - H->P.X, Seat.Y - H->P.Y, 0.0);
				const double SeatAlong = Off.Dot(Ahead(H->Hdg)), SeatAcross = Off.Dot(RightOf(H->Hdg));
				if (bFirst && Back)
				{
					// (up from his thwart: his first step from where he sat, his feet on it, and his seat left empty, so that he is
					// drawn in one place only, never twice and never missing)
					D.Along = SeatAlong;
					D.Across = SeatAcross;
					DeckHand OnThwart = D;
					OnThwart.Z = 0.0;
					D.Z = Seat.Z - HandAt(OnThwart, *H, 0.0).Z;
					for (size_t K = 0; K < Back->Oars.size() && K < Back->Seated.size(); ++K)
					{
						if (Back->Oars[K].Key != D.O.Key) continue;
						Back->Oars.erase(Back->Oars.begin() + static_cast<long>(K));
						Back->Seated.erase(Back->Seated.begin() + static_cast<long>(K));
						break;
					}
					Back->Whaler.Aboard = static_cast<int>(Back->Oars.size());
				}
				if (D.Leg == 0)
				{
					// (up her side from the whaler alongside, onto the edge of her casing)
					const bool bIn = Toward(D.Across, D.Side * Row.Edge, 0.6);
					if (Toward(D.Z, 0.0, 0.45) && bIn) { D.Leg = 1; D.O.Doing = Swim::Walk; }
				}
				else if (D.Leg == 1)
				{
					// (along her casing to her after hatch, a step inboard of the edge)
					if (Toward(D.Across, D.Side * 0.6, 1.25) && Toward(D.Along, Row.AftHatch, 1.25)) D.Leg = 2;
				}
				else if (D.Leg == 2)
				{
					// (in to it)
					if (Toward(D.Across, 0.0, 1.25)) { D.Leg = 3; D.O.Doing = Swim::Hatch; }
				}
				else if (D.Leg == 3)
				{
					if (Toward(D.Z, -1.9, 0.6))
					{
						// (below: shutting her vents)
						D.bBelow = true;
						++Check.BoardersBelow;
						if (Back) ++Back->Below;
					}
				}
				else if (D.Leg == 10)
				{
					// (she is going: up her after hatch again at a run)
					D.O.Doing = Swim::Hatch;
					if (Toward(D.Z, 0.0, Climb)) { D.Leg = 11; D.O.Doing = Swim::Run; }
				}
				else if (D.Leg == 11)
				{
					// (out to her edge at a run, heading along for his seat as he goes, and over it at once: the rest of the way
					// along is made sliding down her side, so that the last of them is in within HurryFor)
					D.O.Doing = Swim::Run;
					Toward(D.Along, SeatAlong, 3.0);
					if (Toward(D.Across, D.Side * Row.Edge, 3.0)) { D.Leg = 12; D.O.Doing = Swim::Hatch; }
				}
				else
				{
					// (down her side into the whaler, to his seat: sliding down her saddle tank and stepping across, quicker hurried)
					D.O.Doing = Swim::Hatch;
					const double Scramble = D.bHurried ? 2.5 : 2.0;
					const bool bAlong = Toward(D.Along, SeatAlong, Scramble), bAcross = Toward(D.Across, SeatAcross, Scramble);
					DeckHand Level = D;
					Level.Z = 0.0;
					const double SeatZ = Seat.Z - HandAt(Level, *H, 0.0).Z;
					if (Toward(D.Z, SeatZ, Climb) && bAlong && bAcross && Back)
					{
						// (in: one of its crew again, sitting)
						ManOut In = D.O;
						In.At = HandAt(D, *H, 0.0);
						In.Doing = Swim::Boat;
						In.On = Back->Whaler.Key;
						In.Since = 0.0;
						Back->Oars.push_back(In);
						Back->Seated.push_back(SeatNo);
						Back->Whaler.Aboard = static_cast<int>(Back->Oars.size());
						D.O.Key = 0;
						continue;
					}
				}
			}
			else if (D.Leg == 0)
			{
				// (up his hatch's ladder, or down from her bridge, hand over hand)
				if (Toward(D.Z, 0.0, Climb)) { D.Leg = 1; D.O.Doing = Swim::Walk; }
			}
			else if (D.Leg == 1 && ShownT < D.TurnAt)
			{
				// (out from her middle to his side first, then along to his place, clear of her tower)
				const bool bAcross = Toward(D.Across, D.PlaceAcross, 1.25);
				const bool bThere = bAcross && Toward(D.Along, D.PlaceAlong, 1.25);
				D.O.Doing = bThere ? (D.bUp ? Swim::HandsUp : Swim::Stand) : Swim::Walk;
			}
			else
			{
				// (his turn: out to the edge of her casing at a run, where her saddle tanks fall away below him)
				D.Leg = 2;
				if (Toward(D.Across, D.Side * Row.Edge, 3.0))
				{
					D.bAtEdge = true;
					D.O.Doing = Swim::Stand;
				}
				else D.O.Doing = Swim::Run;
			}
			D.O.Since = D.O.Doing == Was ? D.O.Since + Dt : 0.0;
			D.At = HandAt(D, *H, 0.0);
		}
		// Her captor's whaler: lowered from his davits once she is up and he lies stopped near her, shoved off square from his
		// side, pulled across to off her after casing on her lee side (round her bow or her stern where he lies on her other
		// side, its way clear of both by the swing of its own length) and in square to her, its party up her side; three of
		// them below for half a minute and she is a prize. Sent down after all, it takes its party back, shoves off her and
		// pulls back to off his davits, in square, to be hoisted in. Square, alongside or off it, it never swings its length
		// into a hull.
		constexpr double LowerFor = 5.0, Shove = 0.6;
		for (Boarding& B : Boardings)
		{
			if (B.bStowed) continue;
			Hull* Boat = FindHull(B.Boat);
			const Hull* By = FindHull(B.By);
			const Wreck* Her = nullptr;
			for (const Wreck& W : Wrecked)
				if (W.Ship == B.Boat) Her = &W;
			if (!Her) continue;
			const BoatDeck& Row = DeckOf(Her->Nation);
			const bool bBoat = Boat && Boat->Doing != Fate::Gone, bBy = By && By->Doing != Fate::Gone;
			// (its middle abreast her after casing, far enough aft that its party's thwarts, forward of its oars, are all abreast
			// that stretch, aft of her tower, where each goes up her side from his seat)
			const double Along = Row.AftTo - 3.5;
			// (on her lee side, out of the wind and clear of her crew going over her weather side; kept clear of her and of him
			// on its way by the swing of its own length, so that it may turn anywhere on it)
			const double Clear = 0.5 * B.Whaler.Long + 0.2;
			// (alongside her, its half beam and a little more out from her side; at his davits, his side toward her, out from it
			// by its half beam and a little more; and off each, square to it, as far again as it swings beyond its half beam)
			const double Swing = 0.5 * (B.Whaler.Long - B.Whaler.Beam) + 0.3;
			const Vec HerOut = bBoat ? RightOf(Boat->Hdg) * (-Her->Lee) : Vec();
			const Vec Alongside = bBoat ? Boat->P + Ahead(Boat->Hdg) * Along + HerOut * (0.5 * Row.Wide + 0.5 * B.Whaler.Beam + 0.3) : B.Whaler.At;
			const Vec OffHer = Alongside + HerOut * Swing;
			const Vec HisOut = bBy ? RightOf(By->Hdg) * B.Hand : Vec();
			const Vec Davits = bBy ? By->P + HisOut * (0.5 * SizeOf(By->Class).Beam + 0.5 * B.Whaler.Beam + 0.6) : B.Whaler.At;
			const Vec OffHim = Davits + HisOut * Swing;
			// (still unless it pulls this step)
			B.Pull = Vec();
			auto PullTo = [&B, Dt](const Vec& Goal, const Vec& Way, double Rate)
			{
				// (pulled at a steady 1.8 m/s, three and a half knots, for the next point of its way; shoved off or brought in
				// square at Shove)
				const Vec To(Way.X - B.Whaler.At.X, Way.Y - B.Whaler.At.Y, 0.0);
				const double Left = Flat(To);
				B.Pull = To * (std::min(Left / Dt, Rate) / std::max(1e-9, Left));
				B.Whaler.At += B.Pull * Dt;
				const double ToGoal = Flat(Vec(Goal.X - B.Whaler.At.X, Goal.Y - B.Whaler.At.Y, 0.0));
				B.Whaler.Way = ToGoal > 0.05 ? Rate : 0.0;
				return ToGoal;
			};
			auto Turn = [&B, Dt](double Want) { B.Whaler.Heading = Wrap(B.Whaler.Heading + Clamp(Wrap(Want - B.Whaler.Heading), -0.35 * Dt, 0.35 * Dt)); };
			// (on its way to off a berth, round her and round him: its head round to the hull's as it comes near; there and
			// square to it, true)
			auto CrossTo = [&](const Vec& Off, double Hdg)
			{
				Vec Way = bBy ? RoundHull(B.Whaler.At, Off, *By, Clear) : Off;
				// (rounding her, it is carried with her as she drifts and turns no more: so a way round her end found clear
				// stays clear as it pulls along it, and it never hangs off her end between two ways round, as it did astern of
				// a prize drifting down the wind at a quarter of a metre a second, the review of 24 November, the second)
				Vec Carried;
				if (bBoat)
				{
					Vec Round = RoundHull(B.Whaler.At, Way, *Boat, Clear);
					if (Flat(Round - Way) > 1e-6 && Boat->bHasLast)
					{
						Carried = Vec(Boat->P.X - Boat->LastP.X, Boat->P.Y - Boat->LastP.Y, 0.0);
						B.Whaler.At += Carried;
						Round = RoundHull(B.Whaler.At, Way, *Boat, Clear);
					}
					Way = Round;
				}
				const double Left = PullTo(Off, Way, 1.8);
				B.Pull += Carried * (1.0 / Dt);
				Turn(Left > 12.0 ? Bearing(B.Whaler.At, Way) : Hdg);
				return Left < 0.05 && std::fabs(Wrap(B.Whaler.Heading - Hdg)) < 0.02;
			};
			if (B.LoweredAt < 0.0)
			{
				if (!bBoat || Boat->UpAt < 0.0 || Boat->SentDownAt >= 0.0 || !bBy || By->Spd > 0.5 || Flat(By->P - Boat->P) > 1800.0) continue;
				// (lowered from his davits on his side toward her, all in it: six at the oars, its coxswain, and its boarding party
				// of five on the thwarts forward of the oars, who cross in it and go up her side from them)
				B.LoweredAt = ShownT;
				B.Hand = (Boat->P - By->P).Dot(RightOf(By->Hdg)) >= 0.0 ? 1.0 : -1.0;
				// (alongside, its heading hers, it lies on her other side from her crew going over (her Lee): so she is to its Lee)
				B.Inboard = Her->Lee;
				B.Whaler.At = By->P + RightOf(By->Hdg) * (B.Hand * (0.5 * SizeOf(By->Class).Beam + 0.5 * B.Whaler.Beam + 0.6));
				B.Whaler.At.Z = B.Davit;
				B.Whaler.Heading = By->Hdg;
				for (int K = 0; K < 12; ++K)
				{
					ManOut O;
					O.Key = NextMan++;
					O.Ship = By->Key;
					O.Side = By->Side;
					O.Nation = By->Nation;
					O.Doing = Swim::Boat;
					O.On = B.Whaler.Key;
					// (its crew's own hash, and its party's)
					const uint64_t Hash = K < 7 ? static_cast<uint64_t>(O.Key) * 131 + 7 : static_cast<uint64_t>(O.Key) * 137 + 11;
					O.Look = static_cast<uint32_t>(Mix(Hash) * 4294967295.0);
					B.Oars.push_back(O);
					B.Seated.push_back(K);
				}
				B.Whaler.Aboard = static_cast<int>(B.Oars.size());
			}
			else if (ShownT < B.LoweredAt + LowerFor)
			{
				// (going down his side to the water as he lies: carried with him, down at a steady rate)
				const Vec Was = B.Whaler.At;
				B.Whaler.At = Vec(Davits.X, Davits.Y, B.Davit * Clamp(1.0 - (ShownT - B.LoweredAt) / LowerFor, 0.0, 1.0));
				B.Whaler.Heading = bBy ? By->Hdg : B.Whaler.Heading;
				B.Pull = (B.Whaler.At - Was) * (1.0 / Dt);
			}
			else if (B.HomeAt >= 0.0)
			{
				if (B.HoistAt < 0.0)
				{
					// (back to his davits: shoved off her square where it lay alongside, round her and round him to off them, and in
					// square)
					B.Whaler.At.Z = 0.0;
					const double HisHdg = bBy ? By->Hdg : B.Whaler.Heading;
					if (B.Leg == 3)
					{
						if (bBoat) Turn(Boat->Hdg);
						if (!bBoat || PullTo(OffHer, OffHer, Shove) < 0.05) B.Leg = 4;
					}
					else if (B.Leg == 4)
					{
						if (CrossTo(OffHim, HisHdg)) B.Leg = 5;
					}
					else
					{
						Turn(HisHdg);
						if (PullTo(Davits, Davits, Shove) < 0.05)
						{
							B.HoistAt = ShownT;
							B.Whaler.Way = 0.0;
						}
					}
				}
				else
				{
					// (hoisted in, its crew in it, and stowed)
					const Vec Was = B.Whaler.At;
					B.Whaler.At = Vec(Davits.X, Davits.Y, B.Davit * Clamp((ShownT - B.HoistAt) / LowerFor, 0.0, 1.0));
					B.Pull = (B.Whaler.At - Was) * (1.0 / Dt);
					if (ShownT >= B.HoistAt + LowerFor) B.bStowed = true;
				}
			}
			else if (B.AlongsideAt < 0.0)
			{
				if (!bBoat || Boat->SentDownAt >= 0.0)
				{
					// (she is going before it is there: back to him)
					B.HomeAt = ShownT;
					B.Leg = 4;
					continue;
				}
				// (shoved off his side square to it; across to off her, round her bow or her stern where she lies between; and in
				// square to her)
				B.Whaler.At.Z = 0.0;
				double Left = 1e9;
				if (B.Leg == 0)
				{
					if (bBy) Turn(By->Hdg);
					if (!bBy || PullTo(OffHim, OffHim, Shove) < 0.05) B.Leg = 1;
				}
				else if (B.Leg == 1)
				{
					if (CrossTo(OffHer, Boat->Hdg)) B.Leg = 2;
				}
				else
				{
					Turn(Boat->Hdg);
					Left = PullTo(Alongside, Alongside, Shove);
				}
				if (Left < 0.05)
				{
					B.AlongsideAt = ShownT;
					B.Whaler.Way = 0.0;
					B.Pull = Vec();
					// (her party up her side from their thwarts: five men, a man each two seconds, each the man who sat there (his
					// key and his look), leaving his seat as he goes, StepHands)
					for (size_t K = 0; K < B.Oars.size() && K < B.Seated.size(); ++K)
					{
						if (B.Seated[K] < 7) continue;
						const int Boarder = B.Seated[K] - 7;
						DeckHand D;
						D.W = static_cast<int>(Her - Wrecked.data());
						D.N = 10000 + Boarder;
						D.O = B.Oars[K];
						D.O.Doing = Swim::Hatch;
						D.O.On = -1;
						D.O.Since = 0.0;
						D.bParty = true;
						D.Side = -Her->Lee;
						D.OutAt = ShownT + 2.0 * Boarder;
						D.TurnAt = 1e18;
						D.At = B.Oars[K].At;
						OnCasing.push_back(D);
						++B.Party;
					}
				}
			}
			else
			{
				// (alongside: kept there as she drifts; her vents shut, three below, she is a prize)
				if (bBoat)
				{
					B.Whaler.At = Vec(Alongside.X, Alongside.Y, 0.0);
					Turn(Boat->Hdg);
				}
				if (B.Below >= 3 && B.PrizeAt < 0.0) B.PrizeAt = ShownT + 30.0;
				if (bBoat && B.PrizeAt >= 0.0 && ShownT >= B.PrizeAt && !Boat->bPrize && Boat->bAbandoned)
				{
					Boat->bPrize = true;
					Boat->FlagDownAt = ShownT;
					++Check.Prizes;
				}
				// (sent down after all: off once its party is back in it, or she is gone)
				if (!bBoat || Boat->SentDownAt >= 0.0)
				{
					bool bAllBack = true;
					for (const DeckHand& D : OnCasing)
						if (D.O.Key != 0 && D.bParty && Wrecked[static_cast<size_t>(D.W)].Ship == B.Boat) bAllBack = false;
					if (bAllBack || !bBoat || ShownT > Boat->SentDownAt + 3.0 * HurryFor)
					{
						B.HomeAt = ShownT;
						B.Leg = 3;
					}
				}
			}
			for (size_t K = 0; K < B.Oars.size() && K < B.Seated.size(); ++K)
			{
				ManOut& O = B.Oars[K];
				O.At = SeatOf(B, B.Seated[K]);
				O.Heading = B.Seated[K] >= 6 ? B.Whaler.Heading : Wrap(B.Whaler.Heading + Pi);
				O.Since += Dt;
			}
		}
		OnCasing.erase(std::remove_if(OnCasing.begin(), OnCasing.end(), [](const DeckHand& D) { return D.O.Key == 0; }), OnCasing.end());
	}

	void Show::NoteLost(const Hull& H, int N)
	{
		int& Count = LostCount[H.Piece];
		const int K = N > 0 ? N : Count + 1;
		Count = std::max(Count, K);
		LostShips[{ H.Piece, K }] = H.Key;
	}

	Show::Wreck* Show::WreckOf(int Piece, int Ship)
	{
		const auto Many = LostCount.find(Piece);
		const int K = Ship > 0 ? Ship : Many != LostCount.end() ? Many->second : 0;
		const auto It = LostShips.find({ Piece, K });
		if (It == LostShips.end()) return nullptr;
		for (Wreck& W : Wrecked)
			if (W.Ship == It->second) return &W;
		return nullptr;
	}

	void Show::RunWrecks(double Dt)
	{
		if (Wrecked.empty()) return;
		bWrecksMoved = true;
		const Vec Blow(WindV.X, WindV.Y, 0.0);
		const double DownWind = Bearing(Vec(), Blow);
		const long Beat = std::lround(ShownT / Fixed);
		auto Under = [this](const Hull& S) { return S.Class == "ss" && S.Keel > SurfacedKeel(S.Class) + 1.0; };

		// The ships near each wreck's men and wreckage, looked for once a second (and found again each step).
		if (Beat % 20 == 0)
		{
			std::vector<double> SX(Wrecked.size(), 0.0), SY(Wrecked.size(), 0.0);
			std::vector<int> N(Wrecked.size(), 0);
			for (const Man& M : Swimmers) { SX[M.W] += M.O.At.X; SY[M.W] += M.O.At.Y; ++N[M.W]; }
			for (const Floater& Fl : Afloat) { SX[Fl.W] += Fl.O.At.X; SY[Fl.W] += Fl.O.At.Y; ++N[Fl.W]; }
			for (size_t WI = 0; WI < Wrecked.size(); ++WI)
			{
				Wreck& W = Wrecked[WI];
				W.Middle = N[WI] ? Vec(SX[WI] / N[WI], SY[WI] / N[WI], 0.0) : W.OilEnd;
				W.Spread = 0.5 * W.Long;
			}
			for (const Man& M : Swimmers) Wrecked[M.W].Spread = std::max(Wrecked[M.W].Spread, Flat(M.O.At - Wrecked[M.W].Middle));
			for (const Floater& Fl : Afloat) Wrecked[Fl.W].Spread = std::max(Wrecked[Fl.W].Spread, Flat(Fl.O.At - Wrecked[Fl.W].Middle) + 0.5 * Fl.O.Long);
			for (Wreck& W : Wrecked)
			{
				W.Near.clear();
				W.Nearest = 1e9;
				W.NearAt = ShownT;
				for (const auto& Pair : Hulls)
				{
					const Hull& S = Pair.second;
					if (S.Doing == Fate::Gone || Under(S)) continue;
					const double D = Flat(S.P - W.Middle);
					if (S.Key != W.Ship) W.Nearest = std::min(W.Nearest, D);
					if (D < W.Spread + 0.5 * SizeOf(S.Class).Long + S.Spd * 1.5 + 60.0) W.Near.push_back(S.Key);
				}
			}
		}

		// What goes over each one's side, or floats off her, now; her oil.
		for (size_t WI = 0; WI < Wrecked.size(); ++WI)
		{
			Wreck& W = Wrecked[WI];
			const Hull* H = FindHull(W.Ship);
			const bool bAfloat = H && H->Doing == Fate::Sinking;
			if (bAfloat) W.OilEnd = Vec(H->P.X, H->P.Y, 0.0);
			else W.bGone = true;
			W.NearNow.clear();
			W.NearHalf.clear();
			W.NearR.clear();
			for (const int Key : W.Near)
			{
				const Hull* S = FindHull(Key);
				if (!S || S->Doing == Fate::Gone || Under(*S)) continue;
				const ClassSize& Sz = SizeOf(S->Class);
				W.NearNow.push_back(S);
				W.NearHalf.push_back(std::max(0.0, 0.5 * (Sz.Long - Sz.Beam)));
				W.NearR.push_back(0.5 * Sz.Beam);
			}
			const Vec At = bAfloat ? Vec(H->P.X, H->P.Y, 0.0) : W.OilEnd;
			const double Hdg = bAfloat ? H->Hdg : W.TrackHdg, Spd = bAfloat ? H->Spd : 0.0;
			const double Half = std::max(0.0, 0.5 * (W.Long - W.Beam)), R = 0.5 * W.Beam;
			// (a point on her side: along her from amidships, on one side, so far out from it)
			auto SideOf = [&](double Along, double Side, double Out)
			{
				const double Over = std::fabs(Along) > Half ? std::sqrt(std::max(0.0, R * R - (std::fabs(Along) - Half) * (std::fabs(Along) - Half))) : R;
				return At + Ahead(Hdg) * Along + RightOf(Hdg) * (Side * (Over + Out));
			};
			auto AddFloat = [&](const Kind& K, const Vec& P, double Heading, const Vec& V, double Turn)
			{
				Floater Fl;
				Fl.O.Key = NextFloat++;
				Fl.O.Ship = W.Ship;
				Fl.O.Kind = K.Name;
				Fl.O.At = Vec(P.X, P.Y, 0.0);
				Fl.O.Heading = Heading;
				Fl.O.Long = K.Long;
				Fl.O.Beam = K.Beam;
				Fl.V = Vec(V.X, V.Y, 0.0);
				Fl.Leeway = K.Leeway;
				Fl.Turn = Turn;
				Fl.Room = K.Room;
				Fl.Hold = K.Hold;
				Fl.W = static_cast<int>(WI);
				Afloat.push_back(Fl);
				W.Floats.push_back(Fl.O.Key);
				++Check.Floats;
				if (K.Room > 0 && std::string(K.Name) == "raft") ++Check.Rafts;
				if (std::string(K.Name) == "boat") ++Check.Boats;
			};
			const double Share = Clamp((ShownT - W.Start) / std::max(1.0, W.End - W.Start), 0.0, 1.0);
			const double Down = Clamp((ShownT - W.T0) / W.For, 0.0, 1.0);
			// A boat that struck: her men go over from her casing, each as he reaches its edge at his turn (Muster's plan,
			// StepHands), as the man he is (his key and his look), running and jumping clear of her saddle tanks; her dead go
			// down in her (those who come up her hatches are her living). Gone under with men still on her casing, they are
			// in the water where they stood.
			if (W.bStruck)
			{
				const BoatDeck& Row = DeckOf(W.Nation);
				for (DeckHand& D : OnCasing)
				{
					if (D.W != static_cast<int>(WI) || D.O.Key == 0) continue;
					if (bAfloat && !D.bAtEdge) continue;
					if (!bAfloat && (!D.bOut || D.bBelow))
					{
						// (still in her as she goes: one of the men the battle counts, over with the rest below)
						D.O.Key = 0;
						continue;
					}
					const uint64_t S = W.Seed + 7919ULL * static_cast<uint64_t>(D.N + 1);
					Man M;
					M.O = D.O;
					M.O.At = D.At;
					M.O.Since = 0.0;
					M.W = static_cast<int>(WI);
					M.Pace = 0.45 + 0.35 * Mix(S + 5);
					M.Next = ShownT + 1.0 + 2.0 * Mix(S + 7);
					M.O.Doing = Swim::Jump;
					M.O.Drop = std::max(0.6, W.Free * (1.0 - 0.8 * Down));
					// (out over her saddle tanks to land a metre clear of them: as fast across as his fall from where he stands allows)
					const Vec Out = RightOf(Hdg) * D.Side;
					const double Z0 = std::max(0.3, D.At.Z);
					const double Fall = (0.8 + std::sqrt(0.64 + 2.0 * Gee * Z0)) / Gee;
					M.O.At.Z = Z0;
					M.V = Ahead(Hdg) * Spd + Out * (std::max(0.5, 0.5 * Row.Wide + 1.0 - std::fabs(D.Across)) / Fall);
					M.V.Z = 0.8;
					M.O.Heading = Bearing(Vec(), Out);
					Swimmers.push_back(M);
					D.O.Key = 0;
					++Check.OverFromDeck;
					++Check.MenOfStruck;
					if (D.bParty) continue;
					++W.Out;
				}
			}
			// Her men over the side: from her deck's edge, the living jumping and the dead fallen or thrown.
			const int MenDue = W.bStruck && bAfloat ? W.Out : bAfloat ? static_cast<int>(std::floor(W.Men * Share)) : W.Men;
			while (W.Out < MenDue)
			{
				const uint64_t S = W.Seed + 7919ULL * static_cast<uint64_t>(W.Out + 1);
				Man M;
				M.O.Key = NextMan++;
				M.O.Ship = W.Ship;
				M.O.Side = W.Side;
				M.O.Nation = W.Nation;
				M.O.Look = static_cast<uint32_t>(Mix(S + 3) * 4294967295.0);
				M.W = static_cast<int>(WI);
				M.Pace = 0.45 + 0.35 * Mix(S + 5);
				M.Next = ShownT + 1.0 + 2.0 * Mix(S + 7);
				// (men leaving her in good order go over her windward side, nearly all of them, down her nets and lines from her
				// waist and her stern; otherwise from anywhere along her, most on that side)
				const double Side = Mix(S + 17) < (W.bLeft ? 0.88 : 0.7) ? W.Lee : -W.Lee;
				const Vec Out = RightOf(Hdg) * Side;
				const double Along = !W.bLeft ? (Mix(S + 13) - 0.5) * 0.8 * W.Long
					: Mix(S + 31) < 0.55 ? (-0.04 + (Mix(S + 13) - 0.5) * 0.16) * W.Long : (-0.36 + (Mix(S + 13) - 0.5) * 0.1) * W.Long;
				const Vec Edge = SideOf(Along, Side, 0.9);
				if (Mix(S + 11) < W.DeadShare)
				{
					M.O.Doing = Swim::Dead;
					M.O.At = Edge + Out * (1.0 + 8.0 * Mix(S + 19));
					M.O.At.Z = 0.0;
					M.O.Heading = Mix(S + 23) * 2.0 * Pi;
					++W.Dead;
					++Check.MenDead;
				}
				else
				{
					M.O.Doing = Swim::Jump;
					// (down a net or a line most of the way, leaving her in good order; or jumping from her deck)
					M.O.Drop = W.bLeft && Mix(S + 37) < 0.6 ? 0.8 : std::max(0.6, W.Free * (1.0 - 0.8 * Down));
					// (a boat has no nets down her side: her men jump from her casing)
					if (W.Class == "ss") M.O.Drop = std::max(0.6, W.Free * (1.0 - 0.8 * Down));
					M.O.At = Vec(Edge.X, Edge.Y, M.O.Drop);
					M.V = Ahead(Hdg) * (Spd * 0.8) + Out * (1.6 + 0.8 * Mix(S + 29));
					M.V.Z = 0.8;
					M.O.Heading = Bearing(Vec(), Out);
				}
				if (W.bStruck) ++Check.MenOfStruck;
				Swimmers.push_back(M);
				++W.Out;
			}
			if (!W.bDue && W.Real > 0 && ShownT >= W.Start + 10.0)
			{
				W.bDue = true;
				++Check.WrecksDue;
				if (W.Out > 0) ++Check.WrecksManned;
			}
			// Her rafts, thrown over or floating free, mostly on the side her men go; her boats lowered, early on.
			const double RaftShare = W.bLeft ? Clamp((ShownT - W.Start) / std::max(1.0, 0.5 * (W.End - W.Start)), 0.0, 1.0) : Share;
			const int RaftsDue = bAfloat ? static_cast<int>(std::ceil(W.Rafts * RaftShare - 1e-9)) : W.Rafts;
			while (W.RaftsOut < RaftsDue)
			{
				const uint64_t S = W.Seed + 104729ULL * static_cast<uint64_t>(W.RaftsOut + 1);
				const double Side = Mix(S + 1) < 0.8 ? W.Lee : -W.Lee;
				const Vec P = SideOf((Mix(S + 2) - 0.5) * (W.bLeft ? 0.6 : 0.85) * W.Long, Side, 2.0 + 4.0 * Mix(S + 3));
				AddFloat(W.Class == "ss" || W.Class == "mv" ? RaftSmall : RaftBig, P, Mix(S + 4) * 2.0 * Pi, RightOf(Hdg) * (Side * (0.3 + 0.3 * Mix(S + 5))), (Mix(S + 6) - 0.5) * 0.04);
				++W.RaftsOut;
			}
			const double BoatsFrom = W.BoatsFrom >= 0.0 ? W.BoatsFrom : W.Start;
			const double BoatShare = Clamp((ShownT - BoatsFrom) / std::max(1.0, 0.5 * (W.End - BoatsFrom)), 0.0, 1.0);
			const int BoatsDue = bAfloat ? static_cast<int>(std::ceil(W.Boats * BoatShare - 1e-9)) : W.Boats;
			while (W.BoatsOut < BoatsDue)
			{
				const uint64_t S = W.Seed + 1299709ULL * static_cast<uint64_t>(W.BoatsOut + 1);
				const double Side = W.BoatsOut % 2 ? -W.Lee : W.Lee;
				const Vec P = SideOf((W.BoatsOut / 2 % 2 ? -0.25 : 0.2) * W.Long, Side, 3.0);
				// (pushed off and pulled clear bow first, her head out from the side and a little aft: the art director, 22
				// November 12:17Z)
				const double Out = Wrap(Hdg + Side * (0.5 * Pi + 0.35 + (Mix(S + 1) - 0.5) * 0.3));
				AddFloat(Lifeboat, P, Out, Ahead(Out) * 0.3, 0.0);
				++W.BoatsOut;
			}
			// Her wreckage: some as she goes, most as she goes under (and what comes up after a boat lost deep). Left by her men,
			// half of it is what they throw over to hold on to as they go (hatch covers, timber, floats).
			const int Early = static_cast<int>(std::lround(W.Bits * (W.bLeft ? 0.5 : 0.3)));
			const double Late = W.bGone ? Clamp((ShownT - (W.T0 + W.For)) / 15.0, 0.0, 1.0) : 0.0;
			const int BitsDue = static_cast<int>(std::lround(Early * (W.bLeft ? Share : Down))) + static_cast<int>(std::lround((W.Bits - Early) * Late));
			while (W.BitsOut < BitsDue)
			{
				const uint64_t S = W.Seed + 15485863ULL * static_cast<uint64_t>(W.BitsOut + 1);
				const Kind& K = WreckageOf(Mix(S + 1));
				Vec P, V;
				if (W.BitsOut < Early)
				{
					const double Side = W.bLeft ? (Mix(S + 2) < 0.85 ? W.Lee : -W.Lee) : Mix(S + 2) < 0.5 ? 1.0 : -1.0;
					P = SideOf((Mix(S + 3) - 0.5) * 0.9 * W.Long, Side, 1.0 + 6.0 * Mix(S + 4));
					V = RightOf(Hdg) * (Side * (0.2 + 0.4 * Mix(S + 5)));
				}
				else
				{
					// (up where she went under, over her length and a beam and a half across, spreading from it)
					const Vec Along = Ahead(W.TrackHdg) * ((Mix(S + 3) - 0.5) * 0.9 * W.Long), Across = RightOf(W.TrackHdg) * ((Mix(S + 4) - 0.5) * 1.4 * W.Beam);
					P = W.OilEnd + Along + Across;
					const Vec Away = (Along * 0.3 + Across).Unit(Ahead(Mix(S + 6) * 2.0 * Pi));
					V = Vec(Away.X, Away.Y, 0.0) * (0.3 + 0.7 * Mix(S + 5));
				}
				AddFloat(K, P, Mix(S + 7) * 2.0 * Pi, V, (Mix(S + 8) - 0.5) * 0.06);
				++W.BitsOut;
			}
			// Her oil: from where she was hit along her track to where she went under, spreading and thinning, drawn out down
			// the wind once she is gone, and drifting at a thirtieth of the wind's speed; alight for a while where it burns.
			{
				SlickOut& S = Oil[W.Slick - 1];
				const double Age = ShownT - W.T0;
				W.Drift += Blow * (0.03 * Dt);
				W.TrackLen = Flat(W.OilEnd - W.Oil0);
				if (W.TrackLen > 1.0) W.TrackHdg = Bearing(W.Oil0, W.OilEnd);
				const double Grown = W.Beam * 0.5 + 6.0 * std::sqrt(std::max(0.0, Age)) * CompanyOf(W.Class).Oil;
				const double Rad = W.OilR > 0.0 ? std::min(Grown, std::max(0.5 * W.Beam, 0.5 * W.OilR)) : Grown;
				const double Turned = Clamp((ShownT - (W.T0 + W.For)) / 300.0, 0.0, 1.0);
				// (an oval has no fore and aft: its long way along her track turned the nearer way to the wind's)
				double Along = W.TrackHdg;
				if (std::fabs(Wrap(DownWind - Along)) > 0.5 * Pi) Along = Wrap(Along + Pi);
				S.At = (W.Oil0 + W.OilEnd) * 0.5 + W.Drift;
				S.At.Z = 0.0;
				S.Heading = W.TrackLen > Rad ? Wrap(Along + Wrap(DownWind - Along) * Turned) : DownWind;
				S.Long = W.TrackLen * (1.0 - 0.5 * Turned) + 2.0 * Rad * (1.0 + (0.2 + 0.05 * WindU) * (0.3 + 0.7 * Turned));
				S.Wide = 2.0 * Rad * 0.85;
				S.Dark = Clamp(Age / 6.0, 0.0, 1.0) * (Age < 600.0 ? 0.5 - 0.2 * Age / 600.0 : std::max(0.22, 0.3 - 0.08 * (Age - 600.0) / 1200.0));
				S.Burning = W.Burn > 0.0 && ShownT < W.BurnUntil
					? W.Burn * Clamp((ShownT - W.T0 - 0.2 * W.For) / 10.0, 0.0, 1.0) * Clamp((W.BurnUntil - ShownT) / 60.0, 0.0, 1.0) : 0.0;
				S.Age = Age;
				if (S.Burning > 0.0 && !W.bLit)
				{
					// (the kit's oil fire where it burns: a fire on the sea stays where it is lit)
					W.bLit = true;
					Fx& E = Emit(FxKind::OilFire, S.At, "burning", SlickFireKeys + W.Slick);
					E.Size = W.LitR > 0.0 ? Clamp(0.5 * W.LitR, 20.0, 250.0)
						: Clamp((W.Beam * 0.5 + 6.0 * std::sqrt(Age + 120.0) * CompanyOf(W.Class).Oil) * std::sqrt(W.Burn), 20.0, 250.0);
					E.Until = W.BurnUntil;
					E.Since = 0.0;
					E.Side = W.Side;
					++Check.SlicksAlight;
				}
			}
		}

		// Her rafts, boats and wreckage: spreading from her and easing, drifting down the wind (a raft at about a thirtieth of
		// its speed, wreckage a fortieth), turning slowly; a raft or boat with men in it paddled or pulled to a ship picking
		// them up; pushed aside by any ship's bow.
		for (Floater& Fl : Afloat)
		{
			Wreck& W = Wrecked[Fl.W];
			Fl.O.Aboard = 0;
			Fl.O.Holding = 0;
			Fl.O.At += (Fl.V + Blow * Fl.Leeway) * Dt;
			Fl.V = Fl.V * (1.0 - Dt / 40.0);
			Fl.O.Heading = Wrap(Fl.O.Heading + Fl.Turn * Dt);
			Fl.Turn *= 1.0 - Dt / 90.0;
			// (and a float with men holding on to it or gathered round it, kicked along slowly toward her: the group comes to her
			// together, the art director, 22 November 18:54Z)
			if (((Fl.Room > 0 && Fl.InMask) || Fl.HoldMask || Fl.Group > 0) && W.Rescuer >= 0 && ShownT < W.RescueUntil)
			{
				const Hull* By = FindHull(W.Rescuer);
				if (By && By->Doing != Fate::Gone && By->Spd < 3.0 && Flat(By->P - Fl.O.At) < 900.0)
				{
					// (round any other ship lying across the way to her, by her nearer end, rather than along her side)
					Vec Aim = By->P;
					double Along = 1e18;
					const double Body0 = 0.5 * std::max(Fl.O.Long, Fl.O.Beam) + 0.8;
					for (size_t K = 0; K < W.NearNow.size(); ++K)
					{
						const Hull* S = W.NearNow[K];
						if (S == By) continue;
						const Vec Path(By->P.X - Fl.O.At.X, By->P.Y - Fl.O.At.Y, 0.0);
						const double Len2 = Path.Dot(Path);
						if (Len2 < 1.0) continue;
						const double U = Clamp((S->P - Fl.O.At).Dot(Path) / Len2, 0.0, 1.0);
						const Vec Closest = Fl.O.At + Path * U;
						const double Clear = W.NearHalf[K] + W.NearR[K] + Body0 + 5.0;
						if (U <= 0.0 || U >= 1.0 || Flat(S->P - Closest) > Clear || U * std::sqrt(Len2) > Along) continue;
						Along = U * std::sqrt(Len2);
						const Vec End = Ahead(S->Hdg) * (W.NearHalf[K] + W.NearR[K] + Body0 + 12.0);
						const Vec E1 = S->P + End, E2 = S->P - End;
						Aim = Flat(E1 - Fl.O.At) + Flat(By->P - E1) <= Flat(E2 - Fl.O.At) + Flat(By->P - E2) ? E1 : E2;
					}
					const Vec To = Aim - Fl.O.At;
					const Vec Want = Vec(To.X, To.Y, 0.0).Unit(Vec()) * (Fl.Room > 0 && Fl.InMask ? (Fl.O.Kind == "boat" ? 0.8 : 0.4) : 0.25);
					Fl.V += (Want - Fl.V) * Clamp(Dt / 5.0, 0.0, 1.0);
				}
			}
			// (its middle kept so far from her side that its ends and the men holding on to it are clear of her too; its way
			// into her side stopped there, only its way along her left)
			const double Body = 0.5 * std::max(Fl.O.Long, Fl.O.Beam) + 0.8;
			for (size_t K = 0; K < W.NearNow.size(); ++K)
			{
				const Vec Was = Fl.O.At;
				if (PushOut(Fl.O.At, W.NearNow[K]->P, W.NearNow[K]->Hdg, W.NearHalf[K], W.NearR[K] + Body))
				{
					const Vec Out = Vec(Fl.O.At.X - Was.X, Fl.O.At.Y - Was.Y, 0.0).Unit(Vec());
					const double Into = Fl.V.X * Out.X + Fl.V.Y * Out.Y;
					if (Into < 0.0) Fl.V = Fl.V - Out * Into;
				}
			}
			Fl.O.At.Z = 0.0;
			// (a boat with men in her making her own way, pushed off or pulled for a ship, goes bow first, her oarsmen's backs
			// to the way she goes: her coxswain brings her head round to it at a steering oar's 17 degrees a second, and she
			// no longer turns on her own; adrift, or held against a hull, she lies as the sea turns her. The art director, 22
			// November 12:17Z)
			Fl.O.Way = 0.0;
			if (Fl.O.Kind == "boat" && Fl.InMask && Flat(Fl.V) > 0.15)
			{
				const double Off = Wrap(Bearing(Vec(), Fl.V) - Fl.O.Heading);
				Fl.O.Heading = Wrap(Fl.O.Heading + Clamp(Off, -0.3 * Dt, 0.3 * Dt));
				Fl.Turn = 0.0;
				Fl.O.Way = Flat(Fl.V);
			}
		}

		// (rafts and boats never lie in one another, brought together at a rescuer's side: pushed apart, half each way, or a
		// raft the whole way from a boat)
		{
			std::vector<Floater*> Manned;
			for (Floater& Fl : Afloat)
				if (Fl.Room > 0) Manned.push_back(&Fl);
			for (size_t I = 0; I < Manned.size(); ++I)
				for (size_t J = I + 1; J < Manned.size(); ++J)
				{
					Floater& A = *Manned[I];
					Floater& B = *Manned[J];
					const double Need = 0.5 * (std::max(A.O.Long, A.O.Beam) + std::max(B.O.Long, B.O.Beam)) + 0.3;
					const Vec D(B.O.At.X - A.O.At.X, B.O.At.Y - A.O.At.Y, 0.0);
					const double L = Flat(D);
					if (L >= Need || L < 1e-6) continue;
					// (a boat under oars holds her way: a raft against her is the one moved)
					const bool bA = A.O.Kind == "boat", bB = B.O.Kind == "boat";
					const double ToA = bA == bB ? 0.5 : bA ? 0.0 : 1.0;
					const Vec Apart = D * ((Need - L) / L);
					A.O.At = A.O.At - Apart * ToA;
					B.O.At = B.O.At + Apart * (1.0 - ToA);
				}
		}

		// Her men. (First, the men round each float now: aboard it, holding on, or gathered by it treading water.)
		for (Floater& Fl : Afloat) Fl.Group = 0;
		for (const Man& M : Swimmers)
		{
			const int Key = M.O.On >= 0 ? M.O.On : M.Near;
			if (Floater* Fl = Key >= 0 ? FloatByKey(Key) : nullptr) ++Fl->Group;
		}
		int InWater = 0;
		for (Man& M : Swimmers)
		{
			Wreck& W = Wrecked[M.W];
			M.O.Since += Dt;
			Floater* On = M.O.On >= 0 ? FloatByKey(M.O.On) : nullptr;
			auto Let = [&]()
			{
				// (his place on his float given up)
				if (On && M.Slot >= 0)
				{
					if (M.bIn) On->InMask &= ~(1u << M.Slot);
					else On->HoldMask &= ~(1u << M.Slot);
				}
				M.O.On = -1;
				M.Slot = -1;
				On = nullptr;
			};
			auto Into = [&](Swim Doing)
			{
				if (M.O.Doing != Doing) M.O.Since = 0.0;
				M.O.Doing = Doing;
			};
			// Looking about him, every few seconds.
			if (ShownT >= M.Next && M.O.Doing != Swim::Jump && M.O.Doing != Swim::Climb && M.O.Doing != Swim::Dead)
			{
				M.Next = ShownT + 2.0 + 2.0 * Mix(static_cast<uint64_t>(M.O.Key) * 131 + static_cast<uint64_t>(Beat));
				const SlickOut& S = Oil[W.Slick - 1];
				const double FromOil = Flat(M.O.At - S.At);
				const double Fire = S.Burning > 0.0 ? 0.5 * S.Wide * std::sqrt(S.Burning) : 0.0;
				const Hull* By = W.Rescuer >= 0 && ShownT < W.RescueUntil ? FindHull(W.Rescuer) : nullptr;
				if (By && (By->Doing == Fate::Gone || By->Spd >= 3.0 || Flat(By->P - M.O.At) > 900.0)) By = nullptr;
				// (one holding on to a float, or gathered by it, keeps with his group until it is brought near her; then he swims
				// the rest of the way. In a raft or a boat, he stays in it until it lies at her side)
				const Floater* Group0 = M.O.Doing == Swim::Cling && On ? On : M.O.Doing == Swim::Tread && M.Near >= 0 ? FloatByKey(M.Near) : nullptr;
				// (most men gather by a float or a piece of her, in groups of three to a dozen, and tread water there; the rest are
				// stragglers between the groups. The art director, 22 November 18:54Z)
				const bool bGatherer = Mix(static_cast<uint64_t>(M.O.Key) * 43 + 13) < 0.67;
				auto GroupNear = [&]() -> Floater*
				{
					Floater* Found = nullptr;
					double FoundD = 120.0;
					for (const int Key : W.Floats)
					{
						Floater& Fl = Afloat[Key - 1];
						const int Most = 3 + static_cast<int>(Mix(static_cast<uint64_t>(Fl.O.Key) * 47 + 3) * 10.0);
						if (Fl.Group >= Most) continue;
						const double D = Flat(Fl.O.At - M.O.At);
						if (D < FoundD) { FoundD = D; Found = &Fl; }
					}
					return Found;
				};
				// (with a ship standing by but far off, he makes for a group near him first, and comes to her with it)
				const bool bHoldBack = By && !Group0 && !On && bGatherer && Flat(By->P - M.O.At) > 80.0 && GroupNear();
				// (the ship he was swimming for no longer standing by: he gives her up, and looks for something to hold on to)
				if (M.To >= 0 && (M.O.Doing == Swim::Swim || M.O.Doing == Swim::Tread) && (!By || By->Key != M.To || bHoldBack))
				{
					M.To = -1;
					M.Goal = M.O.At;
				}
				if (Fire > 0.0 && FromOil < Fire + 10.0 && M.O.Doing != Swim::Raft && M.O.Doing != Swim::Boat)
				{
					// (burning oil: out of it, away from its middle)
					Let();
					M.To = -1;
					M.Near = -1;
					M.Goal = M.O.At + (M.O.At - S.At).Unit(Ahead(Wrap(DownWind + Pi))) * (Fire - FromOil + 25.0);
					Into(Swim::Swim);
				}
				else if (By && M.To == By->Key && M.O.Doing == Swim::Tread && W.PickCap >= 0 && W.Picked >= W.PickCap)
				{
					// (at her side, waiting his turn up it: the battle has picked up no more of them yet)
				}
				else if (By && !bHoldBack && (Group0 ? Flat(Group0->O.At - By->P) < 0.5 * SizeOf(By->Class).Long + 60.0
					: (M.O.Doing != Swim::Raft && M.O.Doing != Swim::Boat) || (On && Flat(On->O.At - By->P) < 0.5 * SizeOf(By->Class).Long + 0.5 * SizeOf(By->Class).Beam + 8.0)))
				{
					// (a ship stopped to pick them up: to her side, the nearest place along it, and up it)
					const ClassSize& Sz = SizeOf(By->Class);
					const Vec Rel = M.O.At - By->P;
					const double Along = Clamp(Rel.Dot(Ahead(By->Hdg)), -0.35 * Sz.Long, 0.35 * Sz.Long);
					const double Side = Rel.Dot(RightOf(By->Hdg)) >= 0.0 ? 1.0 : -1.0;
					Let();
					M.Near = -1;
					if (M.To != By->Key) M.ToSince = ShownT;
					M.To = By->Key;
					M.Off = Vec(Along, Side * (0.5 * Sz.Beam + 1.0), 0.0);
					Into(Swim::Swim);
				}
				else if (M.O.Doing == Swim::Tread || (M.O.Doing == Swim::Swim && !On && M.To < 0 && Flat(M.Goal - M.O.At) < 1.0))
				{
					// (something to hold on to, or a place in a raft or boat, near him)
					Floater* Best = nullptr;
					double BestD = 150.0;
					bool bBestIn = false;
					for (const int Key : W.Floats)
					{
						Floater& Fl = Afloat[Key - 1];
						const double D = Flat(Fl.O.At - M.O.At);
						if (D > BestD + 30.0) continue;
						const bool bIn = FreeSlot(Fl.InMask, Fl.Room) >= 0;
						if (!bIn && FreeSlot(Fl.HoldMask, Fl.Hold) < 0) continue;
						const double Score = D - (bIn ? 30.0 : 0.0);
						if (Score < BestD)
						{
							BestD = Score;
							Best = &Fl;
							bBestIn = bIn;
						}
					}
					// (no place on one: a gatherer keeps by his group's float, or finds one with room near him)
					const Floater* By0 = M.Near >= 0 ? FloatByKey(M.Near) : nullptr;
					Floater* Group = nullptr;
					if (!Best && By0 && Flat(By0->O.At - M.O.At) < 60.0) Group = FloatByKey(M.Near);
					else if (!Best && bGatherer) Group = GroupNear();
					if (Group && Group->O.Key != M.Near)
					{
						Let();
						const uint64_t G = static_cast<uint64_t>(M.O.Key) * 53 + 17;
						const double Round = Mix(G) * 2.0 * Pi, Out = 0.5 * std::max(Group->O.Long, Group->O.Beam) + 1.2 + 2.6 * Mix(G + 1);
						M.Near = Group->O.Key;
						M.NearOff = Vec(std::cos(Round) * Out, std::sin(Round) * Out, 0.0);
						++Group->Group;
						Into(Swim::Swim);
					}
					else if (!Group) M.Near = -1;
					if (Best)
					{
						Let();
						M.Near = -1;
						M.Slot = bBestIn ? FreeSlot(Best->InMask, Best->Room) : FreeSlot(Best->HoldMask, Best->Hold);
						M.bIn = bBestIn;
						if (bBestIn) Best->InMask |= 1u << M.Slot;
						else Best->HoldMask |= 1u << M.Slot;
						M.O.On = Best->O.Key;
						On = Best;
						M.Off = SlotOn(Best->O, M.Slot, M.bIn, Best->Room, Best->Hold);
						Into(Swim::Swim);
					}
					else if (!Group) Into(Swim::Tread);
				}
				// (treading water with a ship in sight: an arm up now and then)
				M.O.bWave = W.Nearest < 1500.0 && Mix(static_cast<uint64_t>(M.O.Key) * 7 + 1) < 0.4;
			}
			switch (M.O.Doing)
			{
			case Swim::Jump:
				M.O.At += Vec(M.V.X, M.V.Y, 0.0) * Dt;
				M.V.Z -= Gee * Dt;
				M.O.At.Z += M.V.Z * Dt;
				if (M.O.At.Z <= 0.0)
				{
					// (in: swimming away from her, then looking about him)
					M.O.At.Z = 0.0;
					M.V = Vec();
					const Hull* Her = FindHull(W.Ship);
					const Vec From = Her && Her->Doing != Fate::Gone ? Her->P : W.OilEnd;
					const Vec Away = Vec(M.O.At.X - From.X, M.O.At.Y - From.Y, 0.0).Unit(Ahead(M.O.Heading));
					// (clear of her fast, for fear of being drawn down with her: farther from one that is left, and quicker)
					M.Goal = M.O.At + Away * ((W.bLeft ? 22.0 : 15.0) + (W.bLeft ? 18.0 : 30.0) * Mix(static_cast<uint64_t>(M.O.Key) * 17 + 5));
					if (W.bLeft) M.Pace = std::max(M.Pace, 0.75 + 0.3 * Mix(static_cast<uint64_t>(M.O.Key) * 41 + 7));
					M.Next = ShownT + 6.0 + 4.0 * Mix(static_cast<uint64_t>(M.O.Key) * 19 + 3);
					Into(Swim::Swim);
				}
				break;
			case Swim::Swim:
			{
				// (to his float's place, to a rescuer's side, or where he is making for)
				const Hull* By = M.To >= 0 ? FindHull(M.To) : nullptr;
				if (M.To >= 0 && (!By || By->Doing == Fate::Gone)) { M.To = -1; By = nullptr; M.Goal = M.O.At; }
				const Floater* Keep = !On && !By && M.Near >= 0 ? FloatByKey(M.Near) : nullptr;
				const Vec Goal = On ? OnFrame(On->O.At, On->O.Heading, M.Off) : By ? OnFrame(By->P, By->Hdg, M.Off) : Keep ? OnFrame(Keep->O.At, Keep->O.Heading, M.NearOff) : M.Goal;
				const Vec To(Goal.X - M.O.At.X, Goal.Y - M.O.At.Y, 0.0);
				const double D = Flat(To);
				if (D > 1e-6) M.O.Heading = Bearing(Vec(), To);
				M.O.At += To * (std::min(D, M.Pace * Dt) / std::max(D, 1e-6));
				if (D <= M.Pace * Dt + 0.05)
				{
					if (On) Into(On->O.Kind == "boat" && M.bIn ? Swim::Boat : M.bIn ? Swim::Raft : Swim::Cling);
					else if (By && W.PickCap >= 0 && W.Picked >= W.PickCap) Into(Swim::Tread); // (at her side, his turn up it to come)
					else if (By && ShownT < M.ToSince + NetDownFor)
					{
						// (at her side before a net could be down there, brought near her on a float: at its foot until it is)
						M.Next = std::min(M.Next, M.ToSince + NetDownFor);
						Into(Swim::Tread);
					}
					else if (By && [&]()
						{
							// (one man up a net at a time: another climbing there, he moves along her side to the next)
							for (const Man& O : Swimmers)
								if (&O != &M && O.O.Doing == Swim::Climb && O.To == By->Key && O.Off.Y * M.Off.Y > 0.0 && std::fabs(O.Off.X - M.Off.X) < 1.0) return true;
							return false;
						}())
					{
						const double Half = 0.35 * SizeOf(By->Class).Long, Step = (M.O.Key & 1) ? 1.2 : -1.2;
						M.Off.X = std::fabs(M.Off.X + Step) <= Half ? M.Off.X + Step : M.Off.X - Step;
					}
					else if (By)
					{
						// (up her side on a scrambling net or a line, her own height in some fifteen to twenty-five seconds)
						++W.Picked;
						if (W.PickCap >= 0 && W.Picked > W.PickCap) ++Check.PickedOver;
						const double Up = 15.0 + 10.0 * Mix(static_cast<uint64_t>(M.O.Key) * 23 + 9);
						M.Off = Vec(M.Off.X, M.Off.Y > 0.0 ? 0.5 * SizeOf(By->Class).Beam + 0.3 : -(0.5 * SizeOf(By->Class).Beam + 0.3), Up);
						M.Until = ShownT + Up;
						Into(Swim::Climb);
					}
					else Into(Swim::Tread);
				}
				break;
			}
			case Swim::Cling:
			case Swim::Raft:
			case Swim::Boat:
				if (!On) { Into(Swim::Tread); break; }
				M.O.At = OnFrame(On->O.At, On->O.Heading, M.Off);
				// (in a boat facing her stern; holding on to a boat GrabTurn off square to her side, toward her bow, his
				// inboard hand on her grab line and his head clear of her planking; to a raft, facing her middle)
				M.O.Heading = M.O.Doing == Swim::Boat ? Wrap(On->O.Heading + Pi)
					: M.O.Doing == Swim::Cling && On->O.Kind == "boat" ? Wrap(On->O.Heading - (M.Off.Y >= 0.0 ? 1.0 : -1.0) * (0.5 * Pi - GrabTurn))
					: Bearing(M.O.At, On->O.At);
				break;
			case Swim::Climb:
			{
				const Hull* By = FindHull(M.To);
				if (!By || By->Doing == Fate::Gone)
				{
					M.O.At.Z = 0.0;
					M.To = -1;
					M.Goal = M.O.At;
					Into(Swim::Tread);
					break;
				}
				M.O.At = OnFrame(By->P, By->Hdg, M.Off);
				M.O.At.Z = FreeOf(By->Class) * Clamp(1.0 - (M.Until - ShownT) / std::max(1.0, M.Off.Z), 0.0, 1.0);
				M.O.Heading = Bearing(Vec(), RightOf(By->Hdg) * (M.Off.Y > 0.0 ? -1.0 : 1.0));
				if (ShownT >= M.Until)
				{
					// (aboard her: picked up)
					M.O.Key = -M.O.Key;
					++Check.MenPicked;
					// (a man of the other side's: a prisoner, a struck boat's crew taken off by her hunter)
					if (By->Side != M.O.Side) ++Check.Prisoners;
				}
				break;
			}
			case Swim::Tread:
				// (in his group: kept by its float as it drifts, treading water; swimming back to it if left behind)
				if (const Floater* Keep = M.Near >= 0 ? FloatByKey(M.Near) : nullptr)
				{
					const Vec Place = OnFrame(Keep->O.At, Keep->O.Heading, M.NearOff);
					const Vec To(Place.X - M.O.At.X, Place.Y - M.O.At.Y, 0.0);
					const double D = Flat(To);
					if (D > 4.0) Into(Swim::Swim);
					else if (D > 0.05) M.O.At += To * (std::min(D, 0.25 * M.Pace * Dt) / D);
				}
				break;
			default:
				break;
			}
			// (in the water and not holding on: drifting with the wind's push on the sea, a sixtieth of its speed)
			if (M.O.Doing == Swim::Swim || M.O.Doing == Swim::Tread || M.O.Doing == Swim::Dead) M.O.At += Blow * (0.015 * Dt);
			if (M.bGoing)
			{
				M.O.At.Z = -2.5 * Clamp(1.0 - (M.Until - ShownT) / 10.0, 0.0, 1.0);
				if (ShownT >= M.Until && M.O.Key > 0) M.O.Key = -M.O.Key;
			}
			// (never inside a ship: pushed aside by her bow or her side, his own ship's too, falling as in the water; a man
			// holding on goes with his float, pushed already)
			if (M.O.Doing == Swim::Swim || M.O.Doing == Swim::Tread || M.O.Doing == Swim::Dead || M.O.Doing == Swim::Jump)
			{
				for (size_t K = 0; K < W.NearNow.size(); ++K)
				{
					// (a struck boat's men jumping from her casing: over her saddle tanks, not out of her, until they are nearly in)
					if (W.bStruck && M.O.Doing == Swim::Jump && W.NearNow[K]->Key == W.Ship && M.O.At.Z > 0.3) continue;
					if (PushOut(M.O.At, W.NearNow[K]->P, W.NearNow[K]->Hdg, W.NearHalf[K], W.NearR[K] + 0.8) && !M.bSwept)
					{
						M.bSwept = true;
						++Check.MenSwept;
					}
				}
			}
			if (On && M.Slot >= 0 && M.O.Doing != Swim::Swim)
			{
				if (M.bIn) ++On->O.Aboard;
				else ++On->O.Holding;
			}
			if (!M.bGoing && M.O.Key > 0) ++InWater;
		}
		Swimmers.erase(std::remove_if(Swimmers.begin(), Swimmers.end(), [](const Man& M) { return M.O.Key < 0; }), Swimmers.end());
		Check.MostMen = std::max(Check.MostMen, InWater);
		// (room for the men of the next ship to go down: past most of the room, the longest dead go under, one a step)
		if (InWater > F.MenMost * 0.85)
		{
			Man* Oldest = nullptr;
			for (Man& M : Swimmers)
				if (M.O.Doing == Swim::Dead && !M.bGoing && (!Oldest || M.O.Key < Oldest->O.Key)) Oldest = &M;
			if (Oldest)
			{
				Oldest->bGoing = true;
				Oldest->Until = ShownT + 10.0;
				++Check.MenUnder;
			}
		}
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
			Elapsed += Fixed;
			ShownT = ClockFrom + Elapsed;
			Interpolate();
			StartColumns();
			while (!Queue.empty() && Queue.front().T <= ShownT + 1e-9)
			{
				const Event E = Queue.front();
				Queue.pop_front();
				Play(E);
			}
			Untold(ShownT);
			Rechosen = 0;
			KeepClear(Fixed);
			Steam(Fixed);
			Gunnery(Fixed);
			LandShells();
			RunTorpedoes(Fixed);
			BurstCharges();
			FlyStrikes(Fixed);
			CoverFleet(Fixed);
			LayScreens();
			StepHands(Fixed);
			RunWrecks(Fixed);
			// (whom the player sees, once every step's finds are in: SetMine)
			Hunt();
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
						// (the bow against the way she made over the step, half a step's turn allowed for the chord; her stern,
						// going astern)
						const double Facing = H->Spd < 0.0 ? Wrap(H->Hdg + Pi) : H->Hdg;
						Check.MostSlip = std::max(Check.MostSlip, std::max(0.0, std::fabs(Wrap(Way - Facing)) - std::fabs(H->Rate) * Fixed * 0.5) / Deg);
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
			Untold(T);
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
			S.DiveAngle = H.DiveAngle;
			S.Heave = H.Heave;
			S.Speed = H.Spd;
			S.Wake = Clamp(H.Spd / (Sz.Speed * 1.3), 0.0, 1.0);
			S.Funnel = H.Doing == Fate::Sinking ? 0.0 : Clamp(0.25 + 0.75 * H.Spd / (Sz.Speed * 1.3), 0.0, 1.0);
			S.Long = Sz.Long;
			S.Beam = Sz.Beam;
			S.Doing = H.Doing;
			S.How = H.How;
			S.DownFor = H.SinkFor;
			// (a prize's sinking clock is held in steps, so she is drawn at the step's Down, not carried on to the frame)
			S.Down = H.Doing == Fate::Sinking ? Clamp(((H.bPrize ? ShownT : DrawT) - H.SinkAt) / std::max(1.0, H.SinkFor), 0.0, 1.0) : 0.0;
			S.Train = H.Train;
			S.Elevation = H.Elev;
			S.Aim = H.AimKey;
			S.bFiring = H.bFiring;
			S.bAftGun = H.bAftGun;
			S.bGunLaid = H.bGunLaid && H.Doing == Fate::Line;
			S.ShakenAt = H.ShakenAt;
			S.FiredAt = H.FiredAt;
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
				// (struck: her flag from her crew's first coming out, until it is hauled down; a prize held as taken)
				S.Struck = H.bStruck && H.UpAt >= 0.0 ? DrawT - H.UpAt : -1.0;
				S.Lowered = H.FlagDownAt >= 0.0 ? DrawT - H.FlagDownAt : -1.0;
				S.bPrize = H.bPrize;
				// (under, she leaves no wake on the sea but her periscope's feather, the scene's own, and her diesels are stopped)
				if (S.Dived > 1.0)
				{
					S.Wake = 0.0;
					S.Funnel = 0.0;
				}
			}
			// An enemy boat the player has not found (SetMine): not drawn, and nothing of her on the sea (her wake and her
			// diesels' smoke, which the scene draws from these; the scene hides her hull, AirStage::SetShown, and her looks,
			// ConvoyLooks).
			S.bShown = InSight(H);
			if (!S.bShown)
			{
				S.Wake = 0.0;
				S.Funnel = 0.0;
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
		if (bWrecksMoved)
		{
			// (laid out once a step: a man swims under a metre a second, and the steps are a twentieth of a second apart)
			bWrecksMoved = false;
			OutFloats.clear();
			for (const Floater& Fl : Afloat) OutFloats.push_back(Fl.O);
			OutSlicks = Oil;
			OutMen.clear();
			for (const Man& M : Swimmers) OutMen.push_back(M.O);
		}
		// A struck boat's crew on her casing and her boarders, laid out every frame at the drawn moment as she is (they move
		// with her as she rides); her captor's whaler and its oarsmen.
		OutHands.clear();
		OutWhalers.clear();
		for (const DeckHand& D : OnCasing)
		{
			if (D.O.Key == 0 || !D.bOut || D.bBelow) continue;
			const Hull* H = FindHull(Wrecked[static_cast<size_t>(D.W)].Ship);
			if (!H || H->Doing == Fate::Gone || !InSight(*H)) continue;
			ManOut O = D.O;
			O.At = HandAt(D, *H, On);
			O.Since += On;
			const double Hdg = H->Hdg + H->Rate * On;
			// (facing the way he walks; out over her side running, standing or with his hands up)
			O.Heading = D.O.Doing == Swim::Walk ? (std::fabs(D.PlaceAcross - D.Across) > 1e-6 ? Wrap(Hdg + (D.PlaceAcross > D.Across ? 0.5 : -0.5) * Pi)
			                                        : D.PlaceAlong >= D.Along ? Hdg : Wrap(Hdg + Pi))
			            : D.O.Doing == Swim::Hatch ? Wrap(Hdg + Pi)
			                                       : Wrap(Hdg + (D.Side > 0.0 ? 0.5 : -0.5) * Pi);
			OutHands.push_back(O);
		}
		for (const Boarding& B : Boardings)
		{
			if (B.LoweredAt < 0.0 || B.bStowed) continue;
			// (carried on from the step by its pull, as the ships are, so it never stands and jumps between steps)
			FloatOut Fl = B.Whaler;
			Fl.At += B.Pull * On;
			OutWhalers.push_back(Fl);
			for (ManOut O : B.Oars)
			{
				O.At += B.Pull * On;
				OutHands.push_back(O);
			}
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
			O.Piece = T.Piece;
			O.Ship = T.Ship;
			O.End = T.End;
			O.bRunOn = ShownT >= std::max(T.AimAt, T.Arrive);
			O.bFromPoint = T.bFromPoint;
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
			std::snprintf(Line, sizeof(Line), "%5d %s %s p%d.%d %s at %.0f,%.0f hdg %.0f %.1f m/s heel %.1f train %.0f%s%s%s%s\n", S.Key, S.Side ? "def" : "att", S.Class.c_str(), S.Piece, S.Slot, Doing,
				S.At.X, S.At.Y, S.Heading, S.Speed, S.Heel, S.Train, S.bFiring ? " FIRE" : "", S.bScreen ? " smoke" : "", S.Doing == Fate::Sinking ? (" " + S.How).c_str() : "",
				S.bShown ? "" : " unseen");
			Out += Line;
		}
		for (const Wreck& W : Wrecked)
		{
			std::snprintf(Line, sizeof(Line), "  wreck %d %s %s: %d men in the water of %d (%d dead), %d rafts, %d boats, %d pieces\n", W.Ship, W.Class.c_str(), W.How.c_str(), W.Out, W.Real,
				W.Dead, W.RaftsOut, W.BoatsOut, W.BitsOut);
			Out += Line;
		}
		std::snprintf(Line, sizeof(Line), "  in the water at the end: %zu men, %zu floats\n", Swimmers.size(), Afloat.size());
		Out += Line;
		for (const PlaneOut& P : OutPlanes)
		{
			std::snprintf(Line, sizeof(Line), "  air %d %s %s at %.0f,%.0f,%.0f hdg %.0f%s\n", P.Key, P.Model.c_str(), P.Role.c_str(), P.At.X, P.At.Y, P.At.Z, P.Heading, P.bFalling ? " falling" : "");
			Out += Line;
		}
		return Out;
	}

	void Show::ChargesDown(std::vector<ChargeDown>& Out) const
	{
		// U1 hook (under water): the charges sinking, for the view under the water; read only, the blow left out.
		Out.clear();
		Out.reserve(Charges.size());
		for (const Charge& C : Charges)
		{
			ChargeDown D;
			D.Escort = C.Escort;
			D.Boat = C.Boat;
			D.At = C.At;
			D.Off = C.Off;
			D.Bursts = C.Bursts;
			D.Dropped = C.Dropped;
			D.Kind = C.Kind;
			D.bOnBoat = C.bOnBoat;
			Out.push_back(D);
		}
	}
}
