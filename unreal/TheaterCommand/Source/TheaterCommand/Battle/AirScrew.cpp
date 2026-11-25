#include "Battle/AirScrew.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <numeric>
#include <tuple>

namespace AirScrew
{
	namespace
	{
		Vec3 Add(const Vec3& A, const Vec3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		Vec3 Sub(const Vec3& A, const Vec3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		Vec3 Mul(const Vec3& A, double S) { return { A.X * S, A.Y * S, A.Z * S }; }
		double Dot(const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		Vec3 Cross(const Vec3& A, const Vec3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		double Len(const Vec3& A) { return std::sqrt(Dot(A, A)); }

		int Root(std::vector<int>& Up, int K)
		{
			while (Up[static_cast<size_t>(K)] != K)
			{
				Up[static_cast<size_t>(K)] = Up[static_cast<size_t>(Up[static_cast<size_t>(K)])];
				K = Up[static_cast<size_t>(K)];
			}
			return K;
		}

		/** One blade read: where its tip is, the way out from root to tip, and the radius its length gives. */
		struct Read
		{
			bool bGood = false;
			Vec3 Hub, Out;
			double Radius = 0.0;
		};

		Read ReadBlade(const std::vector<Vec3>& Points, const std::vector<int>& Of)
		{
			Read R;
			if (Of.size() < 8) return R;
			Vec3 Mid;
			for (int K : Of) Mid = Add(Mid, Points[static_cast<size_t>(K)]);
			Mid = Mul(Mid, 1.0 / static_cast<double>(Of.size()));
			// (its long axis: the way the points spread most, by a few turns of the power method from the farthest point)
			double Cov[3][3] = {};
			Vec3 Far;
			double FarD = -1.0;
			for (int K : Of)
			{
				const Vec3 D = Sub(Points[static_cast<size_t>(K)], Mid);
				const double V[3] = { D.X, D.Y, D.Z };
				for (int I = 0; I < 3; ++I)
					for (int J = 0; J < 3; ++J) Cov[I][J] += V[I] * V[J];
				if (Dot(D, D) > FarD) { FarD = Dot(D, D); Far = D; }
			}
			if (FarD <= 1e-12) return R;
			Vec3 Way = Mul(Far, 1.0 / std::sqrt(FarD));
			for (int Turn = 0; Turn < 24; ++Turn)
			{
				const Vec3 Next = { Cov[0][0] * Way.X + Cov[0][1] * Way.Y + Cov[0][2] * Way.Z, Cov[1][0] * Way.X + Cov[1][1] * Way.Y + Cov[1][2] * Way.Z,
				                    Cov[2][0] * Way.X + Cov[2][1] * Way.Y + Cov[2][2] * Way.Z };
				const double L = Len(Next);
				if (L <= 1e-18) return R;
				Way = Mul(Next, 1.0 / L);
			}
			double Lo = 1e18, Hi = -1e18;
			for (int K : Of)
			{
				const double S = Dot(Sub(Points[static_cast<size_t>(K)], Mid), Way);
				Lo = std::min(Lo, S);
				Hi = std::max(Hi, S);
			}
			const double Long = Hi - Lo;
			if (Long <= 1e-3) return R;
			// (the root is the fat end: its points stand furthest off the axis)
			double SpreadLo = 0.0, SpreadHi = 0.0;
			for (int K : Of)
			{
				const Vec3 D = Sub(Points[static_cast<size_t>(K)], Mid);
				const double S = Dot(D, Way);
				const double Off = Len(Sub(D, Mul(Way, S)));
				if (S < Lo + Long * 0.06) SpreadLo = std::max(SpreadLo, Off);
				if (S > Hi - Long * 0.06) SpreadHi = std::max(SpreadHi, Off);
			}
			if (SpreadLo == SpreadHi) return R;
			const bool bRootLo = SpreadLo > SpreadHi;
			R.Out = bRootLo ? Way : Mul(Way, -1.0);
			const Vec3 Tip = Add(Mid, Mul(Way, bRootLo ? Hi : Lo));
			// (the rings run from a tenth of the radius to the whole of it: the blade is nine tenths of the radius long)
			R.Radius = Long / 0.9;
			R.Hub = Sub(Tip, Mul(R.Out, R.Radius));
			// (a blade much fatter than build_plane.py's is not one of its blades)
			R.bGood = std::max(SpreadLo, SpreadHi) < R.Radius * 0.25;
			return R;
		}
	}

	Found Find(const std::vector<Vec3>& Points, const std::vector<bool>& Blade, const std::vector<int>& Triangles)
	{
		Found Out;
		Out.BladeTriangle.assign(Triangles.size() / 3, false);
		const int N = static_cast<int>(Points.size());
		auto IsBlade = [&](int K) { return K >= 0 && K < N && static_cast<size_t>(K) < Blade.size() && Blade[static_cast<size_t>(K)]; };
		// The blades: the blade points their triangles join.
		std::vector<int> Up(static_cast<size_t>(N));
		std::iota(Up.begin(), Up.end(), 0);
		for (size_t T = 0; T + 2 < Triangles.size(); T += 3)
		{
			const int A = Triangles[T], B = Triangles[T + 1], C = Triangles[T + 2];
			if (!IsBlade(A) || !IsBlade(B) || !IsBlade(C)) continue;
			Up[static_cast<size_t>(Root(Up, B))] = Root(Up, A);
			Up[static_cast<size_t>(Root(Up, C))] = Root(Up, A);
		}
		// (and the points that stand at one place: the model's skin is split where its shading turns, a blade's faces and
		// its end caps each a skin of its own over points of their own)
		{
			std::map<std::tuple<long long, long long, long long>, int> Seen;
			for (int K = 0; K < N; ++K)
			{
				if (!IsBlade(K)) continue;
				const Vec3& P = Points[static_cast<size_t>(K)];
				const auto Cell = std::make_tuple(std::llround(P.X * 2000.0), std::llround(P.Y * 2000.0), std::llround(P.Z * 2000.0));
				const auto It = Seen.find(Cell);
				if (It == Seen.end()) Seen.emplace(Cell, K);
				else Up[static_cast<size_t>(Root(Up, K))] = Root(Up, It->second);
			}
		}
		std::vector<int> Which(static_cast<size_t>(N), -1);
		std::vector<std::vector<int>> Blades;
		for (int K = 0; K < N; ++K)
		{
			if (!IsBlade(K)) continue;
			const int R = Root(Up, K);
			if (Which[static_cast<size_t>(R)] < 0)
			{
				Which[static_cast<size_t>(R)] = static_cast<int>(Blades.size());
				Blades.emplace_back();
			}
			Which[static_cast<size_t>(K)] = Which[static_cast<size_t>(R)];
			Blades[static_cast<size_t>(Which[static_cast<size_t>(R)])].push_back(K);
		}
		// Each blade read, and the blades whose hubs meet gathered into one screw.
		struct Gather
		{
			Vec3 Sum;
			double RadiusSum = 0.0;
			std::vector<int> Of; // (blades)
			std::vector<Vec3> Outs;
		};
		std::vector<Gather> Screws;
		std::vector<int> ScrewOf(Blades.size(), -1);
		for (size_t B = 0; B < Blades.size(); ++B)
		{
			const Read R = ReadBlade(Points, Blades[B]);
			if (!R.bGood) continue;
			int Into = -1;
			for (size_t S = 0; S < Screws.size() && Into < 0; ++S)
			{
				const double Many = static_cast<double>(Screws[S].Of.size());
				const Vec3 At = Mul(Screws[S].Sum, 1.0 / Many);
				const double Radius = Screws[S].RadiusSum / Many;
				if (Len(Sub(At, R.Hub)) < 0.2 * std::max(Radius, R.Radius) && std::fabs(Radius - R.Radius) < 0.15 * Radius) Into = static_cast<int>(S);
			}
			if (Into < 0)
			{
				Into = static_cast<int>(Screws.size());
				Screws.emplace_back();
			}
			Gather& G = Screws[static_cast<size_t>(Into)];
			G.Sum = Add(G.Sum, R.Hub);
			G.RadiusSum += R.Radius;
			G.Of.push_back(static_cast<int>(B));
			G.Outs.push_back(R.Out);
			ScrewOf[B] = Into;
		}
		std::vector<bool> Kept(Screws.size(), false);
		for (size_t S = 0; S < Screws.size(); ++S)
		{
			const Gather& G = Screws[S];
			if (G.Of.size() < 2) continue;
			// (its shaft: square to the blades, from the two that lie most across each other)
			Vec3 Axis;
			double Best = 0.0;
			for (size_t I = 0; I < G.Outs.size(); ++I)
				for (size_t J = I + 1; J < G.Outs.size(); ++J)
				{
					const Vec3 C = Cross(G.Outs[I], G.Outs[J]);
					if (Len(C) > Best) { Best = Len(C); Axis = C; }
				}
			if (Best < 0.3) continue;
			Screw Made;
			const double Many = static_cast<double>(G.Of.size());
			Made.Hub = Mul(G.Sum, 1.0 / Many);
			Made.Axis = Mul(Axis, 1.0 / Best);
			Made.Radius = G.RadiusSum / Many;
			Made.Blades = static_cast<int>(G.Of.size());
			Out.Screws.push_back(Made);
			Kept[S] = true;
		}
		// (A piece painted blade that is no blade to read on its own, made apart from the blade it wraps (the
		// Wellington's painted tip bands, 23 November), goes with the screw whose disc holds every point of it, a
		// quarter of its radius either side of the disc at most: left behind, it would hang still in the turning disc.)
		std::vector<int> KeptAt(Screws.size(), -1);
		for (size_t S = 0, At = 0; S < Screws.size(); ++S)
		{
			if (Kept[S]) KeptAt[S] = static_cast<int>(At++);
		}
		for (size_t B = 0; B < Blades.size() && !Out.Screws.empty(); ++B)
		{
			if (ScrewOf[B] >= 0 && Kept[static_cast<size_t>(ScrewOf[B])]) continue;
			for (size_t S = 0; S < Screws.size(); ++S)
			{
				if (!Kept[S]) continue;
				const Screw& Disc = Out.Screws[static_cast<size_t>(KeptAt[S])];
				bool bInside = true;
				for (const int K : Blades[B])
				{
					const Vec3 D = Sub(Points[static_cast<size_t>(K)], Disc.Hub);
					const double Along = Dot(D, Disc.Axis);
					if (std::fabs(Along) > Disc.Radius * 0.25 || Len(Sub(D, Mul(Disc.Axis, Along))) > Disc.Radius * 1.05) { bInside = false; break; }
				}
				if (bInside) { ScrewOf[B] = static_cast<int>(S); break; }
			}
		}
		for (size_t T = 0; T + 2 < Triangles.size(); T += 3)
		{
			const int A = Triangles[T];
			if (!IsBlade(A) || !IsBlade(Triangles[T + 1]) || !IsBlade(Triangles[T + 2])) continue;
			const int B = Which[static_cast<size_t>(A)];
			const int S = B >= 0 ? ScrewOf[static_cast<size_t>(B)] : -1;
			if (S >= 0 && Kept[static_cast<size_t>(S)]) Out.BladeTriangle[T / 3] = true;
		}
		return Out;
	}

	PaintsAre PaintKinds(const std::string& FileText)
	{
		PaintsAre Are;
		const size_t Key = FileText.find("\"paint_names\"");
		const size_t Open = Key == std::string::npos ? std::string::npos : FileText.find('[', Key);
		const size_t Close = Open == std::string::npos ? std::string::npos : FileText.find(']', Open);
		if (Close == std::string::npos)
		{
			Are.Blade.assign(static_cast<size_t>(BladePaint) + 1, false);
			Are.Blade[static_cast<size_t>(BladePaint)] = true;
			return Are;
		}
		size_t At = Open + 1;
		while (At < Close)
		{
			const size_t Begin = FileText.find('"', At);
			if (Begin == std::string::npos || Begin >= Close) break;
			const size_t End = FileText.find('"', Begin + 1);
			if (End == std::string::npos || End > Close) break;
			const std::string Name = FileText.substr(Begin + 1, End - Begin - 1);
			Are.Blade.push_back(Name.rfind("blade", 0) == 0);
			Are.Blur.push_back(Name.rfind("blur", 0) == 0);
			At = End + 1;
		}
		return Are;
	}

	std::vector<int> NearPaints(const std::string& FileText)
	{
		std::vector<int> Paints;
		const size_t Near = FileText.find("\"near\"");
		if (Near == std::string::npos) return Paints;
		const size_t Key = FileText.find("\"paints\"", Near);
		if (Key == std::string::npos) return Paints;
		const size_t Open = FileText.find('[', Key);
		const size_t Close = Open == std::string::npos ? std::string::npos : FileText.find(']', Open);
		if (Close == std::string::npos) return Paints;
		const char* At = FileText.c_str() + Open + 1;
		const char* End = FileText.c_str() + Close;
		while (At < End)
		{
			char* Next = nullptr;
			const long Value = std::strtol(At, &Next, 10);
			if (Next == At) { ++At; continue; }
			Paints.push_back(static_cast<int>(Value));
			At = Next;
		}
		return Paints;
	}
}
