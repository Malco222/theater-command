#pragma once

// Men in lorries (release 1.5, the Battle rules card's row "trucks"; Brian, 23 November: "troops are stuck under the
// truck"): how a lorried man the drill has getting down over her tailboard is played on his way, in plain C++ (no engine)
// so the cloud's test plays it as the scene does. The drill (src/battle/land/drill.ts, hopOut) has the men of a lorry get
// down one at a time, her rear seats first, each on the ground behind her HOP.takes battle seconds after his turn began.
// The scene shows each on her bench (the soldiers thread's bench_ride), and on his turn: still seated until his own
// way down is due, then up off the bench (bench_rise_l or _r), along her floor to the tailboard (walk), and down over it
// (out_tail_a or _b, whose root motion sets him on the ground behind her), so that he stands there as the drill has him
// stand, and is handed to the drill's man there.

#include "Battle/LandKit.h"
#include "Battle/LandMen.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace LandBench
{
	/** Battle seconds from a man's turn to get down to his standing behind her: the drill's HOP.takes (drill.ts). */
	constexpr double Takes = 2.8;
	/** bench_rise_l and _r's length, life seconds. */
	constexpr double RiseLong = 1.4;
	/** The walk clip's own pace, metres a life second (its stride, 1.42 m, in its 1.133 s). */
	constexpr double Pace = 1.25;
	/** Life seconds a clip takes to take over from the one he leaves. */
	constexpr double Blend = 0.25;
	/** The shortest walk along her floor, life seconds (a rear seat's man is at the way out as he rises). */
	constexpr double ShortestWalk = 0.35;
	/** Metres to his own bench's side of her step that a man comes down (the drill's HOP.abreast): the benches take turns,
	 * so two men one after the other never come down on one spot. */
	constexpr double Abreast = 0.35;

	enum class Step : uint8_t { Seated, Rise, Walk, Out };

	struct Moment
	{
		Step S = Step::Seated;
		/** Life seconds into this step's clip (Seated: since his turn began; the scene adds the loop's own clock). */
		double T = 0.0;
		/** Walk: how far along his floor, 0 where the rise left him, 1 at the way out's start. */
		double Along = 0.0;
		/** Walk: the walk clip's rate, his pace over its own. */
		double Rate = 1.0;
		/** The step he leaves (blended out; W 1: none) and how far into its clip he left it. */
		Step From = Step::Seated;
		double FromT = 0.0;
		double W = 1.0;
	};

	/**
	 * Where a man getting down is, Down battle seconds after his turn began, at Life life seconds a battle second, Walk
	 * metres along her floor from where his rise leaves him to the way out's start, OutLong his way out's length (life
	 * seconds). He sits until it is time to rise, so he stands on the ground behind her as the drill has him (Takes after
	 * his turn); a long walk (the front seats) is hurried to fit, at no less than ShortestWalk. Past the way out's end he
	 * holds it until the drill's man takes over.
	 */
	inline Moment At(double Down, double Life, double Walk, double OutLong)
	{
		Moment N;
		const double E = std::max(0.0, Down) * Life;
		double WalkLong = std::max(ShortestWalk, Walk / Pace);
		double Wait = Takes * Life - (RiseLong + WalkLong + OutLong);
		if (Wait < 0.0)
		{
			WalkLong = std::max(ShortestWalk, WalkLong + Wait);
			Wait = 0.0;
		}
		const double RiseAt = Wait, WalkAt = RiseAt + RiseLong, OutAt = WalkAt + WalkLong;
		const double Rate = std::max(0.3, Walk / WalkLong / Pace);
		if (E < RiseAt)
		{
			N.S = Step::Seated;
			N.T = E;
			return N;
		}
		if (E < WalkAt)
		{
			// (up off the bench from his seated loop, in the same seat)
			N.S = Step::Rise;
			N.T = E - RiseAt;
			N.From = Step::Seated;
			N.FromT = RiseAt;
			N.W = std::clamp(N.T / Blend, 0.0, 1.0);
			return N;
		}
		if (E < OutAt)
		{
			// (the rise ends standing, facing aft, where the walk begins: taken up as it is)
			N.S = Step::Walk;
			N.T = E - WalkAt;
			N.Along = std::clamp(N.T / WalkLong, 0.0, 1.0);
			N.Rate = Rate;
			return N;
		}
		N.S = Step::Out;
		N.T = std::min(E - OutAt, OutLong);
		N.From = Step::Walk;
		N.FromT = WalkLong;
		N.Rate = Rate;
		N.W = std::clamp((E - OutAt) / Blend, 0.0, 1.0);
		return N;
	}

	/** Her bench's socket for the drill's seat Seat (0 her front left, 1 her front right, 2 the next left ...): rider.1 to
	 * rider.5 down her left bench from the front, rider.6 to rider.10 down her right; 0 for a seat past her ten. */
	inline int SocketOf(int Seat)
	{
		if (Seat < 0 || Seat >= 10) return 0;
		return Seat % 2 == 0 ? 1 + Seat / 2 : 6 + Seat / 2;
	}
	/**
	 * The ride (LandMen::Crowd::SetRide) of the rider shown for the drill's man Man aboard lorry V on her seat Seat:
	 * seated on her bench (Down -1), or Down battle seconds into his way down (At), at battle time Now with Life life
	 * seconds a battle second, in his soldier file Sol's truck clips (the soldiers thread's: bench_ride, bench_rise_l
	 * and _r, walk, out_tail_a and _b, their places on the Truck). False where she has no such seat or he no such clips.
	 */
	inline bool RideFor(const LandKit::Vehicle& V, const LandMen::Soldier& Sol, int Seat, double Down, double Now, double Life, int Man, LandMen::Crowd::Ride& Out)
	{
		Out = LandMen::Crowd::Ride();
		const int Socket = SocketOf(Seat);
		LandKit::V3 SeatAt, SeatFacing, StepAt, StepFacing;
		if (Socket <= 0 || !V.SocketNow("rider." + std::to_string(Socket), SeatAt, SeatFacing) || !V.SocketNow("step", StepAt, StepFacing))
		{
			return false;
		}
		const bool bLeft = Socket <= 5;
		const int RideClip = Sol.ClipIndex("bench_ride");
		const int RiseClip = Sol.ClipIndex(bLeft ? "bench_rise_l" : "bench_rise_r");
		const int WalkClip = Sol.ClipIndex("walk");
		const int OutClip = Sol.ClipIndex(Seat % 2 == 0 ? "out_tail_a" : "out_tail_b");
		// (the clips' places name her by her model's file: every lorry is drawn as the Truck, LandKit's)
		const LandMen::Clip::Seat* OnBench = RideClip >= 0 ? Sol.Clips[static_cast<size_t>(RideClip)].SeatOn("Truck") : nullptr;
		const LandMen::Clip::Seat* OnStep = OutClip >= 0 ? Sol.Clips[static_cast<size_t>(OutClip)].SeatOn("Truck") : nullptr;
		if (RiseClip < 0 || WalkClip < 0 || !OnBench || !OnStep)
		{
			return false;
		}
		// Her axes at the bench and at the step: the socket's facing along her floor, her hull's up (as a tank
		// commander's seat is made, RideTanks).
		const LandKit::V3 Up = V.Root.Way(LandKit::V3(0.0, 0.0, 1.0)).Unit();
		const LandKit::V3 BenchFwd = (SeatFacing - Up * SeatFacing.Dot(Up)).Unit(LandKit::V3(0.0, 1.0, 0.0));
		const LandKit::V3 BenchRight = BenchFwd.Cross(Up);
		const LandKit::V3 Aft = (StepFacing - Up * StepFacing.Dot(Up)).Unit(LandKit::V3(0.0, -1.0, 0.0));
		const LandKit::V3 AftRight = Aft.Cross(Up);
		// His root on the bench (the clip's place); where the rise leaves him standing (its pelvis's last place, on her
		// floor: the bench's top less the clips' bench_top_over_floor, 0.455 m); and the way out's start (the way-out
		// clip's place on her step).
		const LandKit::V3 OnSeat = SeatAt + BenchFwd * OnBench->Offset.X + BenchRight * OnBench->Offset.Y + Up * OnBench->Offset.Z;
		LandMen::Vec3 RiseEnd { 0.38f, 0.f, 0.f };
		const int Pelvis = Sol.BoneIndex("pelvis");
		const LandMen::Clip& Rise = Sol.Clips[static_cast<size_t>(RiseClip)];
		if (Pelvis >= 0 && static_cast<size_t>(Pelvis) < Rise.MoveOf.size() && Rise.MoveOf[static_cast<size_t>(Pelvis)] >= 0)
		{
			const LandMen::Keys& K = Rise.Moves[static_cast<size_t>(Rise.MoveOf[static_cast<size_t>(Pelvis)])];
			if (!K.Move.empty())
			{
				RiseEnd = K.Move.back();
			}
		}
		const LandKit::V3 FromFloor = SeatAt + BenchFwd * RiseEnd.X + BenchRight * RiseEnd.Y - Up * 0.455;
		// (his own bench's side of the step, as the drill sets him down: Abreast)
		const double Side = (SeatAt - StepAt).Dot(AftRight) >= 0.0 ? 1.0 : -1.0;
		const LandKit::V3 WayOut = StepAt + Aft * OnStep->Offset.X + AftRight * (OnStep->Offset.Y + Side * Abreast) + Up * OnStep->Offset.Z;
		const double Walk = (WayOut - FromFloor).Length();
		const double OutLong = Sol.Clips[static_cast<size_t>(OutClip)].Long;
		const Moment B = Down >= 0.0 ? At(Down, Life, Walk, OutLong) : Moment();
		// (his own offset into the seated loop, so no two on a bench sway as one)
		const float Loop = static_cast<float>(Now * Life) + 0.37f * static_cast<float>(Man % 11);
		LandKit::V3 Feet = OnSeat, Fwd = BenchFwd, Right = BenchRight;
		switch (B.S)
		{
		case Step::Seated:
			Out.Clip = RideClip >= 0 ? RideClip : RiseClip;
			Out.Time = RideClip >= 0 ? Loop : 0.f;
			break;
		case Step::Rise:
			Out.Clip = RiseClip;
			Out.Time = static_cast<float>(B.T);
			if (B.W < 1.0 && RideClip >= 0)
			{
				Out.FromClip = RideClip;
				Out.FromTime = Loop;
				Out.W = static_cast<float>(B.W);
			}
			break;
		case Step::Walk:
			Out.Clip = WalkClip;
			Out.Time = static_cast<float>(B.T * B.Rate);
			Feet = FromFloor + (WayOut - FromFloor) * B.Along;
			Fwd = Aft;
			Right = AftRight;
			break;
		case Step::Out:
			Out.Clip = OutClip;
			Out.Time = static_cast<float>(B.T);
			if (B.W < 1.0)
			{
				Out.FromClip = WalkClip;
				Out.FromTime = static_cast<float>(B.FromT * B.Rate);
				Out.W = static_cast<float>(B.W);
			}
			Feet = WayOut;
			Fwd = Aft;
			Right = AftRight;
			break;
		}
		float* M = Out.Seat.R;
		M[0] = static_cast<float>(Fwd.X); M[1] = static_cast<float>(Right.X); M[2] = static_cast<float>(Up.X);
		M[3] = static_cast<float>(Fwd.Y); M[4] = static_cast<float>(Right.Y); M[5] = static_cast<float>(Up.Y);
		M[6] = static_cast<float>(Fwd.Z); M[7] = static_cast<float>(Right.Z); M[8] = static_cast<float>(Up.Z);
		Out.Seat.T[0] = static_cast<float>(Feet.X);
		Out.Seat.T[1] = static_cast<float>(Feet.Y);
		Out.Seat.T[2] = static_cast<float>(Feet.Z);
		Out.bShown = true;
		return true;
	}
}
