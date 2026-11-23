#include "LandBail.h"

#include "Battle/Fates.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace LandBail
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		// (his way out, life's seconds: up out of the hatch, along the deck to her edge, down to the ground)
		constexpr double ClimbS = 1.0, StepS = 0.5, JumpS = 0.55, OnGroundS = ClimbS + StepS + JumpS;
		constexpr double ProneS = 0.9; // (a crawler going down flat before he crawls; a wounded man as he falls)

		/** Its own dice (splitmix64): the same wreck, the same crew. */
		struct Dice
		{
			uint64_t S;
			explicit Dice(uint64_t Seed) : S(Seed ^ 0xBA11017C4E3ull) {}
			uint64_t Next()
			{
				uint64_t Z = (S += 0x9E3779B97F4A7C15ull);
				Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
				Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
				return Z ^ (Z >> 31);
			}
			double U() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }
			double U(double A, double B) { return A + (B - A) * U(); }
			int Whole(int A, int B) { return B <= A ? A : A + static_cast<int>(Next() % static_cast<uint64_t>(B - A + 1)); }
			bool P(double Chance) { return U() < Chance; }
		};

		/** The table's numbers and words for one wreck: a dial ("$out") drawn once and kept for every step that names it. */
		struct Draws
		{
			const Fates::Json& Dials;
			Dice& D;
			std::map<std::string, double> Numbers;
			std::map<std::string, std::string> Words;

			static bool IsPair(const Fates::Json& V)
			{
				return V.IsArray() && V.Count() == 2 && V.At(0).IsNumber() && V.At(1).IsNumber();
			}
			double Num(const Fates::Json& V, double Default, bool bWhole)
			{
				if (V.IsNumber())
				{
					return V.Num();
				}
				if (IsPair(V))
				{
					return bWhole ? D.Whole(static_cast<int>(std::lround(V.At(0).Num())), static_cast<int>(std::lround(V.At(1).Num())))
						: D.U(V.At(0).Num(), V.At(1).Num());
				}
				if (V.IsString() && V.Text.size() > 1 && V.Text[0] == '$')
				{
					const std::string Name = V.Text.substr(1);
					const auto Kept = Numbers.find(Name);
					if (Kept != Numbers.end())
					{
						return Kept->second;
					}
					const Fates::Json& Spec = Dials[Name];
					double Got = Default;
					if (Spec.IsObject() && Spec["pick"].IsArray() && Spec["pick"].Count() > 0)
					{
						// (a weighted pick: "pick": [0, 1, 2], "w": [0.3, 0.45, 0.25])
						const Fates::Json& Pick = Spec["pick"];
						const Fates::Json& W = Spec["w"];
						double Sum = 0.0;
						for (size_t I = 0; I < Pick.Count(); ++I)
						{
							Sum += W.IsArray() && I < W.Count() ? std::max(0.0, W.At(I).Num()) : 1.0;
						}
						double Roll = D.U() * Sum;
						Got = Pick.At(Pick.Count() - 1).Num(Default);
						for (size_t I = 0; I < Pick.Count(); ++I)
						{
							Roll -= W.IsArray() && I < W.Count() ? std::max(0.0, W.At(I).Num()) : 1.0;
							if (Roll < 0.0)
							{
								Got = Pick.At(I).Num(Default);
								break;
							}
						}
					}
					else if (Spec.IsObject() && IsPair(Spec["range"]))
					{
						Got = Num(Spec["range"], Default, bWhole);
					}
					else if (Spec.IsNumber() || IsPair(Spec))
					{
						Got = Num(Spec, Default, bWhole);
					}
					Numbers[Name] = Got;
					return Got;
				}
				return Default;
			}
			std::string Word(const Fates::Json& V, const char* Default)
			{
				if (V.IsString() && !(V.Text.size() > 1 && V.Text[0] == '$'))
				{
					return V.Text;
				}
				if (V.IsArray() && V.Count() > 0)
				{
					return V.At(static_cast<size_t>(D.Whole(0, static_cast<int>(V.Count()) - 1))).Str(Default);
				}
				if (V.IsString())
				{
					const std::string Name = V.Text.substr(1);
					const auto Kept = Words.find(Name);
					if (Kept != Words.end())
					{
						return Kept->second;
					}
					const std::string Got = Word(Dials[Name], Default);
					Words[Name] = Got;
					return Got;
				}
				return Default;
			}
		};

		double Ease(double X)
		{
			X = std::clamp(X, 0.0, 1.0);
			return X * X * (3.0 - 2.0 * X);
		}

		V3 Lerp(const V3& A, const V3& B, double T)
		{
			return V3{ A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T, A.Z + (B.Z - A.Z) * T };
		}

		double YawOf(double Dx, double Dy, double Else)
		{
			return Dx * Dx + Dy * Dy > 1e-8 ? std::atan2(Dx, Dy) : Else;
		}
	}

	Crew Make(const Wreck& W)
	{
		Crew Out;
		if (!W.Way || W.Exits.empty())
		{
			Out.Story = "nobody out";
			return Out;
		}
		Dice D(W.Seed);
		Draws Dr{ W.Way->Dials, D, {}, {} };
		auto Ground = [&](double X, double Y) { return W.Ground ? W.Ground(X, Y) : W.Middle.Z; };
		const V3 Fwd{ std::sin(W.Heading), std::cos(W.Heading), 0.0 };
		const V3 Right{ std::cos(W.Heading), -std::sin(W.Heading), 0.0 };
		const int Most = std::clamp(W.MostMen, 0, 8);
		double Back = -1.0;
		int Burning = 0;
		const Fates::Json& Beats = W.Way->Beats;
		for (size_t I = 0; I < Beats.Count(); ++I)
		{
			const Fates::Json& B = Beats.At(I);
			const std::string Do = B["do"].Str();
			if (Do != "crew_out" && Do != "crew_in")
			{
				continue;
			}
			if (B["p"].IsNumber() && !D.P(B["p"].Num(1.0)))
			{
				continue;
			}
			const double T = std::max(0.0, Dr.Num(B["t"], 1.0, false));
			if (Do == "crew_in")
			{
				Back = T;
				continue;
			}
			const int N = static_cast<int>(std::lround(Dr.Num(B["n"], 1.0, true)));
			double At = T;
			for (int J = 0; J < N && static_cast<int>(Out.Men.size()) < Most; ++J)
			{
				if (J > 0)
				{
					At += std::max(0.2, Dr.Num(B["c"], 0.8, false));
				}
				Man M;
				const size_t K = Out.Men.size();
				M.Exit = static_cast<int>(K % W.Exits.size());
				M.Out = At;
				M.Go = Dr.Word(B["kind"], "run");
				// (two carrying a third: run, for now (no carry clip yet); anything else the table names: run)
				if (M.Go != "crawl" && M.Go != "crouch")
				{
					M.Go = "run";
				}
				M.To = Dr.Word(B["at"], "rear");
				if (M.To != "cover" && M.To != "crater" && M.To != "beside")
				{
					M.To = "rear";
				}
				if (M.Go == "crouch")
				{
					M.To = M.To == "rear" ? "beside" : M.To;
				}
				// Where he makes for: away from her enemy, spread so no two run one line.
				double Way = W.Away;
				double Far = 0.0;
				if (M.To == "rear")
				{
					Way += D.U(-0.55, 0.55);
					Far = M.Go == "crawl" ? D.U(6.0, 12.0) : D.U(18.0, 38.0);
				}
				else if (M.To == "cover")
				{
					Way += (D.P(0.5) ? 1.0 : -1.0) * D.U(0.5, 1.3);
					Far = M.Go == "crawl" ? D.U(5.0, 10.0) : D.U(9.0, 18.0);
				}
				else if (M.To == "crater")
				{
					Way += D.U(-1.2, 1.2);
					Far = D.U(5.0, 13.0);
				}
				const V3 Dir{ std::sin(Way), std::cos(Way), 0.0 };
				// (he drops from the side of her hull that faces where he goes)
				const double Lean = Dir.X * Right.X + Dir.Y * Right.Y;
				M.Side = std::abs(Lean) < 0.15 ? (D.P(0.5) ? 1 : -1) : Lean > 0.0 ? 1 : -1;
				M.Hatch = W.Exits[static_cast<size_t>(M.Exit)];
				const double Along = std::clamp((M.Hatch.X - W.Middle.X) * Fwd.X + (M.Hatch.Y - W.Middle.Y) * Fwd.Y, -3.5, 3.5);
				const double Half = std::max(0.8, W.Wide * 0.5);
				M.Deck = V3{ W.Middle.X + Fwd.X * Along + Right.X * M.Side * (Half + 0.05), W.Middle.Y + Fwd.Y * Along + Right.Y * M.Side * (Half + 0.05), 0.0 };
				const double DeckGround = Ground(M.Deck.X, M.Deck.Y);
				M.Deck.Z = std::max(DeckGround + 0.8, std::min(M.Hatch.Z, DeckGround + 1.9));
				M.Down = V3{ W.Middle.X + Fwd.X * Along + Right.X * M.Side * (Half + 1.1), W.Middle.Y + Fwd.Y * Along + Right.Y * M.Side * (Half + 1.1), 0.0 };
				M.Down.Z = Ground(M.Down.X, M.Down.Y);
				if (M.To == "beside")
				{
					// (in her lee: a pace or two out from her side, toward her back)
					M.Goal = V3{ M.Down.X + Right.X * M.Side * D.U(0.8, 1.8) - Fwd.X * D.U(0.5, 2.0), M.Down.Y + Right.Y * M.Side * D.U(0.8, 1.8) - Fwd.Y * D.U(0.5, 2.0), 0.0 };
				}
				else
				{
					M.Goal = V3{ M.Down.X + Dir.X * Far, M.Down.Y + Dir.Y * Far, 0.0 };
				}
				M.Goal.Z = Ground(M.Goal.X, M.Goal.Y);
				M.Speed = M.Go == "crawl" ? D.U(0.45, 0.7) : M.Go == "crouch" ? D.U(2.2, 3.0) : D.U(4.2, 5.4);
				M.Fall = D.P(0.6) ? 0 : 1;
				const double Run = std::hypot(M.Goal.X - M.Down.X, M.Goal.Y - M.Down.Y) / M.Speed + (M.Go == "crawl" ? ProneS : 0.0);
				// How it goes for him.
				if (W.Blast >= 0.0 && M.Out >= W.Blast - 0.3)
				{
					break; // (still inside when her ammunition goes: the rest of her crew are lost with her)
				}
				if (W.Blast >= 0.0 && M.Out + OnGroundS > W.Blast)
				{
					M.How = End::Caught;
					M.FallAt = W.Blast;
				}
				else if (W.bBurns && Burning == 0 && K == 0 && D.P(0.15))
				{
					// (the first out of a burning tank, on fire: he runs a few steps, falls and burns on where he fell)
					M.How = End::Burning;
					M.Go = "run";
					M.Speed = D.U(3.4, 4.4);
					M.FallAt = M.Out + OnGroundS + D.U(2.0, 5.0);
					M.FireOut = M.FallAt + D.U(12.0, 25.0);
					M.Fall = 0;
					++Burning;
				}
				else if (W.bUnderFire && M.To != "beside")
				{
					const double Shot = M.Go == "crawl" ? 0.07 : 0.16;
					const double Hurt = M.Go == "crawl" ? 0.08 : 0.12;
					const double Roll = D.U();
					if (Roll < Shot + Hurt)
					{
						M.How = Roll < Shot ? End::Shot : End::Wounded;
						M.FallAt = M.Out + OnGroundS + (M.Go == "crawl" ? ProneS : 0.0) + Run * D.U(0.2, 0.85);
						M.Crawl = M.How == End::Wounded ? D.U(1.5, 6.0) : 0.0;
					}
				}
				Out.Men.push_back(M);
			}
		}
		// (a crew that bailed out and climbs back in: the men crouched in her lee, a little apart)
		if (Back >= 0.0)
		{
			int Nth = 0;
			for (Man& M : Out.Men)
			{
				if (M.How == End::Safe && M.To == "beside")
				{
					M.BackAt = std::max(M.Out + OnGroundS + 2.0, Back + 0.7 * Nth++);
				}
			}
		}
		int Safe = 0, Shot = 0, Hurt = 0, Fire = 0, Caught = 0, In = 0;
		for (const Man& M : Out.Men)
		{
			Safe += M.How == End::Safe;
			Shot += M.How == End::Shot;
			Hurt += M.How == End::Wounded;
			Fire += M.How == End::Burning;
			Caught += M.How == End::Caught;
			In += M.BackAt >= 0.0;
		}
		Out.Story = std::to_string(Out.Men.size()) + " out";
		if (!Out.Men.empty())
		{
			Out.Story += ": " + std::to_string(Safe) + " clear";
			if (Shot) Out.Story += ", " + std::to_string(Shot) + " cut down";
			if (Hurt) Out.Story += ", " + std::to_string(Hurt) + " wounded";
			if (Fire) Out.Story += ", " + std::to_string(Fire) + " on fire";
			if (Caught) Out.Story += ", " + std::to_string(Caught) + " caught by the blast";
			if (In) Out.Story += ", " + std::to_string(In) + " back in";
		}
		return Out;
	}

	Pose PoseAt(const Man& M, double Since, const std::function<double(double, double)>& Ground)
	{
		Pose P;
		const double T = Since - M.Out;
		if (!(T >= 0.0))
		{
			return P;
		}
		auto G = [&](double X, double Y) { return Ground ? Ground(X, Y) : M.Down.Z; };
		const double Out = YawOf(M.Down.X - M.Hatch.X, M.Down.Y - M.Hatch.Y, 0.0);
		const double Dx = M.Goal.X - M.Down.X, Dy = M.Goal.Y - M.Down.Y;
		const double Long = std::hypot(Dx, Dy);
		const double Go = YawOf(Dx, Dy, Out);
		const V3 Dir{ Long > 1e-6 ? Dx / Long : 0.0, Long > 1e-6 ? Dy / Long : 0.0, 0.0 };
		const double Lead = M.Go == "crawl" ? ProneS : 0.0;
		// (where he is on his way at a time, on the ground: a burning man's run weaves)
		auto OnWay = [&](double Tg) -> V3
		{
			const double D = std::clamp(M.Speed * (Tg - Lead), 0.0, Long);
			V3 At{ M.Down.X + Dir.X * D, M.Down.Y + Dir.Y * D, 0.0 };
			if (M.How == End::Burning)
			{
				const double Weave = std::sin(2.0 * Pi * 0.55 * std::max(0.0, Tg)) * std::min(1.0, D * 0.4);
				At.X += Dir.Y * Weave;
				At.Y -= Dir.X * Weave;
			}
			At.Z = G(At.X, At.Y);
			return At;
		};
		// (on his way out: up out of the hatch, along her deck, down to the ground)
		auto Climbing = [&](double Tc, Pose& Q)
		{
			Q.bShown = true;
			Q.Facing = Out;
			if (Tc < ClimbS)
			{
				const double E = Ease(Tc / ClimbS);
				Q.At = V3{ M.Hatch.X, M.Hatch.Y, M.Hatch.Z - 1.5 * (1.0 - E) };
				Q.Clip = "stand_up";
				Q.Time = Tc;
			}
			else if (Tc < ClimbS + StepS)
			{
				Q.At = Lerp(M.Hatch, M.Deck, Ease((Tc - ClimbS) / StepS));
				Q.Clip = "run";
				Q.Time = Tc - ClimbS;
				Q.bMoving = true;
			}
			else
			{
				const double U = std::clamp((Tc - ClimbS - StepS) / JumpS, 0.0, 1.0);
				Q.At = Lerp(M.Deck, M.Down, U);
				Q.At.Z += 0.35 * std::sin(Pi * U);
				Q.Clip = "run";
				Q.Time = Tc - ClimbS;
				Q.bMoving = true;
			}
		};
		P.bOnFire = M.How == End::Burning && Since < M.FireOut;
		// Thrown down by her ammunition going, wherever he was on his way out.
		if (M.How == End::Caught && Since >= M.FallAt)
		{
			Pose Was;
			Climbing(std::max(0.0, M.FallAt - M.Out), Was);
			const double U = std::clamp((Since - M.FallAt) / 0.6, 0.0, 1.0);
			const V3 Land{ M.Down.X + (M.Down.X - M.Hatch.X) * 0.5, M.Down.Y + (M.Down.Y - M.Hatch.Y) * 0.5, 0.0 };
			V3 At = Lerp(Was.At, V3{ Land.X, Land.Y, G(Land.X, Land.Y) }, Ease(U));
			At.Z += 1.0 * std::sin(Pi * U);
			P.bShown = true;
			P.At = At;
			P.Facing = Out + Pi;
			P.Clip = "die_back";
			P.Time = Since - M.FallAt;
			return P;
		}
		if (T < OnGroundS)
		{
			Climbing(T, P);
			return P;
		}
		const double Tg = T - OnGroundS;
		P.bShown = true;
		P.Facing = Go;
		// Hit on his way.
		if (M.FallAt >= 0.0 && Since >= M.FallAt && M.How != End::Caught)
		{
			const V3 Fell = OnWay(M.FallAt - M.Out - OnGroundS);
			const double Tf = Since - M.FallAt;
			if (M.How == End::Wounded && Tf >= ProneS)
			{
				// (he crawls on a few metres, then lies writhing where he stopped)
				const double D = std::min(M.Crawl, 0.4 * (Tf - ProneS));
				P.At = V3{ Fell.X + Dir.X * D, Fell.Y + Dir.Y * D, 0.0 };
				P.At.Z = G(P.At.X, P.At.Y);
				P.Clip = "crawl";
				P.Time = Tf - ProneS;
				P.bMoving = D < M.Crawl;
				return P;
			}
			P.At = Fell;
			P.Clip = M.How == End::Wounded ? "go_prone" : M.Fall == 0 ? "die_forward" : "die_back";
			P.Time = Tf;
			return P;
		}
		// Going back to her and climbing in again (a crew that bailed out and thought better of it).
		if (M.BackAt >= 0.0 && Since >= M.BackAt)
		{
			const double Tb = Since - M.BackAt;
			const double BackSpeed = 2.6;
			const double Way = std::hypot(M.Goal.X - M.Down.X, M.Goal.Y - M.Down.Y);
			if (Tb * BackSpeed < Way)
			{
				const double U = Way > 1e-6 ? Tb * BackSpeed / Way : 1.0;
				P.At = Lerp(M.Goal, M.Down, U);
				P.At.Z = G(P.At.X, P.At.Y);
				P.Facing = Go + Pi;
				P.Clip = "run";
				P.Time = Tb;
				P.bMoving = true;
				return P;
			}
			const double Tc = Tb - Way / BackSpeed;
			if (Tc >= OnGroundS)
			{
				P.bShown = false; // (in again)
				return P;
			}
			// (the way out played backwards: up onto her deck, along it, down into the hatch)
			Climbing(OnGroundS - Tc, P);
			P.Facing = Out + Pi;
			return P;
		}
		// On his way, or there.
		const double Arrive = Lead + Long / std::max(0.1, M.Speed);
		if (Tg < Lead)
		{
			P.At = M.Down;
			P.Clip = "go_prone";
			P.Time = Tg;
			return P;
		}
		if (Tg < Arrive)
		{
			P.At = OnWay(Tg);
			P.Clip = M.Go == "crawl" ? "crawl" : M.Go == "crouch" ? "advance" : M.How == End::Burning ? "flee" : "run";
			P.Time = Tg - Lead;
			P.bMoving = true;
			if (M.How == End::Burning)
			{
				const V3 Next = OnWay(Tg + 0.1);
				P.Facing = YawOf(Next.X - P.At.X, Next.Y - P.At.Y, Go);
			}
			return P;
		}
		P.At = OnWay(Arrive);
		if (M.To == "beside")
		{
			P.Clip = "cower";
			P.Time = Tg - Arrive;
			P.Facing = Go + Pi; // (crouched with his back to her, watching)
		}
		else if (M.Go == "crawl")
		{
			P.Clip = "crawl";
			P.Time = 0.4; // (stopped flat, his crawl held)
		}
		else
		{
			P.Clip = "go_prone";
			P.Time = Tg - Arrive;
		}
		return P;
	}
}
