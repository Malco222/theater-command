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

		/** (the row "flyPropBlur") The strings of the array a file's key Key holds, the key with its array after it:
		 * "paint_names", "colours". Empty if it has none. */
		std::vector<std::string> Strings(const std::string& FileText, const char* Key)
		{
			std::vector<std::string> Out;
			const std::string Quoted = std::string("\"") + Key + "\"";
			for (size_t At = FileText.find(Quoted); At != std::string::npos; At = FileText.find(Quoted, At + 1))
			{
				size_t Next = At + Quoted.size();
				while (Next < FileText.size() && (FileText[Next] == ' ' || FileText[Next] == '\n')) ++Next;
				if (Next >= FileText.size() || FileText[Next] != ':') continue;
				++Next;
				while (Next < FileText.size() && (FileText[Next] == ' ' || FileText[Next] == '\n')) ++Next;
				if (Next >= FileText.size() || FileText[Next] != '[') continue;
				const size_t Close = FileText.find(']', Next);
				if (Close == std::string::npos) return Out;
				for (size_t From = Next + 1; From < Close;)
				{
					const size_t Begin = FileText.find('"', From);
					if (Begin == std::string::npos || Begin >= Close) break;
					const size_t End = FileText.find('"', Begin + 1);
					if (End == std::string::npos || End > Close) break;
					Out.push_back(FileText.substr(Begin + 1, End - Begin - 1));
					From = End + 1;
				}
				return Out;
			}
			return Out;
		}
	}

	Found Find(const std::vector<Vec3>& Points, const std::vector<bool>& Blade, const std::vector<int>& Triangles)
	{
		Found Out;
		Out.BladeTriangle.assign(Triangles.size() / 3, false);
		Out.ScrewOfTriangle.assign(Triangles.size() / 3, -1);
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
			if (S >= 0 && Kept[static_cast<size_t>(S)])
			{
				Out.BladeTriangle[T / 3] = true;
				Out.ScrewOfTriangle[T / 3] = KeptAt[static_cast<size_t>(S)];
			}
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
			Are.Decal.push_back(Name.rfind("decal", 0) == 0);
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
	std::vector<int> FarPaints(const std::string& FileText)
	{
		std::vector<int> Paints;
		// (the far mesh's own key, "far" with its object after it: the word may stand elsewhere as a value)
		size_t Far = std::string::npos;
		for (size_t At = FileText.find("\"far\""); At != std::string::npos && Far == std::string::npos; At = FileText.find("\"far\"", At + 1))
		{
			size_t Next = At + 5;
			while (Next < FileText.size() && (FileText[Next] == ' ' || FileText[Next] == '\n')) ++Next;
			if (Next < FileText.size() && FileText[Next] == ':')
			{
				++Next;
				while (Next < FileText.size() && (FileText[Next] == ' ' || FileText[Next] == '\n')) ++Next;
				if (Next < FileText.size() && FileText[Next] == '{') Far = Next;
			}
		}
		if (Far == std::string::npos) return Paints;
		const size_t Key = FileText.find("\"paints\"", Far);
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

	Stripped StripScrews(const std::vector<Vec3>& Points, const std::vector<int>& Paints, const PaintsAre& Are, const std::vector<int>& Triangles)
	{
		Stripped Out;
		const size_t Count = Triangles.size() / 3;
		Out.Drop.assign(Count, false);
		// (a point past the paints, a marking laid on after the file's points, is no blade)
		std::vector<bool> Blade(std::min(Points.size(), Paints.size()), false);
		for (size_t K = 0; K < Blade.size(); ++K) Blade[K] = Are.IsBlade(Paints[K]);
		const Found F = Find(Points, Blade, Triangles);
		Out.Screws = F.Screws;
		Out.Blades.assign(F.Screws.size(), {});
		auto IsBlur = [&](int Point) { return Point >= 0 && static_cast<size_t>(Point) < Paints.size() && Are.IsBlur(Paints[static_cast<size_t>(Point)]); };
		for (size_t T = 0; T < Count; ++T)
		{
			if (F.BladeTriangle[T])
			{
				Out.Drop[T] = true;
				++Out.BladeTriangles;
				const int S = F.ScrewOfTriangle[T];
				if (S >= 0 && static_cast<size_t>(S) < Out.Blades.size()) Out.Blades[static_cast<size_t>(S)].push_back(static_cast<int>(T));
			}
			else if (!F.Screws.empty() && IsBlur(Triangles[T * 3]) && IsBlur(Triangles[T * 3 + 1]) && IsBlur(Triangles[T * 3 + 2]))
			{
				Out.Drop[T] = true;
				++Out.BlurTriangles;
			}
		}
		return Out;
	}

	std::vector<int> SameScrews(const std::vector<Screw>& A, const std::vector<Screw>& B, double Share)
	{
		std::vector<int> Which(B.size(), -1);
		if (A.size() != B.size() || A.empty()) return {};
		std::vector<bool> Taken(A.size(), false);
		for (size_t J = 0; J < B.size(); ++J)
		{
			for (size_t I = 0; I < A.size() && Which[J] < 0; ++I)
			{
				if (Taken[I]) continue;
				const double R = A[I].Radius;
				if (Len(Sub(A[I].Hub, B[J].Hub)) <= Share * R && std::fabs(A[I].Radius - B[J].Radius) <= Share * R && A[I].Blades == B[J].Blades)
				{
					Which[J] = static_cast<int>(I);
					Taken[I] = true;
				}
			}
			if (Which[J] < 0) return {};
		}
		return Which;
	}

	double SmearAlpha(double Ahead, int Ring)
	{
		if (Ring <= 0 || Ring >= 3) return 0.0;
		const double Behind = 1.0 - Ahead;
		// (the smear behind a blade, darkest at its trailing edge; its sharp leading edge just ahead of it; a faint haze
		// between; the tips' ring a little lighter, the disc's edge soft)
		const double Smear = 0.22 + 0.45 * std::exp(-6.0 * Behind) + 0.6 * std::exp(-40.0 * Ahead);
		return std::min(0.85, Smear) * (Ring == 2 ? 0.8 : 1.0);
	}

	DiscMade MakeDisc(double Radius, int Blades, bool bHaze)
	{
		DiscMade Out;
		const double TwoPi = 6.28318530717958647692;
		const int Round = bHaze ? HazeRound : NearRound;
		const int Many = std::max(1, Blades);
		for (int Ring = 0; Ring < 4; ++Ring)
		{
			for (int K = 0; K < Round; ++K)
			{
				const double A = TwoPi * K / Round;
				Out.Points.push_back({ 0.0, std::sin(A) * Radius * DiscRings[Ring], std::cos(A) * Radius * DiscRings[Ring] });
				// (the disc turns toward a growing angle: a point a share Ahead past a blade is that far ahead of it)
				const double Turned = A * Many / TwoPi;
				const double Ahead = Turned - std::floor(Turned);
				const double Alpha = bHaze ? HazeAlpha[Ring] : 255.0 * SmearAlpha(Ahead, Ring);
				const unsigned char* Rgb = bHaze ? HazeRgb : SmearRgb;
				Out.Red.push_back(Rgb[0]);
				Out.Green.push_back(Rgb[1]);
				Out.Blue.push_back(Rgb[2]);
				Out.Alpha.push_back(static_cast<unsigned char>(std::lround(std::clamp(Alpha, 0.0, 255.0))));
			}
		}
		// (its front faces, then its back ones)
		for (int Side = 0; Side < 2; ++Side)
		{
			for (int Ring = 0; Ring < 3; ++Ring)
			{
				for (int K = 0; K < Round; ++K)
				{
					const int A = Ring * Round + K, B = Ring * Round + (K + 1) % Round, C = A + Round, D = B + Round;
					if (Side == 0) Out.Triangles.insert(Out.Triangles.end(), { A, B, D, A, D, C });
					else Out.Triangles.insert(Out.Triangles.end(), { A, D, B, A, C, D });
				}
			}
			if (Side == 0) Out.Front = Out.Triangles.size();
		}
		return Out;
	}

	DiscMade MakeBlurDisc(double Radius, int Blades)
	{
		// (MakeDisc's far haze, point for point and triangle for triangle, in the blur's own grey and alpha ring by ring)
		DiscMade Out = MakeDisc(Radius, Blades, true);
		for (size_t K = 0; K < Out.Points.size(); ++K)
		{
			const size_t Ring = std::min<size_t>(3, K / static_cast<size_t>(HazeRound));
			Out.Red[K] = BlurRgb[0];
			Out.Green[K] = BlurRgb[1];
			Out.Blue[K] = BlurRgb[2];
			Out.Alpha[K] = BlurAlpha[Ring];
		}
		return Out;
	}

	std::vector<bool> BladeDecals(const std::vector<Vec3>& Points, const std::vector<int>& Paints, const PaintsAre& Are, const std::vector<int>& Triangles,
	                              const std::vector<Screw>& Screws)
	{
		const size_t Count = Triangles.size() / 3;
		std::vector<bool> Out(Count, false);
		auto IsMark = [&](int Point) {
			return Point >= 0 && static_cast<size_t>(Point) < Paints.size() && static_cast<size_t>(Point) < Points.size() && Are.IsDecal(Paints[static_cast<size_t>(Point)]);
		};
		auto Inside = [&](int Point, const Screw& S) {
			const Vec3 D = Sub(Points[static_cast<size_t>(Point)], S.Hub);
			const double Along = Dot(D, S.Axis);
			return std::fabs(Along) <= S.Radius * 0.25 && Len(Sub(D, Mul(S.Axis, Along))) <= S.Radius * 1.05;
		};
		for (size_t T = 0; T < Count; ++T)
		{
			const int A = Triangles[T * 3], B = Triangles[T * 3 + 1], C = Triangles[T * 3 + 2];
			if (!IsMark(A) || !IsMark(B) || !IsMark(C)) continue;
			for (const Screw& S : Screws)
			{
				if (Inside(A, S) && Inside(B, S) && Inside(C, S)) { Out[T] = true; break; }
			}
		}
		return Out;
	}

	bool TipColour(const std::string& FileText, const std::vector<int>& NearPaints, unsigned char Rgb[3])
	{
		const std::vector<std::string> Names = Strings(FileText, "paint_names");
		const std::vector<std::string> Colours = Strings(FileText, "colours");
		for (const char* Want : { "blade_tip", "blur_tip" })
		{
			const auto Named = std::find(Names.begin(), Names.end(), std::string(Want));
			if (Named == Names.end()) continue;
			const int Paint = static_cast<int>(Named - Names.begin());
			if (std::find(NearPaints.begin(), NearPaints.end(), Paint) == NearPaints.end() || static_cast<size_t>(Paint) >= Colours.size()) continue;
			std::string Hex = Colours[static_cast<size_t>(Paint)];
			if (!Hex.empty() && Hex[0] == '#') Hex = Hex.substr(1);
			if (Hex.size() < 6) continue;
			char* End = nullptr;
			const unsigned long Value = std::strtoul(Hex.substr(0, 6).c_str(), &End, 16);
			if (!End || *End != '\0') continue;
			Rgb[0] = static_cast<unsigned char>((Value >> 16) & 0xFF);
			Rgb[1] = static_cast<unsigned char>((Value >> 8) & 0xFF);
			Rgb[2] = static_cast<unsigned char>(Value & 0xFF);
			return true;
		}
		return false;
	}

	double TipAlpha(double Ahead)
	{
		// (brightest just behind the blade, where its tip has just been; a faint ring all round, as the tips' paint is
		// seen through the whole of a film's turning disc)
		const double Behind = 1.0 - Ahead;
		return (0.15 + 0.85 * std::exp(-3.0 * Behind)) * TipMost / 255.0;
	}

	DiscMade MakeTipArcs(double Radius, int Blades, const unsigned char Rgb[3])
	{
		DiscMade Out;
		const double TwoPi = 6.28318530717958647692;
		const int Round = NearRound;
		const int Many = std::max(1, Blades);
		// (the paint made linear, as the aircraft's own ink makes it: Power 2.2)
		unsigned char Ink[3];
		for (int C = 0; C < 3; ++C) Ink[C] = static_cast<unsigned char>(std::lround(255.0 * std::pow(Rgb[C] / 255.0, 2.2)));
		for (int Ring = 0; Ring < 3; ++Ring)
		{
			for (int K = 0; K < Round; ++K)
			{
				const double A = TwoPi * K / Round;
				Out.Points.push_back({ 0.0, std::sin(A) * Radius * TipRings[Ring], std::cos(A) * Radius * TipRings[Ring] });
				// (Ahead as MakeDisc's: the disc turns toward a growing angle)
				const double Turned = A * Many / TwoPi;
				const double Ahead = Turned - std::floor(Turned);
				const double Alpha = Ring == 1 ? 255.0 * TipAlpha(Ahead) : 0.0;
				Out.Red.push_back(Ink[0]);
				Out.Green.push_back(Ink[1]);
				Out.Blue.push_back(Ink[2]);
				Out.Alpha.push_back(static_cast<unsigned char>(std::lround(std::clamp(Alpha, 0.0, 255.0))));
			}
		}
		for (int Side = 0; Side < 2; ++Side)
		{
			for (int Ring = 0; Ring < 2; ++Ring)
			{
				for (int K = 0; K < Round; ++K)
				{
					const int A = Ring * Round + K, B = Ring * Round + (K + 1) % Round, C = A + Round, D = B + Round;
					if (Side == 0) Out.Triangles.insert(Out.Triangles.end(), { A, B, D, A, D, C });
					else Out.Triangles.insert(Out.Triangles.end(), { A, D, B, A, C, D });
				}
			}
			if (Side == 0) Out.Front = Out.Triangles.size();
		}
		return Out;
	}
}
