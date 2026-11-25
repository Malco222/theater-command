#include "Battle/AirMarks.h"

#include "Battle/LandAir.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace AirMarks
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		/** A point Along metres along a road (a line of board points) from its start, and the road's heading there. */
		void OnRoad(const std::vector<std::pair<double, double>>& Road, double Along, double& X, double& Y, double& Way)
		{
			X = Road.front().first;
			Y = Road.front().second;
			Way = 0.0;
			if (Road.size() < 2) return;
			double Left = std::max(0.0, Along);
			for (size_t K = 1; K < Road.size(); ++K)
			{
				const double Dx = Road[K].first - Road[K - 1].first, Dy = Road[K].second - Road[K - 1].second;
				const double Leg = std::hypot(Dx, Dy);
				if (Leg < 1e-9) continue;
				Way = std::atan2(Dx, Dy);
				if (Left <= Leg || K + 1 == Road.size())
				{
					const double S = std::min(Left, Leg) / Leg;
					X = Road[K - 1].first + Dx * S;
					Y = Road[K - 1].second + Dy * S;
					return;
				}
				Left -= Leg;
			}
		}

		double RoadLong(const std::vector<std::pair<double, double>>& Road)
		{
			double Sum = 0.0;
			for (size_t K = 1; K < Road.size(); ++K) Sum += std::hypot(Road[K].first - Road[K - 1].first, Road[K].second - Road[K - 1].second);
			return Sum;
		}

		/** Where a point lies along a road and how far off it (metres), the nearest of its legs. */
		void OffRoad(const std::vector<std::pair<double, double>>& Road, double X, double Y, double& Along, double& Off)
		{
			Along = 0.0;
			Off = 1e18;
			double Before = 0.0;
			for (size_t K = 1; K < Road.size(); ++K)
			{
				const double Ax = Road[K - 1].first, Ay = Road[K - 1].second;
				const double Dx = Road[K].first - Ax, Dy = Road[K].second - Ay;
				const double Leg2 = Dx * Dx + Dy * Dy;
				const double S = Leg2 > 1e-12 ? std::clamp(((X - Ax) * Dx + (Y - Ay) * Dy) / Leg2, 0.0, 1.0) : 0.0;
				const double D = std::hypot(X - (Ax + Dx * S), Y - (Ay + Dy * S));
				if (D < Off)
				{
					Off = D;
					Along = Before + S * std::sqrt(Leg2);
				}
				Before += std::sqrt(Leg2);
			}
		}

		/** How far a board point is from the nearest piece but one (Skip, its place in Pieces), metres from its footprint. */
		double Clearance(const Column& C, double X, double Y, int Skip)
		{
			double Least = 1e18;
			for (size_t K = 0; K < C.Pieces.size(); ++K)
			{
				if (static_cast<int>(K) != Skip) Least = std::min(Least, Off(C.Pieces[K], X, Y));
			}
			return Least;
		}
	}

	uint32_t Hash(uint32_t A, uint32_t B)
	{
		uint32_t H = A * 2654435761u ^ (B * 0x9E3779B9u + 0x7F4A7C15u);
		H ^= H >> 16;
		H *= 0x85EBCA6Bu;
		H ^= H >> 13;
		H *= 0xC2B2AE35u;
		H ^= H >> 16;
		return H;
	}

	double UnitOf(uint32_t H)
	{
		return static_cast<double>(H) / 4294967296.0;
	}

	State StateOf(const std::string& Word)
	{
		if (Word == "hit") return State::Hit;
		if (Word == "burn") return State::Burn;
		if (Word == "out") return State::Out;
		return State::Ok;
	}

	const char* WordOf(State S)
	{
		switch (S)
		{
		case State::Ok: return "ok";
		case State::Hit: return "hit";
		case State::Burn: return "burn";
		case State::Out: return "out";
		}
		return "ok";
	}

	int Column::IndexOf(int Id) const
	{
		for (size_t K = 0; K < Pieces.size(); ++K)
		{
			if (Pieces[K].Id == Id) return static_cast<int>(K);
		}
		return -1;
	}

	void SizeOf(const std::string& What, double& Long, double& Wide)
	{
		// (the kit's own models: a Panzer IV 5.9 by 2.9 m, the three-ton lorry 6.0 by 2.3, a half-track 5.8 by 2.1, a
		// field gun on its trail about 6 by 2, a light flak gun on its carriage 4 by 2; a block of men marching in threes
		// along the road about 24 m by 4; fieldworks a size of their own)
		if (What == "tank") { Long = 6.2; Wide = 3.0; return; }
		if (What == "lorry") { Long = 6.0; Wide = 2.4; return; }
		if (What == "halftrack") { Long = 5.9; Wide = 2.2; return; }
		if (What == "car") { Long = 4.0; Wide = 1.8; return; }
		if (What == "wagon") { Long = 7.0; Wide = 2.0; return; }
		if (What == "gun") { Long = 6.4; Wide = 2.2; return; }
		if (What == "aa") { Long = 4.2; Wide = 2.0; return; }
		if (What == "men") { Long = 24.0; Wide = 4.0; return; }
		if (What == "bunker" || What == "post") { Long = 6.0; Wide = 6.0; return; }
		if (What == "pit") { Long = 8.0; Wide = 8.0; return; }
		Long = 5.0;
		Wide = 2.5;
	}

	void Lay(Column& C, double AimX, double AimY, double R, uint64_t Seed)
	{
		if (C.Has() || C.Counts.empty()) return;
		const uint32_t S0 = static_cast<uint32_t>(Seed ^ (Seed >> 32)) ^ 0xC0A1u;
		// The road: given, or through the aim from the seed, 3 km of it bending gently, its ends inside the ring.
		if (C.Road.size() < 2)
		{
			C.Road.clear();
			const double Way = UnitOf(Hash(S0, 1)) * Pi;
			const double Half = std::min(1500.0, R * 0.8);
			const double Fx = std::sin(Way), Fy = std::cos(Way);
			for (int K = 0; K <= 6; ++K)
			{
				const double U = -Half + 2.0 * Half * K / 6.0;
				const double Bend = (UnitOf(Hash(S0, 10 + static_cast<uint32_t>(K))) - 0.5) * 80.0;
				C.Road.emplace_back(AimX + Fx * U + Fy * Bend, AimY + Fy * U - Fx * Bend);
			}
		}
		// The column's order: each kind spread evenly down it (a few guns among the lorries, the flak at intervals), a
		// block of men every PerBlock men asked.
		struct Slot { double At; int Rank; std::string What; int N; };
		std::vector<Slot> Order;
		int Rank = 0;
		for (const auto& Count : C.Counts)
		{
			const bool bMen = Count.first == "men";
			const int PerBlock = 60;
			const int Many = bMen ? (Count.second + PerBlock - 1) / PerBlock : Count.second;
			for (int K = 0; K < Many; ++K)
			{
				const int N = bMen ? std::min(PerBlock, Count.second - K * PerBlock) : 1;
				Order.push_back({ (K + 0.5) / std::max(1, Many), Rank, Count.first, std::max(1, N) });
			}
			++Rank;
		}
		std::stable_sort(Order.begin(), Order.end(), [](const Slot& A, const Slot& B) { return A.At != B.At ? A.At < B.At : A.Rank < B.Rank; });
		if (Order.size() > static_cast<size_t>(MostPieces)) Order.resize(static_cast<size_t>(MostPieces));
		const bool bMarch = C.Posture.empty() || C.Posture == "march";
		const double Long = RoadLong(C.Road);
		const double Gap = 55.0;
		const double Begin = std::max(0.0, Long * 0.5 - Gap * 0.5 * static_cast<double>(Order.size()));
		int Id = 1;
		for (size_t K = 0; K < Order.size(); ++K)
		{
			Piece P;
			P.Id = Id++;
			P.What = Order[K].What;
			P.Nat = C.Nat;
			P.Posture = C.Posture;
			P.N = Order[K].N;
			P.Men = P.N;
			SizeOf(P.What, P.Long, P.Wide);
			const uint32_t H = Hash(S0, 100 + static_cast<uint32_t>(K));
			if (bMarch)
			{
				double X = 0.0, Y = 0.0, Way = 0.0;
				OnRoad(C.Road, std::min(Long, Begin + Gap * static_cast<double>(K)), X, Y, Way);
				P.X = X;
				P.Y = Y;
				P.Face = Way;
			}
			else
			{
				// (camped, dug in or in a town: in a loose square about the aim, 70 m apart, each its own way)
				const int Side = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(Order.size())))));
				const int Row = static_cast<int>(K) / Side, Col = static_cast<int>(K) % Side;
				P.X = AimX + (Col - (Side - 1) * 0.5) * 70.0 + (UnitOf(H) - 0.5) * 20.0;
				P.Y = AimY + (Row - (Side - 1) * 0.5) * 70.0 + (UnitOf(Hash(H, 2)) - 0.5) * 20.0;
				P.Face = UnitOf(Hash(H, 3)) * 2.0 * Pi;
			}
			// (never outside the ring: drawn in toward the aim)
			const double Out = std::hypot(P.X - AimX, P.Y - AimY);
			if (Out > R * 0.95 && Out > 0.0)
			{
				P.X = AimX + (P.X - AimX) * (R * 0.95 / Out);
				P.Y = AimY + (P.Y - AimY) * (R * 0.95 / Out);
			}
			C.Pieces.push_back(P);
		}
		C.bLaid = true;
	}

	double Off(const Piece& P, double X, double Y)
	{
		const double Fx = std::sin(P.Face), Fy = std::cos(P.Face);
		const double Dx = X - P.X, Dy = Y - P.Y;
		const double U = Dx * Fx + Dy * Fy;
		const double V = Dx * Fy - Dy * Fx;
		const double Ou = std::max(0.0, std::fabs(U) - P.Long * 0.5);
		const double Ov = std::max(0.0, std::fabs(V) - P.Wide * 0.5);
		return std::hypot(Ou, Ov);
	}

	double BlastOf(double Kg)
	{
		// (what the scene keeps a bomb that struck nothing away from: about 30 m from a 250 kg bomb's burst to the
		// nearest edge of a lorry left unharmed, more for a heavier one, by the cube root of its weight)
		return std::max(15.0, 30.0 * std::cbrt(std::max(50.0, Kg) / 250.0));
	}

	bool Clear(const Column& C, double X, double Y, double Blast, int Skip)
	{
		for (size_t K = 0; K < C.Pieces.size(); ++K)
		{
			if (static_cast<int>(K) == Skip) continue;
			if (Off(C.Pieces[K], X, Y) <= Blast) return false;
		}
		return true;
	}

	bool ClearOf(const Column& C, double X, double Y, double Blast, double& OutX, double& OutY)
	{
		OutX = X;
		OutY = Y;
		if (Clear(C, X, Y, Blast)) return true;
		// (the least move that clears it: out from the nearest piece across its way first, then rings of 32 ways round,
		// 3 m apart; the same point for the same fall)
		const Piece* Near = nullptr;
		double Least = 1e18;
		for (const Piece& P : C.Pieces)
		{
			const double D = Off(P, X, Y);
			if (D < Least) { Least = D; Near = &P; }
		}
		// (across its way, out to the side of it the point already lies on: right of its way where V is plus)
		const double Across = Near ? Near->Face + Pi * 0.5 : 0.0;
		const bool bRight = !Near || ((X - Near->X) * std::cos(Near->Face) - (Y - Near->Y) * std::sin(Near->Face)) >= 0.0;
		for (double Ring = 3.0; Ring <= 800.0; Ring += 3.0)
		{
			for (int K = 0; K < 32; ++K)
			{
				// (0, then 1, -1, 2, -2 ... thirty-seconds of a turn either way from straight out across the road)
				const int Step = (K + 1) / 2 * (K % 2 ? 1 : -1);
				const double Way = Across + (bRight ? 0.0 : Pi) + Step * (2.0 * Pi / 32.0);
				const double Cx = X + std::sin(Way) * Ring, Cy = Y + std::cos(Way) * Ring;
				if (Clear(C, Cx, Cy, Blast))
				{
					OutX = Cx;
					OutY = Cy;
					return true;
				}
			}
		}
		return false;
	}

	void AimOn(const Column& C, int Index, const std::string& How, double Blast, uint32_t Salt, double& OutX, double& OutY)
	{
		const Piece& P = C.Pieces[static_cast<size_t>(Index)];
		const double Fx = std::sin(P.Face), Fy = std::cos(P.Face);
		const uint32_t H = Hash(static_cast<uint32_t>(P.Id) * 7919u, Salt);
		const double A = UnitOf(H) - 0.5, B = UnitOf(Hash(H, 1)) - 0.5;
		// On it: within the middle of its footprint, and clear of every other piece by more than Blast; where that spot is
		// not, another on it that is, or failing any, the one of them farthest from the others (the reviewer, 25
		// November: it had been the first spot whatever stood by it).
		OutX = P.X + Fx * (A * 0.6 * P.Long) + Fy * (B * 0.6 * P.Wide);
		OutY = P.Y + Fy * (A * 0.6 * P.Long) - Fx * (B * 0.6 * P.Wide);
		if (How != "near" && How != "scatter")
		{
			if (Blast <= 0.0 || Clear(C, OutX, OutY, Blast, Index)) return;
			double Room = Clearance(C, OutX, OutY, Index);
			for (uint32_t Try = 0; Try < 8; ++Try)
			{
				const uint32_t Ht = Hash(H, 20 + Try);
				const double At = UnitOf(Ht) - 0.5, Bt = UnitOf(Hash(Ht, 1)) - 0.5;
				const double X = P.X + Fx * (At * 0.8 * P.Long) + Fy * (Bt * 0.8 * P.Wide), Y = P.Y + Fy * (At * 0.8 * P.Long) - Fx * (Bt * 0.8 * P.Wide);
				const double Here = Clearance(C, X, Y, Index);
				if (Here > Room)
				{
					Room = Here;
					OutX = X;
					OutY = Y;
				}
				if (Room > Blast) return;
			}
			return;
		}
		// Beside it: out from its side, a quarter to two thirds of the blast from its edge (men scattered: nearer), and
		// clear of every other piece; the other side, or nearer, if not. Where none of the eight will do, the one of them
		// farthest from the other pieces: still beside it, never on it, so a near miss is never drawn as a hit (the
		// reviewer, 25 November: it had fallen back to the spot on it).
		const double Least = How == "scatter" ? 0.2 : 0.3, Most = How == "scatter" ? 0.45 : 0.65;
		const double Hand = UnitOf(Hash(H, 2)) < 0.5 ? 1.0 : -1.0;
		double Room = -1.0;
		for (int Try = 0; Try < 8; ++Try)
		{
			const double Side = (Try % 2 ? -Hand : Hand);
			const double Edge = Blast * (Least + (Most - Least) * UnitOf(Hash(H, 3 + static_cast<uint32_t>(Try)))) * (Try >= 4 ? 0.5 : 1.0);
			const double V = Side * (P.Wide * 0.5 + std::max(Edge, 1.0));
			const double U = A * 0.5 * P.Long;
			const double X = P.X + Fx * U + Fy * V, Y = P.Y + Fy * U - Fx * V;
			if (Clear(C, X, Y, Blast, Index))
			{
				OutX = X;
				OutY = Y;
				return;
			}
			const double Here = Clearance(C, X, Y, Index);
			if (Here > Room)
			{
				Room = Here;
				OutX = X;
				OutY = Y;
			}
		}
	}

	State StateAfter(const std::string& How, State Now)
	{
		// (a near miss and men scattered leave it as the rules last said it, until a view's word says what it did: the
		// scene does not decide that a near miss damaged a vehicle (the reviewer, 25 November))
		State To = Now;
		if (How == "wreck" || How == "blast") To = State::Out;
		else if (How == "burn") To = State::Burn;
		return static_cast<int>(To) > static_cast<int>(Now) ? To : Now;
	}

	bool Apply(Piece& P, State To, int Men)
	{
		if (P.Now == State::Out) return false;
		const int MenNow = Men >= 0 ? std::clamp(Men, 0, std::max(P.N, 1)) : P.Men;
		if (To == P.Now && MenNow == P.Men) return false;
		P.Now = To;
		P.Men = MenNow;
		return true;
	}

	double Lane::Long() const
	{
		return std::max(1.0, std::hypot(ToX - FromX, ToY - FromY));
	}

	double Lane::Way() const
	{
		return std::atan2(ToX - FromX, ToY - FromY);
	}

	void Lane::Along(double X, double Y, double& U, double& V) const
	{
		const double L = Long();
		const double Fx = (ToX - FromX) / L, Fy = (ToY - FromY) / L;
		U = (X - FromX) * Fx + (Y - FromY) * Fy;
		V = (X - FromX) * Fy - (Y - FromY) * Fx;
	}

	void Lane::At(double U, double V, double& X, double& Y) const
	{
		const double L = Long();
		const double Fx = (ToX - FromX) / L, Fy = (ToY - FromY) / L;
		X = FromX + Fx * U + Fy * V;
		Y = FromY + Fy * U - Fx * V;
	}

	Lane LaneOf(const Column& C, double AimX, double AimY)
	{
		Lane L;
		L.Width = 60.0;
		if (C.Road.size() >= 2)
		{
			double Along = 0.0, OffBy = 0.0;
			OffRoad(C.Road, AimX, AimY, Along, OffBy);
			double Way = 0.0;
			OnRoad(C.Road, std::max(0.0, Along - 600.0), L.FromX, L.FromY, Way);
			OnRoad(C.Road, std::min(RoadLong(C.Road), Along + 600.0), L.ToX, L.ToY, Way);
			return L;
		}
		double Mx = AimX, My = AimY, Face = 0.0;
		if (C.Has())
		{
			Mx = My = 0.0;
			for (const Piece& P : C.Pieces) { Mx += P.X; My += P.Y; }
			Mx /= static_cast<double>(C.Pieces.size());
			My /= static_cast<double>(C.Pieces.size());
			Face = C.Pieces.front().Face;
		}
		L.FromX = Mx - std::sin(Face) * 600.0;
		L.FromY = My - std::cos(Face) * 600.0;
		L.ToX = Mx + std::sin(Face) * 600.0;
		L.ToY = My + std::cos(Face) * 600.0;
		return L;
	}

	std::vector<int> InLane(const Column& C, const Lane& L)
	{
		std::vector<std::pair<double, int>> Found;
		for (size_t K = 0; K < C.Pieces.size(); ++K)
		{
			const Piece& P = C.Pieces[K];
			double U = 0.0, V = 0.0;
			L.Along(P.X, P.Y, U, V);
			const double Reach = 0.5 * std::max(P.Long, P.Wide);
			if (U >= -Reach && U <= L.Long() + Reach && std::fabs(V) <= L.Width * 0.5 + Reach) Found.emplace_back(U, static_cast<int>(K));
		}
		std::sort(Found.begin(), Found.end());
		std::vector<int> Out;
		Out.reserve(Found.size());
		for (const auto& F : Found) Out.push_back(F.second);
		return Out;
	}

	Pass PassOf(const std::string& Model)
	{
		// A fighter flies its pass by LandAir's own figures for one (its Machine as it stands before MachineOf fits it to an
		// air force: from 800 m in a 25 degree dive, guns from 900 m to 250 m, out 45 m up at 4 g), read from there, so the
		// land battle's ground attack and this pass are one table (the reviewer, 25 November). The air battle knows the
		// group's own aircraft, where the land battle picks an air force's machine for a year, so MachineOf itself is not
		// asked; its Stuka and Il-2 rows are the two below, as it gives them (its .cpp is not built into the cloud's air
		// tests, which would need the land battle's feed with it).
		const LandAir::Machine Fighter;
		Pass P;
		P.High = Fighter.High;
		P.Low = Fighter.Low;
		P.Dive = Fighter.Dive;
		P.Climb = Fighter.Climb;
		P.Open = Fighter.Open;
		P.Shut = Fighter.Shut;
		P.PullG = Fighter.PullG;
		P.Speed = Fighter.Speed;
		std::string M;
		for (char Ch : Model) if (Ch != '-' && Ch != ' ' && Ch != '.') M += static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
		if (M == "stuka" || M == "ju87")
		{
			// (LandAir's Stuka: from high, rolled over into a 75 degree dive, a short burst on the way down, out at 300 m)
			P.High = 1800.0; P.Low = 300.0; P.Dive = 75.0; P.Climb = 30.0; P.Open = 1100.0; P.Shut = 700.0; P.PullG = 6.0; P.Speed = 140.0;
		}
		else if (M == "sturmovik" || M == "il2")
		{
			// (LandAir's Il-2: low and shallow, out at 15 m)
			P.High = 160.0; P.Low = 15.0; P.Dive = 8.0; P.Climb = 18.0; P.Open = 700.0; P.Shut = 200.0; P.PullG = 3.0; P.Speed = 105.0;
		}
		else if (M == "fw190" || M == "mustang" || M == "p51" || M == "spitfire")
		{
			P.Speed = 140.0;
		}
		else if (M == "hurricane" || M == "zero" || M == "a6m")
		{
			P.Speed = 120.0;
		}
		return P;
	}

	double PullRoom(double Speed, double Gam, double PullG)
	{
		// (a circle of radius v squared over g (n - 1) from the dive's angle to level: R (1 - cos), and the roll's and the
		// pilot's half second at the dive's sink before the pull bites)
		const double G = 9.81;
		const double Down = std::max(0.0, -Gam);
		const double R = Speed * Speed / (G * std::max(0.5, PullG - 1.0));
		return R * (1.0 - std::cos(Down)) + Speed * std::sin(Down) * 0.5;
	}

	bool BurstAt(int PieceId, double AirT, Burst& Out)
	{
		// (a gun's bursts in 30 s cycles: each cycle's laid out from its own hash, so any air time finds its burst at once)
		const double Cycle = 30.0;
		const double C0 = std::floor(AirT / Cycle);
		const uint32_t Base = Hash(static_cast<uint32_t>(PieceId) * 104729u, static_cast<uint32_t>(static_cast<int64_t>(C0)));
		double T = C0 * Cycle + UnitOf(Base) * 1.5;
		for (uint32_t K = 0; K < 32 && T < (C0 + 1.0) * Cycle; ++K)
		{
			const uint32_t H = Hash(Base, K + 1);
			Burst B;
			B.On = T;
			B.Off = T + 1.0 + 1.4 * UnitOf(H);
			B.Across = -18.0 + 36.0 * UnitOf(Hash(H, 1));
			B.Up = -6.0 + 20.0 * UnitOf(Hash(H, 2));
			B.Lead = 0.2 + 0.5 * UnitOf(Hash(H, 3));
			if (AirT >= B.On && AirT < B.Off)
			{
				Out = B;
				return true;
			}
			T = B.Off + 0.5 + UnitOf(Hash(H, 4));
		}
		return false;
	}
}
