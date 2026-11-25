#include "Battle/AirShow.h"

#include <algorithm>
#include <cmath>

/**
 * (Release 2) A raid on troops in the air show (Brian, 24 November 2026: "i want every air battle playable" and "if a
 * bomber group is bombing troops instead of a factory or town and they get intercepted than i want that playable").
 *
 * What stands on the ground is the rules' (place.marks; SetMarks), and so is every hit on it (air:mark-hit) and what
 * each piece is after (view.marks). Here the scene only shows it: each counted hit as the drawn bomb or rounds that
 * carry it arrive on its piece (one MarkHit, never before), every other bomb clear of every piece by more than its blast
 * (AirMarks: "most shots miss, but every hit shows on what it struck"), a fighter group's pass down its lane
 * (air:strafe) flown as LandAir flies its machines (AirMarks::PassOf), and the column's light flak firing back short of
 * the lead (drawn only). Nothing here draws on the show's dice: every choice is a hash, so the flight plays the same.
 *
 * None of it runs where the sky has no marks: SetMarks is never called, and every hook in AirShow.cpp asks Column.Has()
 * (or a bomb's bWant, set only here) first.
 */
namespace AirShow
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Gee = 9.81;
		constexpr double Deg = Pi / 180.0;
		/** The drag a bomb falls by: SmallThings takes 2% of its velocity a second. */
		constexpr double BombDrag = 0.02;
		/** Air seconds a bomb hit waits for a bomb in the air to carry it, and a strafe hit for a pass's rounds, before it
		 * is shown on its piece with none (Checks::MarkHitsUnseen). */
		constexpr double BombWait = 45.0, StrafeWait = 150.0;
		/** The least fall a bomb has left to be given a hit: its way across the ground eased over at least so long. */
		constexpr double BindLeast = 5.0;
		/** How close to a piece a round's strike may come that carries no hit on it (metres from its footprint). */
		constexpr double RoundClear = 1.5;
		/** The most aircraft of a group in one pass (a four), and the lane's own walk: the stream's strikes go on down the
		 * lane at this share of the aircraft's own way over the ground as it comes on. */
		constexpr int PassMost = 4;
		constexpr double WalkShare = 0.45;

		double Wrap(double A)
		{
			if (!(std::fabs(A) <= 1.0e3)) A = std::isfinite(A) ? std::remainder(A, 2.0 * Pi) : 0.0;
			while (A > Pi) A -= 2.0 * Pi;
			while (A <= -Pi) A += 2.0 * Pi;
			return A;
		}
		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : V > Hi ? Hi : V; }
		double Len(const Vec& V) { return std::sqrt(V.Dot(V)); }
		double Flat(const Vec& V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }
		Vec Norm(const Vec& V, const Vec& Otherwise = Vec(0.0, 1.0, 0.0))
		{
			const double L = Len(V);
			return L > 1e-9 ? V * (1.0 / L) : Otherwise;
		}
		Vec Ahead(double Hdg, double Gam = 0.0) { return Vec(std::sin(Hdg) * std::cos(Gam), std::cos(Hdg) * std::cos(Gam), std::sin(Gam)); }
		Vec RightOf(double Hdg) { return Vec(std::cos(Hdg), -std::sin(Hdg), 0.0); }
		bool Has(const std::vector<std::string>& List, const std::string& Word) { return std::find(List.begin(), List.end(), Word) != List.end(); }
		bool Has(const std::vector<int>& List, int Key) { return std::find(List.begin(), List.end(), Key) != List.end(); }
		/** A bomb's weight, as its BombHit's Size has it. */
		double KgOf(const Thing& B) { return B.Size > 2.0 ? 500.0 : 250.0; }
		/** A fighter's gun muzzle in its own frame (as FireGuns places them): the hub cannon at the spinner, the cowl pair
		 * above it, the wing guns numbered from the fuselage outward, odd to port. */
		Vec MuzzleOf(const Shape& S, const std::string& Socket, const std::string& Kind, int Index, int Of)
		{
			if (Socket == "gun.nose")
			{
				if (Kind == "cannon20") return Vec(S.Long * 0.5, 0.0, 0.0);
				return Vec(S.Long * 0.42, Of > 1 ? (Index % 2 == 0 ? -0.15 : 0.15) : 0.0, 0.3);
			}
			int N = 1;
			if (Socket.size() > 4) N = std::max(1, std::atoi(Socket.c_str() + 4));
			const double Wide = (S.Span / 11.0) * (1.2 + 0.55 * ((N + 1) / 2 - 1));
			return Vec(S.Long * 0.08, (N % 2 == 1 ? -1.0 : 1.0) * Wide, -0.1);
		}
	}

	void Show::SetMarks(const AirMarks::Column& InMarks)
	{
		Column = InMarks;
		for (AirMarks::Piece& P : Column.Pieces)
		{
			P.Now = AirMarks::State::Ok;
			P.Men = P.N;
		}
		MarkHolds.clear();
		MarkWords.clear();
		MarkCuts.clear();
		CutShown = 0.0;
		Lanes.clear();
		StrafeOwing.clear();
		BombSerial = 0;
		MarkSalt = 0;
		MarkFire = 1.0;
	}

	void Show::TakeMarks(double T, const AirMarks::View& V)
	{
		MarkFire = Clamp(V.Fire, 0.0, 1.0);
		for (const AirMarks::Change& C : V.Pieces) MarkWords.push_back(MarkWord{ T, C });
		// (the cut so far, let out once the hits it counts have been shown: TroopsFrame)
		if (V.Cut > CutShown) MarkCuts.push_back(MarkCut{ T, V.Cut });
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The hits, held for the bombs and the rounds that carry them.

	void Show::PlayMark(const Event& E)
	{
		// Held for what carries it, and told (its dispatch and its sound, Release) only as it is shown on its piece
		// (ShowMark), as a ship's hit is told only as its bombs land (the reviewer, 25 November: the dispatch line and the
		// sound had gone out the moment the event came, seconds before the bomb was down). A hit on no piece the column
		// has is told at once: nothing will carry it.
		MarkHold H;
		H.E = E;
		H.Since = AirT;
		H.Index = Column.IndexOf(E.Piece);
		if (H.Index < 0)
		{
			// (a ticket that sent only counts names what was struck, not which: the piece of that kind still standing
			// nearest where the battle has the hit; failing that, the nearest piece of any kind)
			const double X = E.bAt ? E.X : TargetX, Y = E.bAt ? E.Y : TargetY;
			double Best = 1e18;
			for (int Pass = 0; Pass < 2 && H.Index < 0; ++Pass)
				for (size_t K = 0; K < Column.Pieces.size(); ++K)
				{
					const AirMarks::Piece& P = Column.Pieces[K];
					if (Pass == 0 && ((!E.PieceWhat.empty() && P.What != E.PieceWhat) || P.Now == AirMarks::State::Out)) continue;
					const double D = std::hypot(P.X - X, P.Y - Y);
					if (D < Best)
					{
						Best = D;
						H.Index = static_cast<int>(K);
					}
				}
		}
		if (H.Index < 0)
		{
			Release(E);
			return;
		}
		MarkHolds.push_back(H);
		Check.MarkHitsTold++;
		// (a bomb in the air takes it at once where one has the fall left to be laid on its piece)
		if (E.Strike != "strafe") BindBomb(MarkHolds.back(), static_cast<int>(MarkHolds.size()) - 1);
	}

	double Show::FallLeft(const Thing& B, double X, double Y) const
	{
		// (dv/dt = -k v - g: z(t) = z0 + (vz + g/k)(1 - e^-kt)/k - g t/k, down to the ground under (X, Y))
		const double Floor = GroundAt(X, Y);
		const double Z0 = B.At.Z, Vz = B.Vel.Z, K = BombDrag;
		if (Z0 <= Floor) return 0.0;
		auto Z = [Z0, Vz, K](double T) { return Z0 + (Vz + Gee / K) * (1.0 - std::exp(-K * T)) / K - Gee / K * T; };
		double Lo = 0.0, Hi = 4.0;
		while (Z(Hi) > Floor && Hi < 2000.0)
		{
			Lo = Hi;
			Hi *= 2.0;
		}
		for (int I = 0; I < 40; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(Z(Mid) > Floor ? Lo : Hi) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	double Show::LandsAt(const Thing& B, double& X, double& Y) const
	{
		X = B.At.X;
		Y = B.At.Y;
		double T = 0.0;
		// (the ground where it comes down, found again from where it comes down: three times is ample over the board)
		for (int I = 0; I < 3; ++I)
		{
			T = FallLeft(B, X, Y);
			const double F = (1.0 - std::exp(-BombDrag * T)) / BombDrag;
			X = B.At.X + B.Vel.X * F;
			Y = B.At.Y + B.Vel.Y * F;
		}
		return T;
	}

	void Show::LayBomb(Thing& B)
	{
		B.Serial = ++BombSerial;
		double X = 0.0, Y = 0.0;
		B.LandLeft = LandsAt(B, X, Y);
		const double Blast = AirMarks::BlastOf(KgOf(B));
		double Cx = X, Cy = Y;
		// (clear of every piece by more than its blast, with a little to spare for the frames' own small errors; it is
		// steered all the way down, so that it comes down where it is laid)
		if (!AirMarks::Clear(Column, X, Y, Blast * 1.1) && AirMarks::ClearOf(Column, X, Y, Blast * 1.15, Cx, Cy))
		{
			Check.MarkBombsCleared++;
			Check.MarkMostMove = std::max(Check.MarkMostMove, std::hypot(Cx - X, Cy - Y));
		}
		B.bWant = true;
		B.WantX = Cx;
		B.WantY = Cy;
	}

	void Show::SteerBomb(Thing& B) const
	{
		// (its way across the ground for where it is to come down, in the fall it has left: a few metres a second at
		// most across a fall of half a minute; none in its last moment, where the frames' own errors are centimetres)
		const double T = FallLeft(B, B.WantX, B.WantY);
		B.LandLeft = T;
		if (T < 0.3) return;
		const double F = (1.0 - std::exp(-BombDrag * T)) / BombDrag;
		B.Vel.X = (B.WantX - B.At.X) / F;
		B.Vel.Y = (B.WantY - B.At.Y) / F;
	}

	Vec Show::Crossing(const Thing& B, double Dt) const
	{
		// (where its frame's flight crossed the ground: a bomb falls 200 m and more a second, and a frame's whole step
		// would put it metres past where it struck)
		const Vec Prev = B.At - B.Vel * Dt;
		double F = 1.0;
		if (Prev.Z > B.At.Z + 1e-9) F = Clamp((Prev.Z - GroundAt(B.At.X, B.At.Y)) / (Prev.Z - B.At.Z), 0.0, 1.0);
		Vec P = Prev + (B.At - Prev) * F;
		P.Z = GroundAt(P.X, P.Y);
		return P;
	}

	bool Show::BindBomb(MarkHold& H, int At)
	{
		// The bomb that carries it: of the hit's own group first, then any, with the fall left to be laid on the piece,
		// the one that need move least for it (metres a second across its fall). Each bomb comes down where it is laid
		// (WantX, WantY: it is steered there all the way), with the fall it had left there as it was last steered
		// (LandLeft): nothing is worked out again here, so a hit that waits costs next to nothing a frame.
		Thing* Best = nullptr;
		double BestCost = 1e18, BestX = 0.0, BestY = 0.0, BestMove = 0.0;
		for (int Pass = 0; Pass < 2 && !Best; ++Pass)
		{
			for (Thing& B : Small)
			{
				if (B.Kind != ThingKind::Bomb || B.bTorpedo || B.Serial <= 0 || !B.bWant || B.Carries >= 0) continue;
				if (Pass == 0 && B.Group != H.E.Unit) continue;
				const double Lx = B.WantX, Ly = B.WantY;
				const double Left = B.LandLeft;
				if (Left < BindLeast) continue;
				double X = 0.0, Y = 0.0;
				AirMarks::AimOn(Column, H.Index, H.E.How, AirMarks::BlastOf(KgOf(B)), static_cast<uint32_t>(B.Serial), X, Y);
				const double Move = std::hypot(X - Lx, Y - Ly);
				const double Cost = Move / Left;
				if (Cost < BestCost)
				{
					BestCost = Cost;
					Best = &B;
					BestX = X;
					BestY = Y;
					BestMove = Move;
				}
			}
		}
		if (!Best) return false;
		Best->bWant = true;
		Best->WantX = BestX;
		Best->WantY = BestY;
		Best->LandLeft = FallLeft(*Best, BestX, BestY);
		Best->Carries = At;
		H.Bomb = Best->Serial;
		Check.MarkBombsLaid++;
		Check.MarkMostMove = std::max(Check.MarkMostMove, BestMove);
		return true;
	}

	void Show::BombDown(const Thing& B)
	{
		if (B.Carries < 0 || B.Carries >= static_cast<int>(MarkHolds.size())) return;
		MarkHold& H = MarkHolds[static_cast<size_t>(B.Carries)];
		if (H.bDone || H.Bomb != B.Serial) return;
		ShowMark(H, B.Serial, true);
	}

	void Show::ShowMark(MarkHold& H, int Bomb, bool bSeen)
	{
		H.bDone = true;
		AirMarks::Piece& P = Column.Pieces[static_cast<size_t>(H.Index)];
		Fx& F = Emit(FxKind::MarkHit, Vec(P.X, P.Y, GroundAt(P.X, P.Y)), H.E.Strike.empty() ? std::string("bomb") : H.E.Strike, P.Id);
		F.Text = H.E.How;
		F.Size = H.E.Men;
		F.Id = Bomb;
		F.Side = H.E.Side;
		F.Part = H.E.bBig ? "big" : "";
		Check.MarkHitsShown++;
		// (and now it is told: its dispatch and its sound, in the frame its bomb or rounds arrive; F is not used after,
		// as Release may add effects)
		Release(H.E);
		if (!bSeen) Check.MarkHitsUnseen++;
		// (the piece as the hit leaves it, never down the list; a block of men thinned by the men it cost. The views'
		// words, which come after, have the last say: MarkWords)
		const int MenLeft = P.What == "men" && H.E.Men > 0 ? std::max(0, P.Men - H.E.Men) : -1;
		AirMarks::Apply(P, MenLeft == 0 ? AirMarks::State::Out : AirMarks::StateAfter(H.E.How, P.Now), MenLeft);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The strafing passes.

	void Show::StrafeBegin(const Event& E)
	{
		StrafeRun R;
		if (E.bLane)
		{
			R.L.FromX = E.FromX;
			R.L.FromY = E.FromY;
			R.L.ToX = E.ToX;
			R.L.ToY = E.ToY;
			R.L.Width = E.Width > 0.0 ? E.Width : 60.0;
		}
		else R.L = AirMarks::LaneOf(Column, E.bAt ? E.X : TargetX, E.bAt ? E.Y : TargetY);
		const auto G = Now.find(E.Unit);
		R.P = AirMarks::PassOf(G != Now.end() ? G->second.Model : std::string());
		R.Unit = E.Unit;
		R.At = AirT;
		// (a four of the group, at most, of those flying their places: one in a fight stays in it)
		for (int Key : LiveOf(E.Unit))
		{
			if (static_cast<int>(R.Keys.size()) >= PassMost) break;
			const Craft* C = Find(Key);
			if (C && C->Doing == Role::Slot && !C->bLaneRun) R.Keys.push_back(Key);
		}
		if (R.Keys.empty()) return;
		const int Index = static_cast<int>(Lanes.size());
		const double Long = R.L.Long();
		const int N = static_cast<int>(R.Keys.size());
		for (int K = 0; K < N; ++K)
		{
			Craft& C = *Find(R.Keys[static_cast<size_t>(K)]);
			C.Doing = Role::Dive;
			C.Step = Phase::Setup;
			C.StepT = 0.0;
			C.bLaneRun = true;
			C.LaneOf = Index;
			C.LaneK = K;
			// (its stretch of the lane, the four's share of it one after another; a hit held for the pass moves it there)
			C.LaneU0 = Long * K / N;
			C.LaneSide = (K % 2 ? 1.0 : -1.0) * std::min(4.0, R.L.Width * 0.08);
			C.LaneHold = -1;
			C.bLaneGuns = false;
			C.bJoining = false;
			Check.StrafePasses++;
		}
		Lanes.push_back(R);
	}

	void Show::EndStrafe(Craft& C)
	{
		C.Doing = Role::Slot;
		C.Step = Phase::None;
		C.StepT = 0.0;
		C.bJoining = true;
		C.bLaneRun = false;
		C.LaneOf = -1;
		C.LaneHold = -1;
		C.bLaneGuns = false;
	}

	void Show::DropStrafe(Craft& C)
	{
		// (the reviewer, 25 November: a strafer shot down or gone mid-pass had kept its guns drawn and its claim on a
		// piece; its role is left as the show has made it, a fall or the way home)
		for (MarkHold& H : MarkHolds)
			if (!H.bDone && H.Claimed == C.Key && H.Due < 0.0) H.Claimed = 0;
		C.bLaneRun = false;
		C.LaneOf = -1;
		C.LaneHold = -1;
		C.bLaneGuns = false;
	}

	int Show::MarkStrikes() const
	{
		// (the reviewer, 25 November: every held hit had kept the strike on, the x1 clock with it, while a strafe hit
		// waited out a pass's lining up: 48.8 air s after the last bomb on the test's raid. A bomb hit only waiting for a
		// bomb holds nothing; one whose bomb falls is held by the bomb itself as well. A strafe hit holds it while the
		// strafer carrying it dives on its piece, and its rounds fly: not on his run in at height, where his lining up
		// may turn him back and forth a while, which had flicked the strike on and off and held it 40 s)
		int Count = 0;
		for (const MarkHold& H : MarkHolds)
		{
			if (H.bDone) continue;
			if (H.E.Strike != "strafe")
			{
				Count += H.Bomb > 0 ? 1 : 0;
				continue;
			}
			bool bGoing = H.Due >= 0.0;
			if (!bGoing && H.Claimed != 0)
			{
				const Craft* C = Find(H.Claimed);
				bGoing = C && C->bLaneRun && C->Doing == Role::Dive && C->Step == Phase::Plunge;
			}
			Count += bGoing ? 1 : 0;
		}
		return Count;
	}

	void Show::PilotStrafe(Craft& C, double Dt)
	{
		if (C.LaneOf < 0 || C.LaneOf >= static_cast<int>(Lanes.size()))
		{
			EndStrafe(C);
			return;
		}
		const StrafeRun& R = Lanes[static_cast<size_t>(C.LaneOf)];
		const AirMarks::Lane& L = R.L;
		const AirMarks::Pass& P = R.P;
		const double Way = L.Way();
		const double Long = L.Long();
		double U = 0.0, V = 0.0;
		L.Along(C.P.X, C.P.Y, U, V);
		const double DiveRad = P.Dive * Deg;
		// (its dive begins this far short of where its guns' walk does: from its machine's height at its dive's angle)
		const double Lead = P.High / std::tan(DiveRad);
		C.Task = "strafing";
		C.bLaneGuns = false;
		Check.StrafeSteps++;
		// A hit held for this pass, not yet carried: his stretch of the lane moved to begin a little short of its piece,
		// where he still has the room to line up on it.
		auto Claim = [this, &C, &R, U, Lead, Long]()
		{
			if (C.LaneHold >= 0) return;
			for (size_t K = 0; K < MarkHolds.size(); ++K)
			{
				MarkHold& H = MarkHolds[K];
				if (H.bDone || H.Claimed != 0 || H.Due >= 0.0 || H.E.Strike != "strafe" || H.E.Unit != R.Unit) continue;
				const AirMarks::Piece& Pc = Column.Pieces[static_cast<size_t>(H.Index)];
				double Up = 0.0, Vp = 0.0;
				R.L.Along(Pc.X, Pc.Y, Up, Vp);
				const double U0 = Clamp(Up - 150.0, -100.0, Long);
				if (U0 - Lead < U + 100.0) continue;
				H.Claimed = C.Key;
				C.LaneHold = static_cast<int>(K);
				C.LaneU0 = U0;
				C.LaneSide = Vp;
				return;
			}
		};
		// (its own limits on the pass, apart from the rest of the show's: Fly's own reckoning of each step taken)
		const double G0 = Check.MostG, R0 = Check.LeastRadius, Roll0 = Check.MostRoll;
		Check.MostG = 0.0;
		Check.LeastRadius = 1e9;
		Check.MostRoll = 0.0;
		switch (C.Step)
		{
		case Phase::Setup:
		{
			// Lining up: to the gate, back along the lane at his machine's height over the ground, one behind another
			// Gap seconds apart; or on down it at once, already lined up behind where his dive begins.
			const double Ug = C.LaneU0 - Lead - 1500.0 - C.LaneK * P.Gap * P.Speed;
			double Gx = 0.0, Gy = 0.0;
			L.At(Ug, C.LaneSide, Gx, Gy);
			const Vec Gate(Gx, Gy, GroundAt(Gx, Gy) + P.High);
			Cmd K = Toward(C, Gate, P.Speed + 10.0);
			K.Gam = Clamp(K.Gam, -30.0 * Deg, 20.0 * Deg);
			if (C.P.Z - GroundAt(C.P.X, C.P.Y) < 0.5 * P.High) K.Gam = std::max(K.Gam, 8.0 * Deg);
			Fly(C, K, Dt, true);
			const bool bLined = U < C.LaneU0 - Lead - 250.0 && U <= Ug + 700.0 && std::fabs(Wrap(C.Hdg - Way)) < 35.0 * Deg && std::fabs(V - C.LaneSide) < 250.0;
			if (bLined || Flat(Gate - C.P) < 450.0 || C.StepT > 120.0)
			{
				C.Step = Phase::Run;
				C.StepT = 0.0;
				Claim();
			}
			break;
		}
		case Phase::Run:
		{
			// Down the lane's line at his machine's height to where his dive begins, no nearer the one ahead of him than
			// most of the gap.
			Claim();
			const double Entry = C.LaneU0 - Lead;
			double Ax = 0.0, Ay = 0.0;
			L.At(std::max(U + 900.0, Entry), C.LaneSide, Ax, Ay);
			Cmd K = Toward(C, Vec(Ax, Ay, GroundAt(Ax, Ay) + P.High), P.Speed);
			K.Gam = Clamp(K.Gam, -20.0 * Deg, 15.0 * Deg);
			if (C.LaneK > 0)
			{
				const Craft* Ahead_ = Find(R.Keys[static_cast<size_t>(C.LaneK - 1)]);
				if (Ahead_ && Ahead_->bLaneRun && Ahead_->LaneOf == C.LaneOf && (Ahead_->Step == Phase::Run || Ahead_->Step == Phase::Plunge))
				{
					double Ua = 0.0, Va = 0.0;
					L.Along(Ahead_->P.X, Ahead_->P.Y, Ua, Va);
					if (Ua - U < 0.8 * P.Gap * P.Speed) K.Spd = std::max(C.VMin + 10.0, P.Speed - 25.0);
				}
			}
			Fly(C, K, Dt, true);
			if (U >= Entry)
			{
				C.Step = Phase::Plunge;
				C.StepT = 0.0;
			}
			else if (std::fabs(Wrap(C.Hdg - Way)) > 100.0 * Deg || C.StepT > 120.0)
			{
				C.Step = Phase::Setup;
				C.StepT = 0.0;
			}
			break;
		}
		case Phase::Plunge:
		{
			// Down at his dive's angle on to the lane; his guns from Open to Shut, the stream walked on down the lane as he
			// comes on (the nose eased up a little: WalkShare), on to the piece whose hit his pass carries.
			const double Opened = C.LaneU0 - P.Open * std::cos(DiveRad);
			const double Ua = std::min(Long + 30.0, C.LaneU0 + std::max(0.0, U - Opened) * WalkShare);
			double Ax = 0.0, Ay = 0.0;
			L.At(Ua, C.LaneSide, Ax, Ay);
			const Vec Aim(Ax, Ay, GroundAt(Ax, Ay));
			const Vec Rel = Aim - C.P;
			Cmd K;
			K.Hdg = std::atan2(Rel.X, Rel.Y);
			K.Gam = Clamp(std::atan2(Rel.Z, std::max(1.0, Flat(Rel))), -(P.Dive + 15.0) * Deg, 0.0);
			K.Spd = P.Speed + 25.0;
			Fly(C, K, Dt, true);
			const Vec Rel2 = Aim - C.P;
			const double Slant = Len(Rel2);
			const double Off = std::acos(Clamp(Ahead(C.Hdg, C.Gam).Dot(Norm(Rel2)), -1.0, 1.0));
			C.bLaneGuns = Slant <= P.Open && Slant >= P.Shut * 0.8 && Ua >= -30.0 && Ua <= Long + 30.0 && Off < 4.0 * Deg;
			// (out at the bottom LandAir's height: the pull-out's own room at his speed, angle and g, begun in time)
			const double Over = C.P.Z - GroundAt(C.P.X, C.P.Y);
			if (Over <= P.Low + AirMarks::PullRoom(C.Spd, C.Gam, P.PullG) || Slant < P.Shut * 0.8 || C.StepT > 40.0)
			{
				C.Step = Phase::Pull;
				C.StepT = 0.0;
				C.bLaneGuns = false;
			}
			break;
		}
		case Phase::Pull:
		{
			// Out of it at his machine's g, wings level, all of it into the pull.
			const double S = std::max(C.Spd, 30.0);
			const double Q = (P.PullG - std::cos(C.Gam)) * Gee / S;
			Cmd K;
			K.Hdg = C.Hdg;
			K.Gam = std::min(C.Gam + Q / 1.1, P.Climb * Deg);
			K.Spd = P.Speed + 20.0;
			K.bBank = true;
			K.Bank = 0.0;
			Fly(C, K, Dt, true);
			if (C.Gam >= P.Climb * Deg * 0.9 || C.StepT > 8.0)
			{
				C.Step = Phase::Egress;
				C.StepT = 0.0;
			}
			break;
		}
		case Phase::Egress:
		default:
		{
			// Away in his climb, straight on, then turned off the lane his own way; back to his place once well up.
			Cmd K;
			K.Hdg = C.StepT > 3.0 ? Way + C.Hand * 45.0 * Deg : C.Hdg;
			K.Gam = P.Climb * Deg;
			K.Spd = P.Speed;
			Fly(C, K, Dt, true);
			if (C.P.Z - GroundAt(C.P.X, C.P.Y) > std::max(700.0, P.High) || C.StepT > 25.0) EndStrafe(C);
			break;
		}
		}
		Check.StrafeMostG = std::max(Check.StrafeMostG, Check.MostG);
		Check.StrafeLeastRadius = std::min(Check.StrafeLeastRadius, Check.LeastRadius);
		Check.StrafeMostRoll = std::max(Check.StrafeMostRoll, Check.MostRoll);
		Check.MostG = std::max(G0, Check.MostG);
		Check.LeastRadius = std::min(R0, Check.LeastRadius);
		Check.MostRoll = std::max(Roll0, Check.MostRoll);
		if (C.bLaneRun && (C.Step == Phase::Plunge || C.Step == Phase::Pull || C.Step == Phase::Egress))
			Check.StrafeLowest = std::min(Check.StrafeLowest, C.P.Z - GroundAt(C.P.X, C.P.Y));
	}

	void Show::StrafeGuns(double Dt)
	{
		std::vector<int>& Kept = KeysNow;
		Kept.clear();
		for (const StrafeRun& R : Lanes)
		{
			for (int Key : R.Keys)
			{
				Craft* C = Find(Key);
				if (!C || !C->bLaneRun || !C->bLaneGuns || C->Doing != Role::Dive || !C->bShown) continue;
				const auto L = Loadouts.find(C->Nation + (C->bFighter ? ":ftr" : ":bmr"));
				if (L == Loadouts.end()) continue;
				const Vec Carry = Velocity(*C);
				const Vec Nose = Ahead(C->Hdg, C->Gam);
				// Where the stream comes down (his guns along his nose, harmonised on it): the hit his pass carries is
				// bound to these rounds as they come down on its piece (or any hit held for his group whose piece they
				// come down on), and shown as they land.
				Barrel Main;
				Main.At = Local(*C, Vec(C->Form.Long * 0.5, 0.0, 0.0));
				Main.Carry = Carry;
				const Gun* First = nullptr;
				for (const Gun& Row : L->second.Guns) if (Row.Mount == "fixed" && !First) First = &Row;
				if (!First) continue;
				Main.Mv = First->Mv;
				Main.Fl = FlightOf(First->Kind);
				const double MainTof = ToGround(Main, Nose);
				Vec Struck;
				int Allowed = -1;
				if (MainTof > 0.0)
				{
					Struck = RoundAt(Main.At, Nose * Main.Mv + Carry, MainTof, Main.Fl.Drag, Main.Fl.Drop);
					for (size_t K = 0; K < MarkHolds.size(); ++K)
					{
						MarkHold& H = MarkHolds[K];
						if (H.bDone || H.E.Strike != "strafe") continue;
						const AirMarks::Piece& Pc = Column.Pieces[static_cast<size_t>(H.Index)];
						const bool bMine = H.Claimed == C->Key || (H.Claimed == 0 && H.E.Unit == R.Unit);
						if (H.Due >= 0.0)
						{
							// (bound already: its rounds go on landing on it while the stream is on it, a second at most)
							if (H.Claimed == C->Key && std::fabs(H.Due - (AirT + MainTof)) <= 1.0 && AirMarks::Off(Pc, Struck.X, Struck.Y) <= 8.0)
								Allowed = H.Index;
							continue;
						}
						if (!bMine || AirMarks::Off(Pc, Struck.X, Struck.Y) > 4.0) continue;
						H.Due = AirT + MainTof;
						H.Claimed = C->Key;
						Allowed = H.Index;
						// (a round of it on the piece as it lands, whatever the guns' own count this frame)
						double Px = 0.0, Py = 0.0;
						AirMarks::AimOn(Column, H.Index, "wreck", 0.0, ++MarkSalt, Px, Py);
						Spurt Sp;
						Sp.At = H.Due;
						Sp.Where = Vec(Px, Py, GroundAt(Px, Py));
						Sp.In = Norm(Nose * Main.Mv + Carry, Nose);
						Sp.Kind = First->Kind;
						Spurts.push_back(Sp);
						Check.StrafeRounds++;
					}
				}
				int Index = 40;
				for (const Gun& Row : L->second.Guns)
				{
					if (Row.Mount != "fixed") continue;
					const int Of = static_cast<int>(Row.At.size());
					for (int K = 0; K < std::max(1, Of) && Index < 64; ++K)
					{
						const std::string Socket = Of > 0 ? Row.At[static_cast<size_t>(K)] : std::string("gun.nose");
						const int GunKey = Key * 64 + Index++;
						if (Has(L->second.Silent, Socket)) continue;
						Barrel B;
						B.At = Local(*C, MuzzleOf(C->Form, Socket, Row.Kind, K, Of));
						B.Carry = Carry;
						B.Mv = Row.Mv;
						B.Fl = FlightOf(Row.Kind);
						B.Own = Key;
						const Vec V0 = Nose * B.Mv + Carry;
						const double Tof = ToGround(B, Nose);
						const Vec End = Tof > 0.0 ? RoundAt(B.At, V0, Tof, B.Fl.Drag, B.Fl.Drop) : B.At + Nose * 1000.0;
						Fx& F = Emit(FxKind::Gun, B.At, Row.Kind, GunKey);
						F.Aim = End;
						F.Vel = Carry;
						F.Text = Row.Mount;
						F.Rate = Row.Rate;
						F.Mv = Row.Mv;
						F.Tof = Tof > 0.0 ? Tof : 0.0;
						F.Reach = Tof > 0.0 ? Len(End - B.At) : 0.0;
						Kept.push_back(GunKey);
						if (Tof <= 0.0) continue;
						// Every round of it strikes the ground, where it comes down and when (LandStray, Hit "ground"): off
						// every piece but the one whose counted hit these rounds carry.
						double& Owe = StrafeOwing[GunKey];
						Owe = std::min(Owe + (Row.Rate > 0.0 ? Row.Rate : 800.0) / 60.0 * Dt, 8.0);
						const Vec In = Norm(V0 * std::exp(-B.Fl.Drag * Tof) - Vec(0.0, 0.0, B.Fl.Drop * Tof), Nose);
						while (Owe >= 1.0)
						{
							Owe -= 1.0;
							const uint32_t H = AirMarks::Hash(static_cast<uint32_t>(GunKey), ++MarkSalt);
							const double Scatter = 0.006 * Len(End - B.At) + 1.0;
							double X = End.X + (AirMarks::UnitOf(H) - 0.5) * 2.0 * Scatter;
							double Y = End.Y + (AirMarks::UnitOf(AirMarks::Hash(H, 1)) - 0.5) * 2.0 * Scatter;
							bool bOnHit = false;
							if (Allowed >= 0 && AirMarks::Off(Column.Pieces[static_cast<size_t>(Allowed)], X, Y) <= 6.0)
							{
								// (on the piece whose hit they carry: on it)
								AirMarks::AimOn(Column, Allowed, "wreck", 0.0, H, X, Y);
								bOnHit = true;
							}
							else
							{
								double Cx = X, Cy = Y;
								if (!AirMarks::Clear(Column, X, Y, RoundClear) && AirMarks::ClearOf(Column, X, Y, RoundClear, Cx, Cy))
								{
									X = Cx;
									Y = Cy;
								}
							}
							if (!bOnHit && !AirMarks::Clear(Column, X, Y, 0.0)) Check.StrafeOnPieces++;
							Spurt Sp;
							Sp.At = AirT + Tof;
							Sp.Where = Vec(X, Y, GroundAt(X, Y));
							Sp.In = In;
							Sp.Kind = Row.Kind;
							Spurts.push_back(Sp);
							Check.StrafeRounds++;
						}
					}
				}
			}
		}
		for (int Key : StrafeFiring)
		{
			if (Has(Kept, Key)) continue;
			Emit(FxKind::GunStop, Vec(), std::string(), Key);
			StrafeOwing.erase(Key);
		}
		StrafeFiring.swap(Kept);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The column's light flak, and the frame.

	void Show::MarkGuns()
	{
		// Its light guns still standing, as many of them firing as the view says are manned (view.marks.fire), each on
		// the nearest raider in its reach, in bursts of its own (AirMarks::BurstAt), its stream laid short of the lead as
		// LandAir lays a ground gun's, so that the tracers fall behind. Drawn only: it never decides a loss.
		std::vector<int>& Kept = KeysNow;
		std::vector<size_t>& Guns = FlakNow;
		Kept.clear();
		Guns.clear();
		for (size_t K = 0; K < Column.Pieces.size(); ++K)
			if (Column.Pieces[K].What == "aa" && Column.Pieces[K].Now != AirMarks::State::Out) Guns.push_back(K);
		const size_t Manned = static_cast<size_t>(std::lround(MarkFire * static_cast<double>(Guns.size())));
		for (size_t N = 0; N < Manned && N < Guns.size(); ++N)
		{
			const AirMarks::Piece& P = Column.Pieces[Guns[N]];
			AirMarks::Burst B;
			if (!AirMarks::BurstAt(P.Id, AirT, B)) continue;
			const Vec Muzzle(P.X, P.Y, GroundAt(P.X, P.Y) + 2.0);
			const Craft* On = nullptr;
			double Best = AirMarks::FlakReach;
			for (const auto& Pair : Crafts)
			{
				const Craft& Q = Pair.second;
				if (Q.Side != 0 || !Live(Q) || !Q.bShown || Q.Doing == Role::Ground) continue;
				const double D = Len(Q.P - Muzzle);
				if (D < Best)
				{
					Best = D;
					On = &Q;
				}
			}
			if (!On) continue;
			Vec To = On->P + Velocity(*On) * (B.Lead * Best / AirMarks::TracerMps) + RightOf(On->Hdg) * B.Across + Vec(0.0, 0.0, B.Up);
			To.Z = std::max(To.Z, GroundAt(To.X, To.Y) + 12.0);
			const int Key = MarkGunBase + P.Id;
			Fx& F = Emit(FxKind::MarkGun, Muzzle, "cannon20", Key);
			F.Aim = To;
			F.Rate = 180.0;
			F.Mv = 880.0;
			F.Text = "column";
			Kept.push_back(Key);
		}
		for (int Key : MarkFiring) if (!Has(Kept, Key)) Emit(FxKind::GunStop, Vec(), std::string(), Key);
		if (!Kept.empty()) Check.MarkGunFrames++;
		MarkFiring.swap(Kept);
	}

	void Show::TroopsFrame(double Dt)
	{
		if (Dt <= 0.0)
		{
			for (int Key : StrafeFiring) Emit(FxKind::GunStop, Vec(), std::string(), Key);
			for (int Key : MarkFiring) Emit(FxKind::GunStop, Vec(), std::string(), Key);
			StrafeFiring.clear();
			MarkFiring.clear();
			StrafeOwing.clear();
			return;
		}
		// A strafer no longer flying its pass that did not end it (shot down and falling, gone home, taken into a fight:
		// its role is no longer the dive): let go of it, its guns and its claim with it. One gone from the show
		// altogether (StrafeGuns stops its guns) gives its claim back too, for the rest of its group's pass to carry.
		for (const StrafeRun& R : Lanes)
			for (int Key : R.Keys)
				if (Craft* C = Find(Key); C && C->bLaneRun && C->Doing != Role::Dive) DropStrafe(*C);
		for (MarkHold& H : MarkHolds)
			if (!H.bDone && H.Claimed != 0 && H.Due < 0.0 && !Find(H.Claimed)) H.Claimed = 0;
		// The held hits: a bomb hit bound to a bomb in the air (BindBomb; shown as it lands, BombDown), a strafe hit shown
		// as its rounds land on its piece (StrafeGuns), each shown with none when nothing came to carry it in its time,
		// or the battle's end is told.
		for (size_t K = 0; K < MarkHolds.size(); ++K)
		{
			MarkHold& H = MarkHolds[K];
			if (H.bDone) continue;
			if (H.E.Strike == "strafe")
			{
				if (H.Due >= 0.0 && AirT >= H.Due - 1e-9) ShowMark(H, 0, true);
				else if (bEnded || (H.Due < 0.0 && AirT - H.Since > StrafeWait)) ShowMark(H, 0, false);
				continue;
			}
			if (H.Bomb > 0)
			{
				bool bFalling = false;
				for (const Thing& T : Small) bFalling = bFalling || (T.Kind == ThingKind::Bomb && T.Serial == H.Bomb);
				if (!bFalling || bEnded) ShowMark(H, 0, false);
				continue;
			}
			if (!BindBomb(H, static_cast<int>(K)) && (bEnded || AirT - H.Since > BombWait)) ShowMark(H, 0, false);
		}
		// The views' words on the pieces, each from its battle time, after any hit on its piece still held. A piece that
		// is out never changes again: a word that would is refused.
		size_t Left = 0;
		for (size_t K = 0; K < MarkWords.size(); ++K)
		{
			const MarkWord W = MarkWords[K];
			const int Index = Column.IndexOf(W.C.Id);
			bool bWait = W.At > ShownT + 1e-9;
			if (!bWait && Index >= 0)
				for (const MarkHold& H : MarkHolds) bWait = bWait || (!H.bDone && H.Index == Index);
			if (bWait)
			{
				MarkWords[Left++] = W;
				continue;
			}
			if (Index < 0) continue;
			AirMarks::Piece& P = Column.Pieces[static_cast<size_t>(Index)];
			if (P.Now == AirMarks::State::Out && (W.C.To != AirMarks::State::Out || (W.C.Men >= 0 && W.C.Men != P.Men)))
			{
				Check.MarkChangesRefused++;
				continue;
			}
			AirMarks::Apply(P, W.C.To, W.C.Men);
		}
		MarkWords.resize(Left);
		// The views' organisation cut, each from its battle time once every hit told up to that time has been shown (so
		// the top bar never tells of a hit still in the air: AirDesk::Desk::Struck).
		Left = 0;
		for (size_t K = 0; K < MarkCuts.size(); ++K)
		{
			const MarkCut Cu = MarkCuts[K];
			bool bWait = Cu.At > ShownT + 1e-9;
			for (const MarkHold& H : MarkHolds) bWait = bWait || (!H.bDone && H.E.T <= Cu.At + 1e-9);
			if (bWait)
			{
				MarkCuts[Left++] = Cu;
				continue;
			}
			CutShown = std::max(CutShown, Cu.Cut);
		}
		MarkCuts.resize(Left);
		StrafeGuns(Dt);
		MarkGuns();
	}
}
