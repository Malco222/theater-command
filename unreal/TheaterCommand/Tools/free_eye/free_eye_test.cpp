// The free camera's cloud test: flies Battle/FreeEye through scripted keys, slides, zooms, turns, locks, skips and let
// goes, over a still frame, a raid's moving middle and a heaving sea, at 60 frames a second, 30, and on jittered frames
// with hitches, and holds it to every numbered rule of its specification. One line per scenario with the measured
// numbers, then a FAULT line for each rule broken; the exit status is the number of faults (capped).
#include "Battle/FreeEye.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using FreeEye::Vec;

namespace
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double Rad = Pi / 180.0;
	constexpr double Deg = 180.0 / Pi;
	constexpr double KickMost = 1200.0;

	int Faults = 0;

	void Fault(const char* Format, ...)
	{
		va_list Args;
		va_start(Args, Format);
		std::printf("FAULT: ");
		std::vprintf(Format, Args);
		std::printf("\n");
		va_end(Args);
		++Faults;
	}

	/** A small seeded generator of our own (no rand: the same run every time). */
	struct Lcg
	{
		uint64_t State = 1;
		explicit Lcg(uint64_t Seed) : State(Seed * 2654435761ULL + 12345ULL) {}
		double Next()
		{
			State = State * 6364136223846793005ULL + 1442695040888963407ULL;
			return static_cast<double>(State >> 11) * (1.0 / 9007199254740992.0);
		}
		double In(double Lo, double Hi) { return Lo + (Hi - Lo) * Next(); }
	};

	enum class Timing { F60, F30, Jitter };
	const char* TimingName(Timing K) { return K == Timing::F60 ? "60fps" : K == Timing::F30 ? "30fps" : "jitter"; }

	/** Frame times: steady 60 or 30, or jittered 8 to 33 ms with a hitch of 100 to 250 ms every second or two. */
	struct Clock
	{
		Timing Kind;
		Lcg R;
		double ToHitch = 1.3;
		Clock(Timing K, uint64_t Seed) : Kind(K), R(Seed) {}
		double Next()
		{
			if (Kind == Timing::F60) return 1.0 / 60.0;
			if (Kind == Timing::F30) return 1.0 / 30.0;
			double Dt = R.In(0.008, 0.033);
			ToHitch -= Dt;
			if (ToHitch <= 0.0)
			{
				ToHitch = R.In(0.8, 2.2);
				Dt = R.In(0.100, 0.250);
			}
			return Dt;
		}
	};

	struct Rec
	{
		double Time = 0.0, Dt = 0.0, Bt = 0.0;
		FreeEye::Pose P;
		Vec Pivot;
		double Arm = 0.0, Clear = 1e9;
		int Key = -1;
		bool bHard = false;
	};

	/** A scenario: what it feeds each frame, where each reference truly is (to measure the eye against its own
	 * reference's movement), the battle pace, and the world. */
	struct Script
	{
		std::function<void(FreeEye::Input&, FreeEye::Eye&, double Time, double Bt)> Feed;
		std::function<Vec(int Key, double Bt)> Where;
		std::function<double(double Time)> Pace;
		std::function<void(double Bt)> Tick;
		const FreeEye::World* Ground = nullptr;
	};

	struct Measured
	{
		double Kick = 0.0, KickAt = 0.0, Felt = 0.0, FeltAt = 0.0, Lowest = 1e9, LowestAt = 0.0;
		int Cuts = 0, HardLifts = 0, HardStops = 0, Frames = 0;
		bool bFinite = true;
		std::vector<Rec> Recs;
	};

	/** Flies a script for Seconds of real time on the given frames. */
	Measured Fly(FreeEye::Eye& E, const Script& Sc, Timing Kind, double Seconds, uint64_t Seed = 7)
	{
		Measured M;
		Clock C(Kind, Seed);
		double Time = 0.0, Bt = 0.0;
		const int Hard0 = E.HardLifts();
		const int Stops0 = E.HardStops();
		M.Recs.reserve(static_cast<size_t>(Seconds * 70.0) + 16);
		while (Time < Seconds)
		{
			const double Dt = C.Next();
			Time += Dt;
			Bt += Dt * (Sc.Pace ? Sc.Pace(Time) : 1.0);
			if (Sc.Tick) Sc.Tick(Bt);
			FreeEye::Input In;
			Sc.Feed(In, E, Time, Bt);
			const int HardWas = E.HardLifts();
			Rec R;
			R.P = E.Update(In, Dt, Sc.Ground);
			R.Time = Time;
			R.Dt = Dt;
			R.Bt = Bt;
			R.Pivot = E.Pivot();
			R.Arm = E.Arm();
			R.Key = E.Locked();
			R.bHard = E.HardLifts() != HardWas;
			const double Floor = Sc.Ground ? Sc.Ground->Floor(R.P.Eye.X, R.P.Eye.Y) : E.Set.FloorFlat;
			R.Clear = R.P.Eye.Z - Floor;
			if (!R.P.Eye.Finite() || !R.P.Look.Finite() || !std::isfinite(R.P.Fov)) M.bFinite = false;
			if (R.P.bCut) ++M.Cuts;
			if (R.Clear < M.Lowest)
			{
				M.Lowest = R.Clear;
				M.LowestAt = Time;
			}
			M.Recs.push_back(R);
		}
		M.HardLifts = E.HardLifts() - Hard0;
		M.HardStops = E.HardStops() - Stops0;
		M.Frames = static_cast<int>(M.Recs.size());
		return M;
	}

	/** The look's heading and tilt from the pose (the scene's own reading of it). */
	void LookOf(const FreeEye::Pose& P, double& OutH, double& OutT)
	{
		const Vec D = (P.Look - P.Eye).Unit();
		OutH = std::atan2(D.X, D.Y);
		OutT = std::asin(std::clamp(-D.Z, -1.0, 1.0));
	}

	/** The smoothness, by finite differences of the pose on real time: the look's kick (change of its angular velocity,
	 * a roll-free camera's: the heading's turn about the vertical and the tilt's about the view's right, per second),
	 * and the eye's acceleration against its reference's own movement, felt from max(Arm, FeelArm). A cut, and a frame
	 * the floor put clear at once, are left out (they are counted). Over a change of reference the eye is measured
	 * against whichever of the two it is nearer riding. */
	void Smoothness(Measured& M, const Script& Sc, double FeelArm, double From = 0.0)
	{
		const std::vector<Rec>& R = M.Recs;
		for (size_t I = 1; I + 1 < R.size(); ++I)
		{
			if (R[I].Time < From) continue;
			if (R[I].P.bCut || R[I + 1].P.bCut || R[I - 1].bHard || R[I].bHard || R[I + 1].bHard) continue;
			const double D1 = R[I].Dt, D2 = R[I + 1].Dt, Mid = 0.5 * (D1 + D2);
			// the look
			double H0, T0, H1, T1, H2, T2;
			LookOf(R[I - 1].P, H0, T0);
			LookOf(R[I].P, H1, T1);
			LookOf(R[I + 1].P, H2, T2);
			auto Omega = [](double Ha, double Ta, double Hb, double Tb, double Dt)
			{
				const double DH = std::remainder(Hb - Ha, 2.0 * Pi) / Dt, DT = (Tb - Ta) / Dt;
				const double Hm = Ha + 0.5 * std::remainder(Hb - Ha, 2.0 * Pi);
				return Vec{ std::cos(Hm) * DT, -std::sin(Hm) * DT, -DH };
			};
			const double Kick = (Omega(H1, T1, H2, T2, D2) - Omega(H0, T0, H1, T1, D1)).Length() / Mid * Deg;
			if (Kick > M.Kick)
			{
				M.Kick = Kick;
				M.KickAt = R[I].Time;
			}
			// the eye
			auto Accel = [&R, &Sc, I, D1, D2, Mid](int Key)
			{
				const Vec A0 = R[I - 1].P.Eye - Sc.Where(Key, R[I - 1].Bt);
				const Vec A1 = R[I].P.Eye - Sc.Where(Key, R[I].Bt);
				const Vec A2 = R[I + 1].P.Eye - Sc.Where(Key, R[I + 1].Bt);
				return ((A2 - A1) * (1.0 / D2) - (A1 - A0) * (1.0 / D1)).Length() / Mid;
			};
			double A = Accel(R[I].Key);
			if (R[I - 1].Key != R[I].Key) A = std::min(A, Accel(R[I - 1].Key));
			if (R[I + 1].Key != R[I].Key) A = std::min(A, Accel(R[I + 1].Key));
			const double Felt = A / std::max(R[I].Arm, FeelArm) * Deg;
			if (Felt > M.Felt)
			{
				M.Felt = Felt;
				M.FeltAt = R[I].Time;
			}
		}
	}

	void Report(const std::string& Name, const Measured& M, const char* Extra = "")
	{
		std::printf("%-44s kick %7.1f deg/s/s at %6.2fs  eye %7.1f deg/s/s at %6.2fs  lowest %8.2f m  cuts %d  hard lifts %d  hard stops %d%s%s\n",
			Name.c_str(), M.Kick, M.KickAt, M.Felt, M.FeltAt, M.Lowest, M.Cuts, M.HardLifts, M.HardStops, *Extra ? "  " : "", Extra);
	}

	void GateSmooth(const std::string& Name, const Measured& M)
	{
		if (M.Kick > KickMost) Fault("%s: the look kicked %.1f deg/s/s at %.2fs (most %.0f)", Name.c_str(), M.Kick, M.KickAt, KickMost);
		if (M.Felt > KickMost) Fault("%s: the eye pushed %.1f deg/s/s at %.2fs (most %.0f)", Name.c_str(), M.Felt, M.FeltAt, KickMost);
		if (!M.bFinite) Fault("%s: a pose not finite", Name.c_str());
	}

	bool Same(const FreeEye::Pose& A, const FreeEye::Pose& B)
	{
		return std::memcmp(&A.Eye, &B.Eye, sizeof(Vec)) == 0 && std::memcmp(&A.Look, &B.Look, sizeof(Vec)) == 0 &&
			std::memcmp(&A.Fov, &B.Fov, sizeof(double)) == 0 && A.bCut == B.bCut;
	}

	const Timing Timings[] = { Timing::F60, Timing::F30, Timing::Jitter };

	Vec StillAt(int, double) { return Vec{}; }

	/** The camera as the director left it: some way back and up from the origin. */
	void BeginAt(FreeEye::Eye& E, double Zoom, const Vec& Frame = Vec{})
	{
		const double Arm = E.ArmOf(Zoom);
		const double Tl = 30.0 * Rad;
		const Vec Look = Frame + Vec{ 0.0, 0.0, 50.0 };
		E.Begin(Look + Vec{ 0.0, -Arm * std::cos(Tl), Arm * std::sin(Tl) }, Look, Frame);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rule 1: rest is rest.
	void RuleRest()
	{
		{
			FreeEye::Eye E;
			BeginAt(E, 0.5);
			FreeEye::Input In;
			const FreeEye::Pose First = E.Update(In, 1.0 / 60.0, nullptr);
			int Moved = 0;
			for (int K = 0; K < 60 * 3600; ++K)
			{
				if (!Same(E.Update(In, 1.0 / 60.0, nullptr), First)) ++Moved;
			}
			std::printf("%-44s frames that moved %d of %d\n", "rule 1 rest, an hour still", Moved, 60 * 3600);
			if (Moved) Fault("rule 1: at rest the pose changed on %d frames of an hour", Moved);
		}
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			auto Raid = [](int, double Bt) { return Vec{ 120.0 * Bt, 30.0 * Bt, 6000.0 + 40.0 * std::sin(0.2 * Bt) }; };
			BeginAt(E, 0.6, Raid(-1, 0.0));
			Script Sc;
			Sc.Where = Raid;
			Sc.Feed = [&Raid](FreeEye::Input& In, FreeEye::Eye&, double, double Bt) { In.Frame = Raid(-1, Bt); };
			const Vec Rel0 = E.Rel();
			Measured M = Fly(E, Sc, K, 600.0);
			double RelOff = 0.0, EyeOff = 0.0;
			const Vec Off0 = M.Recs.front().P.Eye - Raid(-1, M.Recs.front().Bt);
			for (const Rec& R : M.Recs)
			{
				EyeOff = std::max(EyeOff, ((R.P.Eye - Raid(-1, R.Bt)) - Off0).Length());
			}
			RelOff = (E.Rel() - Rel0).Length();
			char Line[160];
			std::snprintf(Line, sizeof(Line), "Rel moved %.3g m, eye off the frame's movement %.3g m", RelOff, EyeOff);
			Smoothness(M, Sc, E.Set.FeelArm);
			Report(std::string("rule 1 rest on a moving raid ") + TimingName(K), M, Line);
			if (RelOff > 1e-9) Fault("rule 1 %s: Rel moved %.3g m with no input", TimingName(K), RelOff);
			if (EyeOff > 1e-9) Fault("rule 1 %s: the eye left its reference's movement by %.3g m", TimingName(K), EyeOff);
		}
		{
			// after a move, back to rest to the last bit
			FreeEye::Eye E;
			BeginAt(E, 0.5);
			FreeEye::Input In;
			In.PanRight = 1.0;
			In.Turn = 1.0;
			In.Zoom = 0.05;
			In.Tilt = 0.3;
			for (int K = 0; K < 120; ++K) E.Update(In, 1.0 / 60.0, nullptr);
			In = FreeEye::Input();
			// (the springs' tails are let go of once they are under a hundredth of a nanometre: by 4 s he is at rest to
			// the last bit, not creeping in the last digits for seconds more)
			for (int K = 0; K < 4 * 60; ++K) E.Update(In, 1.0 / 60.0, nullptr);
			const FreeEye::Pose A = E.Update(In, 1.0 / 60.0, nullptr);
			int Moved = 0;
			for (int K = 0; K < 600; ++K) Moved += Same(E.Update(In, 1.0 / 60.0, nullptr), A) ? 0 : 1;
			std::printf("%-44s frames that moved %d of 600, from 4 s after\n", "rule 1 rest after a move", Moved);
			if (Moved) Fault("rule 1: 4 s after a move the view still crept on %d frames", Moved);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rule 2: every move by his hand is eased (and rules 10's soft caps met in passing).
	struct Segment
	{
		double From, To;
		std::function<void(FreeEye::Input&, FreeEye::Eye&, double Dt)> Do;
	};

	void HandScript(std::vector<Segment>& Out)
	{
		Out.push_back({ 0.5, 3.0, [](FreeEye::Input& In, FreeEye::Eye&, double) { In.PanRight = 1.0; } });
		Out.push_back({ 4.0, 6.0, [](FreeEye::Input& In, FreeEye::Eye&, double) { In.PanAhead = -1.0; } });
		Out.push_back({ 7.0, 9.0, [](FreeEye::Input& In, FreeEye::Eye&, double) { In.Rise = 1.0; } });
		Out.push_back({ 9.0, 9.6, [](FreeEye::Input& In, FreeEye::Eye&, double) { In.Rise = -1.0; } });
		Out.push_back({ 10.0, 13.0, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.Turn = 160.0 * Dt; } });
		Out.push_back({ 13.0, 13.3, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.Turn = -300.0 * Dt; } });
		Out.push_back({ 14.0, 16.0, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.Tilt = 60.0 * Dt; } });
		Out.push_back({ 16.0, 17.5, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.Tilt = -90.0 * Dt; } });
		Out.push_back({ 18.0, 19.0, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.Zoom = 12.0 * Dt; } });
		Out.push_back({ 20.0, 21.5, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.Zoom = -15.0 * Dt; } });
		// everything at once
		Out.push_back({ 23.0, 27.0, [](FreeEye::Input& In, FreeEye::Eye&, double Dt)
			{ In.PanRight = 1.0; In.PanAhead = 1.0; In.Rise = 0.5; In.Turn = 150.0 * Dt; In.Tilt = 50.0 * Dt; In.Zoom = 8.0 * Dt; } });
		Out.push_back({ 27.0, 29.0, [](FreeEye::Input& In, FreeEye::Eye&, double Dt)
			{ In.PanRight = -1.0; In.Turn = -150.0 * Dt; In.Tilt = -50.0 * Dt; In.Zoom = -8.0 * Dt; } });
		// strafe: a pan with a turn the other way
		Out.push_back({ 30.0, 33.0, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.PanRight = 1.0; In.Turn = -150.0 * Dt; } });
		// slides: one big flick, then a run of small ones
		Out.push_back({ 34.0, 34.0001, [](FreeEye::Input& In, FreeEye::Eye&, double) { In.SlideRight = 4.0; In.SlideAhead = -4.0; } });
		Out.push_back({ 36.0, 37.0, [](FreeEye::Input& In, FreeEye::Eye&, double Dt) { In.SlideRight = 0.8 * Dt; In.SlideAhead = 0.5 * Dt; } });
		// a wheel's flick of 40 notches in one frame
		Out.push_back({ 38.0, 38.0001, [](FreeEye::Input& In, FreeEye::Eye&, double) { In.Zoom = -40.0; } });
		Out.push_back({ 41.0, 41.0001, [](FreeEye::Input& In, FreeEye::Eye&, double) { In.Zoom = 40.0; } });
		// presets: the top view and back level
		Out.push_back({ 44.0, 44.0001, [](FreeEye::Input&, FreeEye::Eye& E, double) { E.Want(E.Heading() + Pi, 89.0 * Rad, 0.9); } });
		Out.push_back({ 50.0, 50.0001, [](FreeEye::Input&, FreeEye::Eye& E, double) { E.Want(E.Heading(), -60.0 * Rad, 0.1); } });
	}

	void RuleEased()
	{
		std::vector<Segment> Segs;
		HandScript(Segs);
		const double Zooms[] = { 0.0, 0.5, 0.95 };
		for (Timing K : Timings)
		{
			for (double Z0 : Zooms)
			{
				for (int Moving = 0; Moving < 2; ++Moving)
				{
					FreeEye::Eye E;
					auto Ref = [Moving](int, double Bt) { return Moving ? Vec{ 140.0 * Bt, -40.0 * Bt, 3000.0 + 2.0 * Bt } : Vec{ 0.0, 0.0, 3000.0 }; };
					BeginAt(E, Z0, Ref(-1, 0.0));
					Script Sc;
					Sc.Where = Ref;
					double Last = 0.0;
					Sc.Feed = [&Segs, &Ref, &Last](FreeEye::Input& In, FreeEye::Eye& Ey, double Time, double Bt)
					{
						const double Dt = Time - Last;
						Last = Time;
						In.Frame = Ref(-1, Bt);
						for (const Segment& G : Segs)
						{
							// (a one-frame segment fires on the first frame at or after its time)
							const bool bOnce = G.To - G.From < 0.001;
							if (bOnce ? (Time >= G.From && Time - Dt < G.From) : (Time >= G.From && Time < G.To)) G.Do(In, Ey, Dt);
						}
					};
					Measured M = Fly(E, Sc, K, 56.0);
					Smoothness(M, Sc, E.Set.FeelArm);
					// the fastest swings, from the poses: the turn and the tilt of the look, and the zoom from the boom
					double TurnMost = 0.0, TiltMost = 0.0, ZoomMost = 0.0;
					for (size_t I = 1; I < M.Recs.size(); ++I)
					{
						double Ha, Ta, Hb, Tb;
						LookOf(M.Recs[I - 1].P, Ha, Ta);
						LookOf(M.Recs[I].P, Hb, Tb);
						const double Dt = M.Recs[I].Dt;
						TurnMost = std::max(TurnMost, std::abs(std::remainder(Hb - Ha, 2.0 * Pi)) / Dt * Deg);
						TiltMost = std::max(TiltMost, std::abs(Tb - Ta) / Dt * Deg);
						ZoomMost = std::max(ZoomMost, std::abs(E.ZoomOf(M.Recs[I].Arm) - E.ZoomOf(M.Recs[I - 1].Arm)) / Dt);
					}
					char Name[96];
					std::snprintf(Name, sizeof(Name), "rule 2 hand, zoom %.2f%s %s", Z0, Moving ? " moving" : "", TimingName(K));
					char Line[120];
					std::snprintf(Line, sizeof(Line), "fastest turn %.1f deg/s, tilt %.1f deg/s, zoom %.3f /s", TurnMost, TiltMost, ZoomMost);
					Report(Name, M, Line);
					GateSmooth(Name, M);
					if (TurnMost > E.Set.TurnFastest * 1.01) Fault("%s: the turn swung %.1f deg/s (fastest %.0f)", Name, TurnMost, E.Set.TurnFastest);
					// (and a key held long reaches each fastest: a hold slower than asked is a fault too)
					if (TurnMost < E.Set.TurnFastest * 0.99 || TiltMost < E.Set.TiltFastest * 0.99 || ZoomMost < E.Set.ZoomFastest * 0.99)
					{
						Fault("%s: a key held never reached its fastest (turn %.1f of %.0f, tilt %.1f of %.0f, zoom %.3f of %.2f)", Name, TurnMost, E.Set.TurnFastest, TiltMost, E.Set.TiltFastest, ZoomMost, E.Set.ZoomFastest);
					}
					if (TiltMost > E.Set.TiltFastest * 1.01) Fault("%s: the tilt swung %.1f deg/s (fastest %.0f)", Name, TiltMost, E.Set.TiltFastest);
					if (ZoomMost > E.Set.ZoomFastest * 1.01) Fault("%s: the zoom swung %.3f /s (fastest %.2f)", Name, ZoomMost, E.Set.ZoomFastest);
					if (M.Cuts) Fault("%s: %d cuts from his hand alone", Name, M.Cuts);
					if (M.HardStops) Fault("%s: %d hard stops at a cap from his hand alone (the caps must be met softly)", Name, M.HardStops);
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rules 3, 5 and 6: lock on, carried or cut; let go; teleport.
	/** Straight flights at a constant speed: their own acceleration is none, so the eye's is all the core's. */
	Vec Flier(int Key, double Bt)
	{
		switch (Key)
		{
		case 7: return Vec{ 300.0, 400.0 + 150.0 * Bt, 200.0 };
		case 8: return Vec{ 900.0 - 60.0 * Bt, 100.0, 450.0 + 3.0 * Bt };
		case 9: return Vec{ 5200.0, 0.0, 500.0 };
		default: return Vec{};
		}
	}

	void RuleLock()
	{
		for (Timing K : Timings)
		{
			for (int Fed = 0; Fed < 2; ++Fed)
			{
				FreeEye::Eye E;
				BeginAt(E, 0.62);
				Script Sc;
				Sc.Where = Flier;
				int LockFrame = -1, Frame = 0;
				Sc.Feed = [Fed, &LockFrame, &Frame](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
				{
					++Frame;
					if (Time >= 2.0 && Time < 12.0)
					{
						In.bLock = true;
						In.AnchorId = 7;
						if (LockFrame < 0) LockFrame = Frame - 1;
					}
					else if (Time >= 12.0 && Time < 22.0)
					{
						In.bLock = true;
						In.AnchorId = 8;
					}
					if (In.bLock)
					{
						In.Anchor = Flier(In.AnchorId, Bt);
						In.bAnchorVel = Fed != 0;
						In.AnchorVel = In.AnchorId == 7 ? Vec{ 0.0, 150.0, 0.0 } : Vec{ -60.0, 0.0, 3.0 };
					}
				};
				Measured M = Fly(E, Sc, K, 34.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				// the pivot's world speed over each change, against what the push allows
				double Jump = 0.0;
				for (size_t I = 2; I + 1 < M.Recs.size(); ++I)
				{
					if (M.Recs[I].Key == M.Recs[I - 1].Key) continue;
					const Vec V0 = (M.Recs[I - 1].Pivot - M.Recs[I - 2].Pivot) * (1.0 / M.Recs[I - 1].Dt);
					const Vec V1 = (M.Recs[I + 1].Pivot - M.Recs[I].Pivot) * (1.0 / M.Recs[I + 1].Dt);
					const double Allowed = E.Set.GlidePush * std::max(M.Recs[I].Arm, E.Set.FeelArm) * (M.Recs[I].Dt + M.Recs[I + 1].Dt) + 1e-6;
					Jump = std::max(Jump, (V1 - V0).Length() / Allowed);
				}
				// at rest on anchor 8, then in the frame
				const Rec& At21 = *std::find_if(M.Recs.begin(), M.Recs.end(), [](const Rec& R) { return R.Time >= 21.9; });
				const double OffAt21 = (At21.Pivot - Flier(8, At21.Bt)).Length();
				const Rec& End = M.Recs.back();
				const Rec& Before = M.Recs[M.Recs.size() - 2];
				const double Drift = (End.Pivot - Before.Pivot).Length() / End.Dt;
				// let go, the view coasts to rest where its own speed takes it: it never turns back in the frame
				double Back = 0.0;
				Vec First{};
				for (size_t I = 1; I < M.Recs.size(); ++I)
				{
					const Rec& R = M.Recs[I];
					if (R.Key != -1 || R.Time < 22.0) continue;
					const Vec V = (R.Pivot - M.Recs[I - 1].Pivot) * (1.0 / R.Dt);
					if (First.Length() == 0.0) First = V.Unit();
					Back = std::max(Back, -(V.X * First.X + V.Y * First.Y + V.Z * First.Z));
				}
				char Line[240];
				std::snprintf(Line, sizeof(Line), "speed change over a carry %.3f of the push's  on 8 off %.3f m  let go drifts %.4f m/s, turns back %.4f m/s", Jump, OffAt21, Drift, Back);
				char Name[96];
				std::snprintf(Name, sizeof(Name), "rule 3/5 lock, skip, let go%s %s", Fed ? " fed" : " measured", TimingName(K));
				Report(Name, M, Line);
				GateSmooth(Name, M);
				if (M.Cuts) Fault("%s: %d cuts (the anchors are within CutBeyond)", Name, M.Cuts);
				if (Jump > 1.0) Fault("%s: the pivot's world speed changed over a carry by %.2f times what the push allows", Name, Jump);
				if (OffAt21 > 0.05) Fault("%s: 10 s after the skip the pivot is %.3f m off anchor 8 (it should have glided onto it)", Name, OffAt21);
				if (Back > 0.01) Fault("%s: let go, the view turned back in the frame at %.3f m/s (it is to coast to rest)", Name, Back);
				if (Drift > 0.01) Fault("%s: 12 s after the let go the view still drifts %.4f m/s in the frame", Name, Drift);
				if (End.Key != -1) Fault("%s: still locked on %d after the lock fell", Name, End.Key);
			}
		}
		// the cut: too far to glide
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			BeginAt(E, 0.62);
			FreeEye::Input In;
			Clock C(K, 3);
			int CutFrames = 0, CutFrame = -1, Frames = 0;
			bool bZeroAtCut = false;
			for (double Time = 0.0; Time < 6.0;)
			{
				const double Dt = C.Next();
				Time += Dt;
				In = FreeEye::Input();
				if (Time > 1.0)
				{
					In.bLock = true;
					In.AnchorId = 9;
					In.Anchor = Flier(9, Time);
				}
				const FreeEye::Pose P = E.Update(In, Dt, nullptr);
				if (P.bCut)
				{
					++CutFrames;
					CutFrame = Frames;
					bZeroAtCut = E.Rel().Length() == 0.0 && E.WantRel().Length() == 0.0 && E.RelSwing().Length() == 0.0;
				}
				++Frames;
			}
			std::printf("%-44s cut frames %d (frame %d), Rel and its swing zero there %s\n", (std::string("rule 3 lock 5.2 km off, a cut ") + TimingName(K)).c_str(),
				CutFrames, CutFrame, bZeroAtCut ? "yes" : "no");
			if (CutFrames != 1) Fault("rule 3 %s: a lock beyond CutBeyond cut on %d frames (exactly one wanted)", TimingName(K), CutFrames);
			if (!bZeroAtCut) Fault("rule 3 %s: at the cut Rel, WantRel and the swing are not zero", TimingName(K));
		}
		// the same lock just inside CutBeyond is carried
		{
			FreeEye::Eye E;
			BeginAt(E, 0.62);
			FreeEye::Input In;
			int Cuts = 0;
			for (int F = 0; F < 600; ++F)
			{
				In = FreeEye::Input();
				In.bLock = F > 60;
				In.AnchorId = 9;
				In.Anchor = Vec{ 2900.0, 0.0, 50.0 };
				Cuts += E.Update(In, 1.0 / 60.0, nullptr).bCut ? 1 : 0;
			}
			std::printf("%-44s cuts %d, pivot off the anchor %.3f m after 9 s\n", "rule 3 lock 2.9 km off, carried", Cuts, (E.Pivot() - In.Anchor).Length());
			if (Cuts) Fault("rule 3: a lock 2.9 km off (within CutBeyond 3000) cut");
			if ((E.Pivot() - In.Anchor).Length() > 0.5) Fault("rule 3: a lock 2.9 km off had not glided onto it in 9 s");
		}
		// teleport: the same id jumping, and a fast one not jumping over a hitch
		{
			FreeEye::Eye E;
			BeginAt(E, 0.5);
			FreeEye::Input In;
			int Cuts = 0, CutsFast = 0;
			for (int F = 0; F < 400; ++F)
			{
				In = FreeEye::Input();
				In.bLock = true;
				In.AnchorId = 3;
				In.Anchor = Vec{ F < 200 ? 0.0 : 1000.0, 0.0, 100.0 };
				Cuts += E.Update(In, 1.0 / 60.0, nullptr).bCut ? 1 : 0;
			}
			const double Speed = 1800.0;  // 180 m/s at pace 10
			double X = 0.0;
			for (int F = 0; F < 400; ++F)
			{
				const double Dt = F % 50 == 49 ? 0.25 : 1.0 / 60.0;
				X += Speed * Dt;
				In = FreeEye::Input();
				In.bLock = true;
				In.AnchorId = 4;
				In.Anchor = Vec{ X, 0.0, 3000.0 };
				const bool bCut = E.Update(In, Dt, nullptr).bCut;
				if (F > 0) CutsFast += bCut ? 1 : 0;
			}
			std::printf("%-44s a 1 km jump cut %d times; 1800 m/s over hitches cut %d times\n", "rule 6 teleport", Cuts, CutsFast);
			// (the first lock on anchor 3 is carried: the pivot starts within CutBeyond)
			if (Cuts != 1) Fault("rule 6: a 1 km jump of the same anchor cut %d times (once wanted)", Cuts);
			if (CutsFast) Fault("rule 6: an anchor at 1800 m/s over hitches was taken for a teleport %d times", CutsFast);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rule 4: pan and rise while locked.
	void RulePanLock()
	{
		for (int Keep = 0; Keep < 2; ++Keep)
		{
			FreeEye::Eye E;
			E.Set.bPanKeepsLock = Keep != 0;
			BeginAt(E, 0.45);
			auto Ship = [](int, double Bt) { return Vec{ 0.0, 15.0 * Bt, 5.0 }; };
			Script Sc;
			Sc.Where = [&Ship](int Key, double Bt) { return Key < 0 ? Vec{} : Ship(Key, Bt); };
			int LockedAfterTurn = -1, LockedAfterPan = -1;
			Sc.Feed = [&](FreeEye::Input& In, FreeEye::Eye& Ey, double Time, double Bt)
			{
				In.bLock = true;
				In.AnchorId = 5;
				In.Anchor = Ship(5, Bt);
				In.bAnchorVel = true;
				In.AnchorVel = Vec{ 0.0, 15.0, 0.0 };
				if (Time > 3.0 && Time < 5.0)
				{
					In.Turn = 1.0;
					In.Tilt = 0.3;
					In.Zoom = 0.05;
				}
				if (Time >= 5.5 && Time < 5.6) LockedAfterTurn = Ey.Locked();
				if (Time > 6.0 && Time < 8.0) In.PanRight = 1.0;
				if (Time > 8.0 && Time < 9.0) In.Rise = 1.0;
				if (Time >= 9.5 && Time < 9.6) LockedAfterPan = Ey.Locked();
			};
			Measured M = Fly(E, Sc, Timing::Jitter, 16.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			char Line[160];
			std::snprintf(Line, sizeof(Line), "locked after turn %d, after pan %d, his offset %.1f m", LockedAfterTurn, LockedAfterPan, E.WantRel().Length());
			const std::string Name = Keep ? "rule 4 pan keeps the lock (air, sea)" : "rule 4 pan lets go (ground style)";
			Report(Name, M, Line);
			GateSmooth(Name, M);
			if (LockedAfterTurn != 5) Fault("%s: zoom, turn or tilt let go of the lock", Name.c_str());
			if (Keep && LockedAfterPan != 5) Fault("%s: the pan let go though bPanKeepsLock", Name.c_str());
			if (Keep && E.WantRel().Length() < 10.0) Fault("%s: the pan did not move him inside the anchor's frame", Name.c_str());
			if (!Keep && LockedAfterPan != -1) Fault("%s: a pan without bPanKeepsLock did not let go", Name.c_str());
			if (!Keep && E.Locked() != -1) Fault("%s: the lock took again while bLock stayed held after the pan let go", Name.c_str());
			if (M.Cuts) Fault("%s: %d cuts", Name.c_str(), M.Cuts);
		}
	}

	/** Rule 5's let go by bLetGo while the scene goes on sending bLock: it lets go, and the lock takes again only on a
	 * new rise or a new id. */
	void RuleLetGo()
	{
		FreeEye::Eye E;
		BeginAt(E, 0.5);
		FreeEye::Input In;
		int AfterPulse = 0, Cuts = 0, AfterNewId = -1;
		for (int F = 0; F < 900; ++F)
		{
			In = FreeEye::Input();
			In.bLock = true;
			In.AnchorId = F < 600 ? 6 : 7;
			In.Anchor = Vec{ 100.0, 15.0 * F / 60.0, 5.0 };
			In.bLetGo = F == 300;
			Cuts += E.Update(In, 1.0 / 60.0, nullptr).bCut ? 1 : 0;
			if (F > 300 && F < 600 && E.Locked() != -1) ++AfterPulse;
			if (F == 620) AfterNewId = E.Locked();
		}
		std::printf("%-44s frames locked again after the let go %d, after a new id locked on %d, cuts %d\n", "rule 5 bLetGo with bLock still held", AfterPulse, AfterNewId, Cuts);
		if (AfterPulse) Fault("rule 5: after bLetGo the lock took again on %d frames with no new rise", AfterPulse);
		if (AfterNewId != 7) Fault("rule 5: a new id after a let go did not lock (%d)", AfterNewId);
		if (Cuts) Fault("rule 5: a let go or a near lock cut");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rule 7: ride; rule 8: steady height (a ship).
	double ShipHeading(double Bt)
	{
		// 0 for 3 s, then a smooth turn of 90 degrees over 20 s
		const double U = std::clamp((Bt - 3.0) / 20.0, 0.0, 1.0);
		return (U * U * (3.0 - 2.0 * U)) * 0.5 * Pi;
	}

	Vec ShipAt(double Bt, double Heave)
	{
		// integrate the turning course at 15 m/s (fine steps, the same every call)
		double X = 0.0, Y = 0.0;
		const int Steps = static_cast<int>(Bt * 20.0);
		const double Step = Steps > 0 ? Bt / Steps : 0.0;
		for (int K = 0; K < Steps; ++K)
		{
			const double Hm = ShipHeading((K + 0.5) * Step);
			X += 15.0 * std::sin(Hm) * Step;
			Y += 15.0 * std::cos(Hm) * Step;
		}
		return Vec{ X, Y, 8.0 + Heave * std::sin(2.0 * Pi * Bt / 8.0) };
	}

	void RuleRide()
	{
		for (int Ride = 0; Ride < 2; ++Ride)
		{
			FreeEye::Eye E;
			BeginAt(E, 0.5);
			Script Sc;
			Sc.Where = [](int Key, double Bt) { return Key < 0 ? Vec{} : ShipAt(Bt, 0.0); };
			double Worst = 0.0, H0 = 0.0, Off0 = 0.0;
			Sc.Feed = [Ride](FreeEye::Input& In, FreeEye::Eye&, double, double Bt)
			{
				In.bLock = true;
				In.AnchorId = 2;
				In.Anchor = ShipAt(Bt, 0.0);
				In.AnchorHeading = ShipHeading(Bt);
				In.bRide = Ride != 0;
			};
			Measured M = Fly(E, Sc, Timing::Jitter, 30.0);
			Smoothness(M, Sc, E.Set.FeelArm, 3.0);
			for (size_t I = 0; I < M.Recs.size(); ++I)
			{
				double Hl, Tl;
				LookOf(M.Recs[I].P, Hl, Tl);
				const double Off = std::remainder(Hl - (Ride ? ShipHeading(M.Recs[I].Bt) : 0.0), 2.0 * Pi);
				if (I == 0)
				{
					Off0 = Off;
					H0 = Hl;
				}
				Worst = std::max(Worst, std::abs(std::remainder(Off - Off0, 2.0 * Pi)));
			}
			double Hl, Tl;
			LookOf(M.Recs.back().P, Hl, Tl);
			char Line[160];
			std::snprintf(Line, sizeof(Line), "view heading turned %.4f deg, off the ship's %.2e deg", std::remainder(Hl - H0, 2.0 * Pi) * Deg, Worst * Deg);
			const std::string Name = Ride ? "rule 7 ride on, the ship turns 90" : "rule 7 ride off, the ship turns 90";
			Report(Name, M, Line);
			GateSmooth(Name, M);
			if (Worst > 1e-6) Fault("%s: the view's heading left %s by %.3g deg", Name.c_str(), Ride ? "the ship's" : "his own", Worst * Deg);
		}
		for (int Steady = 0; Steady < 2; ++Steady)
		{
			FreeEye::Eye E;
			E.Set.SteadyZ = Steady ? 3.0 : 0.0;
			BeginAt(E, 0.4);
			Script Sc;
			Sc.Where = [](int Key, double Bt) { return Key < 0 ? Vec{} : ShipAt(Bt, 3.0); };
			Sc.Feed = [](FreeEye::Input& In, FreeEye::Eye&, double, double Bt)
			{
				In.bLock = true;
				In.AnchorId = 2;
				In.Anchor = ShipAt(Bt, 3.0);
			};
			Measured M = Fly(E, Sc, Timing::F60, 60.0);
			double Lo = 1e9, Hi = -1e9, Flat = 0.0;
			for (const Rec& R : M.Recs)
			{
				if (R.Time < 30.0) continue;
				const Vec A = ShipAt(R.Bt, 3.0);
				Lo = std::min(Lo, R.Pivot.Z - 8.0);
				Hi = std::max(Hi, R.Pivot.Z - 8.0);
				Flat = std::max(Flat, std::hypot(R.Pivot.X - A.X, R.Pivot.Y - A.Y));
			}
			char Line[160];
			std::snprintf(Line, sizeof(Line), "pivot heaves %.2f m of the ship's 6.00, off her in X,Y %.2e m", Hi - Lo, Flat);
			const std::string Name = Steady ? "rule 8 steady height 3 s" : "rule 8 steady height off";
			Report(Name, M, Line);
			if (Flat > 1e-6) Fault("%s: X and Y not followed exactly (%.3g m off)", Name.c_str(), Flat);
			// (a first-order low-pass of time constant SteadyZ passes 1 / sqrt(1 + (2 pi SteadyZ / 8)^2) of an 8 s swell)
			const double Gain = 6.0 / std::sqrt(1.0 + std::pow(2.0 * Pi * 3.0 / 8.0, 2.0));
			if (Steady && std::abs(Hi - Lo - Gain) > 0.05 * Gain) Fault("%s: the pivot heaves %.2f m of the ship's 6 (a low-pass of 3 s passes %.2f)", Name.c_str(), Hi - Lo, Gain);
			if (!Steady && std::abs(Hi - Lo - 6.0) > 0.05) Fault("%s: without it the pivot should heave with the ship (%.2f m)", Name.c_str(), Hi - Lo);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rule 9: the floor.
	struct Waves : FreeEye::World
	{
		double Bt = 0.0, Height = 12.0;
		double Floor(double X, double Y) const override
		{
			// a swell 12 m from trough to crest, 120 m long, running at about 14 m/s; a cross sea on it
			const double K = 2.0 * Pi / 120.0;
			return 0.5 * Height * (1.0 + 0.8 * std::sin(K * (0.8 * X + 0.6 * Y) - 0.72 * Bt) + 0.2 * std::sin(K * 2.3 * (0.3 * X - 0.95 * Y) - 1.1 * Bt));
		}
	};

	struct Cliff : FreeEye::World
	{
		double Floor(double X, double) const override { return X > 80.0 ? 60.0 : 0.0; }
	};

	void RuleFloor()
	{
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			Waves Sea;
			E.Begin(Vec{ 0.0, -20.0, 25.0 }, Vec{ 0.0, 0.0, 20.0 }, Vec{});
			Script Sc;
			Sc.Where = StillAt;
			Sc.Ground = &Sea;
			Sc.Tick = [&Sea](double Bt) { Sea.Bt = Bt; };
			Sc.Feed = [](FreeEye::Input& In, FreeEye::Eye&, double Time, double Dt)
			{
				(void)Dt;
				// down to the water, then a drone's cruise over the swell, a look about, and a climb away
				if (Time < 6.0) In.Rise = -1.0;
				else if (Time < 40.0) In.PanAhead = 0.6;
				if (Time > 20.0 && Time < 24.0) In.Turn = 1.5;
				if (Time > 40.0 && Time < 42.0) In.Rise = 1.0;
			};
			Measured M = Fly(E, Sc, K, 50.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			int Under = 0;
			for (const Rec& R : M.Recs) Under += R.Clear < E.Set.FloorClear * 0.9 ? 1 : 0;
			char Line[160];
			std::snprintf(Line, sizeof(Line), "frames under 0.9 of FloorClear %d of %d", Under, M.Frames);
			const std::string Name = std::string("rule 9 drone over a 12 m swell ") + TimingName(K);
			Report(Name, M, Line);
			GateSmooth(Name, M);
			if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the wave (FloorHard %.2f)", Name.c_str(), M.Lowest, E.Set.FloorHard);
			if (M.HardLifts) Fault("%s: the lift could not climb in time on %d frames (a swell is to be met by the eased lift)", Name.c_str(), M.HardLifts);
			if (Under > M.Frames / 50) Fault("%s: %d frames under 0.9 of FloorClear (the lift does not keep him clear)", Name.c_str(), Under);
			// (a crest rising under him is met by the lift led at the crest's own rate: at FloorClear, not under it)
			if (M.Lowest < E.Set.FloorClear * 0.95) Fault("%s: the eye stood %.3f m over the wave (a crest rising under him met late)", Name.c_str(), M.Lowest);
		}
		// the lift carries the picture and never tips it
		{
			FreeEye::Eye E;
			Waves Sea;
			E.Begin(Vec{ 0.0, -3.0, 13.0 }, Vec{ 0.0, 0.0, 12.0 }, Vec{});
			FreeEye::Input In;
			double Tip = 0.0, MostLift = 0.0;
			const Vec D0 = (E.Last().Look - E.Last().Eye).Unit();
			for (int F = 0; F < 1800; ++F)
			{
				Sea.Bt = F / 60.0;
				const FreeEye::Pose P = E.Update(In, 1.0 / 60.0, &Sea);
				Tip = std::max(Tip, ((P.Look - P.Eye).Unit() - D0).Length());
				MostLift = std::max(MostLift, E.Lift());
			}
			std::printf("%-44s lifted up to %.2f m, the look tipped %.2e\n", "rule 9 the lift never tips the picture", MostLift, Tip);
			if (Tip > 1e-12) Fault("rule 9: the lift tipped the picture (%.3g)", Tip);
			if (MostLift < 1.0) Fault("rule 9: the eye low over the swell was never lifted (%.2f m)", MostLift);
		}
		// a cliff flown at: what the spring cannot climb is put clear at once, never under
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			Cliff Rock;
			E.Begin(Vec{ 0.0, 0.0, 6.0 }, Vec{ 10.0, 0.0, 6.0 }, Vec{});
			Script Sc;
			Sc.Where = StillAt;
			Sc.Ground = &Rock;
			Sc.Feed = [](FreeEye::Input& In, FreeEye::Eye&, double, double) { In.PanAhead = 1.0; };
			Measured M = Fly(E, Sc, K, 12.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			const std::string Name = std::string("rule 9 a 60 m cliff flown at ") + TimingName(K);
			Report(Name, M);
			if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the ground (FloorHard %.2f)", Name.c_str(), M.Lowest, E.Set.FloorHard);
			if (M.Recs.back().P.Eye.X < 100.0) Fault("%s: the view never got over the cliff", Name.c_str());
		}
	}

	/** A long boom swung under the sea: tilted from looking down to looking up from 1.4 km away, the eye flung at the
	 * water at well over a kilometre a second. The lift cannot climb that in time, so it is put clear at once (counted);
	 * the rule held is that it is never under. */
	void RuleFloorFling()
	{
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			Waves Sea;
			BeginAt(E, 0.85);
			Script Sc;
			Sc.Where = StillAt;
			Sc.Ground = &Sea;
			Sc.Tick = [&Sea](double Bt) { Sea.Bt = Bt; };
			Sc.Feed = [](FreeEye::Input& In, FreeEye::Eye&, double Time, double) { if (Time > 1.0 && Time < 5.0) In.Tilt = -3.0; };
			Measured M = Fly(E, Sc, K, 12.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			const std::string Name = std::string("rule 9 a 1.4 km boom swung under the sea ") + TimingName(K);
			Report(Name, M, "(the hard lifts are this scenario's point)");
			if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the wave (FloorHard %.2f)", Name.c_str(), M.Lowest, E.Set.FloorHard);
			if (!M.bFinite) Fault("%s: not finite", Name.c_str());
		}
	}

	/** Looking straight up at a ship from the water, her clearance 20 m: under her the eye is either in the sea or too
	 * near her, so the floor's lift and her clearance meet, and the hard rules act on most frames there (counted). Held:
	 * never under the wave, never inside her clearance, finite, and the picture to the kick all the same. */
	void RuleFloorAndClear()
	{
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			E.Set.SteadyZ = 3.0;
			Waves Sea;
			E.Begin(Vec{ 0.0, -60.0, 30.0 }, Vec{ 0.0, 0.0, 10.0 }, Vec{});
			Script Sc;
			Sc.Ground = &Sea;
			Sc.Tick = [&Sea](double Bt) { Sea.Bt = Bt; };
			Sc.Where = [](int Key, double Bt) { return Key < 0 ? Vec{} : ShipAt(Bt, 3.0) + Vec{ 0.0, 0.0, 2.0 }; };
			double Last = 0.0;
			Sc.Feed = [&Last](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
			{
				const double Dt = Time - Last;
				Last = Time;
				In.bLock = true;
				In.AnchorId = 0;
				In.Anchor = ShipAt(Bt, 3.0) + Vec{ 0.0, 0.0, 2.0 };
				In.AnchorClear = 20.0;
				if (Time > 1.0 && Time < 5.0) In.Zoom = 15.0 * Dt;
				if (Time > 2.0 && Time < 8.0) In.Tilt = -40.0 * Dt;
			};
			Measured M = Fly(E, Sc, K, 20.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			double Nearest = 1e9;
			for (const Rec& R : M.Recs) Nearest = std::min(Nearest, (R.P.Eye - Sc.Where(0, R.Bt)).Length());
			char Line[120];
			std::snprintf(Line, sizeof(Line), "nearest her %.6f m (clear 20)", Nearest);
			const std::string Name = std::string("sea straight up at a ship from the water ") + TimingName(K);
			Report(Name, M, Line);
			GateSmooth(Name, M);
			if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the wave", Name.c_str(), M.Lowest);
			if (Nearest < 20.0 - 1e-6) Fault("%s: the eye came %.3f m from her (clear 20)", Name.c_str(), Nearest);
			if (!M.bFinite) Fault("%s: not finite", Name.c_str());
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rules 10 to 13: caps, lens, look, Begin.
	void RuleCaps()
	{
		for (Timing K : Timings)
		{
			// tilt, zoom and EyeMost, met softly
			FreeEye::Eye E;
			E.Set.EyeMost = 1500.0;
			BeginAt(E, 0.4);
			Script Sc;
			Sc.Where = StillAt;
			double TMost = -1e9, TLeast = 1e9, ArmMost = 0.0, ArmLeast = 1e9, Highest = -1e9;
			Sc.Feed = [&](FreeEye::Input& In, FreeEye::Eye& Ey, double Time, double)
			{
				TMost = std::max(TMost, Ey.Tilt());
				TLeast = std::min(TLeast, Ey.Tilt());
				ArmMost = std::max(ArmMost, Ey.Arm());
				ArmLeast = std::min(ArmLeast, Ey.Arm());
				Highest = std::max(Highest, Ey.Last().Eye.Z);
				// down to the steepest, out to the ceiling, right in, up to the most it looks up, then level and out to the
				// longest boom
				if (Time < 4.0) In.Tilt = 3.0;
				else if (Time < 8.0) In.Zoom = -1.0;
				else if (Time < 14.0) In.Zoom = 1.0;
				else if (Time < 20.0) In.Tilt = -3.0;
				if (Time >= 20.0 && Time < 21.0 && Ey.WantTilt() != 0.0) Ey.Want(Ey.Heading(), 0.0, Ey.WantZoom());
				if (Time > 24.0 && Time < 38.0) In.Zoom = -1.0;
				// and the pivot itself raised at the ceiling, level: his offset held under it
				if (Time > 38.0 && Time < 44.0) In.Rise = 1.0;
			};
			Measured M = Fly(E, Sc, K, 48.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			char Line[200];
			std::snprintf(Line, sizeof(Line), "tilt %.2f..%.2f deg, boom %.3f..%.1f m, eye up to %.2f m", TLeast * Deg, TMost * Deg, ArmLeast, ArmMost, Highest);
			const std::string Name = std::string("rule 10 tilt, boom and EyeMost 1500 ") + TimingName(K);
			Report(Name, M, Line);
			GateSmooth(Name, M);
			if (TMost > E.Set.TiltMost * Rad + 1e-12 || TLeast < E.Set.TiltLeast * Rad - 1e-12) Fault("%s: the tilt left its range", Name.c_str());
			if (ArmMost > E.Set.ArmFar * (1.0 + 1e-12) || ArmLeast < E.Set.ArmNear * (1.0 - 1e-12)) Fault("%s: the boom left its range", Name.c_str());
			if (Highest > 1500.0 + 1e-9) Fault("%s: the eye rose to %.3f m over EyeMost 1500", Name.c_str(), Highest);
			if (TMost < E.Set.TiltMost * Rad - 1e-3 || TLeast > E.Set.TiltLeast * Rad + 1e-3 || ArmMost < E.Set.ArmFar * 0.999 || ArmLeast > E.Set.ArmNear * 1.001 || Highest < 1499.0) Fault("%s: the caps were never reached", Name.c_str());
			if (M.HardStops) Fault("%s: %d hard stops (the caps must be met softly)", Name.c_str(), M.HardStops);
		}
		for (Timing K : Timings)
		{
			// the pivot's box, met softly; then the box moved onto him (a hard stop is allowed only then)
			FreeEye::Eye E;
			BeginAt(E, 0.5);
			Script Sc;
			Sc.Where = StillAt;
			double Out = 0.0;
			int StopsBeforeMove = -1;
			Sc.Feed = [&](FreeEye::Input& In, FreeEye::Eye& Ey, double Time, double)
			{
				In.bBox = true;
				const double Shift = Time > 12.0 ? 40.0 * (Time - 12.0) : 0.0;
				In.BoxMin = Vec{ -300.0 + Shift, -300.0, -50.0 };
				In.BoxMax = Vec{ 300.0, 300.0, 400.0 };
				if (Time < 10.0) In.PanRight = -1.0;
				if (Time > 4.0 && Time < 10.0) In.PanAhead = 1.0;
				if (Time > 11.0 && StopsBeforeMove < 0) StopsBeforeMove = Ey.HardStops();
			};
			Measured M = Fly(E, Sc, K, 16.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			for (const Rec& R : M.Recs)
			{
				const double Shift = R.Time > 12.0 ? 40.0 * (R.Time - 12.0) : 0.0;
				Out = std::max({ Out, -300.0 + Shift - R.Pivot.X, R.Pivot.X - 300.0, R.Pivot.Y - 300.0, -300.0 - R.Pivot.Y });
			}
			Measured Early = M;
			Early.Recs.erase(std::remove_if(Early.Recs.begin(), Early.Recs.end(), [](const Rec& R) { return R.Time > 11.0; }), Early.Recs.end());
			Early.Kick = Early.Felt = 0.0;
			Smoothness(Early, Sc, E.Set.FeelArm);
			char Line[160];
			std::snprintf(Line, sizeof(Line), "pivot out of the box by %.3g m, hard stops before the box moved %d", std::max(0.0, Out), StopsBeforeMove);
			const std::string Name = std::string("rule 10 box ") + TimingName(K);
			Report(Name, Early, Line);
			GateSmooth(Name, Early);
			if (Out > 1e-9) Fault("%s: the pivot left the box by %.3g m", Name.c_str(), Out);
			if (StopsBeforeMove != 0) Fault("%s: %d hard stops at a still box (it must be met softly)", Name.c_str(), StopsBeforeMove);
		}
		{
			// the anchor's clearance: zoom right in on an aircraft, and fly through it
			FreeEye::Eye E;
			BeginAt(E, 0.4);
			Script Sc;
			auto Plane = [](int, double Bt) { return Vec{ 0.0, 120.0 * Bt, 4000.0 }; };
			Sc.Where = [&Plane](int Key, double Bt) { return Key < 0 ? Vec{} : Plane(Key, Bt); };
			double Nearest = 1e9;
			Sc.Feed = [&](FreeEye::Input& In, FreeEye::Eye& Ey, double Time, double Bt)
			{
				if (Time > 1.0) Nearest = std::min(Nearest, (Ey.Last().Eye - Plane(1, Bt - 0.0)).Length());
				In.bLock = true;
				In.AnchorId = 1;
				In.Anchor = Plane(1, Bt);
				In.bAnchorVel = true;
				In.AnchorVel = Vec{ 0.0, 120.0, 0.0 };
				In.AnchorClear = 30.0;
				if (Time < 8.0) In.Zoom = 1.0;
				else if (Time < 14.0) In.PanAhead = 1.0;
				else if (Time < 20.0) In.PanAhead = -1.0;
			};
			Measured M = Fly(E, Sc, Timing::Jitter, 24.0);
			Smoothness(M, Sc, E.Set.FeelArm, 3.0);
			Nearest = 1e9;
			for (const Rec& R : M.Recs)
			{
				if (R.Key == 1) Nearest = std::min(Nearest, (R.P.Eye - Plane(1, R.Bt)).Length());
			}
			char Line[120];
			std::snprintf(Line, sizeof(Line), "nearest the eye came to the aircraft %.3f m (AnchorClear 30)", Nearest);
			Report("rule 10 anchor clearance", M, Line);
			if (Nearest < 30.0 - 1e-6) Fault("rule 10: the eye came %.3f m from the anchor (AnchorClear 30)", Nearest);
		}
	}

	void RuleLensLook()
	{
		FreeEye::Eye E;
		BeginAt(E, 0.5);
		FreeEye::Input In;
		E.Want(E.Heading(), E.Tilt(), 0.0);
		for (int F = 0; F < 900; ++F) E.Update(In, 1.0 / 60.0, nullptr);
		const double FovNearAt = E.Last().Fov, ArmNearAt = E.Arm();
		const FreeEye::Pose Near = E.Last();
		E.Want(E.Heading(), E.Tilt(), 1.0);
		for (int F = 0; F < 900; ++F) E.Update(In, 1.0 / 60.0, nullptr);
		const double FovFarAt = E.Last().Fov;
		const FreeEye::Pose Far = E.Last();
		std::printf("%-44s Fov %.6f at zoom 0 (boom %.4f m), %.6f at zoom 1\n", "rule 11 lens", FovNearAt, ArmNearAt, FovFarAt);
		if (std::abs(FovNearAt - 44.0) > 1e-9 || std::abs(FovFarAt - 50.0) > 1e-9) Fault("rule 11: the lens is %.4f..%.4f (44..50 wanted)", FovNearAt, FovFarAt);
		// rule 12: the look at max(Arm, LookLeast) along the line to the pivot
		const double LNear = (Near.Look - Near.Eye).Length(), LFar = (Far.Look - Far.Eye).Length();
		const Vec PivotWay = (E.Pivot() - Far.Eye).Unit(), LookWay = (Far.Look - Far.Eye).Unit();
		std::printf("%-44s |Look-Eye| %.6f m on the drone's boom, %.3f m far; off the pivot's line %.2e\n", "rule 12 look", LNear, LFar, (PivotWay - LookWay).Length());
		if (std::abs(LNear - 10.0) > 1e-9) Fault("rule 12: on the drone's boom |Look - Eye| is %.6f (LookLeast 10)", LNear);
		if (std::abs(LFar - 4000.0) > 1e-6) Fault("rule 12: far out |Look - Eye| is %.6f (the boom 4000)", LFar);
		if ((PivotWay - LookWay).Length() > 1e-12) Fault("rule 12: the look is off the line to the pivot");
	}

	void RuleBegin()
	{
		const Vec Shots[][2] = { { Vec{ 100.0, -300.0, 250.0 }, Vec{ 0.0, 0.0, 0.0 } }, { Vec{ 5.0, 5.0, 3.0 }, Vec{ 5.5, 5.2, 3.1 } },
			{ Vec{ 0.0, 0.0, 6000.0 }, Vec{ 300.0, 200.0, 6100.0 } }, { Vec{ -2000.0, 800.0, 3500.0 }, Vec{ 0.0, 0.0, 0.0 } } };
		double Worst = 0.0, WorstDir = 0.0, WorstMoving = 0.0;
		int Cuts = 0;
		for (const auto& Shot : Shots)
		{
			for (int Moving = 0; Moving < 2; ++Moving)
			{
				FreeEye::Eye E;
				const Vec F0 = Moving ? Vec{ 1000.0, 2000.0, 3000.0 } : Vec{};
				const Vec F1 = Moving ? F0 + Vec{ 2.0, 1.0, 0.1 } : Vec{};
				E.Begin(Shot[0], Shot[1], F0);
				FreeEye::Input In;
				In.Frame = F1;
				const FreeEye::Pose P = E.Update(In, 1.0 / 60.0, nullptr);
				const double Off = (P.Eye - (Shot[0] + (F1 - F0))).Length();
				const double Dir = ((P.Look - P.Eye).Unit() - (Shot[1] - Shot[0]).Unit()).Length();
				(Moving ? WorstMoving : Worst) = std::max(Moving ? WorstMoving : Worst, Off);
				WorstDir = std::max(WorstDir, Dir);
				Cuts += P.bCut ? 1 : 0;
			}
		}
		std::printf("%-44s eye off %.2e m (moving frame %.2e), direction off %.2e, cuts %d\n", "rule 13 Begin from the director's shot", Worst, WorstMoving, WorstDir, Cuts);
		if (Worst > 1e-9 || WorstMoving > 1e-9) Fault("rule 13: the first frame's eye is %.3g m off the director's", std::max(Worst, WorstMoving));
		if (WorstDir > 1e-9) Fault("rule 13: the first frame's direction is %.3g off the director's", WorstDir);
		if (Cuts) Fault("rule 13: Begin's first frame is a cut");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Rules 14 and 15: robust; same in, same out.
	struct Hash
	{
		uint64_t H = 1469598103934665603ULL;
		void Add(const void* P, size_t N)
		{
			const unsigned char* B = static_cast<const unsigned char*>(P);
			for (size_t K = 0; K < N; ++K) H = (H ^ B[K]) * 1099511628211ULL;
		}
		void Add(const FreeEye::Pose& P)
		{
			Add(&P.Eye, sizeof(Vec));
			Add(&P.Look, sizeof(Vec));
			Add(&P.Fov, sizeof(double));
		}
	};

	struct Broken
	{
		int Floor = 0, Tilt = 0, Arm = 0, High = 0, Box = 0, Clear = 0, NotFinite = 0, Wrap = 0;
		double Lowest = 1e9, Nearest = 1e9;
		long Frames = 0;
		int Cuts = 0;
	};

	uint64_t RandomHour(uint64_t Seed, double Seconds, Broken& B)
	{
		FreeEye::Eye E;
		E.Set.EyeMost = 9000.0;
		Waves Sea;
		Lcg R(Seed);
		BeginAt(E, 0.5);
		Hash Hs;
		double Bt = 0.0, Time = 0.0;
		FreeEye::Input Held;
		bool bBox = false;
		int Anchor = -1, FrameId = 0;
		double Pace = 1.0;
		const double Nan = std::nan("");
		auto Plane = [](int Id, double At)
		{
			const double A = 0.05 * At + Id;
			return Vec{ 3000.0 * std::sin(A) + 200.0 * Id, 3000.0 * std::cos(A), 50.0 + 800.0 * Id + 30.0 * std::sin(0.3 * At + Id) };
		};
		while (Time < Seconds)
		{
			double Dt = R.In(0.004, 0.05);
			const double U = R.Next();
			if (U < 0.01) Dt = R.In(0.1, 3.0);
			else if (U < 0.015) Dt = 0.0;
			else if (U < 0.017) Dt = Nan;
			if (std::isfinite(Dt)) Time += Dt;
			if (std::isfinite(Dt)) Bt += Dt * Pace;
			Sea.Bt = Bt;
			if (R.Next() < 0.02)
			{
				// a new hand: keys, slides, wheel, turn, tilt
				Held = FreeEye::Input();
				Held.PanRight = R.Next() < 0.5 ? R.In(-1.0, 1.0) : 0.0;
				Held.PanAhead = R.Next() < 0.5 ? R.In(-1.0, 1.0) : 0.0;
				Held.Rise = R.Next() < 0.4 ? R.In(-1.0, 1.0) : 0.0;
				Held.Turn = R.Next() < 0.4 ? R.In(-5.0, 5.0) : 0.0;
				Held.Tilt = R.Next() < 0.4 ? R.In(-3.0, 3.0) : 0.0;
				Held.Zoom = R.Next() < 0.3 ? R.In(-0.5, 0.5) : 0.0;
				Held.SlideRight = R.Next() < 0.2 ? R.In(-0.05, 0.05) : 0.0;
				Held.bRide = R.Next() < 0.5;
				bBox = R.Next() < 0.3;
				Pace = R.Next() < 0.2 ? 0.0 : R.In(0.5, 10.0);
				const double L = R.Next();
				Anchor = L < 0.3 ? -1 : L < 0.95 ? static_cast<int>(R.In(0.0, 12.0)) : 99;
				if (R.Next() < 0.1) ++FrameId;
			}
			FreeEye::Input In = Held;
			In.Frame = Vec{ 100.0 * Bt, 0.0, 2000.0 } + Vec{ 0.0, 1000.0 * FrameId, 0.0 };
			In.FrameId = FrameId;
			if (Anchor >= 0)
			{
				In.bLock = true;
				In.AnchorId = Anchor;
				In.Anchor = Anchor == 99 ? Vec{ R.In(-1e5, 1e5), R.In(-1e5, 1e5), R.In(-10.0, 9000.0) } : Plane(Anchor, Bt);
				In.bAnchorVel = R.Next() < 0.5;
				In.AnchorVel = (Plane(Anchor, Bt + 0.01) - Plane(Anchor, Bt)) * (100.0 * Pace);
				In.AnchorHeading = 0.05 * Bt + Anchor;
				In.AnchorClear = Anchor % 3 == 0 ? 25.0 : 0.0;
			}
			if (R.Next() < 0.01) In.bLetGo = true;
			if (R.Next() < 0.03) In.Zoom = R.In(-20.0, 20.0);
			if (R.Next() < 0.02) In.SlideAhead = R.In(-6.0, 6.0);
			if (R.Next() < 0.005) In.Turn = R.In(-1e6, 1e6);
			if (bBox)
			{
				In.bBox = true;
				In.BoxMin = Vec{ -5000.0 + 100.0 * Bt, -8000.0, -100.0 };
				In.BoxMax = Vec{ 9000.0 + 100.0 * Bt, 8000.0, 8000.0 };
			}
			// garbage now and then: every kind of number not finite
			if (R.Next() < 0.01)
			{
				const double G = R.Next() < 0.5 ? Nan : (R.Next() < 0.5 ? INFINITY : -INFINITY);
				switch (static_cast<int>(R.In(0.0, 10.0)))
				{
				case 0: In.PanRight = G; break;
				case 1: In.Zoom = G; break;
				case 2: In.Turn = G; break;
				case 3: In.Tilt = G; break;
				case 4: In.Anchor.X = G; break;
				case 5: In.Frame.Z = G; break;
				case 6: In.AnchorVel.Y = G; break;
				case 7: In.AnchorHeading = G; break;
				case 8: In.SlideRight = G; break;
				default: In.Aspect = G; break;
				}
			}
			if (R.Next() < 0.002)
			{
				// (drawn one by one: the order a call's arguments are worked out in is the compiler's)
				const double WantH = R.Next() < 0.5 ? Nan : R.In(-50.0, 50.0);
				const double WantT = R.Next() < 0.3 ? (R.Next() < 0.5 ? Nan : -INFINITY) : R.In(-3.0, 3.0);
				const double WantZ = R.Next() < 0.3 ? (R.Next() < 0.5 ? Nan : INFINITY) : R.In(-2.0, 3.0);
				E.Want(WantH, WantT, WantZ);
			}
			const FreeEye::Pose P = E.Update(In, Dt, &Sea);
			Hs.Add(P);
			++B.Frames;
			B.Cuts += P.bCut ? 1 : 0;
			if (!P.Eye.Finite() || !P.Look.Finite() || !std::isfinite(P.Fov) || !E.Pivot().Finite()) ++B.NotFinite;
			const double Clear = P.Eye.Z - Sea.Floor(P.Eye.X, P.Eye.Y);
			B.Lowest = std::min(B.Lowest, Clear);
			if (Clear < E.Set.FloorHard - 1e-9) ++B.Floor;
			if (E.Tilt() > E.Set.TiltMost * Rad + 1e-12 || E.Tilt() < E.Set.TiltLeast * Rad - 1e-12) ++B.Tilt;
			if (E.Arm() > E.Set.ArmFar * (1 + 1e-12) || E.Arm() < E.Set.ArmNear * (1 - 1e-12)) ++B.Arm;
			if (std::abs(E.Heading()) > Pi + 1e-12) ++B.Wrap;
			// (the caps rank: floor, ceiling, the anchor's clearance, the box; each is checked where no higher one holds him)
			const bool bFloorHeld = Clear < E.Set.FloorHard + 1e-6;
			const bool bCeilingHeld = P.Eye.Z > E.Set.EyeMost - 1e-6;
			if (!bFloorHeld && P.Eye.Z > E.Set.EyeMost + 1e-6) ++B.High;
			bool bClearHeld = false;
			if (E.Locked() >= 0 && In.AnchorClear > 0.0 && In.Anchor.Finite()) bClearHeld = (P.Eye - In.Anchor).Length() < In.AnchorClear + 1e-6;
			if (bBox && !bFloorHeld && !bCeilingHeld && !bClearHeld && std::isfinite(Dt) && Dt > 0.0)
			{
				const Vec Pv = E.Pivot();
				const double Out = std::max({ In.BoxMin.X - Pv.X, Pv.X - In.BoxMax.X, In.BoxMin.Y - Pv.Y, Pv.Y - In.BoxMax.Y, In.BoxMin.Z - Pv.Z, Pv.Z - In.BoxMax.Z });
				if (Out > 1e-6) ++B.Box;
			}
			if (E.Locked() >= 0 && In.AnchorClear > 0.0 && !bFloorHeld && !bCeilingHeld && In.Anchor.Finite() && std::isfinite(Dt) && Dt > 0.0)
			{
				const double Near = (P.Eye - In.Anchor).Length();
				B.Nearest = std::min(B.Nearest, Near);
				if (Near < In.AnchorClear - 1e-6) ++B.Clear;
			}
		}
		return Hs.H;
	}

	void RuleRobust()
	{
		{
			FreeEye::Eye E;
			BeginAt(E, 0.5);
			FreeEye::Input In;
			In.PanRight = 1.0;
			In.Turn = 2.0;
			for (int F = 0; F < 30; ++F) E.Update(In, 1.0 / 60.0, nullptr);
			const FreeEye::Pose Before = E.Last();
			const Vec Rel0 = E.Rel(), Sw0 = E.RelSwing();
			const double H0 = E.Heading();
			In.bLock = true;
			In.AnchorId = 4;
			In.Anchor = Vec{ 50.0, 0.0, 0.0 };
			const FreeEye::Pose Zero = E.Update(In, 0.0, nullptr);
			const bool bSame = Same(Zero, Before) && E.Rel().X == Rel0.X && E.RelSwing().X == Sw0.X && E.Heading() == H0 && E.Locked() == -1;
			std::printf("%-44s %s\n", "rule 14 RealDt 0 changes nothing", bSame ? "unchanged" : "CHANGED");
			if (!bSame) Fault("rule 14: RealDt 0 changed the view");
		}
		{
			FreeEye::Eye A, B;
			BeginAt(A, 0.5);
			BeginAt(B, 0.5);
			FreeEye::Input In;
			In.PanAhead = 1.0;
			In.Zoom = 0.5;
			In.Turn = 5.0;
			A.Update(In, 3.0, nullptr);
			B.Update(In, 0.25, nullptr);
			In = FreeEye::Input();
			A.Update(In, 1e9, nullptr);
			B.Update(In, 0.25, nullptr);
			const bool bSame = Same(A.Last(), B.Last());
			std::printf("%-44s %s\n", "rule 14 RealDt over 0.25 counts as 0.25", bSame ? "the same" : "DIFFERENT");
			if (!bSame) Fault("rule 14: a long RealDt is not taken as 0.25");
		}
		{
			// garbage taken as nothing: a twin fed zeros flies the same to the last bit
			FreeEye::Eye A, B;
			BeginAt(A, 0.5);
			BeginAt(B, 0.5);
			int Differ = 0;
			const double Nan = std::nan("");
			for (int F = 0; F < 600; ++F)
			{
				FreeEye::Input Good;
				Good.PanRight = F < 200 ? 1.0 : 0.0;
				Good.Frame = Vec{ 3.0 * F, 0.0, 0.0 };
				FreeEye::Input Bad = Good;
				Bad.PanAhead = Nan;
				Bad.Rise = INFINITY;
				Bad.SlideRight = -INFINITY;
				Bad.SlideAhead = Nan;
				Bad.Zoom = Nan;
				Bad.Turn = INFINITY;
				Bad.Tilt = Nan;
				Bad.Aspect = Nan;
				if (F % 7 == 3) Bad.Frame = Vec{ Nan, 0.0, 0.0 };
				if (F % 7 == 3) Good.Frame = Vec{ 3.0 * (F - 1), 0.0, 0.0 };
				A.Update(Good, 1.0 / 60.0, nullptr);
				B.Update(Bad, 1.0 / 60.0, nullptr);
				Differ += Same(A.Last(), B.Last()) ? 0 : 1;
			}
			std::printf("%-44s frames differing from a twin fed zeros %d of 600\n", "rule 14 numbers not finite taken as 0", Differ);
			if (Differ) Fault("rule 14: inputs not finite were not taken as 0 (%d frames differ)", Differ);
		}
		{
			FreeEye::Eye E;
			BeginAt(E, 0.5);
			FreeEye::Input In;
			double Most = 0.0;
			for (int F = 0; F < 2000; ++F)
			{
				In.Turn = F < 1000 ? 37.0 : -1e7;
				E.Update(In, 1.0 / 60.0, nullptr);
				Most = std::max({ Most, std::abs(E.Heading()), std::abs(E.WantHeading() - E.Heading()) });
			}
			std::printf("%-44s |heading| and |want - heading| at most %.6f rad\n", "rule 14 the heading stays wrapped", Most);
			if (Most > Pi + 1e-12) Fault("rule 14: the heading left -pi..pi (%.6f)", Most);
		}
		{
			Broken B;
			const uint64_t H1 = RandomHour(11, 3600.0, B);
			std::printf("%-44s frames %ld  under the floor %d (lowest %.3f m)  tilt %d  boom %d  over EyeMost %d  out of box %d  into the anchor %d (nearest %.2f m)  not finite %d  unwrapped %d  cuts %d\n",
				"rule 14 an hour of random hands", B.Frames, B.Floor, B.Lowest, B.Tilt, B.Arm, B.High, B.Box, B.Clear, B.Nearest, B.NotFinite, B.Wrap, B.Cuts);
			if (B.Floor || B.Tilt || B.Arm || B.High || B.Box || B.Clear || B.NotFinite || B.Wrap)
			{
				Fault("rule 14: an hour of random hands broke rule 9 or 10 or went not finite (floor %d tilt %d boom %d ceiling %d box %d anchor %d not finite %d wrap %d)",
					B.Floor, B.Tilt, B.Arm, B.High, B.Box, B.Clear, B.NotFinite, B.Wrap);
			}
			Broken B2;
			const uint64_t H2 = RandomHour(11, 3600.0, B2);
			std::printf("%-44s %016llx and %016llx\n", "rule 15 same in, same out (two hours)", static_cast<unsigned long long>(H1), static_cast<unsigned long long>(H2));
			if (H1 != H2) Fault("rule 15: two runs of the same script differ");
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The air battle.
	/** A squadron of 12 at 6 km: 111 m/s straight for 8 s, then a 6 g turn of 210 m radius (rolled into over a second
	 * and a half), then straight again; each aircraft at its own place in the formation, 30 to 400 m off the leader,
	 * kept as the formation turns. Key 100 is the squadron's middle. */
	struct Squadron
	{
		static constexpr double Speed = 111.0, Radius = 210.0;
		static double TurnRate(double Bt)
		{
			const double Full = Speed / Radius;
			auto Ramp = [](double U) { U = std::clamp(U, 0.0, 1.0); return U * U * (3.0 - 2.0 * U); };
			return Full * (Ramp((Bt - 8.0) / 1.5) - Ramp((Bt - 14.0) / 1.5));
		}
		std::vector<Vec> Lead;
		std::vector<double> Heads;
		double Step = 1.0 / 100.0;
		double HeadingAt(double Bt) const
		{
			const double U = std::clamp(Bt / Step, 0.0, static_cast<double>(Heads.size() - 2));
			const size_t K = static_cast<size_t>(U);
			return Heads[K] + (Heads[K + 1] - Heads[K]) * (U - K);
		}
		void Build(double Until)
		{
			Lead.clear();
			Heads.clear();
			Vec P{ 0.0, 0.0, 6000.0 };
			double Hd = 0.0;
			for (double U = 0.0; U <= Until + 1.0; U += Step)
			{
				Lead.push_back(P);
				Heads.push_back(Hd);
				const double Rate = TurnRate(U + 0.5 * Step);
				const double Hm = Hd + 0.5 * Rate * Step;
				P += Vec{ std::sin(Hm), std::cos(Hm), 0.0 } * (Speed * Step);
				Hd += Rate * Step;
			}
		}
		Vec LeadAt(double Bt) const
		{
			const double U = std::clamp(Bt / Step, 0.0, static_cast<double>(Lead.size() - 2));
			const size_t K = static_cast<size_t>(U);
			const double F = U - K;
			// a cubic through the samples, so its own acceleration is smooth
			const Vec& P1 = Lead[K];
			const Vec& P2 = Lead[K + 1];
			const Vec& P0 = K > 0 ? Lead[K - 1] : P1;
			const Vec& P3 = K + 2 < Lead.size() ? Lead[K + 2] : P2;
			const double F2 = F * F, F3 = F2 * F;
			return (P1 * 2.0 + (P2 - P0) * F + (P0 * 2.0 - P1 * 5.0 + P2 * 4.0 - P3) * F2 + (P1 * 3.0 - P0 - P2 * 3.0 + P3) * F3) * 0.5;
		}
		static Vec Slot(int Id)
		{
			if (Id >= 100) return Vec{ 0.0, -150.0, 0.0 };
			if (Id == 1) return Vec{ 3.0, -52.0, 0.0 };
			const double Back = 30.0 + 33.0 * Id, Side = (Id % 2 ? 1.0 : -1.0) * (15.0 + 18.0 * Id);
			return Vec{ Side, -Back, (Id % 3) * 10.0 };
		}
		Vec At(int Id, double Bt) const
		{
			const Vec S = Slot(Id);
			return LeadAt(Bt) + Vec{ 0.0, 0.0, S.Z } + [](const Vec& V, double A) { return Vec{ V.X * std::cos(A) + V.Y * std::sin(A), -V.X * std::sin(A) + V.Y * std::cos(A), 0.0 }; }(Vec{ S.X, S.Y, 0.0 }, HeadingAt(Bt));
		}
	};

	void AirBattle()
	{
		Squadron Sq;
		Sq.Build(30.0 * 10.0 + 10.0);
		const double Paces[] = { 0.5, 1.0, 4.0, 10.0, 0.0 };
		for (double Pace : Paces)
		{
			for (Timing K : Timings)
			{
				for (int Fed = 0; Fed < 2; ++Fed)
				{
					FreeEye::Eye E;
					E.Set.TiltLeast = -80.0;
					// begin over the squadron's middle at battle time 2 (paused: the squadron stands mid turn)
					const double B0 = Pace > 0.0 ? 2.0 : 10.0;
					BeginAt(E, 0.55, Sq.At(100, B0));
					Script Sc;
					Sc.Pace = [Pace](double) { return Pace; };
					Sc.Where = [&Sq, B0](int Key, double Bt) { return Key < 0 ? Vec{} : Sq.At(Key, B0 + Bt); };
					Sc.Feed = [&Sq, B0, Pace, Fed](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
					{
						In.Frame = Sq.At(100, B0 + Bt);
						In.bLock = true;
						// the squadron first; then Tab through its planes every 2.5 s; a drone's look about in the middle
						// (an order that steps near and far: 0 to 11 is some 400 m)
						static const int Order[12] = { 0, 1, 11, 2, 9, 4, 7, 6, 5, 8, 3, 10 };
						In.AnchorId = Time < 3.0 ? 100 : Order[static_cast<int>((Time - 3.0) / 2.5) % 12];
						In.Anchor = Sq.At(In.AnchorId, B0 + Bt);
						In.bAnchorVel = Fed != 0;
						In.AnchorVel = (Sq.At(In.AnchorId, B0 + Bt + 1e-3) - Sq.At(In.AnchorId, B0 + Bt - 1e-3)) * (Pace / 2e-3);
						In.AnchorHeading = Sq.HeadingAt(B0 + Bt);
						In.AnchorClear = 12.0;
						if (Time > 12.0 && Time < 14.0) In.Zoom = 0.3;
						if (Time > 15.0 && Time < 18.0) In.Turn = 1.5;
						if (Time > 18.0 && Time < 20.0) In.PanRight = 0.7;
					};
					Measured M = Fly(E, Sc, K, 32.0);
					Smoothness(M, Sc, E.Set.FeelArm);
					double Lag = 0.0, StepLeast = 1e9, StepMost = 0.0;
					for (size_t I = 1; I < M.Recs.size(); ++I)
					{
						const Rec& R0 = M.Recs[I - 1];
						const Rec& R1 = M.Recs[I];
						if (R0.Key >= 0 && R1.Key >= 0 && R0.Key != R1.Key)
						{
							const double Far = (Sq.At(R0.Key, B0 + R1.Bt) - Sq.At(R1.Key, B0 + R1.Bt)).Length();
							StepLeast = std::min(StepLeast, Far);
							StepMost = std::max(StepMost, Far);
						}
					}
					for (size_t I = 1; I < M.Recs.size(); ++I)
					{
						// riding rigidly: once settled on a plane, the pivot sits on it, whatever the pace
						const Rec& R = M.Recs[I];
						const double Into = R.Time - 3.0 - 2.5 * std::floor((R.Time - 3.0) / 2.5);
						if (R.Time > 3.0 && R.Time < 12.0 && Into > 2.45 && R.Key >= 0) Lag = std::max(Lag, (R.Pivot - Sq.At(R.Key, B0 + R.Bt)).Length());
					}
					char Name[96];
					std::snprintf(Name, sizeof(Name), "air squadron pace %.1f%s %s", Pace, Fed ? " fed" : "", TimingName(K));
					char Line[160];
					std::snprintf(Line, sizeof(Line), "steps %.0f to %.0f m, pivot off its plane 2.45 s after a step %.2f m", StepLeast, StepMost, Lag);
					Report(Name, M, Line);
					GateSmooth(Name, M);
					if (M.Cuts) Fault("%s: %d cuts stepping inside a squadron (30 to 400 m)", Name, M.Cuts);
					if (Lag > 1.5) Fault("%s: the pivot is %.2f m off its plane 2.45 s after the step (a glide of up to 400 m is to be done by then)", Name, Lag);
				}
			}
		}
		// a lock on one 20 km away: a cut, once
		{
			FreeEye::Eye E;
			BeginAt(E, 0.6, Sq.At(100, 0.0));
			FreeEye::Input In;
			int Cuts = 0;
			for (int F = 0; F < 600; ++F)
			{
				In = FreeEye::Input();
				In.Frame = Sq.At(100, F / 60.0);
				In.bLock = F >= 120;
				In.AnchorId = 40;
				In.Anchor = Vec{ 20000.0, 3000.0 + 180.0 * F / 60.0, 9000.0 };
				Cuts += E.Update(In, 1.0 / 60.0, nullptr).bCut ? 1 : 0;
			}
			std::printf("%-44s cuts %d, pivot on it %.3f m (180 m/s at 9 km)\n", "air lock on one 20 km off", Cuts, (E.Pivot() - In.Anchor).Length());
			if ((E.Pivot() - In.Anchor).Length() > 1e-6) Fault("air: after the cut to one 20 km off the pivot is not on it");
			if (Cuts != 1) Fault("air: a lock 20 km off cut %d times (once wanted)", Cuts);
		}
		// the frame the raid's moving middle; a drone inside the stream
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			auto Middle = [](int, double Bt) { return Vec{ 30.0 * Bt, 120.0 * Bt, 5000.0 + 5.0 * Bt }; };
			E.Begin(Middle(-1, 0.0) + Vec{ 0.0, -3.0, 0.5 }, Middle(-1, 0.0), Middle(-1, 0.0));
			Script Sc;
			Sc.Where = Middle;
			Sc.Feed = [&Middle](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
			{
				In.Frame = Middle(-1, Bt);
				if (Time < 6.0) In.PanAhead = 1.0;
				else if (Time < 9.0) In.PanRight = -1.0;
				if (Time > 4.0 && Time < 12.0) In.Turn = 2.0;
				if (Time > 9.0 && Time < 12.0) In.Rise = -1.0;
				if (Time > 13.0 && Time < 15.0) In.Tilt = -1.0;
			};
			Measured M = Fly(E, Sc, K, 20.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			char Name[96];
			std::snprintf(Name, sizeof(Name), "air drone inside the stream %s", TimingName(K));
			char Line[120];
			std::snprintf(Line, sizeof(Line), "boom %.2f m, left behind by the raid %s", E.Arm(), (E.Pivot() - Middle(-1, M.Recs.back().Bt)).Length() < 2000.0 ? "no" : "YES");
			Report(Name, M, Line);
			GateSmooth(Name, M);
			if ((E.Pivot() - Middle(-1, M.Recs.back().Bt)).Length() > 2000.0) Fault("%s: the camera was left behind by the raid", Name);
		}
		// low: a fighter at 60 m/s 40 m over hills, the drone under it looking up
		for (Timing K : Timings)
		{
			struct Hills : FreeEye::World
			{
				double Floor(double X, double Y) const override { return 20.0 + 15.0 * std::sin(X / 90.0) * std::cos(Y / 130.0); }
			} Ground;
			FreeEye::Eye E;
			E.Set.TiltLeast = -80.0;
			auto Fighter = [](int, double Bt) { return Vec{ 60.0 * Bt, 20.0 * Bt, 75.0 }; };
			E.Begin(Fighter(0, 0.0) + Vec{ 0.0, -30.0, -20.0 }, Fighter(0, 0.0), Vec{});
			Script Sc;
			Sc.Ground = &Ground;
			Sc.Where = [&Fighter](int Key, double Bt) { return Key < 0 ? Vec{} : Fighter(Key, Bt); };
			Sc.Feed = [&Fighter](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
			{
				In.bLock = true;
				In.AnchorId = 0;
				In.Anchor = Fighter(0, Bt);
				In.bAnchorVel = true;
				In.AnchorVel = Vec{ 60.0, 20.0, 0.0 };
				In.AnchorClear = 12.0;
				if (Time < 5.0) In.Tilt = -0.6;
				if (Time > 5.0 && Time < 8.0) In.Rise = -1.0;
			};
			Measured M = Fly(E, Sc, K, 20.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			char Name[96];
			std::snprintf(Name, sizeof(Name), "air low under a fighter over hills %s", TimingName(K));
			Report(Name, M);
			GateSmooth(Name, M);
			if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the ground", Name, M.Lowest);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The sea battle.
	void SeaBattle()
	{
		// three ships at 15 m/s, heaving 3 m each way (6 m), the second 300 m off the first, the third 5 km off
		auto ShipOf = [](int Id, double Bt)
		{
			const Vec Off = Id == 0 ? Vec{} : Id == 1 ? Vec{ 300.0, 40.0, 0.0 } : Vec{ 5000.0, -200.0, 0.0 };
			const Vec A = ShipAt(Bt, 3.0);
			return Vec{ A.X + Off.X, A.Y + Off.Y, 10.0 + 3.0 * std::sin(2.0 * Pi * Bt / 8.0 + Id) };
		};
		for (int Ride = 0; Ride < 2; ++Ride)
		{
			for (Timing K : Timings)
			{
				FreeEye::Eye E;
				E.Set.FloorClear = 3.0;
				E.Set.EyeMost = 1500.0;
				E.Set.CutBeyond = 800.0;
				E.Set.SteadyZ = 3.0;
				Waves Sea;
				E.Begin(Vec{ -100.0, -200.0, 120.0 }, Vec{ 0.0, 0.0, 10.0 }, Vec{});
				Script Sc;
				Sc.Ground = &Sea;
				Sc.Tick = [&Sea](double Bt) { Sea.Bt = Bt; };
				Sc.Where = [&ShipOf](int Key, double Bt) { return Key < 0 ? Vec{} : ShipOf(Key, Bt); };
				double Last = 0.0;
				Sc.Feed = [&ShipOf, Ride, &Last](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
				{
					const double Dt = Time - Last;
					Last = Time;
					In.bLock = Time > 1.0 && Time < 44.0;
					In.AnchorId = Time < 16.0 ? 0 : Time < 30.0 ? 1 : 2;
					In.Anchor = ShipOf(In.AnchorId, Bt);
					In.AnchorHeading = ShipHeading(Bt);
					In.bRide = Ride != 0;
					In.AnchorClear = 20.0;
					// in close, down to the water off her side, sit there as she steams and turns; up and out to the ceiling;
					// after the cut, in again, looking up at her a little from the water
					if (Time > 3.0 && Time < 7.0) In.Zoom = 12.0 * Dt;
					if (Time > 4.0 && Time < 9.0) In.Rise = -0.6;
					if (Time > 9.0 && Time < 11.0) In.PanRight = 0.5;
					if (Time > 20.0 && Time < 23.0) In.Tilt = 30.0 * Dt;
					if (Time > 23.0 && Time < 27.0) In.Zoom = -25.0 * Dt;
					if (Time > 33.0 && Time < 38.0) In.Zoom = 20.0 * Dt;
					if (Time > 34.0 && Time < 40.0) In.Tilt = -12.0 * Dt;
				};
				Measured M = Fly(E, Sc, K, 50.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				double Highest = -1e9;
				for (const Rec& R : M.Recs) Highest = std::max(Highest, R.P.Eye.Z);
				char Name[96];
				std::snprintf(Name, sizeof(Name), "sea ships, swell, steady 3, ride %s %s", Ride ? "on" : "off", TimingName(K));
				char Line[120];
				std::snprintf(Line, sizeof(Line), "eye up to %.1f m (EyeMost 1500)", Highest);
				Report(Name, M, Line);
				GateSmooth(Name, M);
				if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the wave", Name, M.Lowest);
				if (M.HardLifts) Fault("%s: the lift could not climb in time on %d frames", Name, M.HardLifts);
				if (Highest > 1500.0 + 1e-9) Fault("%s: the eye rose to %.2f m (EyeMost 1500)", Name, Highest);
				if (M.Cuts != 1) Fault("%s: %d cuts (one wanted: the 5 km step; the 300 m one is carried)", Name, M.Cuts);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Exact amounts and exact curves: each hand moves what it says by the amount it says, the springs step on the
	// critically damped curve itself, and the edges of Begin, of RealDt and of the world are as written.
	struct Flat : FreeEye::World
	{
		double Height = 0.0;
		bool bNan = false;
		double Floor(double, double) const override { return bNan ? std::nan("") : Height; }
	};

	/** Begins looking at Look from a heading and a tilt (degrees) on the boom of a zoom. */
	void BeginOn(FreeEye::Eye& E, const Vec& Look, double HeadingDeg, double TiltDeg, double Zoom, const Vec& Frame = Vec{})
	{
		const double Arm = E.ArmOf(Zoom), Hd = HeadingDeg * Rad, Tl = TiltDeg * Rad;
		const Vec Boom{ -std::sin(Hd) * Arm * std::cos(Tl), -std::cos(Hd) * Arm * std::cos(Tl), Arm * std::sin(Tl) };
		E.Begin(Look + Boom, Look, Frame);
	}

	void Rest(FreeEye::Eye& E, double Seconds, const FreeEye::World* W = nullptr)
	{
		FreeEye::Input In;
		for (int F = 0; F < static_cast<int>(Seconds * 60.0); ++F) E.Update(In, 1.0 / 60.0, W);
	}

	void RuleHandsExact()
	{
		const double Dt = 1.0 / 60.0;
		double Worst = 0.0;
		int Checked = 0;
		auto Check = [&Worst, &Checked](const char* What, double Zoom, double Got, double Wanted)
		{
			const double Off = std::abs(Got - Wanted) / std::max(1.0, std::abs(Wanted));
			Worst = std::max(Worst, Off);
			++Checked;
			if (!(Off <= 1e-9)) Fault("hands at zoom %.2f: %s came to %.12g (wanted %.12g)", Zoom, What, Got, Wanted);
		};
		const double Zooms[] = { 0.0, 0.3, 0.8 };
		for (double Z : Zooms)
		{
			const Vec Look{ 40.0, -70.0, 200.0 };
			auto Fresh = [&](FreeEye::Eye& E)
			{
				BeginOn(E, Look, 30.0, 25.0, Z);
				Rest(E, 0.1);
			};
			const double Hd = 30.0 * Rad;
			const Vec Right{ std::cos(Hd), -std::sin(Hd), 0.0 }, Ahead{ std::sin(Hd), std::cos(Hd), 0.0 };
			// one frame of a key, then still: the pivot moves by the key's rate times that frame, the turn and the tilt
			// by the degrees given, the zoom by its step
			auto One = [&](const std::function<void(FreeEye::Input&)>& Set, FreeEye::Eye& E)
			{
				FreeEye::Input In;
				Set(In);
				E.Update(In, Dt, nullptr);
				Rest(E, 6.0);
			};
			const double Reach = std::max(FreeEye::Eye().ArmOf(Z), 8.0);
			{
				FreeEye::Eye E;
				Fresh(E);
				const Vec P0 = E.Pivot();
				One([](FreeEye::Input& In) { In.PanRight = 1.0; }, E);
				const Vec D = E.Pivot() - P0;
				Check("a frame of pan right (along the view's right)", Z, D.X * Right.X + D.Y * Right.Y, 0.9 * Reach * Dt);
				Check("a frame of pan right (nothing ahead)", Z, D.X * Ahead.X + D.Y * Ahead.Y + D.Z, 0.0);
			}
			{
				FreeEye::Eye E;
				Fresh(E);
				const Vec P0 = E.Pivot();
				One([](FreeEye::Input& In) { In.PanAhead = -1.0; }, E);
				const Vec D = E.Pivot() - P0;
				Check("a frame of pan back (along the view's ahead)", Z, D.X * Ahead.X + D.Y * Ahead.Y, -0.9 * Reach * Dt);
				Check("a frame of pan back (nothing right)", Z, D.X * Right.X + D.Y * Right.Y + D.Z, 0.0);
			}
			{
				FreeEye::Eye E;
				Fresh(E);
				const Vec P0 = E.Pivot();
				One([](FreeEye::Input& In) { In.Rise = 1.0; }, E);
				const Vec D = E.Pivot() - P0;
				Check("a frame of rise", Z, D.Z, 0.6 * Reach * Dt);
				Check("a frame of rise (nothing level)", Z, std::hypot(D.X, D.Y), 0.0);
			}
			{
				// a slide after a zoom notch, while the boom is still growing: by what the picture shows now
				FreeEye::Eye E;
				Fresh(E);
				FreeEye::Input In;
				In.Zoom = Z > 0.5 ? 2.0 : -2.0;
				E.Update(In, Dt, nullptr);
				In = FreeEye::Input();
				for (int F = 0; F < 6; ++F) E.Update(In, Dt, nullptr);
				const double Tall = 2.0 * E.Arm() * std::tan(0.5 * E.Last().Fov * Rad);
				const Vec P0 = E.Pivot();
				In.SlideRight = 0.1;
				In.SlideAhead = -0.05;
				In.Aspect = 2.0;
				E.Update(In, Dt, nullptr);
				Rest(E, 6.0);
				const Vec D = E.Pivot() - P0;
				Check("a slide right of a tenth (its width)", Z, D.X * Right.X + D.Y * Right.Y, 0.1 * Tall * 2.0);
				Check("a slide back of a twentieth (its height)", Z, D.X * Ahead.X + D.Y * Ahead.Y, -0.05 * Tall);
			}
			{
				FreeEye::Eye E;
				Fresh(E);
				double H0, T0, H1, T1;
				LookOf(E.Last(), H0, T0);
				One([](FreeEye::Input& In) { In.Turn = 10.0; In.Tilt = 10.0; }, E);
				LookOf(E.Last(), H1, T1);
				Check("10 degrees of turn (clockwise from above)", Z, std::remainder(H1 - H0, 2.0 * Pi) * Deg, 10.0);
				Check("10 degrees of tilt (further down)", Z, (T1 - T0) * Deg, 10.0);
			}
			{
				FreeEye::Eye E;
				Fresh(E);
				const double In1 = Z > 0.0 ? 1.0 : -1.0;
				const double Want = E.ArmOf(std::clamp(Z - In1 * 0.06, 0.0, 1.0));
				One([In1](FreeEye::Input& In) { In.Zoom = In1; }, E);
				Check("a notch of zoom (the boom's length)", Z, E.Arm(), Want);
				Check("the lens at that zoom", Z, E.Last().Fov, 44.0 + 6.0 * E.Zoom());
			}
		}
		{
			// the lens follows the zoom on every frame of a zoom (not its want)
			FreeEye::Eye E;
			BeginAt(E, 0.2);
			FreeEye::Input In;
			double Off = 0.0;
			for (int F = 0; F < 240; ++F)
			{
				In.Zoom = F < 30 ? -0.3 : 0.0;
				E.Update(In, Dt, nullptr);
				Off = std::max(Off, std::abs(E.Last().Fov - (44.0 + 6.0 * E.Zoom())));
			}
			if (Off > 1e-9) Fault("hands: the lens left the zoom's by %.3g degrees on a frame", Off);
		}
		{
			// ground style: a slide alone, or a rise alone, lets go too
			for (int Which = 0; Which < 2; ++Which)
			{
				FreeEye::Eye E;
				E.Set.bPanKeepsLock = false;
				BeginAt(E, 0.4);
				FreeEye::Input In;
				In.bLock = true;
				In.AnchorId = 3;
				In.Anchor = Vec{ 10.0, 0.0, 50.0 };
				for (int F = 0; F < 60; ++F) E.Update(In, Dt, nullptr);
				(Which ? In.Rise : In.SlideRight) = 0.02;
				E.Update(In, Dt, nullptr);
				if (E.Locked() != -1) Fault("rule 4 ground style: a %s alone did not let go", Which ? "rise" : "slide");
			}
		}
		std::printf("%-44s %d amounts, worst off %.2e of each\n", "hands move by what they say", Checked, Worst);
	}

	/** The springs' curves: a small step of each (no hold acting) follows the exact critically damped curve,
	 * x(t) = 1 - (1 + w t) e^(-w t) of the step, on any frames; and a long move held to its fastest stops on its want
	 * without running past it. */
	void RuleSpringCurves()
	{
		double Worst = 0.0;
		for (Timing K : Timings)
		{
			for (int Which = 0; Which < 4; ++Which)
			{
				FreeEye::Eye E;
				BeginAt(E, 0.4);
				Rest(E, 0.1);
				const double Step = Which == 0 ? 2.0 * Rad : Which == 1 ? -2.0 * Rad : Which == 2 ? -0.01 : 1.0;
				const double W = Which == 0 ? E.Set.TurnEase : Which == 1 ? E.Set.TiltEase : Which == 2 ? E.Set.ZoomEase : E.Set.Ease;
				const double X0 = Which == 0 ? E.Heading() : Which == 1 ? E.Tilt() : Which == 2 ? E.Zoom() : E.Rel().X;
				Clock C(K, 3);
				double Time = 0.0, Off = 0.0;
				FreeEye::Input In;
				for (int F = 0; Time < (Which == 3 ? 0.4 : 1.5); ++F)
				{
					const double Dt = C.Next();
					In = FreeEye::Input();
					if (F == 0)
					{
						if (Which == 0) E.Want(X0 + Step, E.Tilt(), E.Zoom());
						if (Which == 1) E.Want(E.Heading(), X0 + Step, E.Zoom());
						if (Which == 2) E.Want(E.Heading(), E.Tilt(), X0 + Step);
						// (a slide of a metre to the right, the heading 0)
						if (Which == 3) In.SlideRight = 1.0 / (2.0 * E.Arm() * std::tan(0.5 * E.Last().Fov * Rad) * In.Aspect);
					}
					E.Update(In, Dt, nullptr);
					Time += Dt;
					const double X = Which == 0 ? E.Heading() : Which == 1 ? E.Tilt() : Which == 2 ? E.Zoom() : E.Rel().X;
					const double Curve = X0 + Step * (1.0 - (1.0 + W * Time) * std::exp(-W * Time));
					Off = std::max(Off, std::abs(X - Curve) / std::abs(Step));
				}
				Worst = std::max(Worst, Off);
				const char* What = Which == 0 ? "turn" : Which == 1 ? "tilt" : Which == 2 ? "zoom" : "offset";
				if (Off > 1e-9) Fault("springs %s: the %s's step left the critically damped curve by %.3g of it", TimingName(K), What, Off);
			}
		}
		std::printf("%-44s off the exact curve by %.2e of the step at most\n", "springs: small steps on the exact curve", Worst);
		double Past = 0.0;
		for (Timing K : Timings)
		{
			for (int Which = 0; Which < 8; ++Which)
			{
				FreeEye::Eye E;
				// (and on stiff springs, which brake late: there the holds alone stop them on the want)
				const bool bStiff = Which >= 4;
				if (bStiff) E.Set.TurnEase = E.Set.TiltEase = E.Set.ZoomEase = E.Set.Ease = 40.0;
				const int What = Which % 4;
				BeginAt(E, 0.0);
				Rest(E, 0.1);
				if (What == 0) E.Want(E.Heading() + 170.0 * Rad, E.Tilt(), E.Zoom());
				if (What == 1) E.Want(E.Heading(), 89.0 * Rad, 0.3);
				if (What == 2) E.Want(E.Heading(), E.Tilt(), 1.0);
				const double Goal = What == 0 ? E.WantHeading() : What == 1 ? 89.0 * Rad : What == 2 ? 1.0 : 0.0;
				const double Way = What == 0 ? 1.0 : 1.0;
				Clock C(K, 5);
				FreeEye::Input In;
				double Most = 0.0, Start = 0.0;
				for (int F = 0; F < 60 * 12; ++F)
				{
					In = FreeEye::Input();
					if (What == 3 && F == 0) In.SlideRight = 4.0;
					E.Update(In, C.Next(), nullptr);
					if (What == 3 && F == 0) Start = E.WantRel().X;
					const double X = What == 0 ? E.Heading() : What == 1 ? E.Tilt() : What == 2 ? E.Zoom() : E.Rel().X;
					const double G = What == 3 ? Start : Goal;
					Most = std::max(Most, Way * (X - G) / (What == 3 ? std::max(E.Arm(), E.Set.FeelArm) : 1.0));
				}
				Past = std::max(Past, Most);
				if (Most > 1e-9) Fault("springs %s%s: a long %s ran past its want by %.3g", TimingName(K), bStiff ? " stiff" : "", What == 0 ? "turn" : What == 1 ? "tilt" : What == 2 ? "zoom" : "slide", Most);
			}
		}
		std::printf("%-44s ran past its want by %.2e at most\n", "springs: long holds stop on the want", Past);
	}

	/** A long glide onto a still anchor comes to rest on it without running past (the pivot's way along the line). */
	void RuleGlideEnd()
	{
		double Past = 0.0;
		int Cuts = 0;
		for (Timing K : Timings)
		{
			FreeEye::Eye E;
			BeginAt(E, 0.7);
			const Vec Anchor{ 2900.0, 0.0, 50.0 };
			const Vec Way = (Anchor - E.Pivot()).Unit();
			Script Sc;
			Sc.Where = StillAt;
			Sc.Feed = [&Anchor](FreeEye::Input& In, FreeEye::Eye&, double Time, double)
			{
				In.bLock = Time > 0.5;
				In.AnchorId = 2;
				In.Anchor = Anchor;
			};
			Measured M = Fly(E, Sc, K, 30.0);
			Smoothness(M, Sc, E.Set.FeelArm);
			for (const Rec& R : M.Recs)
			{
				const Vec Off = R.Pivot - Anchor;
				Past = std::max(Past, Off.X * Way.X + Off.Y * Way.Y + Off.Z * Way.Z);
			}
			Cuts += M.Cuts;
			GateSmooth(std::string("glide 2.9 km onto a still anchor ") + TimingName(K), M);
		}
		std::printf("%-44s past the anchor by %.4f m at most, cuts %d\n", "glide 2.9 km onto a still anchor", Past, Cuts);
		if (Past > 0.01) Fault("glide: the pivot ran %.3f m past a still anchor", Past);
		if (Cuts) Fault("glide: a 2.9 km lock at zoom 0.7 cut");
	}

	/** The lift up on a floor risen under him (eased, held, clear of it) and back down when it goes, on the spring's
	 * own curve at LiftSettle: 1 - ... from L0 down, L0 (1 + 3 t) e^(-3 t). */
	void RuleLiftCurve()
	{
		FreeEye::Eye E;
		Flat W;
		BeginOn(E, Vec{ 0.0, 0.0, 20.0 }, 0.0, 30.0, 0.6);
		Rest(E, 0.5, &W);
		const double Low = E.Last().Eye.Z;
		W.Height = Low + 8.0;
		FreeEye::Input In;
		double Up = 0.0, Time = 0.0, Lowest = 1e9;
		int Hard0 = E.HardLifts();
		for (int F = 0; F < 300; ++F)
		{
			E.Update(In, 1.0 / 60.0, &W);
			Time += 1.0 / 60.0;
			Lowest = std::min(Lowest, E.Last().Eye.Z - W.Height);
			if (Up == 0.0 && E.Lift() > 0.9 * 11.0) Up = Time;
		}
		const double L0 = E.Lift();
		W.Height = -50.0;
		double Off = 0.0;
		Time = 0.0;
		for (int F = 0; F < 60 * 12; ++F)
		{
			E.Update(In, 1.0 / 60.0, &W);
			Time += 1.0 / 60.0;
			if (Time < 3.0) Off = std::max(Off, std::abs(E.Lift() - L0 * (1.0 + 3.0 * Time) * std::exp(-3.0 * Time)) / L0);
		}
		std::printf("%-44s up to %.4f m (11 wanted) in %.2f s to 0.9, lowest %.3f m over the floor; down off the curve by %.2e, left %.3g m\n",
			"the lift on a floor risen 8 m and gone", L0, Up, Lowest, Off, E.Lift());
		if (std::abs(L0 - 11.0) > 1e-6) Fault("lift: it stood %.4f m up for a floor 8 m over the eye (11 wanted: 8 and FloorClear)", L0);
		if (!(Up > 0.0 && Up < 0.5)) Fault("lift: it took %.2f s to climb 0.9 of 11 m (at LiftRise 12 some 0.3)", Up);
		if (E.HardLifts() != Hard0 + 1) Fault("lift: %d hard lifts for one floor risen at once (one wanted)", E.HardLifts() - Hard0);
		if (Off > 1e-6) Fault("lift: settling down left the spring's curve at LiftSettle by %.3g of it", Off);
		if (E.Lift() != 0.0) Fault("lift: 12 s after the floor went the lift still stood %.3g m", E.Lift());
	}

	/** Begin on an eye already flown (turning, panning, locked, lifted): the first frame is the shot exactly and the
	 * next stands still; and Begin's edges: straight down, straight up, under the floor, over the ceiling, between
	 * FloorHard and FloorClear. */
	void RuleBeginEdges()
	{
		{
			FreeEye::Eye E;
			Flat W;
			BeginAt(E, 0.1);
			FreeEye::Input In;
			In.bLock = true;
			In.AnchorId = 1;
			for (int F = 0; F < 120; ++F)
			{
				In.Anchor = Vec{ 2.0 * F, 0.0, 30.0 };
				In.PanRight = 1.0;
				In.Turn = 2.0;
				In.Tilt = -0.5;
				In.Zoom = 0.1;
				if (F == 60) W.Height = 60.0;
				E.Update(In, 1.0 / 60.0, &W);
			}
			const Vec Shot{ 500.0, 300.0, 900.0 }, At{ 600.0, 700.0, 500.0 };
			E.Begin(Shot, At, Vec{});
			In = FreeEye::Input();
			const FreeEye::Pose P1 = E.Update(In, 1.0 / 60.0, &W);
			const FreeEye::Pose P2 = E.Update(In, 1.0 / 60.0, &W);
			const double Off = (P1.Eye - Shot).Length(), Dir = ((P1.Look - P1.Eye).Unit() - (At - Shot).Unit()).Length();
			const double Moved = (P2.Eye - P1.Eye).Length() + (P2.Look - P1.Look).Length();
			std::printf("%-44s eye off %.2e m, direction %.2e, then moved %.2e m, locked %d, cut %d\n", "Begin on an eye already flown", Off, Dir, Moved, E.Locked(), P1.bCut ? 1 : 0);
			if (Off > 1e-9 || Dir > 1e-9) Fault("Begin again: the first frame is %.3g m and %.3g off the shot", Off, Dir);
			if (Moved > 1e-9) Fault("Begin again: the view moved %.3g m on the second frame with no input (swings or lift kept)", Moved);
			if (E.Locked() != -1 || P1.bCut) Fault("Begin again: still locked, or a cut");
		}
		struct Shot
		{
			const char* Name;
			Vec Eye, Look;
			bool bCut;
		};
		const Shot Shots[] = {
			{ "straight down", Vec{ 10.0, 20.0, 600.0 }, Vec{ 10.0, 20.0, 0.0 }, false },
			{ "straight up", Vec{ 10.0, 20.0, 40.0 }, Vec{ 10.0, 20.0, 900.0 }, false },
			{ "2 m over the floor", Vec{ 0.0, 0.0, 2.0 }, Vec{ 0.0, 300.0, 2.0 }, false },
			{ "under the sea", Vec{ 0.0, 0.0, -5.0 }, Vec{ 0.0, 300.0, 20.0 }, true },
			{ "over the ceiling", Vec{ 0.0, 0.0, 2000.0 }, Vec{ 0.0, 600.0, 1000.0 }, true },
		};
		for (const Shot& S : Shots)
		{
			for (Timing K : Timings)
			{
				FreeEye::Eye E;
				E.Set.EyeMost = 1500.0;
				E.Begin(S.Eye, S.Look, Vec{});
				Script Sc;
				Sc.Where = StillAt;
				Sc.Feed = [](FreeEye::Input&, FreeEye::Eye&, double, double) {};
				Measured M = Fly(E, Sc, K, 6.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				const FreeEye::Pose& P = M.Recs.front().P;
				const double Off = (P.Eye - S.Eye).Length(), Dir = ((P.Look - P.Eye).Unit() - (S.Look - S.Eye).Unit()).Length();
				char Name[96];
				std::snprintf(Name, sizeof(Name), "Begin %s %s", S.Name, TimingName(K));
				char Line[120];
				std::snprintf(Line, sizeof(Line), "first frame %.2e m, %.2e off the shot, cut %d", Off, Dir, P.bCut ? 1 : 0);
				Report(Name, M, Line);
				GateSmooth(Name, M);
				if (P.bCut != S.bCut) Fault("%s: the first frame %s a cut", Name, P.bCut ? "is" : "is not");
				if (!S.bCut && (Off > 1e-9 || Dir > 1e-9)) Fault("%s: the first frame is %.3g m and %.3g off the shot", Name, Off, Dir);
				if (M.Cuts != (S.bCut ? 1 : 0)) Fault("%s: %d cuts", Name, M.Cuts);
				if (!S.bCut && M.HardLifts) Fault("%s: %d hard lifts", Name, M.HardLifts);
				if (M.HardStops) Fault("%s: %d hard stops (a shot over the ceiling is let down on its first frame)", Name, M.HardStops);
				if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the floor", Name, M.Lowest);
				double High = -1e9, TiltOut = 0.0;
				for (const Rec& R : M.Recs)
				{
					High = std::max(High, R.P.Eye.Z);
					double Hh, Tt;
					LookOf(R.P, Hh, Tt);
					if (R.Time > 3.0) TiltOut = std::max({ TiltOut, Tt - E.Set.TiltMost * Rad, E.Set.TiltLeast * Rad - Tt });
				}
				if (High > 1500.0 + 1e-9) Fault("%s: the eye stood at %.2f m (EyeMost 1500)", Name, High);
				if (TiltOut > 1e-9) Fault("%s: 3 s on the tilt is still %.3g degrees out of its range", Name, TiltOut * Deg);
			}
		}
	}

	/** The edges of time and of the world: a frame of no time after a cut is no cut; RealDt not finite is no time; a
	 * floor not finite is FloorFlat; FloorFlat is kept with no world; a wave risen on a frame of no time is held. */
	void RuleEdges()
	{
		{
			FreeEye::Eye E;
			BeginAt(E, 0.4);
			const double T0 = E.WantTilt(), S0 = E.WantZoom();
			E.Want(std::nan(""), -INFINITY, INFINITY);
			if (E.WantTilt() != T0 || E.WantZoom() != S0) Fault("edges: Want of a tilt or zoom not finite changed it (%.4f, %.4f)", E.WantTilt(), E.WantZoom());
			FreeEye::Input In;
			In.bLock = true;
			In.AnchorId = 2;
			In.Anchor = Vec{ 50.0, 0.0, 50.0 };
			In.bLetGo = true;
			E.Update(In, 1.0 / 60.0, nullptr);
			const int OnLetGo = E.Locked();
			FreeEye::Input Nan;
			Nan.bLock = true;
			Nan.AnchorId = 3;
			Nan.Anchor = Vec{ std::nan(""), 0.0, 0.0 };
			E.Update(Nan, 1.0 / 60.0, nullptr);
			const int OnNan = E.Locked();
			const bool bFinite = E.Last().Eye.Finite() && E.Pivot().Finite();
			if (OnLetGo != -1) Fault("edges: a rise of bLock with bLetGo on the same frame locked on");
			if (OnNan != -1 || !bFinite) Fault("edges: a lock on an anchor not finite took (%d), or the view went not finite", OnNan);
		}
		{
			FreeEye::Eye E;
			BeginAt(E, 0.4);
			FreeEye::Input In;
			In.bLock = true;
			In.AnchorId = 1;
			In.Anchor = Vec{ 9000.0, 0.0, 100.0 };
			const bool bCut = E.Update(In, 1.0 / 60.0, nullptr).bCut;
			const bool bAgain = E.Update(In, 0.0, nullptr).bCut;
			if (!bCut || bAgain) Fault("edges: a far lock cut %d, then a frame of no time cut %d (1 then 0 wanted)", bCut ? 1 : 0, bAgain ? 1 : 0);
			In.PanRight = 1.0;
			E.Update(In, 1.0 / 60.0, nullptr);
			const FreeEye::Pose Was = E.Last();
			const Vec Rel0 = E.Rel();
			const FreeEye::Pose A = E.Update(In, std::nan(""), nullptr);
			const FreeEye::Pose B = E.Update(In, -INFINITY, nullptr);
			if (!Same(A, Was) || !Same(B, Was) || E.Rel().X != Rel0.X) Fault("edges: a RealDt not finite moved the view");
		}
		{
			Flat W;
			W.bNan = true;
			for (int WithWorld = 0; WithWorld < 2; ++WithWorld)
			{
				FreeEye::Eye E;
				E.Set.FloorFlat = 100.0;
				BeginOn(E, Vec{ 0.0, 0.0, 112.0 }, 0.0, 20.0, 0.2);
				FreeEye::Input In;
				double Lowest = 1e9;
				for (int F = 0; F < 600; ++F)
				{
					In.Rise = F < 400 ? -1.0 : 0.0;
					E.Update(In, 1.0 / 60.0, WithWorld ? &W : nullptr);
					Lowest = std::min(Lowest, E.Last().Eye.Z);
				}
				std::printf("%-44s lowest %.3f m (FloorFlat 100)\n", WithWorld ? "a world whose floor is not finite" : "no world, FloorFlat 100", Lowest);
				if (Lowest < 100.5 - 1e-9) Fault("edges: %s, the eye went down to %.3f m (FloorFlat 100)", WithWorld ? "a floor not finite" : "no world", Lowest);
				if (E.Last().Eye.Z < 103.0 - 1e-6) Fault("edges: %s, the eye came to rest %.3f m over FloorFlat", WithWorld ? "a floor not finite" : "no world", E.Last().Eye.Z - 100.0);
			}
		}
		{
			FreeEye::Eye E;
			Flat W;
			BeginAt(E, 0.3);
			Rest(E, 0.2, &W);
			W.Height = E.Last().Eye.Z + 2.0;
			const FreeEye::Pose P = E.Update(FreeEye::Input(), 0.0, &W);
			if (P.Eye.Z < W.Height + E.Set.FloorHard - 1e-9) Fault("edges: a wave risen over the eye on a frame of no time left it %.3f m under", W.Height - P.Eye.Z);
		}
	}

	/** A new frame (a new FrameId, or the same one jumped as a group joins the raid) is carried, never flung, and a
	 * view let go at speed comes to rest without turning back; also a let go or a lock on a frame that jumps. */
	void RuleFrames()
	{
		for (Timing K : Timings)
		{
			for (int Which = 0; Which < 3; ++Which)
			{
				FreeEye::Eye E;
				auto FrameOf = [Which](double Bt, int& Id)
				{
					Id = Bt < 5.0 ? 0 : 1;
					if (Bt < 5.0) return Vec{ 100.0 * Bt, 0.0, 3000.0 };
					if (Which == 0) return Vec{ 500.0, 900.0 + 100.0 * (Bt - 5.0), 3000.0 };
					if (Which == 1) return Vec{ 500.0 + 600.0, 0.0, 3000.0 };
					Id = 0;
					return Vec{ 500.0 + 600.0 + 100.0 * (Bt - 5.0), 0.0, 3000.0 };
				};
				int Id0 = 0;
				BeginAt(E, 0.5, FrameOf(0.0, Id0));
				Script Sc;
				Sc.Where = StillAt;
				Sc.Feed = [&FrameOf](FreeEye::Input& In, FreeEye::Eye&, double, double Bt)
				{
					In.Frame = FrameOf(Bt, In.FrameId);
				};
				Measured M = Fly(E, Sc, K, 14.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				double Back = 0.0;
				for (size_t I = 1; I < M.Recs.size(); ++I)
				{
					if (M.Recs[I].Time < 5.0 || Which != 1) continue;
					Back = std::max(Back, -(M.Recs[I].Pivot.X - M.Recs[I - 1].Pivot.X));
				}
				char Name[96];
				std::snprintf(Name, sizeof(Name), "frame %s %s", Which == 0 ? "changed (new id, 900 m off)" : Which == 1 ? "changed and still" : "jumped 600 m, same id", TimingName(K));
				char Line[64];
				std::snprintf(Line, sizeof(Line), "turned back %.3g m", Back);
				Report(Name, M, Line);
				GateSmooth(Name, M);
				if (M.Cuts) Fault("%s: %d cuts", Name, M.Cuts);
				if (Back > 1e-9) Fault("%s: the view let go at 100 m/s turned back by %.3g m", Name, Back);
			}
			const double Jumps[] = { 40.0, 200.0, 1500.0 };
			for (double J : Jumps)
			{
				for (int Lock = 0; Lock < 2; ++Lock)
				{
					FreeEye::Eye E;
					auto Plane = [](int, double Bt) { return Vec{ 150.0 * Bt, 0.0, 2000.0 }; };
					auto FrameAt = [J](double Bt) { return Vec{ 0.0, 80.0 * Bt + (Bt > 6.0 ? J : 0.0), 2000.0 }; };
					BeginAt(E, 0.6, Plane(1, 0.0));
					Script Sc;
					Sc.Where = StillAt;
					Sc.Feed = [&Plane, &FrameAt, Lock, J](FreeEye::Input& In, FreeEye::Eye&, double, double Bt)
					{
						In.Frame = FrameAt(Bt);
						// (a shift under TeleportBeyond is a new frame only by its id: the scene's to say, FREE-EYE.md)
						In.FrameId = Bt > 6.0 && J < 1000.0 ? 1 : 0;
						In.bLock = Lock ? Bt > 6.0 : Bt < 6.0;
						In.AnchorId = 1;
						In.Anchor = Plane(1, Bt);
					};
					Measured M = Fly(E, Sc, K, 16.0);
					Smoothness(M, Sc, E.Set.FeelArm);
					char Name[96];
					std::snprintf(Name, sizeof(Name), "%s as the frame jumps %.0f m %s", Lock ? "lock" : "let go", J, TimingName(K));
					Report(Name, M);
					GateSmooth(Name, M);
					if (M.Cuts) Fault("%s: %d cuts", Name, M.Cuts);
				}
			}
		}
	}

	/** Cuts and teleports start the view from rest where they put it: no turn, tilt, zoom, offset or lift carried
	 * across; and the teleport's slack is three frames of the anchor's own speed. */
	void RuleCutsClean()
	{
		const double Dt = 1.0 / 60.0;
		{
			FreeEye::Eye E;
			Flat W;
			W.Height = 0.0;
			BeginOn(E, Vec{ 0.0, 0.0, 30.0 }, 0.0, 10.0, 0.3);
			FreeEye::Input In;
			for (int F = 0; F < 120; ++F)
			{
				In.Turn = 2.5;
				In.Tilt = 0.6;
				In.Zoom = 0.02;
				In.Rise = -1.0;
				if (F == 30) W.Height = 40.0;
				E.Update(In, Dt, &W);
			}
			const double LiftWas = E.Lift();
			W.Height = 0.0;
			In = FreeEye::Input();
			In.bLock = true;
			In.AnchorId = 4;
			In.Anchor = Vec{ 8000.0, 0.0, 200.0 };
			const double H0 = E.Heading(), T0 = E.Tilt(), S0 = E.Zoom();
			const bool bCut = E.Update(In, Dt, &W).bCut;
			const double H1 = E.Heading(), T1 = E.Tilt(), S1 = E.Zoom();
			const Vec Rel1 = E.Rel();
			E.Update(In, Dt, &W);
			const double DH = std::abs(std::remainder(E.Heading() - H1, 2.0 * Pi)), DT = std::abs(E.Tilt() - T1), DS = std::abs(E.Zoom() - S1);
			const double DRel = (E.Rel() - Rel1).Length();
			const double Most = 1.5 * Dt * Dt * 1.0001;
			std::printf("%-44s cut %d; then turned %.3g deg, tilted %.3g deg, zoomed %.3g, offset %.3g m; lift %.3g m (was %.1f)\n",
				"a cut while turning, tilting, zooming, lifted", bCut ? 1 : 0, DH * Deg, DT * Deg, DS, DRel, E.Lift(), LiftWas);
			if (!bCut) Fault("cuts: an 8 km lock did not cut");
			if (DH > E.Set.TurnPush * Rad * Most || DT > E.Set.TiltPush * Rad * Most || DS > E.Set.ZoomPush * Most)
			{
				Fault("cuts: the frame after a cut turned %.3g deg, tilted %.3g deg, zoomed %.3g: a swing carried across the cut", DH * Deg, DT * Deg, DS);
			}
			if (DRel > 1e-9) Fault("cuts: the offset moved %.3g m the frame after a cut onto it", DRel);
			if (E.Lift() > 1e-9) Fault("cuts: a cut to open water kept a lift of %.3g m", E.Lift());
			(void)H0;
			(void)T0;
			(void)S0;
		}
		{
			// a cut is judged from where the pivot is, not where he wants it: a slide far off, then a lock right here
			FreeEye::Eye E;
			BeginAt(E, 0.8);
			FreeEye::Input In;
			In.SlideRight = 4.0;
			E.Update(In, Dt, nullptr);
			In = FreeEye::Input();
			In.bLock = true;
			In.AnchorId = 2;
			In.Anchor = E.Pivot() + Vec{ 50.0, 0.0, 0.0 };
			const bool bCut = E.Update(In, Dt, nullptr).bCut;
			if (bCut) Fault("cuts: a lock 50 m from the pivot cut because his slide wanted it %.0f m off", E.WantRel().Length());
		}
		{
			// a teleport mid-glide: the glide starts again from rest, and the slack is three frames of speed
			FreeEye::Eye E;
			BeginAt(E, 0.4);
			FreeEye::Input In;
			In.bLock = true;
			In.AnchorId = 5;
			int Cuts = 0;
			double DRel = 0.0;
			Vec RelCut;
			for (int F = 0; F < 120; ++F)
			{
				In.Anchor = Vec{ 400.0 + (F >= 40 ? 1500.0 : 0.0), 0.0, 50.0 };
				const bool bCut = E.Update(In, Dt, nullptr).bCut;
				Cuts += bCut ? 1 : 0;
				if (bCut) RelCut = E.Rel();
				if (F == 41) DRel = (E.Rel() - RelCut).Length();
			}
			const double Most = 1.5 * E.Set.GlidePush * std::max(E.Arm(), E.Set.FeelArm) * Dt * Dt * 1.0001;
			std::printf("%-44s cuts %d; the offset then moved %.4f m (a push from rest %.4f)\n", "a teleport mid-glide", Cuts, DRel, Most);
			if (Cuts != 1) Fault("teleport: %d cuts for one 1.5 km jump mid-glide", Cuts);
			if (DRel > Most) Fault("teleport: the glide went on at its speed across a teleport (%.3f m in a frame)", DRel);
		}
		{
			// a let go on the frame after a teleport: the cut's own step is no speed to carry
			FreeEye::Eye E;
			BeginAt(E, 0.4);
			FreeEye::Input In;
			In.AnchorId = 5;
			double Most = 0.0;
			Vec Was;
			for (int F = 0; F < 200; ++F)
			{
				In.bLock = F < 61;
				In.Anchor = Vec{ 100.0 + (F >= 60 ? 1500.0 : 0.0), 0.0, 50.0 };
				E.Update(In, Dt, nullptr);
				if (F > 60) Most = std::max(Most, (E.Pivot() - Was).Length());
				Was = E.Pivot();
			}
			std::printf("%-44s the pivot then moved %.3f m a frame at most\n", "a let go the frame after a teleport", Most);
			if (Most > 1.0) Fault("teleport: a let go the frame after a teleport flung the pivot %.1f m in a frame", Most);
		}
		for (int Big = 0; Big < 2; ++Big)
		{
			FreeEye::Eye E;
			BeginAt(E, 0.5, Vec{});
			FreeEye::Input In;
			In.bLock = true;
			In.AnchorId = 6;
			In.bAnchorVel = true;
			In.AnchorVel = Vec{ 1200.0, 0.0, 0.0 };
			int Cuts = 0;
			double X = 0.0;
			for (int F = 0; F < 600; ++F)
			{
				// one frame's step of 200 m and two frames of its speed (no teleport), or 200 m and three and a half
				X += 20.0 + (F == 400 ? (Big ? 270.0 : 240.0) - 20.0 : 0.0);
				In.Anchor = Vec{ X, 0.0, 50.0 };
				Cuts += E.Update(In, Dt, nullptr).bCut && F > 0 ? 1 : 0;
			}
			if (Cuts != Big) Fault("teleport: a step of %s at 1200 m/s cut %d times (%d wanted: TeleportBeyond and three frames of its speed)", Big ? "270 m" : "240 m", Cuts, Big);
		}
	}

	/** Ride turns where he wants to sit with the ship, and a skip to a ship headed otherwise is no turn of the view;
	 * a skip between two heaving ships with their speed fed is carried as smoothly as one measured. */
	void RuleRideMore()
	{
		{
			FreeEye::Eye E;
			BeginAt(E, 0.4);
			FreeEye::Input In;
			In.bLock = true;
			In.AnchorId = 1;
			In.bRide = true;
			double Jump = 0.0;
			Vec Want0;
			for (int F = 0; F < 60 * 30; ++F)
			{
				const double Bt = F / 60.0;
				In.AnchorId = Bt < 25.0 ? 1 : 2;
				In.AnchorHeading = Bt < 25.0 ? ShipHeading(Bt) : 2.5;
				In.Anchor = In.AnchorId == 1 ? ShipAt(Bt, 0.0) : ShipAt(Bt, 0.0) + Vec{ 300.0, 0.0, 0.0 };
				In.PanAhead = Bt > 0.5 && Bt < 1.5 ? 1.0 : 0.0;
				const double Hw = E.Heading();
				E.Update(In, 1.0 / 60.0, nullptr);
				if (F == 120) Want0 = E.WantRel();
				if (In.AnchorId == 2) Jump = std::max(Jump, std::abs(std::remainder(E.Heading() - Hw, 2.0 * Pi)));
				if (F == 60 * 25 - 1)
				{
					const Vec W1 = E.WantRel();
					const double Turned = std::atan2(W1.X, W1.Y) - std::atan2(Want0.X, Want0.Y);
					std::printf("%-44s his offset turned %.3f deg with the ship's 90\n", "rule 7 ride turns where he sits", Turned * Deg);
					if (std::abs(Turned * Deg - 90.0) > 0.01) Fault("rule 7: where he sits off her turned %.3f deg as she turned 90", Turned * Deg);
				}
			}
			std::printf("%-44s the view's heading moved %.3f deg a frame at most after the skip\n", "rule 7 skip to a ship headed otherwise", Jump * Deg);
			if (Jump * Deg > 2.4) Fault("rule 7: the skip to a ship headed otherwise turned the view %.2f deg in a frame", Jump * Deg);
		}
		for (Timing K : Timings)
		{
			for (int Fed = 0; Fed < 2; ++Fed)
			{
				FreeEye::Eye E;
				E.Set.SteadyZ = 3.0;
				auto Ship = [](int Key, double Bt)
				{
					return Vec{ 200.0 * Key, 12.0 * Bt, 8.0 + (Key == 1 ? 3.0 : -3.0) * std::sin(2.0 * Pi * Bt / 8.0 + Key) };
				};
				BeginAt(E, 0.35);
				Script Sc;
				Sc.Where = [&Ship](int Key, double Bt) { return Key < 0 ? Vec{} : Ship(Key, Bt); };
				Sc.Feed = [&Ship, Fed](FreeEye::Input& In, FreeEye::Eye&, double, double Bt)
				{
					In.bLock = true;
					In.AnchorId = Bt < 10.0 ? 0 : 1;
					In.Anchor = Ship(In.AnchorId, Bt);
					In.bAnchorVel = Fed != 0;
					In.AnchorVel = (Ship(In.AnchorId, Bt + 1e-4) - Ship(In.AnchorId, Bt - 1e-4)) * (1.0 / 2e-4);
				};
				Measured M = Fly(E, Sc, K, 20.0);
				Smoothness(M, Sc, E.Set.FeelArm, 0.5);
				char Name[96];
				std::snprintf(Name, sizeof(Name), "rule 8 skip between heaving ships, %s %s", Fed ? "fed" : "measured", TimingName(K));
				Report(Name, M);
				GateSmooth(Name, M);
				if (M.Cuts) Fault("%s: %d cuts", Name, M.Cuts);
			}
		}
	}

	/** The caps' edges: a ceiling lowered under a rising eye stops it once; the anchor's clearance is no rule after a
	 * let go; a box given with its corners swapped is the same box. */
	void RuleCapsMore()
	{
		{
			FreeEye::Eye E;
			E.Set.EyeMost = 5000.0;
			BeginOn(E, Vec{ 0.0, 0.0, 1000.0 }, 0.0, 20.0, 0.5);
			FreeEye::Input In;
			int Stops = 0;
			double High = 0.0;
			for (int F = 0; F < 600; ++F)
			{
				In.Rise = F < 300 ? 1.0 : 0.0;
				if (F == 150) E.Set.EyeMost = E.Last().Eye.Z + 1.0;
				E.Update(In, 1.0 / 60.0, nullptr);
				if (F >= 150) High = std::max(High, E.Last().Eye.Z - E.Set.EyeMost);
			}
			Stops = E.HardStops();
			std::printf("%-44s hard stops %d, over it by %.3g m at most\n", "a ceiling lowered onto a rising eye", Stops, High);
			if (Stops != 1) Fault("caps: a ceiling lowered once onto a rising eye stopped it %d times (once: its rise is spent there)", Stops);
			if (High > 1e-9) Fault("caps: the eye stood %.3g m over a lowered ceiling", High);
		}
		{
			FreeEye::Eye E;
			BeginAt(E, 0.0);
			FreeEye::Input In;
			double Nearest = 1e9;
			for (int F = 0; F < 600; ++F)
			{
				In.bLock = F < 60;
				In.AnchorId = 3;
				In.Anchor = Vec{ 0.0, 0.0, 50.0 };
				In.AnchorClear = 30.0;
				In.PanAhead = F > 120 && F < 300 ? 1.0 : 0.0;
				E.Update(In, 1.0 / 60.0, nullptr);
				if (F > 120) Nearest = std::min(Nearest, (E.Last().Eye - In.Anchor).Length());
			}
			std::printf("%-44s nearest %.2f m (clearance 30, no rule unlocked), hard stops %d\n", "the anchor's clearance after a let go", Nearest, E.HardStops());
			if (Nearest > 29.0) Fault("caps: after a let go the eye was still kept %.2f m off the anchor it let go of", Nearest);
			if (E.HardStops()) Fault("caps: %d hard stops with nothing locked", E.HardStops());
		}
		{
			double Out = 0.0, Reached = 1e9;
			for (int Swap = 0; Swap < 2; ++Swap)
			{
				FreeEye::Eye E;
				BeginAt(E, 0.4);
				FreeEye::Input In;
				double Lo = 0.0, Hi = 0.0;
				for (int F = 0; F < 1000; ++F)
				{
					In.bBox = true;
					In.BoxMin = Swap ? Vec{ 100.0, 100.0, 300.0 } : Vec{ -100.0, -100.0, -300.0 };
					In.BoxMax = Swap ? Vec{ -100.0, -100.0, -300.0 } : Vec{ 100.0, 100.0, 300.0 };
					In.PanRight = F < 300 ? 1.0 : -1.0;
					In.PanAhead = F < 300 ? -1.0 : 1.0;
					E.Update(In, 1.0 / 60.0, nullptr);
					const Vec P = E.Pivot();
					Out = std::max({ Out, std::abs(P.X) - 100.0, std::abs(P.Y) - 100.0, std::abs(P.Z) - 300.0 });
					Lo = std::min(Lo, P.X);
					Hi = std::max(Hi, P.X);
				}
				Reached = std::min({ Reached, -Lo, Hi });
			}
			if (Reached < 90.0) Fault("caps: a box with its corners swapped held the pivot short of a wall it should reach (%.1f m of 100)", Reached);
			std::printf("%-44s out of it by %.3g m at most\n", "a box with its corners given swapped", Out);
			if (Out > 1e-9) Fault("caps: a box with its corners swapped let the pivot %.3g m out", Out);
		}
	}

	/** The carries review round 1 found rough, each now flown and gated: a let go at speed, a lock on a much faster
	 * aircraft, an aircraft met head on and one passing through the eye, a moving frame run into a fixed box. */
	void RuleCarries()
	{
		for (Timing K : Timings)
		{
			const double Zooms[] = { 0.0, 0.4 };
			const double Paces[] = { 1.0, 4.0 };
			for (double Z : Zooms)
			{
				for (double Pace : Paces)
				{
					// let go of an aircraft at 150 m/s of battle time, the frame still: it coasts on and never turns back
					FreeEye::Eye E;
					auto Plane = [](int Key, double Bt) { return Key < 0 ? Vec{} : Vec{ 150.0 * Bt, 0.0, 800.0 }; };
					BeginAt(E, Z, Plane(1, 0.0));
					Script Sc;
					Sc.Where = Plane;
					Sc.Pace = [Pace](double) { return Pace; };
					Sc.Feed = [&Plane](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
					{
						In.bLock = Time < 6.0;
						In.AnchorId = 1;
						In.Anchor = Plane(1, Bt);
					};
					Measured M = Fly(E, Sc, K, 16.0);
					Smoothness(M, Sc, E.Set.FeelArm);
					double Back = 0.0;
					for (size_t I = 1; I < M.Recs.size(); ++I)
					{
						if (M.Recs[I].Time > 6.0) Back = std::max(Back, M.Recs[I - 1].Pivot.X - M.Recs[I].Pivot.X);
					}
					char Name[96];
					std::snprintf(Name, sizeof(Name), "let go at %.0f m/s, zoom %.1f %s", 150.0 * Pace, Z, TimingName(K));
					char Line[64];
					std::snprintf(Line, sizeof(Line), "turned back %.3g m", Back);
					Report(Name, M, Line);
					GateSmooth(Name, M);
					// (the lock itself on a plane four times faster than the view glides may cut; the let go never)
					int LateCuts = 0;
					for (const Rec& R : M.Recs) LateCuts += R.P.bCut && R.Time > 1.0 ? 1 : 0;
					if (Back > 1e-9) Fault("%s: the view turned back %.3g m", Name, Back);
					if (LateCuts) Fault("%s: %d cuts after the lock", Name, LateCuts);
				}
			}
			const double Fast[] = { 300.0, 600.0, 1500.0 };
			for (double Z : Zooms)
			{
				for (double V : Fast)
				{
					// a lock on an aircraft much faster than the view: glided onto within GlideLongest, or cut
					FreeEye::Eye E;
					auto Plane = [V](int Key, double Bt) { return Key < 0 ? Vec{} : Vec{ -300.0 + V * Bt, 200.0, 900.0 }; };
					BeginAt(E, Z, Vec{ 0.0, 0.0, 850.0 });
					Script Sc;
					Sc.Where = Plane;
					Sc.Feed = [&Plane](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
					{
						In.bLock = Time > 0.3;
						In.AnchorId = 1;
						In.Anchor = Plane(1, Bt);
						In.bAnchorVel = true;
						In.AnchorVel = Plane(1, Bt + 1.0) - Plane(1, Bt);
					};
					Measured M = Fly(E, Sc, K, 24.0);
					Smoothness(M, Sc, E.Set.FeelArm);
					double Stray = 0.0, OnAt = -1.0;
					for (const Rec& R : M.Recs)
					{
						if (R.Key < 0) continue;
						const double Off = (R.Pivot - Plane(1, R.Bt)).Length();
						Stray = std::max(Stray, Off);
						if (OnAt < 0.0 && Off < 1.0) OnAt = R.Time;
					}
					char Name[96];
					std::snprintf(Name, sizeof(Name), "lock on %.0f m/s, zoom %.1f %s", V, Z, TimingName(K));
					char Line[96];
					std::snprintf(Line, sizeof(Line), "strayed %.0f m, on it at %.1f s", Stray, OnAt);
					Report(Name, M, Line);
					GateSmooth(Name, M);
					if (Stray > E.Set.CutBeyond) Fault("%s: the pivot fell %.0f m behind it (CutBeyond %.0f)", Name, Stray, E.Set.CutBeyond);
					if (M.Cuts == 0 && !(OnAt > 0.0 && OnAt < E.Set.GlideLongest + 2.0)) Fault("%s: not cut, and on it only at %.1f s", Name, OnAt);
				}
			}
			for (int Side = 0; Side < 3; ++Side)
			{
				// an aircraft met head on (dead on, 40 m off), and one locked as it passes through the eye
				FreeEye::Eye E;
				const double Off = Side == 1 ? 40.0 : 0.0;
				auto Plane = [Off, Side](int Key, double Bt)
				{
					if (Key < 0) return Vec{};
					return Side == 2 ? Vec{ Off, -200.0 + 200.0 * Bt, 1000.0 } : Vec{ Off, 700.0 - 200.0 * Bt, 1000.0 };
				};
				BeginOn(E, Vec{ 0.0, 0.0, 1000.0 }, 0.0, 5.0, 0.2);
				const double LockAt = Side == 2 ? 1.0 - E.Arm() * std::cos(5.0 * Rad) / 200.0 : 0.5;
				Script Sc;
				Sc.Where = Plane;
				Sc.Feed = [&Plane, LockAt](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
				{
					In.bLock = Time > LockAt;
					In.AnchorId = 1;
					In.Anchor = Plane(1, Bt);
					In.bAnchorVel = true;
					In.AnchorVel = Plane(1, Bt + 1.0) - Plane(1, Bt);
					In.AnchorClear = 30.0;
				};
				Measured M = Fly(E, Sc, K, 10.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				char Name[96];
				std::snprintf(Name, sizeof(Name), "%s %s", Side == 0 ? "head on, dead on" : Side == 1 ? "head on, 40 m off" : "locked as it passes through the eye", TimingName(K));
				Report(Name, M);
				GateSmooth(Name, M);
				if (M.HardStops) Fault("%s: %d hard stops at its clearance", Name, M.HardStops);
			}
			{
				// let go of a 6 g turn at four times the battle's pace, its speed fed for the end of the frame (half a
				// frame late for the movement this frame, which the carry must not take up). (A skip to a wingman in
				// such a turn is no test of smoothness: a glide onto a reference turning at 940 m/s/s is felt at that.)
				FreeEye::Eye E;
				auto Turner = [](int Key, double Bt)
				{
					if (Key < 0) return Vec{};
					const double A = 111.0 / 210.0 * Bt;
					return Vec{ 210.0 * std::sin(A), 210.0 * std::cos(A), 1500.0 };
				};
				BeginAt(E, 0.3, Vec{ 0.0, 210.0, 1450.0 });
				Script Sc;
				Sc.Where = Turner;
				Sc.Pace = [](double) { return 4.0; };
				Sc.Feed = [&Turner](FreeEye::Input& In, FreeEye::Eye&, double Time, double Bt)
				{
					In.Frame = Vec{ 0.0, 210.0, 1450.0 };
					In.bLock = Time < 3.0;
					In.AnchorId = 1;
					In.Anchor = Turner(1, Bt);
					In.bAnchorVel = true;
					In.AnchorVel = (Turner(1, Bt + 1e-4) - Turner(1, Bt - 1e-4)) * (4.0 / 2e-4);
				};
				Measured M = Fly(E, Sc, K, 8.0);
				Smoothness(M, Sc, E.Set.FeelArm, 1.0);
				const std::string Name = std::string("let go of a 6 g turn at pace 4, fed ") + TimingName(K);
				Report(Name, M);
				GateSmooth(Name, M);
			}
			{
				// a let go on the frame a new frame comes in: its speed measured a frame on, his rest then set by it
				FreeEye::Eye E;
				auto Plane = [](int Key, double Bt) { return Key < 0 ? Vec{} : Vec{ 150.0 * Bt, 0.0, 800.0 }; };
				auto FrameAt = [](double Bt) { return Bt < 4.0 ? Vec{} : Vec{ 0.0, 100.0 * (Bt - 4.0), 0.0 }; };
				BeginAt(E, 0.4, Vec{});
				Script Sc;
				Sc.Where = StillAt;
				Sc.Feed = [&Plane, &FrameAt](FreeEye::Input& In, FreeEye::Eye&, double, double Bt)
				{
					In.Frame = FrameAt(Bt);
					In.FrameId = Bt < 4.0 ? 0 : 1;
					In.bLock = Bt < 4.0;
					In.AnchorId = 1;
					In.Anchor = Plane(1, Bt);
				};
				Measured M = Fly(E, Sc, K, 14.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				// (in the new frame he goes on at 150 east and -100 north and comes to rest: never turning back)
				double Back = 0.0;
				for (size_t I = 1; I < M.Recs.size(); ++I)
				{
					if (M.Recs[I - 1].Bt < 4.0) continue;
					const Vec D = (M.Recs[I].Pivot - FrameAt(M.Recs[I].Bt)) - (M.Recs[I - 1].Pivot - FrameAt(M.Recs[I - 1].Bt));
					Back = std::max(Back, -(D.X * 150.0 - D.Y * 100.0) / std::hypot(150.0, 100.0));
				}
				char Name[96];
				std::snprintf(Name, sizeof(Name), "let go as a new frame comes in %s", TimingName(K));
				char Line[64];
				std::snprintf(Line, sizeof(Line), "turned back %.3g m", Back);
				Report(Name, M, Line);
				GateSmooth(Name, M);
				if (Back > 1e-9) Fault("%s: the view turned back %.3g m in the new frame", Name, Back);
				if (M.Cuts) Fault("%s: %d cuts", Name, M.Cuts);
			}
			{
				// a long boom near the ceiling tilted down at once: the tilt waits for the pivot to make room
				FreeEye::Eye E;
				E.Set.EyeMost = 1500.0;
				// (his offset slow, so the pivot is slow to make room)
				E.Set.GlidePush = 1.0;
				E.Set.GlideFastest = 0.5;
				BeginOn(E, Vec{ 0.0, 0.0, 1400.0 }, 0.0, 10.0, 0.6);
				E.Want(E.Heading(), 89.0 * Rad, 0.75);
				Script Sc;
				Sc.Where = StillAt;
				Sc.Feed = [](FreeEye::Input&, FreeEye::Eye&, double, double) {};
				Measured M = Fly(E, Sc, K, 10.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				char Name[96];
				std::snprintf(Name, sizeof(Name), "tilted down and out under the ceiling %s", TimingName(K));
				Report(Name, M);
				GateSmooth(Name, M);
				if (M.HardStops) Fault("%s: %d hard stops at the ceiling", Name, M.HardStops);
			}
			{
				// low over a ridge at 130 m/s: the lift looks ahead and climbs before the slope is under the eye
				struct Ridge : FreeEye::World
				{
					double Floor(double X, double) const override { return std::clamp((X - 1500.0) * 0.35, 0.0, 60.0); }
				};
				Ridge W;
				FreeEye::Eye E;
				auto FrameAt = [](double Bt) { return Vec{ 130.0 * Bt, 0.0, 0.0 }; };
				BeginOn(E, Vec{ 0.0, 0.0, 8.0 }, 90.0, 20.0, 0.3);
				Script Sc;
				Sc.Where = [&FrameAt](int, double Bt) { return FrameAt(Bt); };
				Sc.Ground = &W;
				Sc.Feed = [&FrameAt](FreeEye::Input& In, FreeEye::Eye&, double, double Bt) { In.Frame = FrameAt(Bt); };
				Measured M = Fly(E, Sc, K, 16.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				char Name[96];
				std::snprintf(Name, sizeof(Name), "low over a ridge at 130 m/s %s", TimingName(K));
				Report(Name, M);
				GateSmooth(Name, M);
				if (M.HardLifts) Fault("%s: %d hard lifts", Name, M.HardLifts);
				if (M.Lowest < 0.9 * E.Set.FloorClear) Fault("%s: the eye came down to %.2f m over the slope (0.9 of FloorClear wanted)", Name, M.Lowest);
			}
			const double BoxZooms[] = { 0.3, 0.6 };
			for (double Z : BoxZooms)
			{
				// a frame running at 100 m/s into a fixed box: the pivot is met by his offset, not stopped on the wall
				FreeEye::Eye E;
				auto FrameAt = [](double Bt) { return Vec{ 100.0 * Bt, 0.0, 2000.0 }; };
				BeginAt(E, Z, FrameAt(0.0));
				Script Sc;
				Sc.Where = StillAt;
				Sc.Feed = [&FrameAt](FreeEye::Input& In, FreeEye::Eye&, double, double Bt)
				{
					In.Frame = FrameAt(Bt);
					In.bBox = true;
					In.BoxMin = Vec{ -5000.0, -5000.0, 0.0 };
					In.BoxMax = Vec{ 600.0, 5000.0, 6000.0 };
				};
				Measured M = Fly(E, Sc, K, 14.0);
				Smoothness(M, Sc, E.Set.FeelArm);
				char Name[96];
				std::snprintf(Name, sizeof(Name), "a frame run into a fixed box, zoom %.1f %s", Z, TimingName(K));
				Report(Name, M);
				GateSmooth(Name, M);
				if (M.HardStops) Fault("%s: %d hard stops on the box", Name, M.HardStops);
			}
		}
	}

	/** Rule 2 under random hands (keys pressed and let go at random, wheel notches, two-finger slides, turns and tilts),
	 * high in a clear sky over a still frame, twenty minutes a timing: every move is his, so the picture is held to the
	 * kick all the way, and the ceiling (which his rise and his zoom meet often here) holds by its wants. A ceiling met
	 * as the pivot's spring comes in sideways may still be stopped by a centimetre or two now and then; more than a
	 * handful of stops means it is being held by the hard rule again. */
	void RuleHandsRandom()
	{
		for (uint64_t Seed : { 1ULL, 2ULL })
		{
			for (Timing K : Timings)
			{
				FreeEye::Eye E;
				BeginAt(E, 0.5, Vec{ 0.0, 0.0, 6000.0 });
				Lcg R(Seed);
				FreeEye::Input Held;
				Script Sc;
				Sc.Where = StillAt;
				Sc.Feed = [&R, &Held](FreeEye::Input& In, FreeEye::Eye&, double, double)
				{
					if (R.Next() < 0.02)
					{
						auto Key = [&R]() { const double V = R.Next(); return V < 0.3 ? -1.0 : V < 0.7 ? 0.0 : 1.0; };
						Held = FreeEye::Input();
						Held.PanRight = Key();
						Held.PanAhead = Key();
						Held.Rise = Key() * 0.5;
						Held.Turn = Key() * 3.0;
						Held.Tilt = Key() * 2.0;
					}
					In = Held;
					In.Frame = Vec{ 0.0, 0.0, 6000.0 };
					if (R.Next() < 0.03) In.Zoom = R.Next() < 0.5 ? 1.0 : -1.0;
					if (R.Next() < 0.02)
					{
						In.SlideRight = R.In(-0.05, 0.05);
						In.SlideAhead = R.In(-0.05, 0.05);
					}
				};
				Measured M = Fly(E, Sc, K, 1200.0, Seed + 40);
				Smoothness(M, Sc, E.Set.FeelArm);
				char Name[64];
				std::snprintf(Name, sizeof(Name), "rule 2 random hands seed %d %s", static_cast<int>(Seed), TimingName(K));
				double Top = -1e9;
				int AtTop = 0;
				for (const Rec& Fr : M.Recs)
				{
					Top = std::max(Top, Fr.P.Eye.Z);
					if (Fr.P.Eye.Z > E.Set.EyeMost - 1.0) ++AtTop;
				}
				char Line[96];
				std::snprintf(Line, sizeof(Line), "eye up to %.2f m, %d frames within a metre of EyeMost", Top, AtTop);
				Report(Name, M, Line);
				GateSmooth(Name, M);
				if (M.HardStops > 8) Fault("%s: %d hard stops with no cap moving (a handful at most)", Name, M.HardStops);
				if (M.Lowest < E.Set.FloorHard - 1e-9) Fault("%s: the eye stood %.3f m over the floor", Name, M.Lowest);
			}
		}
		for (Timing K : Timings)
		{
			// his descent held for half a minute onto a flat floor, then his rise for a second: the want stops at the
			// floor's clearance (it does not sink on under the ground), so the view comes to rest there softly and rises
			// at once when he lets it.
			FreeEye::Eye E;
			BeginAt(E, 0.3, Vec{ 0.0, 0.0, 10.0 });
			Script Sc;
			Sc.Where = StillAt;
			Sc.Feed = [](FreeEye::Input& In, FreeEye::Eye&, double, double) { In.Rise = -1.0; };
			Measured M = Fly(E, Sc, K, 30.0);
			const double Rested = M.Recs.back().P.Eye.Z;
			const double Lifted = E.Lift();
			Sc.Feed = [](FreeEye::Input& In, FreeEye::Eye&, double, double) { In.Rise = 1.0; };
			Measured Up = Fly(E, Sc, K, 1.0);
			M.HardLifts += Up.HardLifts;
			Smoothness(M, Sc, E.Set.FeelArm);
			const double Rose = E.Last().Eye.Z - Rested;
			char Line[96];
			std::snprintf(Line, sizeof(Line), "rested at %.3f m (lifted %.3f m), then rose %.2f m in a second", Rested, Lifted, Rose);
			const std::string Name = std::string("rule 9 his descent held into the floor ") + TimingName(K);
			Report(Name, M, Line);
			GateSmooth(Name, M);
			if (M.HardLifts) Fault("%s: %d hard lifts from his own descent", Name.c_str(), M.HardLifts);
			if (!(Lifted < 0.5)) Fault("%s: the floor's lift held the view up %.2f m (his want sank under the floor)", Name.c_str(), Lifted);
			if (Rested < E.Set.FloorClear - 0.05 || Rested > E.Set.FloorClear + 0.5) Fault("%s: rested %.3f m over the floor (FloorClear %.1f)", Name.c_str(), Rested, E.Set.FloorClear);
			if (!(Rose > 3.0)) Fault("%s: rose only %.2f m in a second of his rise (the want sank under the floor)", Name.c_str(), Rose);
		}
	}
}

int main()
{
	RuleRest();
	RuleEased();
	RuleLock();
	RulePanLock();
	RuleLetGo();
	RuleRide();
	RuleFloor();
	RuleFloorFling();
	RuleFloorAndClear();
	RuleCaps();
	RuleLensLook();
	RuleBegin();
	RuleRobust();
	AirBattle();
	SeaBattle();
	RuleHandsExact();
	RuleSpringCurves();
	RuleGlideEnd();
	RuleLiftCurve();
	RuleBeginEdges();
	RuleEdges();
	RuleFrames();
	RuleCutsClean();
	RuleRideMore();
	RuleCapsMore();
	RuleCarries();
	RuleHandsRandom();
	std::printf("%d fault%s\n", Faults, Faults == 1 ? "" : "s");
	return std::min(Faults, 100);
}
