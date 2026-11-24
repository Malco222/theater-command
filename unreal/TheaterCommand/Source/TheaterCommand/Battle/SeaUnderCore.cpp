#include "Battle/SeaUnderCore.h"

#include <algorithm>
#include <cmath>

namespace SeaUnder
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		double Clamp(double X, double A, double B) { return X < A ? A : (X > B ? B : X); }
		double Lerp(double A, double B, double S) { return A + (B - A) * S; }
		P3 Lerp(const P3& A, const P3& B, double S) { return A + (B - A) * S; }
		double Dot(const P3& A, const P3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		double Length(const P3& V) { return std::sqrt(Dot(V, V)); }
		double Level(const P3& V) { return std::hypot(V.X, V.Y); }
		P3 Up(double Z) { return P3(0.0, 0.0, Z); }
		/** Two keys made one, for the look's hash. */
		uint64_t Mix(uint64_t A, uint64_t B) { return (A * 0x9E3779B97F4A7C15ull) ^ (B + 0x632BE59BD9B4E019ull + (A << 6) + (A >> 2)); }
		/** A battle time as whole milliseconds, for a key. */
		uint64_t Ms(double T) { return static_cast<uint64_t>(std::llround(std::max(0.0, T) * 1000.0)); }
		/** V wrapped into the box Size wide about C (the silt and the shafts: always as many about the eye, none popping). */
		double Wrap(double V, double C, double Size)
		{
			double R = std::fmod(V - C + Size * 0.5, Size);
			if (R < 0.0) R += Size;
			return C - Size * 0.5 + R;
		}
		/** How a candidate's kind ranks when two are too close: known as it opens, never what follows. */
		int RankOf(Kind K) { return K == Kind::Pattern ? 0 : K == Kind::Ahead ? 1 : K == Kind::Rise ? 2 : 3; }
		/** The ripples on the face, as shares of RippleWave: three crossing, none shorter than it. */
		constexpr double RippleShares[3] = { 1.0, 1.31, 1.73 };
	}

	// -------------------------------------------------------------------------------------------------------------
	// The asdic on her hull (who is found is the show's: ShipOut::bShown)

	void Sight::Begin()
	{
		Pings.clear();
		Open.clear();
		Echoes.clear();
		Sunk.clear();
	}

	void Sight::Close(std::map<Hunt, double>& Holding, std::map<int, std::vector<Span>>& Into, const Hunt& Which, double T)
	{
		const auto It = Holding.find(Which);
		if (It == Holding.end()) return;
		Into[Which.second].push_back(Span{ It->second, T, Which.first });
		Holding.erase(It);
	}

	void Sight::Told(const std::vector<SeaShow::Event>& Events)
	{
		for (const SeaShow::Event& E : Events)
		{
			if (E.What == "sea:sonar" && E.On >= 0 && E.Unit >= 0 && E.Kind != "bearing")
			{
				const Hunt Which(E.Unit, E.On);
				// (a contact lost, or gained by radar or sight: she is up, or he has her by other means; a contact with no kind
				// is the asdic's, as the show pings it: SeaShow's sea:sonar)
				const bool bAsdic = E.Kind.empty() || E.Kind == "asdic";
				if (!E.Lost.empty() || !bAsdic) Close(Open, Pings, Which, E.T);
				else if (!Open.count(Which) && !Sunk.count(E.Unit) && !Sunk.count(E.On)) Open[Which] = E.T;
			}
			else if (E.What == "sea:sunk" && E.Unit >= 0)
			{
				// Her end, or her hunter's: every asdic on her or his over at once.
				Sunk.insert(E.Unit);
				for (std::map<Hunt, double>* Holding : { &Open, &Echoes })
				{
					std::vector<Hunt> Over;
					for (const auto& Held : *Holding)
						if (Held.first.first == E.Unit || Held.first.second == E.Unit) Over.push_back(Held.first);
					for (const Hunt& Which : Over) Close(*Holding, Pings, Which, E.T);
				}
			}
		}
	}

	void Sight::Echoed(double T, const std::vector<SeaShow::Piece>& Pieces)
	{
		// (each escort's echo as this view says it: on a boat by asdic, from the view it is first in to the view it is gone
		// from; none once she or he is sunk)
		std::set<Hunt> Now;
		for (const SeaShow::Piece& P : Pieces)
			if (P.EchoOn >= 0 && P.EchoKind == "asdic" && !Sunk.count(P.Id) && !Sunk.count(P.EchoOn)) Now.insert(Hunt(P.Id, P.EchoOn));
		std::vector<Hunt> Gone;
		for (const auto& Held : Echoes)
			if (!Now.count(Held.first)) Gone.push_back(Held.first);
		for (const Hunt& Which : Gone) Close(Echoes, Pings, Which, T);
		for (const Hunt& Which : Now)
			if (!Echoes.count(Which)) Echoes[Which] = T;
	}

	bool Sight::Pinged(int BoatPiece, double T, int& OutEscort) const
	{
		if (const auto It = Pings.find(BoatPiece); It != Pings.end())
			for (const Span& S : It->second)
				if (S.From <= T && T < S.Until) { OutEscort = S.By; return true; }
		for (const std::map<Hunt, double>* Holding : { &Open, &Echoes })
			for (const auto& Held : *Holding)
				if (Held.first.second == BoatPiece && Held.second <= T) { OutEscort = Held.first.first; return true; }
		return false;
	}

	// -------------------------------------------------------------------------------------------------------------
	// Hulls

	double DraughtOf(const std::string& Class, double Long)
	{
		// [P] the ship models' keels: Destroyer to z -3.6, Freighter to -7.4; [EST] any other by her length
		if (Class == "dd" || Class == "de") return 3.6;
		if (Class == "mv") return 7.4;
		return Clamp(0.045 * Long, 3.0, 10.0);
	}

	Hull HullOf(const SeaShow::ShipOut& Ship)
	{
		Hull H;
		H.Key = Ship.Key;
		H.Heading = Ship.Heading;
		H.Long = Ship.Long;
		H.Beam = Ship.Beam;
		if (Ship.Class == "ss")
		{
			const double Keel = std::max(0.0, Ship.Keel);
			H.bBoat = true;
			H.At = P3(Ship.At.X, Ship.At.Y, -MiddleOf(Keel));
			H.Bottom = -Keel;
			H.Top = -Keel + BoatDraught + 10.49; // [P] her tower's top over her waterline, the model's
		}
		else
		{
			H.At = P3(Ship.At.X, Ship.At.Y, 0.0);
			H.Bottom = -DraughtOf(Ship.Class, Ship.Long);
			H.Top = 30.0;
		}
		return H;
	}

	bool InsideHull(const P3& At, const Hull& H, double Margin)
	{
		const P3 Ahead = ConvoyLook::AheadOf(H.Heading), Right = ConvoyLook::RightOf(H.Heading);
		const P3 Rel = At - H.At;
		const double Along = Rel.X * Ahead.X + Rel.Y * Ahead.Y;
		if (H.bBoat)
		{
			const double Kept = Clamp(Along, -0.5 * H.Long, 0.5 * H.Long);
			return Length(At - (H.At + Ahead * Kept)) < 0.5 * H.Beam + Margin;
		}
		const double Across = Rel.X * Right.X + Rel.Y * Right.Y;
		return std::fabs(Along) < 0.5 * H.Long + Margin && std::fabs(Across) < 0.5 * H.Beam + Margin && At.Z > H.Bottom - Margin && At.Z < H.Top;
	}

	// -------------------------------------------------------------------------------------------------------------
	// The director

	double Director::PaceOf(const Beat& B)
	{
		// (ASeaBattle::RunMonthScene: Clamp((To - From) / Screen, 0.25, FastestWindow); a beat with no screen time, fastest)
		return B.Screen > 0.0 ? Clamp((B.To - B.From) / B.Screen, SlowestBeat, FastestBeat) : FastestBeat;
	}

	void Director::Begin(const SeaShow::Field& InBoard, double InClearBy)
	{
		Board = InBoard;
		ClearBy = InClearBy;
		Pings.Begin();
		Attacks.clear();
		Candidates.clear();
		Bursts.clear();
		Ends.clear();
		Moments.clear();
	}

	void Director::Told(double T, const std::vector<SeaShow::Piece>& Pieces, const std::vector<SeaShow::Event>& Events)
	{
		// (the events first: a boat or her hunter sunk is never pinged again by an echo the view still carries)
		Pings.Told(Events);
		Pings.Echoed(T, Pieces);
		for (const SeaShow::Event& E : Events)
			if (E.What == "sea:depthCharge" && E.On >= 0 && E.Unit >= 0) Attacks.push_back(Attack{ E.Unit, E.On, E.T });
	}

	void Director::Opened(Window& W, const SeaShow::ShipOut& Boat, double T) const
	{
		W.Boat = Boat.Key;
		W.BoatPiece = Boat.Piece;
		W.Side = Boat.Side;
		const BoatPose Pose = PoseOf(Boat);
		W.HerAt = Pose.At;
		W.Heading = Pose.Heading;
		W.Keel = Pose.Keel;
		W.Long = Pose.Long;
		W.Beam = Pose.Beam;
		// (the eye on the far side of her from the pattern's middle, looking across her at the charges coming down round
		// her: the back-up shot's side, and under her where its charges near her burst all round her (EyeOf: else the way
		// to them); a dive's or a rise's side, or a pattern run right over her, from the look's hash)
		const double Across = W.Escort >= 0 ? Dot(W.Aim - W.HerAt, ConvoyLook::RightOf(W.Heading)) : 0.0;
		W.Hand = Across > 1.0 ? -1 : Across < -1.0 ? 1 : (ConvoyLook::Hash01(Mix(static_cast<uint64_t>(Boat.Key), Ms(T))) < 0.5 ? -1 : 1);
	}

	void Director::Step(double T, const SeaShow::Show& Trial)
	{
		for (const SeaShow::Fx& E : Trial.Effects)
		{
			if (E.Kind == SeaShow::FxKind::ChargeDrop)
			{
				const SeaShow::ShipOut* Escort = Trial.Ship(E.Key);
				if (!Escort) continue;
				// (the boat it went in on: the recording's own sea:depthCharge from her piece, the latest within 2 s)
				int BoatPiece = -1;
				for (const Attack& A : Attacks)
					if (A.Escort == Escort->Piece && std::fabs(A.T - T) <= 2.0) BoatPiece = A.Boat;
				const SeaShow::ShipOut* Boat = nullptr;
				for (const SeaShow::ShipOut& Ship : Trial.Ships())
					if (Ship.Piece == BoatPiece && Ship.Class == "ss") { Boat = &Ship; break; }
				if (!Boat) continue;
				Window W;
				W.What = E.What == "ahead" ? Kind::Ahead : Kind::Pattern;
				W.Name = W.What == Kind::Ahead ? "under-ahead" : "under-pattern";
				W.Escort = E.Key;
				W.Drop = T;
				W.Sinking = E.Sinking;
				W.Spread = E.Spread;
				W.Run = E.Until;
				W.N = static_cast<int>(std::lround(E.Size));
				W.Aim = P3(E.Aim.X, E.Aim.Y, 0.0);
				const P3 Way(E.Vel.X, E.Vel.Y, 0.0);
				W.Track = Length(Way) > 0.1 ? Way * (1.0 / Length(Way)) : ConvoyLook::AheadOf(Escort->Heading);
				Opened(W, *Boat, T);
				Candidates.push_back(W);
			}
			else if (E.Kind == SeaShow::FxKind::ChargeBurst)
			{
				Bursts.push_back(Burst{ E.Key, T, -E.At.Z, E.At.X, E.At.Y, E.Size > 0.5 });
			}
			else if (E.Kind == SeaShow::FxKind::Dive)
			{
				// (a crash dive deep, from 3 s on as she goes; a boat forced up from deep, from 1 s on)
				const SeaShow::ShipOut* Boat = Trial.Ship(E.Key);
				if (!Boat) continue;
				const bool bDeep = E.What == "dive" && Boat->Dive == "deep";
				const bool bUp = E.What == "surface" && Boat->Keel >= Board.ScopeKeel + 10.0;
				if (!bDeep && !bUp) continue;
				Window W;
				W.What = bDeep ? Kind::Dive : Kind::Rise;
				W.Name = bDeep ? "under-dive" : "under-rise";
				Opened(W, *Boat, T);
				W.From = T + (bDeep ? 3.0 : 1.0);
				W.Until = T + (bDeep ? 16.0 : 15.0);
				Candidates.push_back(W);
			}
			else if (E.Kind == SeaShow::FxKind::Sink)
			{
				if (const SeaShow::ShipOut* Boat = Trial.Ship(E.Key); Boat && Boat->Class == "ss") Ends.emplace(E.Key, T);
			}
		}
		Moment Now;
		Now.T = T;
		BoatsOf(Trial, Now.Boats);
		Moments.push_back(std::move(Now));
	}

	bool Director::Fits(Window& W, const std::vector<double>& Busy, const std::vector<Beat>& Beats) const
	{
		// A staged scene: inside the one beat it starts in or first overlaps (the scene cuts between beats), at that beat's
		// pace; the surface hold too.
		if (!Beats.empty())
		{
			const Beat* In = nullptr;
			for (const Beat& B : Beats)
				if (B.To > W.From + 1e-9 && B.From < W.Until - 1e-9) { In = &B; break; }
			if (!In) return false;
			W.From = std::max(W.From, In->From);
			W.Until = std::min(W.Until, In->To);
			W.HoldUntil = std::min(W.HoldUntil, In->To);
			W.Pace = PaceOf(*In);
		}
		// The other shots' cuts: one just before it (or just in) starts it ClearCut s after; one later in it ends it there, the
		// eye coming up in that very cut (one cut, never two close together); its surface hold ends at the next, or before the
		// next by ClearCut where that next is ClearCut s or more after it comes up.
		std::vector<double> Cuts = Busy;
		std::sort(Cuts.begin(), Cuts.end());
		for (const double Cut : Cuts)
		{
			if (Cut <= W.From - ClearCut || Cut >= W.Until) continue;
			if (Cut < W.From + ClearCut) W.From = Cut + ClearCut;
			else W.Until = std::min(W.Until, Cut);
		}
		for (const double Cut : Cuts)
			if (Cut >= W.Until - 1e-9) { W.HoldUntil = std::min(W.HoldUntil, Cut); break; }
		W.HoldUntil = std::max(W.HoldUntil, W.Until);
		return LongEnough(W);
	}

	bool Director::LongEnough(const Window& W)
	{
		// Enough left: ShortestUnder s, ScreenLeast s of screen; a pattern seen sinking before its first burst, and that burst
		// with its globe (a second).
		if (W.Until - W.From < ShortestUnder || (W.Until - W.From) / std::max(0.01, W.Pace) < ScreenLeast) return false;
		if (W.What == Kind::Pattern && (W.FirstBurst < W.From + 4.0 || W.Until < W.FirstBurst + 1.0)) return false;
		return true;
	}

	bool Director::Clear(Window& W) const
	{
		// Every step she is under for, as the trial (the scene's own show, SetMine and all) has her: drawn by the show (his
		// own, or found: ShipOut::bShown, the very rule the surface draws by) to her end where it is in it, her crew below
		// (not struck and up, nor at her gun: CrewUp), and no boat the show hides nor one at her gun near her (ClearOfUnheld:
		// the player's own dive asks the very same). All of it so as it opens, or it is not taken; where it stops being so
		// (found no longer, her crew coming up on deck, such a boat coming near) it ends at the step before, the eye coming up
		// as it does, and what is left must still be worth going under for. The trial's steps are coarser than the scene's
		// frames, so the step before it opens and the step after it ends are asked too: what holds at both ends of a step
		// holds between them (the show finds her, or loses her, at one moment). Not so at the step before: it opens at the
		// first step that is.
		auto Each = std::lower_bound(Moments.begin(), Moments.end(), W.From - 1e-9, [](const Moment& M, double When) { return M.T < When; });
		if (Each != Moments.begin() && (Each == Moments.end() || Each->T > W.From + 1e-9)) --Each;
		double Kept = -1.0;
		bool bLate = false;
		for (; Each != Moments.end(); ++Each)
		{
			const Moment& M = *Each;
			const bool bBefore = M.T < W.From - 1e-9, bAfter = M.T > W.Until + 1e-9;
			const BoatAt* Her = nullptr;
			for (const BoatAt& B : M.Boats)
				if (B.Key == W.Boat) Her = &B;
			const bool bEnded = W.End >= 0.0 && M.T >= W.End;
			if ((!Her || !Her->bAfloat) && bEnded)
			{
				if (bAfter) break;
				continue;
			}
			if (Her && Her->bAfloat && Her->bShown && !Her->bStruck && !Her->bGun && ClearOfUnheld(M.Boats, W.Boat, ClearBy))
			{
				if (bAfter) break;
				if (bBefore) continue;
				if (bLate && Kept < 0.0) W.From = M.T;
				Kept = M.T;
				continue;
			}
			if (bBefore)
			{
				bLate = true;
				continue;
			}
			if (Kept < 0.0) return false;
			W.Until = Kept;
			if (W.End > W.Until) W.End = -1.0;
			W.Until = std::min(W.Until, W.From + (W.End >= 0.0 ? LongestWithEnd : Longest));
			W.HoldUntil = std::max(W.Until, std::min(W.HoldUntil, W.Until + SurfaceHold));
			return LongEnough(W);
		}
		if (Kept < 0.0) return false;
		return !bLate || LongEnough(W);
	}

	double Director::FromHer(int BoatKey, double T, double X, double Y, P3* OutOff) const
	{
		// (the step at T, or the first after it)
		const auto At = std::lower_bound(Moments.begin(), Moments.end(), T - 1e-9, [](const Moment& M, double When) { return M.T < When; });
		if (At == Moments.end()) return -1.0;
		for (const BoatAt& B : At->Boats)
		{
			if (B.Key != BoatKey) continue;
			if (OutOff) *OutOff = P3(X - B.X, Y - B.Y, 0.0);
			return std::hypot(B.X - X, B.Y - Y);
		}
		return -1.0;
	}

	// -------------------------------------------------------------------------------------------------------------
	// What may be near her, and the player's own dive

	bool CrewUp(const SeaShow::ShipOut& Ship, double T)
	{
		if (Ship.Class != "ss") return false;
		return Ship.Struck >= 0.0 || Ship.bGunLaid || (Ship.FiredAt >= 0.0 && T - Ship.FiredAt <= GunQuiet);
	}

	void BoatsOf(const SeaShow::Show& Show, std::vector<BoatAt>& Out)
	{
		Out.clear();
		const double T = Show.Shown();
		for (const SeaShow::ShipOut& Ship : Show.Ships())
		{
			if (Ship.Class != "ss") continue;
			const bool bUnderWay = Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping;
			const bool bStruck = Ship.Struck >= 0.0;
			const bool bGun = CrewUp(Ship, T) && !bStruck;
			Out.push_back(BoatAt{ Ship.Key, Ship.Piece, Ship.Side, Ship.At.X, Ship.At.Y, Ship.bShown, Ship.Doing != SeaShow::Fate::Gone, bUnderWay, bStruck, bGun });
		}
	}

	double UnheldClearBy(double DaySee, double NightSee)
	{
		// (2.5 sights: LightLeft gives 0.05^2.5, under a thousandth of her light; the eye's own stand-off from her middle added)
		return std::max(200.0, 2.5 * std::max(DaySee, NightSee) + 20.0);
	}

	bool ClearOfUnheld(const std::vector<BoatAt>& Boats, int BoatKey, double ClearBy)
	{
		const BoatAt* Her = nullptr;
		for (const BoatAt& B : Boats)
			if (B.Key == BoatKey) Her = &B;
		if (!Her) return true;
		for (const BoatAt& B : Boats)
		{
			if (B.Key == BoatKey || !B.bAfloat || (B.bShown && !B.bGun)) continue;
			if (std::hypot(B.X - Her->X, B.Y - Her->Y) <= ClearBy) return false;
		}
		return true;
	}

	int HandBoatFor(const std::vector<BoatAt>& Boats, const P3& Eye, double ClearBy)
	{
		// (the nearest to the eye of those he may see, in the show's own order where two are as near)
		int Best = -1;
		double BestFar = HandReach;
		for (const BoatAt& B : Boats)
		{
			if (!B.bUnderWay || !B.bShown || B.bStruck || B.bGun || !ClearOfUnheld(Boats, B.Key, ClearBy)) continue;
			const double Far = std::hypot(B.X - Eye.X, B.Y - Eye.Y);
			if (Far < BestFar)
			{
				BestFar = Far;
				Best = B.Key;
			}
		}
		return Best;
	}

	bool HandHolds(const std::vector<BoatAt>& Boats, int BoatKey, double ClearBy)
	{
		for (const BoatAt& B : Boats)
			if (B.Key == BoatKey) return B.bAfloat && B.bShown && !B.bStruck && !B.bGun && ClearOfUnheld(Boats, BoatKey, ClearBy);
		return false;
	}

	int HandOf(int BoatKey) { return ConvoyLook::Hash01(static_cast<uint64_t>(BoatKey)) < 0.5 ? -1 : 1; }

	std::vector<Window> Director::Finish(const std::vector<double>& Busy, const std::vector<Beat>& Beats, double BeginT, double EndT) const
	{
		// Each pattern's bursts: its escort's, from the drop to its sinking and going-in time and 3 s more (an ahead
		// salvo's bombs burst only on her, when the battle tells it: up to half a minute on).
		std::vector<Window> Pool;
		for (Window W : Candidates)
		{
			if (W.What == Kind::Pattern || W.What == Kind::Ahead)
			{
				const double Latest = W.Drop + W.Sinking + W.Run + (W.What == Kind::Ahead ? 30.0 : 3.0);
				double First = 1e18, Last = -1.0, Deep = 0.0;
				int Count = 0;
				for (const Burst& B : Bursts)
				{
					if (B.Escort != W.Escort || B.T < W.Drop || B.T > Latest) continue;
					First = std::min(First, B.T);
					Last = std::max(Last, B.T);
					Deep += B.Depth;
					++Count;
				}
				if (W.What == Kind::Pattern && Count == 0) continue;
				// (a pattern laid off her, none of its bursts within PatternSeen m of her: nothing of it seen from beside her. The
				// booked blow is not one: the show bursts it at her keel wherever the pattern fell, SeaShow's BurstCharges)
				// Those that come near her are the ones the eye under her looks toward (EyeOf).
				if (W.What == Kind::Pattern)
				{
					double Nearest = 1e18;
					P3 NearSum;
					int NearCount = 0;
					for (const Burst& B : Bursts)
					{
						if (B.Escort != W.Escort || B.bBooked || B.T < W.Drop || B.T > Latest) continue;
						P3 FromHerAt;
						const double Far = FromHer(W.Boat, B.T, B.X, B.Y, &FromHerAt);
						if (Far < 0.0) continue;
						Nearest = std::min(Nearest, Far);
						if (Far <= PatternSeen)
						{
							NearSum = NearSum + FromHerAt;
							++NearCount;
						}
					}
					if (Nearest > PatternSeen) continue;
					W.Near = NearSum * (1.0 / NearCount);
				}
				W.FirstBurst = Count ? First : -1.0;
				W.LastBurst = Count ? Last : -1.0;
				W.Set = Count ? Deep / Count : -1.0;
				if (W.What == Kind::Pattern)
				{
					W.From = W.Drop + 0.2;
					W.Until = W.LastBurst + 5.0;
				}
				else
				{
					// (from the throw, the eye under her waiting as the bombs fly (ConvoyLook::DropPlan's flight), till they could
					// reach her and 4 s more, or her blow and 5 s more)
					W.From = W.Drop + 0.2;
					W.Until = std::max({ W.From + 8.0, W.Drop + std::min(5.0, 0.35 * W.Sinking) + W.Keel / AheadSink + 4.0, Count ? W.LastBurst + 5.0 : 0.0 });
				}
			}
			Pool.push_back(W);
		}
		std::stable_sort(Pool.begin(), Pool.end(), [](const Window& L, const Window& R) { return L.From < R.From; });
		// One boat's candidates close together run on as one: a rise, or a second pattern, within RunsOn s of a pattern's
		// end joins it (the eye stays with her); a dive or a rise on its own stands alone.
		std::vector<Window> Runs;
		for (const Window& W : Pool)
		{
			Window* Into = nullptr;
			for (Window& R : Runs)
				if (R.Boat == W.Boat && (R.What == Kind::Pattern || R.What == Kind::Ahead) && W.From <= R.Until + RunsOn) Into = &R;
			if (Into) Into->Until = std::max(Into->Until, W.Until);
			else Runs.push_back(W);
		}
		// Her end in it, or within RunsOn s after it, seen through: to 8 s past it. Then each at most Longest s.
		for (Window& R : Runs)
		{
			if (const auto It = Ends.find(R.Boat); It != Ends.end() && It->second >= R.From && It->second <= R.Until + RunsOn)
			{
				R.End = It->second;
				R.Until = std::max(R.Until, R.End + 8.0);
			}
			R.Until = std::min(R.Until, R.From + (R.End >= 0.0 ? LongestWithEnd : Longest));
		}
		// Taken by kind (a pattern, an ahead salvo, a rise, a dive), then by time: never by what follows.
		std::vector<const Window*> Order;
		for (const Window& R : Runs) Order.push_back(&R);
		std::stable_sort(Order.begin(), Order.end(), [](const Window* L, const Window* R) { return RankOf(L->What) < RankOf(R->What); });
		const int Most = 1 + static_cast<int>(std::max(0.0, EndT - BeginT) / EveryMore);
		std::vector<Window> Taken;
		for (const Window* R : Order)
		{
			if (static_cast<int>(Taken.size()) >= Most) break;
			Window W = *R;
			if (W.From < BeginT + 2.0) continue;
			W.Until = std::min(W.Until, EndT);
			W.HoldUntil = W.Until + SurfaceHold;
			if (!Fits(W, Busy, Beats) || !Clear(W)) continue;
			bool bNear = false;
			for (const Window& Had : Taken) bNear = bNear || (W.From < Had.Until + Apart && Had.From < W.Until + Apart);
			if (!bNear) Taken.push_back(W);
		}
		std::stable_sort(Taken.begin(), Taken.end(), [](const Window& L, const Window& R) { return L.From < R.From; });
		return Taken;
	}

	// -------------------------------------------------------------------------------------------------------------
	// The eye

	double MiddleOf(double Keel) { return std::max(1.5, Keel - 0.5 * (BoatDraught + BoatCasing)); }

	BoatPose PoseOf(const SeaShow::ShipOut& Ship)
	{
		BoatPose Pose;
		Pose.Key = Ship.Key;
		Pose.bThere = Ship.Doing != SeaShow::Fate::Gone && Ship.bShown;
		Pose.Heading = Ship.Heading;
		Pose.Long = Ship.Long;
		Pose.Beam = Ship.Beam;
		Pose.Keel = std::max(0.0, Ship.Keel);
		Pose.At = P3(Ship.At.X, Ship.At.Y, -MiddleOf(Pose.Keel));
		return Pose;
	}

	bool EyeOf(const Window& W, const BoatPose& Boat, const std::vector<Hull>& Others, double T, EyePose& Out)
	{
		if (T < W.From || T > std::max(W.Until, W.HoldUntil)) return false;
		if (T > W.Until || !Boat.bThere)
		{
			// Back up: 14 m up on the escort's quarter, 380 m off the pattern's middle, as the sea over it heaves white
			// (ConvoyLooks' domes and columns); a dive's or a rise's from astern of her, on her side.
			const P3 Track = W.Escort >= 0 ? W.Track : ConvoyLook::AheadOf(W.Heading);
			const P3 Across = ConvoyLook::RightOf(ConvoyLook::HeadingOf(Track, W.Heading));
			const P3 Over = W.Escort >= 0 ? W.Aim : (Boat.bThere ? P3(Boat.At.X, Boat.At.Y, 0.0) : P3(W.HerAt.X, W.HerAt.Y, 0.0));
			Out.Eye = Over - Track * 270.0 + Across * (W.Hand * 270.0) + Up(14.0);
			Out.Look = Over + Up(6.0);
			Out.Lens = 38.0;
			Out.bUnder = false;
			return true;
		}
		// Under: in her frame as she is now (she turns and goes deeper while the eye is with her), beam on and near enough
		// that the murk leaves her shape.
		const double Mid = MiddleOf(Boat.Keel);
		const P3 Ahead = ConvoyLook::AheadOf(Boat.Heading), Right = ConvoyLook::RightOf(Boat.Heading);
		const P3 Middle(Boat.At.X, Boat.At.Y, -Mid);
		const double Off = 12.0 + Boat.Beam * 0.5;
		switch (W.What)
		{
		case Kind::Pattern:
		{
			// The war film's shot: under her, looking up past her belly, dark against the light, at the charges coming down
			// round her where they will burst near her (Near: those of the pattern the eye beside her can see, on both sides
			// of her where they straddle her; the way to them held for the window, as they fall where they were thrown); in
			// the last 8 s before the first burst the eye eases out to her side, on the far side from them, and the look comes
			// down to her depth, where they burst. (Where they burst all round her, toward the pattern's middle, from her Hand side.)
			const double FirstAt = W.FirstBurst > W.From ? W.FirstBurst : W.From + 20.0;
			const double Settle = ConvoyLook::Smooth(FirstAt - 8.0, FirstAt, T);
			const P3 Way = Length(W.Near) >= 1.0 ? W.Near : P3(W.Aim.X - Middle.X, W.Aim.Y - Middle.Y, 0.0) + Right * (-W.Hand * 15.0);
			const P3 Toward = Way * (1.0 / std::max(1e-6, Length(Way)));
			Out.Eye = Lerp(Middle - Toward * 4.0 + Up(-12.0), Middle - Toward * Off + Up(-4.0), Settle);
			const double Pitch = Lerp(66.0, 12.0, Settle) * Pi / 180.0;
			Out.Look = Out.Eye + Toward * (30.0 * std::cos(Pitch)) + Up(30.0 * std::sin(Pitch));
			Out.Lens = Lerp(80.0, 70.0, Settle);
			break;
		}
		case Kind::Ahead:
		{
			// Off her side and a little under her, looking up and forward where the bombs come down at her.
			Out.Eye = Middle + ConvoyLook::AheadOf(Boat.Heading + W.Hand * 80.0) * Off + Up(-4.0);
			Out.Look = Middle + Ahead * (Boat.Long * 0.2) + Up(7.0);
			Out.Lens = 66.0;
			break;
		}
		case Kind::Dive:
		{
			// Off her bow and over her as she goes down: she slides down and away beneath.
			Out.Eye = Middle + Ahead * (Boat.Long * 0.3) + Right * (W.Hand * Off) + Up(3.0);
			Out.Look = Middle + Ahead * (Boat.Long * 0.1);
			Out.Lens = 62.0;
			break;
		}
		case Kind::Rise:
		{
			// Under her and off her side, looking up past her at the light: she goes up toward it.
			Out.Eye = Middle + Right * (W.Hand * Off) + Up(-8.0);
			Out.Look = Middle + Up(8.0);
			Out.Lens = 66.0;
			break;
		}
		case Kind::ByHand:
		default:
			Out = EyeByHand(Boat, Others, T - W.From, W.Hand);
			break;
		}
		Out.Eye = ClearOf(Out.Eye, Boat, Others);
		Out.bUnder = true;
		return true;
	}

	EyePose EyeByHand(const BoatPose& Boat, const std::vector<Hull>& Others, double Since, int Hand)
	{
		// The player's own dive: off her side at her depth, drifting slowly round her (20 degrees either way, once in 100 s).
		EyePose Out;
		const P3 Middle(Boat.At.X, Boat.At.Y, -MiddleOf(Boat.Keel));
		const double Round = 20.0 * std::sin(Since * 2.0 * Pi / 100.0);
		const P3 Side = ConvoyLook::AheadOf(Boat.Heading + Hand * (90.0 + Round));
		Out.Eye = ClearOf(Middle + Side * (14.0 + Boat.Beam * 0.5) + Up(-1.0), Boat, Others);
		Out.Look = Middle + Up(2.0);
		Out.Lens = 60.0;
		return Out;
	}

	P3 ClearOf(const P3& Eye, const BoatPose& Boat, const std::vector<Hull>& Others)
	{
		P3 Out = Eye;
		if (Boat.bThere)
		{
			// (off her hull's axis, stern to bow at her middle's depth, by her half beam and 6 m at the least)
			const P3 Ahead = ConvoyLook::AheadOf(Boat.Heading);
			const double Along = Clamp(Dot(Out - Boat.At, Ahead), -0.5 * Boat.Long, 0.5 * Boat.Long);
			const P3 Near = Boat.At + Ahead * Along;
			const P3 Away = Out - Near;
			const double Far = Length(Away), Least = 0.5 * Boat.Beam + 6.0;
			if (Far < Least)
			{
				const P3 Way = Far > 1e-6 ? Away * (1.0 / Far) : ConvoyLook::RightOf(Boat.Heading);
				Out = Near + Way * Least;
			}
		}
		for (const Hull& H : Others)
		{
			if (Boat.bThere && H.bBoat && Level(H.At - Boat.At) < 0.5) continue; // (hers: above)
			const P3 Ahead = ConvoyLook::AheadOf(H.Heading), Right = ConvoyLook::RightOf(H.Heading);
			const P3 Rel = Out - H.At;
			const double Along = Rel.X * Ahead.X + Rel.Y * Ahead.Y;
			if (H.bBoat)
			{
				// (another boat: off her axis by her half beam and 3 m)
				const P3 Near = H.At + Ahead * Clamp(Along, -0.5 * H.Long, 0.5 * H.Long);
				const P3 Away = Out - Near;
				const double Far = Length(Away), Least = 0.5 * H.Beam + 3.0;
				if (Far < Least) Out = Near + (Far > 1e-6 ? Away * (1.0 / Far) : Right) * Least;
				continue;
			}
			// (a ship: under her keel by 4 m, wholly under her and easing back up over the 8 m beside her)
			const double Across = Rel.X * Right.X + Rel.Y * Right.Y;
			const double Outside = std::max(std::fabs(Along) - 0.5 * H.Long, std::fabs(Across) - 0.5 * H.Beam);
			const double Weight = 1.0 - ConvoyLook::Smooth(0.0, 8.0, Outside);
			const double Floor = H.Bottom - 4.0;
			if (Weight > 0.0 && Out.Z > Floor) Out.Z = Lerp(Out.Z, Floor, Weight);
		}
		Out.Z = std::min(Out.Z, -1.5);
		return Out;
	}

	// -------------------------------------------------------------------------------------------------------------
	// The water

	double DaylightOf(double SunUpDeg) { return ConvoyLook::Smooth(-6.0, 10.0, SunUpDeg); }

	Murk MurkAt(double Depth, double Daylight, double SunUp, double DaySee, double NightSee)
	{
		// [EST] North Atlantic water as the war films have it: green, a ship's length or so seen; darker the deeper (a
		// quarter at the least, so a boat at 200 m keeps her shape); with no sun only the sky's light, a fifth of the day's
		// at the least (day for night, as the films were shot). The sun's share left: [EST] e-folding over 25 m.
		Murk M;
		const double D = std::max(0.0, Depth), Lit = Clamp(Daylight, 0.0, 1.0);
		M.See = Lerp(NightSee, DaySee, Lit);
		const double Fade = std::max(0.25, std::exp(-D / 30.0)) * std::max(Lit, 0.22);
		M.R = 0.035 * Fade;
		M.G = 0.075 * Fade;
		M.B = 0.065 * Fade;
		M.SunShare = Clamp(std::exp(-D / 25.0), 0.08, 0.6);
		M.bShafts = SunUp > 10.0 && D < 60.0 && Lit > 0.5;
		return M;
	}

	double FogDensity(double See, double UnitsPerMetre)
	{
		// (the engine's height fog: exp2 of minus Density/1000 a unit along the ray; 2^-4.32 is 5 in the hundred)
		return 4.32 * 1000.0 / (std::max(1.0, See) * UnitsPerMetre);
	}

	double LightLeft(double Metres, double See) { return std::pow(0.05, std::max(0.0, Metres) / std::max(1.0, See)); }

	double RippleWave(double Depth) { return std::max(20.0, Depth); }

	double CeilingCell(double Depth)
	{
		// (the widest gap between neighbouring points inside the window: along a ring, and from one ring to the next)
		const double D = std::max(0.5, Depth - 0.15);
		double Widest = 0.0, Last = 0.0;
		for (int Ring = 1; Ring < CeilingRings; ++Ring)
		{
			const double Reach = D * std::tan(CeilingReach * Ring / (CeilingRings - 1) * Pi / 180.0);
			Widest = std::max({ Widest, Reach - Last, 2.0 * Pi * Reach / CeilingSpokes });
			Last = Reach;
		}
		return Widest;
	}

	void CeilingAt(const P3& Eye, double T, double Daylight, double SunUp, double SunFrom, const Murk& M, std::vector<CeilingPoint>& Out)
	{
		// The sea's face from under it, a disc about the eye just under the face: straight up, Snell's window (all the sky
		// squeezed into a cone 48.6 degrees about the vertical: water's index 1.333), the brighter toward its rim; the sun
		// a blob in it asin(cos(its height) / 1.333) from the vertical, toward it; outside it the face a mirror of the dark
		// below, out to a skirt in the murk's own colour. Three crossing ripples on the battle's clock move its light, none
		// shorter than RippleWave (so 2.5 of the mesh's cells at the least).
		Out.clear();
		const double D = std::max(0.5, -Eye.Z - 0.15);
		const double Snell = 48.6;
		const double SunOff = std::asin(Clamp(std::cos(SunUp * Pi / 180.0) / 1.333, -1.0, 1.0)) * 180.0 / Pi;
		const double Lit = Clamp(Daylight, 0.0, 1.0);
		const P3 SkyLight(0.30 * Lit + 0.02, 0.46 * Lit + 0.03, 0.44 * Lit + 0.03);
		const P3 Below(M.R * 0.8, M.G * 0.8, M.B * 0.8);
		double Wave[3];
		for (int K = 0; K < 3; ++K) Wave[K] = 2.0 * Pi / (RippleWave(-Eye.Z) * RippleShares[K]);
		auto Paint = [&](double Angle, double Bearing, const P3& Where)
		{
			CeilingPoint C;
			C.At = Where;
			P3 Shade = Below;
			if (Angle < Snell + 2.0)
			{
				const double Rim = 0.55 + 0.45 * ConvoyLook::Smooth(30.0, Snell, Angle) + 0.3 * ConvoyLook::Smooth(Snell - 3.0, Snell, Angle);
				const double Edge = 1.0 - ConvoyLook::Smooth(Snell, Snell + 2.0, Angle);
				const double Ripple = 0.85 + 0.05 * std::sin(Where.X * Wave[0] + T * 1.3) + 0.05 * std::sin(Where.Y * Wave[1] - T * 1.1) +
				                      0.05 * std::sin((Where.X + Where.Y) * 0.7071 * Wave[2] + T * 0.7);
				P3 Through = SkyLight * (Rim * Ripple);
				if (SunUp > 0.0)
				{
					const double Apart = std::hypot(Angle * std::cos(Bearing * Pi / 180.0) - SunOff * std::cos(SunFrom * Pi / 180.0),
					                                Angle * std::sin(Bearing * Pi / 180.0) - SunOff * std::sin(SunFrom * Pi / 180.0));
					Through = Through + P3(1.0, 0.95, 0.8) * (1.6 * Lit * std::exp(-(Apart / 6.0) * (Apart / 6.0)) * Ripple);
				}
				Shade = Lerp(Below, Through, Edge);
			}
			C.R = Shade.X;
			C.G = Shade.Y;
			C.B = Shade.Z;
			Out.push_back(C);
		};
		Paint(0.0, 0.0, P3(Eye.X, Eye.Y, -0.15));
		double Last = 0.0;
		for (int Ring = 1; Ring <= CeilingRings; ++Ring)
		{
			const bool bSkirt = Ring == CeilingRings;
			const double Angle = bSkirt ? CeilingSkirt : CeilingReach * Ring / (CeilingRings - 1);
			const double Reach = bSkirt ? std::max(Last * 1.2, std::min(D * std::tan(CeilingSkirt * Pi / 180.0), 4.0 * M.See)) : D * std::tan(Angle * Pi / 180.0);
			Last = Reach;
			for (int Spoke = 0; Spoke < CeilingSpokes; ++Spoke)
			{
				const double Bearing = 360.0 * Spoke / CeilingSpokes;
				const P3 Way = ConvoyLook::AheadOf(Bearing);
				Paint(Angle, Bearing, P3(Eye.X + Way.X * Reach, Eye.Y + Way.Y * Reach, -0.15));
			}
		}
	}

	double GlobeAt(double Kg, double Depth, double Age)
	{
		// [P] Cole, Underwater Explosions (1948): the gas globe swells to Rmax = 3.38 (W/(D+10))^(1/3) m in half its period,
		// 2.11 W^(1/3) / (D+10)^(5/6) s (W kg of TNT, D metres), and falls in; it swells again to about 0.6 of it and once
		// more to about 0.35, each period about 0.75 of the one before; then it is a cloud of bubbles rising. (A Mk VII's
		// 132 kg at 50 m: 4.4 m, 0.35 s.)
		const double W = std::max(0.1, Kg), D = std::max(0.0, Depth) + 10.0;
		double Start = 0.0, Period = 2.11 * std::cbrt(W) / std::pow(D, 5.0 / 6.0), Size = 3.38 * std::cbrt(W / D);
		for (int K = 0; K < 3; ++K)
		{
			if (Age >= Start && Age < Start + Period) return Size * std::sin(Pi * (Age - Start) / Period);
			Start += Period;
			Period *= 0.75;
			Size *= K == 0 ? 0.6 : 0.58;
		}
		return -1.0;
	}

	void MotesAt(const P3& Eye, double T, std::vector<P3>& Out)
	{
		// Silt and plankton hung in the water, fixed in it (the eye goes past them; they drift a little), each wrapped into a
		// box MoteBox wide about the eye, so there are always as many and none pops in.
		Out.clear();
		const double Box = Water::MoteBox;
		for (int K = 0; K < Water::MostMotes; ++K)
		{
			const uint64_t Key = 0x5EA5117ull + static_cast<uint64_t>(K) * 7919ull;
			const P3 Home(Box * ConvoyLook::Hash01(Key), Box * ConvoyLook::Hash01(Key + 1), -Box * ConvoyLook::Hash01(Key + 2));
			const P3 Drift(0.03 * T + 0.2 * std::sin(T * 0.11 + K), 0.02 * T + 0.2 * std::cos(T * 0.09 + 1.7 * K), -0.015 * T);
			const P3 P = Home + Drift;
			Out.push_back(P3(Wrap(P.X, Eye.X, Box), Wrap(P.Y, Eye.Y, Box), std::min(-0.3, Wrap(P.Z, Eye.Z, Box))));
		}
	}

	void ShaftsAt(const P3& Eye, double T, double SunUp, double SunFrom, const Murk& M, std::vector<Shaft>& Out)
	{
		// By day, near the face: the sun's light down through the water at its angle through the face (away from it), each
		// shaft wavering with the swell; fixed in the water about the eye as the silt is.
		Out.clear();
		const double Bent = std::asin(Clamp(std::cos(SunUp * Pi / 180.0) / 1.333, -1.0, 1.0));
		const P3 Way = ConvoyLook::AheadOf(SunFrom + 180.0) * std::sin(Bent) + Up(-std::cos(Bent));
		for (int K = 0; K < Water::MostShafts; ++K)
		{
			const uint64_t Key = 0x5AF7ull + static_cast<uint64_t>(K) * 104729ull;
			const P3 Top(Wrap(Water::MoteBox * ConvoyLook::Hash01(Key), Eye.X, Water::MoteBox), Wrap(Water::MoteBox * ConvoyLook::Hash01(Key + 1), Eye.Y, Water::MoteBox), -0.2);
			const double Waver = 0.6 + 0.4 * std::sin(T * (0.5 + 0.4 * ConvoyLook::Hash01(Key + 2)) + 6.0 * ConvoyLook::Hash01(Key + 3));
			Out.push_back(Shaft{ Top, Way, 30.0, 0.6 + 0.8 * Waver, 0.10 * Waver * M.SunShare / 0.6 });
		}
	}

	// -------------------------------------------------------------------------------------------------------------
	// What is in the water

	uint64_t Water::KeyOf(const SeaShow::Show::ChargeDown& C, int Again)
	{
		// (its escort, its drop and its burst time: steady for its whole fall; a booked charge told within 15 s bursts at
		// once, SeaShow.cpp's sea:depthChargeHit, so is never listed under a changed key; two alike told apart by order)
		return Mix(Mix(Mix(static_cast<uint64_t>(C.Escort + 1), Ms(C.Dropped)), Ms(C.Bursts)), static_cast<uint64_t>(Again));
	}

	void Water::Begin(const SeaShow::Field& InBoard, double InDaySee, double InNightSee)
	{
		Board = InBoard;
		DaySee = InDaySee;
		NightSee = InNightSee;
		Seen.clear();
		BoatsNow.clear();
		Alike.clear();
		Blown.clear();
		Bombs.clear();
		Ends.clear();
		AheadTooSoon = 0;
	}

	P3 Water::Target(const Sinking& S, double T) const
	{
		// (a charge told on her bursts Off from her where she is then, at her keel: SeaShow's BurstCharges. Where she will be
		// is reckoned from her way now, so its path runs straight rather than chasing her)
		if (S.bOnBoat)
		{
			if (const auto It = BoatsNow.find(S.Boat); It != BoatsNow.end())
			{
				const P3 Then = It->second.At + It->second.Vel * std::max(0.0, S.Bursts - T);
				return P3(Then.X + S.Off.X, Then.Y + S.Off.Y, It->second.At.Z);
			}
		}
		return S.To;
	}

	P3 Water::DrawnAt(const Sinking& S, double T) const
	{
		const double Share = Clamp((T - S.FromT) / std::max(1e-3, S.Bursts - S.FromT), 0.0, 1.0);
		return Lerp(S.From, Target(S, T), Share);
	}

	void Water::Remember(double T, const std::vector<SeaShow::Show::ChargeDown>& Down, const std::map<int, BoatNow>& Boats)
	{
		BoatsNow = Boats;
		for (auto& Pair : Seen) Pair.second.bLive = false;
		Alike.clear();
		for (const SeaShow::Show::ChargeDown& C : Down)
		{
			if (C.Kind == "ahead") continue; // (an ahead salvo's bombs are drawn from its throw: AheadDropped)
			// (how many alike before it in the list: a handful at the most, so a short search)
			const uint64_t First = KeyOf(C, 0);
			int Again = 0;
			bool bCounted = false;
			for (std::pair<uint64_t, int>& Count : Alike)
			{
				if (Count.first != First) continue;
				Again = Count.second++;
				bCounted = true;
				break;
			}
			if (!bCounted) Alike.emplace_back(First, 1);
			const uint64_t Key = KeyOf(C, Again);
			const P3 To(C.At.X, C.At.Y, C.At.Z);
			const P3 Off(C.Off.X, C.Off.Y, 0.0);
			const auto It = Seen.find(Key);
			if (It == Seen.end())
			{
				// (first seen: in the water from as late as it can be and sink to its burst at the show's rate, never before its
				// drop and 0.6 s, its fall from her rails)
				Sinking S;
				S.Escort = C.Escort;
				S.Bursts = C.Bursts;
				S.In = std::max(C.Dropped + 0.6, C.Bursts - std::max(1.0, -C.At.Z) / std::max(0.5, Board.ChargeSink));
				S.To = To;
				S.bOnBoat = C.bOnBoat;
				S.Boat = C.Boat;
				S.Off = Off;
				// (one told on her already: in where it will burst by her, as she is going now, so it sinks straight; else where the
				// show has it)
				const P3 Entry = S.bOnBoat ? Target(S, S.In) : To;
				S.From = P3(Entry.X, Entry.Y, 0.0);
				S.FromT = S.In;
				Seen.emplace(Key, S);
				continue;
			}
			Sinking& S = It->second;
			S.bLive = true;
			const bool bMoved = S.bOnBoat != C.bOnBoat || Length(S.To - To) > 0.01 || S.Boat != C.Boat || Length(S.Off - Off) > 0.01;
			if (!bMoved) continue;
			// (its burst moved by the battle: from where it is drawn now, eased there over the rest of its fall)
			if (T > S.FromT)
			{
				S.From = DrawnAt(S, T);
				S.FromT = T;
			}
			S.To = To;
			S.bOnBoat = C.bOnBoat;
			S.Boat = C.Boat;
			S.Off = Off;
		}
		for (auto It = Seen.begin(); It != Seen.end();)
		{
			if (It->second.bLive) ++It;
			else It = Seen.erase(It);
		}
	}

	Drum Water::DrumOf(uint64_t Key, const Sinking& S, double T) const
	{
		Drum D;
		D.At = DrawnAt(S, T);
		D.Key = Key;
		D.Escort = S.Escort;
		D.Yaw = 360.0 * ConvoyLook::Hash01(D.Key);
		D.Tilt = 25.0 * std::sin(T * 0.8 + 6.0 * ConvoyLook::Hash01(D.Key + 1)); // (tumbling a little as it sinks)
		return D;
	}

	void Water::Charges(double T, std::vector<Drum>& Out) const
	{
		Out.clear();
		for (const auto& Pair : Seen)
		{
			// (drawn while the show lists it: up to a twentieth of a second past its time it waits at its burst for the show's step)
			if (T >= Pair.second.In) Out.push_back(DrumOf(Pair.first, Pair.second, T));
		}
	}

	void Water::Burst(double T, const P3& At, const std::string& What)
	{
		Blow B;
		B.T = T;
		B.At = At;
		B.Kg = ConvoyLook::ChargeKg(What);
		B.Key = Mix(Ms(T), static_cast<uint64_t>(std::llround(std::fabs(At.X) * 10.0 + std::fabs(At.Y) * 1000.0)));
		Blown.push_back(B);
		// (an ahead bomb bursts only on her hull: the nearest still sinking that is down there is the one that struck. One
		// more than AheadAbove m over the burst never is: a hit told before any could have sunk so deep, a timing the rules
		// decide, bursts at their place alone, the bombs sinking on)
		if (What == "ahead")
		{
			Bomb* Struck = nullptr;
			double Best = 1e18;
			for (Bomb& M : Bombs)
			{
				if (M.BurstT >= 0.0 || T < M.In || T >= M.Gone) continue;
				const P3 Now = M.Entry + Up(-AheadSink * (T - M.In));
				if (Now.Z > At.Z + AheadAbove) continue;
				const double Far = Length(Now - At);
				if (Far < Best) { Best = Far; Struck = &M; }
			}
			if (Struck) Struck->BurstT = T;
			else ++AheadTooSoon;
		}
		Blown.erase(std::remove_if(Blown.begin(), Blown.end(), [T](const Blow& Old) { return T - Old.T > 12.0; }), Blown.end());
	}

	void Water::AheadDropped(double T, const ConvoyLook::DropIn& In, uint64_t Seed, double Deepest)
	{
		// The ahead salvo's bombs where ConvoyLooks throws them (the same plan from the same seed: its splashes and these
		// agree), sinking at AheadSink until one strikes (Burst) or they pass Deepest.
		for (const ConvoyLook::DropShot& Shot : ConvoyLook::DropPlan(In, Seed))
		{
			if (Shot.Side != 2) continue;
			Bomb M;
			M.In = T + Shot.Dt + Shot.Flight;
			M.Entry = P3(Shot.To.X, Shot.To.Y, 0.0);
			M.Gone = M.In + Deepest / AheadSink;
			M.Key = Mix(Seed, Bombs.size());
			Bombs.push_back(M);
		}
		Bombs.erase(std::remove_if(Bombs.begin(), Bombs.end(), [T](const Bomb& Old) { return T > Old.Gone + 1.0; }), Bombs.end());
	}

	void Water::AheadSinking(double T, std::vector<Drum>& Out) const
	{
		// (nose down: finned, they fall straight; one that struck is drawn to its burst, the rest to past her depth)
		Out.clear();
		for (const Bomb& B : Bombs)
		{
			if (T < B.In || T >= (B.BurstT >= 0.0 ? B.BurstT : B.Gone)) continue;
			Drum D;
			D.At = B.Entry + Up(-AheadSink * (T - B.In));
			D.bBomb = true;
			D.Key = B.Key;
			Out.push_back(D);
		}
	}

	void Water::Lay(double T, const P3& Eye, const BoatPose* Boat, double Daylight, double SunUp, double SunFrom, Frame& Out) const
	{
		// (each list cleared, its room kept: laid every frame under the water, so nothing is made afresh once grown)
		for (std::vector<Glob>* Each : { &Out.Globes, &Out.Bubbles, &Out.Flashes, &Out.Air }) Each->clear();
		Out.Drums.clear();
		Out.Motes.clear();
		Out.Shafts.clear();
		const Murk M = MurkAt(-Eye.Z, Daylight, SunUp, DaySee, NightSee);
		const double Near = M.See;
		// The charges sinking (the show's list, each drawn from where it was first seen: Remember).
		for (const auto& Pair : Seen)
		{
			if (static_cast<int>(Out.Drums.size()) >= MostDrums) break;
			if (T < Pair.second.In) continue;
			const Drum D = DrumOf(Pair.first, Pair.second, T);
			if (Length(D.At - Eye) <= Near) Out.Drums.push_back(D);
		}
		// The ahead salvo's bombs (nose down: finned, they fall straight).
		for (const Bomb& B : Bombs)
		{
			if (static_cast<int>(Out.Drums.size()) >= MostDrums) break;
			if (T < B.In || T >= (B.BurstT >= 0.0 ? B.BurstT : B.Gone)) continue;
			Drum D;
			D.At = B.Entry + Up(-AheadSink * (T - B.In));
			if (Length(D.At - Eye) > Near) continue;
			D.bBomb = true;
			D.Key = B.Key;
			Out.Drums.push_back(D);
		}
		// Each burst: its flash (an eighth of a second), its gas globe swelling and falling in (GlobeAt), then a cloud of
		// bubbles rising from it at [EST] 2 m a second, spreading, gone in 8 s.
		for (const Blow& B : Blown)
		{
			const double Age = T - B.T;
			if (Age < 0.0 || Age > 8.0 || Length(B.At - Eye) > Near + 10.0) continue;
			if (Age < 0.12 && static_cast<int>(Out.Flashes.size()) < MostGlobes) Out.Flashes.push_back(Glob{ B.At, 3.0 + 5.0 * (1.0 - Age / 0.12), 1.0 });
			const double R = GlobeAt(B.Kg, -B.At.Z, Age);
			if (R > 0.0 && static_cast<int>(Out.Globes.size()) < MostGlobes) Out.Globes.push_back(Glob{ B.At, R, 0.55 });
			if (R >= 0.0) continue;
			const double Rise = 2.0 * Age, Wide = 2.0 + 1.2 * Age, Fade = 1.0 - ConvoyLook::Smooth(3.0, 8.0, Age);
			for (int K = 0; K < 18 && static_cast<int>(Out.Bubbles.size()) < MostBubbles; ++K)
			{
				const uint64_t Key = Mix(B.Key, static_cast<uint64_t>(K));
				P3 At = B.At + ConvoyLook::AheadOf(360.0 * ConvoyLook::Hash01(Key)) * (Wide * ConvoyLook::Hash01(Key + 7)) + Up(Rise * (0.7 + 0.6 * ConvoyLook::Hash01(Key + 11)));
				At.Z = std::min(At.Z, -0.3);
				Out.Bubbles.push_back(Glob{ At, 0.12 + 0.3 * ConvoyLook::Hash01(Key + 13), 0.5 * Fade });
			}
		}
		// Her air going once she is lost, hers only (her own end, never another boat's): three streams from her hull rising at
		// [EST] 1.2 m a second (the hump of air on the sea is ConvoyLooks'), within the bubbles' budget.
		const auto Lost = Boat && Boat->bThere ? Ends.find(Boat->Key) : Ends.end();
		if (Lost != Ends.end() && T >= Lost->second)
		{
			const double EndT = Lost->second;
			const P3 Ahead = ConvoyLook::AheadOf(Boat->Heading);
			for (int Stream = 0; Stream < 3; ++Stream)
			{
				const P3 Source = Boat->At + Ahead * (Boat->Long * 0.3 * (Stream - 1)) + Up(2.0);
				for (int K = 0; K < 20 && static_cast<int>(Out.Bubbles.size() + Out.Air.size()) < MostBubbles; ++K)
				{
					const uint64_t Key = Mix(static_cast<uint64_t>(Stream + 1), static_cast<uint64_t>(K));
					P3 At = Source + Up(std::fmod(1.2 * (T - EndT) + 3.0 * K, std::max(3.0, -Source.Z))) + ConvoyLook::AheadOf(360.0 * ConvoyLook::Hash01(Key)) * (0.6 * ConvoyLook::Hash01(Key + 1));
					At.Z = std::min(At.Z, -0.3);
					Out.Air.push_back(Glob{ At, 0.08 + 0.2 * ConvoyLook::Hash01(Key + 2), 0.5 });
				}
			}
		}
		MotesAt(Eye, T, Out.Motes);
		if (M.bShafts) ShaftsAt(Eye, T, SunUp, SunFrom, M, Out.Shafts);
	}

	P3 Water::ShakeAt(double T, const P3& Eye) const
	{
		// Each burst's shock in the eye: a jolt as the pressure wave passes (1,500 m a second: at once at these ranges),
		// dying in 0.6 s, the more the nearer, none past 400 m; the whole never over 0.6 m. Its wobble from the burst's hash.
		P3 Sum;
		for (const Blow& B : Blown)
		{
			const double Age = T - B.T, Far = std::max(8.0, Length(B.At - Eye));
			if (Age < 0.0 || Age > 2.0 || Far > 400.0) continue;
			const double A = std::min(0.6, 12.0 / Far) * std::exp(-Age / 0.6), Ph = 2.0 * Pi * ConvoyLook::Hash01(B.Key);
			Sum = Sum + P3(std::sin(Age * 31.0 + Ph), std::sin(Age * 27.0 + 1.7 * Ph), 0.6 * std::sin(Age * 23.0 + 2.3 * Ph)) * A;
		}
		const double L = Length(Sum);
		return L > 0.6 ? Sum * (0.6 / L) : Sum;
	}

	double Water::ShudderAt(double T, const P3& HerAt) const
	{
		// Her hull shaken by a burst near her (degrees of heel): at most 3, none past 120 m, dying in 1.2 s.
		double Sum = 0.0;
		for (const Blow& B : Blown)
		{
			const double Age = T - B.T, Far = std::max(5.0, Length(B.At - HerAt));
			if (Age < 0.0 || Age > 4.0 || Far > 120.0) continue;
			Sum += std::min(3.0, 60.0 / Far) * std::exp(-Age / 1.2) * std::sin(Age * 14.0 + 2.0 * Pi * ConvoyLook::Hash01(B.Key + 3));
		}
		return Clamp(Sum, -3.0, 3.0);
	}

	void Water::CreaksBetween(double T0, double T1, double Since, uint64_t Seed, double Keel0, double Keel1, const P3& HerAt, double& LastAt, std::vector<Creak>& Out) const
	{
		// The hull creaking as the films have it and as a pressure hull does: at every 6 m deeper past 40 m; half a second to
		// 1.6 s after a burst within 60 m of her (a rattle of things shaken loose with it, within 25 m); and, while she lies
		// 40 m down or more, every 11 to 17 s from the window's start. None within 0.8 s of the last.
		std::vector<Creak> Due;
		for (double Mark = 46.0; Mark <= 400.0; Mark += 6.0)
			if (Keel0 < Mark && Keel1 >= Mark) Due.push_back(Creak{ T1, 1 + static_cast<int>(3.0 * ConvoyLook::Hash01(Mix(Seed, Ms(Mark)))), 0.8, false });
		for (const Blow& B : Blown)
		{
			const double Far = Length(B.At - HerAt), At = B.T + 0.5 + 1.1 * ConvoyLook::Hash01(B.Key + 5);
			if (Far <= 60.0 && At > T0 && At <= T1)
				Due.push_back(Creak{ At, 1 + static_cast<int>(3.0 * ConvoyLook::Hash01(B.Key + 6)), Clamp(60.0 / std::max(10.0, Far), 0.5, 1.0), Far <= 25.0 });
		}
		if (std::max(Keel0, Keel1) >= 40.0)
		{
			double At = Since;
			for (uint64_t K = 0; At <= T1 && K < 1000; ++K)
			{
				At += 11.0 + 6.0 * ConvoyLook::Hash01(Mix(Seed, K + 100));
				if (At > T0 && At <= T1) Due.push_back(Creak{ At, 1 + static_cast<int>(3.0 * ConvoyLook::Hash01(Mix(Seed, K + 200))), 0.6, false });
			}
		}
		std::sort(Due.begin(), Due.end(), [](const Creak& L, const Creak& R) { return L.T < R.T; });
		for (const Creak& C : Due)
		{
			if (C.T - LastAt < 0.8) continue;
			LastAt = C.T;
			Out.push_back(C);
		}
	}
}
