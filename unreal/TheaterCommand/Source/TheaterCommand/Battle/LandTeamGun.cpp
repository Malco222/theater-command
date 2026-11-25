#include "Battle/LandTeamGun.h"

#include <algorithm>
#include <cmath>

namespace LandTeamGun
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		double Radians(double Degrees) { return Degrees * Pi / 180.0; }
		double Degrees(double Radians) { return Radians * 180.0 / Pi; }

		/** An angle put into (-pi, pi]. */
		double Wrapped(double A)
		{
			A = std::fmod(A, 2.0 * Pi);
			if (A > Pi) A -= 2.0 * Pi;
			if (A <= -Pi) A += 2.0 * Pi;
			return A;
		}

		/** From toward To by no more than Step (both in the same measure). */
		double Toward(double From, double To, double Step)
		{
			const double D = To - From;
			return std::fabs(D) <= Step ? To : From + (D > 0.0 ? Step : -Step);
		}

		/** A facing turned the short way toward another by no more than Step radians. */
		double FacingToward(double From, double To, double Step)
		{
			const double D = Wrapped(To - From);
			return std::fabs(D) <= Step ? To : Wrapped(From + (D > 0.0 ? Step : -Step));
		}

		/** The army's letters alone: "UK@desert" is UK, "SOV_winter" SOV. */
		std::string Letters(const std::string& Army)
		{
			size_t N = 0;
			while (N < Army.size() && Army[N] >= 'A' && Army[N] <= 'Z')
			{
				++N;
			}
			return Army.substr(0, N);
		}

		// A weapon's own axes on the field: x forward (sin F, cos F), y right (cos F, -sin F), z up; row by row.
		void Facing3(double F, float* R)
		{
			const float S = static_cast<float>(std::sin(F)), C = static_cast<float>(std::cos(F));
			R[0] = S;
			R[1] = C;
			R[2] = 0.f;
			R[3] = C;
			R[4] = -S;
			R[5] = 0.f;
			R[6] = 0.f;
			R[7] = 0.f;
			R[8] = 1.f;
		}

		void Mul3(const float* A, const float* B, float* Out)
		{
			for (int Row = 0; Row < 3; ++Row)
			{
				for (int Col = 0; Col < 3; ++Col)
				{
					Out[Row * 3 + Col] = A[Row * 3] * B[Col] + A[Row * 3 + 1] * B[3 + Col] + A[Row * 3 + 2] * B[6 + Col];
				}
			}
		}

		LandKit::V3 Turned3(const float* R, const LandKit::V3& V)
		{
			return LandKit::V3(R[0] * V.X + R[1] * V.Y + R[2] * V.Z, R[3] * V.X + R[4] * V.Y + R[5] * V.Z, R[6] * V.X + R[7] * V.Y + R[8] * V.Z);
		}

		/** In a weapon's own axes: swung Right degrees to the right, then tipped Up degrees up (the tip in the swung
		 * gun's own frame); row by row. Forward (1, 0, 0) goes to (cos r cos u, sin r cos u, sin u). */
		void SwungAndTipped(double Right, double Up, float* R)
		{
			const double Cr = std::cos(Radians(Right)), Sr = std::sin(Radians(Right));
			const double Cu = std::cos(Radians(Up)), Su = std::sin(Radians(Up));
			R[0] = static_cast<float>(Cr * Cu);
			R[1] = static_cast<float>(-Sr);
			R[2] = static_cast<float>(-Cr * Su);
			R[3] = static_cast<float>(Sr * Cu);
			R[4] = static_cast<float>(Cr);
			R[5] = static_cast<float>(-Sr * Su);
			R[6] = static_cast<float>(Su);
			R[7] = 0.f;
			R[8] = static_cast<float>(Cu);
		}

		bool Moving(const std::string& Pose)
		{
			return Pose == "walk" || Pose == "run" || Pose == "flee" || Pose == "advance" || Pose == "creep" || Pose == "crawl";
		}
	}

	std::string FileFor(const std::string& Army, const std::string& Role, int Year, int Month)
	{
		const std::string A = Letters(Army);
		if (Role == "hmg")
		{
			// (each army's own where it is drawn. Stand-ins, the nearest shape: Finland's Maxim on the same wheeled mount;
			// China's Hotchkiss, which she bought and made; Poland's wz. 30, the water-cooled Browning made under her own
			// name; Hungary's water-cooled Schwarzlose as the water-cooled Browning; Romania's ZB 53, a finned barrel on a
			// tripod, as the Breda)
			if (A == "GER") return Year * 100 + Month >= 194206 ? "HmgMG42" : "HmgMG34";
			if (A == "USA") return "HmgM1919";
			if (A == "UK") return "HmgVickers";
			if (A == "SOV" || A == "FIN") return "HmgMaxim";
			if (A == "JAP") return "HmgType92";
			if (A == "ITA" || A == "ROM") return "HmgBreda37";
			if (A == "FRA" || A == "CHN") return "HmgHotchkiss";
			if (A == "POL" || A == "HUN") return "HmgM1917";
			return "HmgM1917";
		}
		if (Role == "mortar")
		{
			// (the Brandt pattern every army made; one with none of its own drawn takes the French original)
			for (const char* Own : { "GER", "USA", "UK", "SOV", "JAP", "ITA", "FRA" })
			{
				if (A == Own)
				{
					return "Mortar" + A;
				}
			}
			return A == "HUN" ? "MortarGER" : "MortarFRA";
		}
		return std::string();
	}

	bool BurstOf(const std::string& File, int& OutRounds, double& OutPerSecond)
	{
		// (each gun's own rate: the MG 42's 1,200 rounds a minute and the MG 34's 900; the Brownings' and the Vickers'
		// about 500, the Maxim's 600; the strip-fed guns' 450. The rules fire a team's gun every second or so (0.9 to
		// 1.6 s apart), so a burst of nine tenths of a second at most keeps her firing nearly without a break, and never
		// two bursts of hers at once.)
		struct Row
		{
			const char* File;
			int Rounds;
			double PerSecond;
		};
		static const Row Rows[] = { { "HmgMG42", 18, 20.0 }, { "HmgMG34", 13, 15.0 }, { "HmgM1919", 7, 8.0 }, { "HmgM1917", 7, 8.0 }, { "HmgVickers", 7, 8.0 },
			{ "HmgMaxim", 9, 10.0 }, { "HmgType92", 6, 7.5 }, { "HmgHotchkiss", 6, 7.5 }, { "HmgBreda37", 6, 7.5 } };
		for (const Row& R : Rows)
		{
			if (File == R.File)
			{
				OutRounds = R.Rounds;
				OutPerSecond = R.PerSecond;
				return true;
			}
		}
		return false;
	}

	bool Kind::Read(const LandKit::Model& M, std::string& OutWhy)
	{
		const int Tube = M.PartIndex("tube"), Cradle = M.PartIndex("cradle"), Barrel = M.PartIndex("gun");
		bMortar = Tube >= 0;
		if (!bMortar && (Cradle < 0 || Barrel < 0))
		{
			OutWhy = "no tube, and no cradle and gun: not a support weapon's file";
			return false;
		}
		const int SeatAt = M.SocketIndex("seat"), MuzzleAt = M.SocketIndex("muzzle");
		if (SeatAt < 0 || MuzzleAt < 0)
		{
			OutWhy = "no seat or no muzzle socket";
			return false;
		}
		Title = M.Name;
		Seat = M.Sockets[static_cast<size_t>(SeatAt)].At;
		Muzzle = M.Sockets[static_cast<size_t>(MuzzleAt)].At;
		const LandKit::Part& Tips = M.Parts[static_cast<size_t>(bMortar ? Tube : Barrel)];
		Pivot = Tips.Pivot;
		if (Tips.bLimits)
		{
			Low = Tips.Lo;
			High = Tips.Hi;
		}
		if (!bMortar && M.Parts[static_cast<size_t>(Cradle)].bLimits)
		{
			Left = -M.Parts[static_cast<size_t>(Cradle)].Lo;
			Right = M.Parts[static_cast<size_t>(Cradle)].Hi;
		}
		Bore = bMortar ? (Muzzle - Pivot).Unit(LandKit::V3(1.0, 0.0, 0.0)) : LandKit::V3(1.0, 0.0, 0.0);
		// What turns: every body whose part is the tube, the cradle or the gun, or hangs on one.
		Mount.clear();
		Turned.clear();
		double XLo = 1e9, XHi = -1e9;
		for (size_t B = 0; B < M.Bodies.size(); ++B)
		{
			bool bTurns = false;
			int P = M.Bodies[B].Part;
			for (int Guard = 0; P >= 0 && Guard < 16; ++Guard)
			{
				if (P == Tube || P == Cradle || P == Barrel)
				{
					bTurns = true;
					break;
				}
				P = M.Parts[static_cast<size_t>(P)].Parent;
			}
			(bTurns ? Turned : Mount).push_back(static_cast<int>(B));
			if (bTurns)
			{
				const LandKit::Mesh& Near = M.Bodies[B].Near;
				for (size_t K = 0; K + 2 < Near.Points.size(); K += 3)
				{
					const double X = Near.Points[K] + M.Bodies[B].Pivot.X;
					XLo = std::min(XLo, X);
					XHi = std::max(XHi, X);
				}
			}
		}
		if (Mount.empty() || Turned.empty())
		{
			OutWhy = "nothing stands or nothing turns";
			return false;
		}
		Balance = bMortar ? Pivot + Bore * (0.45 * (Muzzle - Pivot).Length()) : LandKit::V3(XLo <= XHi ? 0.5 * (XLo + XHi) : Pivot.X, 0.0, Muzzle.Z - 0.03);
		const int PullAt = M.SocketIndex("pull");
		bWheeled = PullAt >= 0;
		if (bWheeled)
		{
			Pull = M.Sockets[static_cast<size_t>(PullAt)].At;
		}
		const int FeedAt = M.SocketIndex("feed");
		FeedSide = FeedAt < 0 ? 0 : M.Sockets[static_cast<size_t>(FeedAt)].At.Y > 0.0 ? 1 : -1;
		const int GripAt = M.SocketIndex("grip.gunner.r");
		bKneels = bMortar || bWheeled || (GripAt >= 0 && M.Sockets[static_cast<size_t>(GripAt)].At.Z > 0.52);
		return true;
	}

	void Gun::Step(const Kind& K, const Crew& C, double Now, double Dt, const std::function<double(double, double)>& Height)
	{
		const State Was = Is;
		bFiring = false;
		Recoil = 0.0;
		if (C.bDown)
		{
			// (he has fallen: she stays where she stood; one he was carrying is dropped with him and not drawn, a wheeled
			// one stands where she stopped)
			Is = Was == State::SetUp || Was == State::Rolled || Was == State::Left ? State::Left : State::Away;
			return;
		}
		if (!C.bShown || C.Pose == "mounted" || C.Pose == "ride")
		{
			Is = State::Away;
			return;
		}
		if (Moving(C.Pose))
		{
			Traverse = 0.0;
			Elevation = 0.0;
			if (K.bWheeled)
			{
				// (pulled by her trail: her muzzle back the way he came, the trail's handle a pace behind him)
				const double Sw = std::sin(C.Way), Cw = std::cos(C.Way);
				const double Back = 0.3 - K.Pull.X;
				Is = State::Rolled;
				Facing = Wrapped(C.Way + Pi);
				X = C.X - Sw * Back;
				Y = C.Y - Cw * Back;
				Z = Height(X, Y);
				return;
			}
			if (!C.Frame)
			{
				Is = State::Away;
				return;
			}
			// (on his right shoulder: what is carried lies along his way, its muzzle a little up, its middle on the
			// shoulder. His own axes are hers, so her frame is his, then a tip and a shift.)
			Is = State::Carried;
			const double Rest = K.bMortar ? Degrees(std::atan2(K.Bore.Z, K.Bore.X)) : 0.0;
			float Tip[9];
			SwungAndTipped(0.0, 12.0 - Rest, Tip);
			const LandKit::V3 OnShoulder(0.02, 0.19, 1.47);
			const LandKit::V3 Shift = OnShoulder - Turned3(Tip, K.Balance);
			Mul3(C.Frame->R, Tip, Shoulder.R);
			const LandKit::V3 T = Turned3(C.Frame->R, Shift);
			Shoulder.T[0] = static_cast<float>(T.X) + C.Frame->T[0];
			Shoulder.T[1] = static_cast<float>(T.Y) + C.Frame->T[1];
			Shoulder.T[2] = static_cast<float>(T.Z) + C.Frame->T[2];
			return;
		}
		if (C.Pose == "cower" && Was != State::SetUp)
		{
			// (a routed crew that had not set her up: nothing to stand by)
			Is = State::Away;
			return;
		}
		// Set up at her gunner: her seat on his own place, facing as he faces.
		Is = State::SetUp;
		const bool bFresh = Was != State::SetUp;
		const double Bearing = C.bAim ? std::atan2(C.AimX - C.X, C.AimY - C.Y) : C.Facing;
		// (a mortar has no traverse to speak of: her whole bipod is shifted toward what she fires at)
		const double FaceTo = K.bMortar && C.bAim ? Bearing : C.Facing;
		Facing = bFresh ? FaceTo : FacingToward(Facing, FaceTo, Radians(40.0) * Dt);
		const double Sm = std::sin(Facing), Cm = std::cos(Facing);
		X = C.X - (Sm * K.Seat.X + Cm * K.Seat.Y);
		Y = C.Y - (Cm * K.Seat.X - Sm * K.Seat.Y);
		Z = Height(X, Y);
		double Swing = 0.0, Lay = 0.0;
		if (C.bAim && !K.bMortar)
		{
			Swing = std::clamp(Degrees(Wrapped(std::atan2(C.AimX - X, C.AimY - Y) - Facing)), -K.Left, K.Right);
			const double Far = std::hypot(C.AimX - X, C.AimY - Y);
			// (laid on a man's chest out there, and a little over it for the round's fall)
			const double Rise = Height(C.AimX, C.AimY) + 0.9 - (Z + K.Muzzle.Z);
			Lay = std::clamp(Degrees(std::atan2(Rise, std::max(1.0, Far))) + 0.0006 * Far, K.Low, K.High);
		}
		Traverse = bFresh ? Swing : Toward(Traverse, Swing, 45.0 * Dt);
		Elevation = bFresh ? Lay : Toward(Elevation, Lay, 20.0 * Dt);
		// (each round knocks the gun 6 mm back on her mount and she runs out again before the next)
		const auto Knock = [](double In, double PerSecond)
		{
			const double Along = In * PerSecond;
			return 0.006 * (1.0 - (Along - std::floor(Along)));
		};
		if (bByRounds)
		{
			const double In = Now - FiredAt;
			bFiring = In >= 0.0 && In < FiredFor;
			if (bFiring && !K.bMortar)
			{
				Recoil = Knock(In, FiredRate);
			}
			return;
		}
		bFiring = C.Pose == "fire" && C.bAim;
		if (bFiring && !K.bMortar)
		{
			// (a burst and a pause: 1.3 s of fire in every 2.8 s, twelve rounds a second)
			const double In = std::fmod(Now / 2.8 + Phase, 1.0) * 2.8;
			if (In < 1.3)
			{
				Recoil = Knock(In, 12.0);
			}
		}
	}

	void Gun::Fired(double T, double Seconds, double PerSecond)
	{
		if (T > FiredAt && Seconds > 0.0)
		{
			FiredAt = T;
			FiredFor = Seconds;
			FiredRate = std::max(1.0, PerSecond);
		}
	}

	LandMen::Xform Gun::MountFrame() const
	{
		LandMen::Xform Out;
		Facing3(Facing, Out.R);
		Out.T[0] = static_cast<float>(X);
		Out.T[1] = static_cast<float>(Y);
		Out.T[2] = static_cast<float>(Z);
		return Out;
	}

	LandMen::Xform Gun::TurnedFrame(const Kind& K) const
	{
		if (Is == State::Carried)
		{
			return Shoulder;
		}
		// own point p goes to Pivot + L * (p - Pivot - Bore * Recoil), then onto the field by the mount's frame.
		const LandMen::Xform On = MountFrame();
		float L[9];
		SwungAndTipped(K.bMortar ? 0.0 : Traverse, K.bMortar ? 0.0 : Elevation, L);
		LandMen::Xform Out;
		Mul3(On.R, L, Out.R);
		const LandKit::V3 Own = K.Pivot - Turned3(L, K.Pivot + K.Bore * Recoil);
		const LandKit::V3 T = Turned3(On.R, Own);
		Out.T[0] = static_cast<float>(T.X) + On.T[0];
		Out.T[1] = static_cast<float>(T.Y) + On.T[1];
		Out.T[2] = static_cast<float>(T.Z) + On.T[2];
		return Out;
	}

	bool Gun::MuzzleAt(const Kind& K, LandMen::Vec3& OutAt, LandMen::Vec3& OutWay) const
	{
		if (Is != State::SetUp)
		{
			return false;
		}
		const LandMen::Xform F = TurnedFrame(K);
		const LandKit::V3 At = Turned3(F.R, K.Muzzle);
		const LandKit::V3 Way = Turned3(F.R, K.Bore);
		OutAt = LandMen::Vec3{ static_cast<float>(At.X) + F.T[0], static_cast<float>(At.Y) + F.T[1], static_cast<float>(At.Z) + F.T[2] };
		OutWay = LandMen::Vec3{ static_cast<float>(Way.X), static_cast<float>(Way.Y), static_cast<float>(Way.Z) };
		return true;
	}
}
