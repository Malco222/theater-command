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
		// The soldiers' bail clips (labtrr 42bb946f, each file's bail_clips): bail_climb_out, 1.5 s, his boots 0.92 m
		// under the hatch's point, up 0.95 m onto her roof, 0.47 m to his right and 0.10 m forward; along her deck at a
		// run; bail_jump_down from her hull's edge facing out, off at 0.40 s, down 1.5 m and out 1.70 m at 1.05 s, into
		// his first stride, 1.70 m out at its end, where the run takes him on; burning_run, in place, 7.35 m a 2.1 s loop; burning_fall_roll,
		// 3.8 s, still from 3.3 s, his pelvis 0.95 m forward and 1.20 m to his right at the end.
		constexpr double ClipClimbS = 1.5, ClipJumpS = 1.53, ClipOnGroundS = ClipClimbS + StepS + ClipJumpS;
		constexpr double BootsUnder = 0.92, ClimbUp = 0.95, ClimbRight = 0.47, ClimbFwd = 0.10;
		constexpr double JumpOff = 0.40, JumpDown = 1.05, JumpDrop = 1.5, JumpOut = 1.70;
		constexpr double BurnStride = 7.35, BurnLoop = 2.1, RollStill = 3.3, RollFwd = 0.95, RollRight = 1.20;

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

		/** His way on the ground: down from her to the end of his first leg in her lee, then on to his goal. */
		struct Legs
		{
			V3 A, B, C;             // Down, Lee, Goal
			double L1 = 0.0, L2 = 0.0;
			V3 D1, D2;              // each leg's way (unit; along the other where one is no length)
			explicit Legs(const Man& M) : A(M.Down), B(M.Lee), C(M.Goal)
			{
				L1 = std::hypot(B.X - A.X, B.Y - A.Y);
				L2 = std::hypot(C.X - B.X, C.Y - B.Y);
				D2 = L2 > 1e-6 ? V3{ (C.X - B.X) / L2, (C.Y - B.Y) / L2, 0.0 } : V3{};
				D1 = L1 > 1e-6 ? V3{ (B.X - A.X) / L1, (B.Y - A.Y) / L1, 0.0 } : D2;
				if (L2 <= 1e-6)
				{
					D2 = D1;
				}
			}
			double Long() const { return L1 + L2; }
			/** Where he is D metres along it (its height the caller's), and his way there. */
			V3 At(double D, V3& Way) const
			{
				D = std::clamp(D, 0.0, L1 + L2);
				if (D <= L1)
				{
					Way = D1;
					return V3{ A.X + D1.X * D, A.Y + D1.Y * D, 0.0 };
				}
				Way = D2;
				return V3{ B.X + D2.X * (D - L1), B.Y + D2.Y * (D - L1), 0.0 };
			}
		};

		/** His time on his way, life's seconds since he reached the ground, his flinch at her blast taken out. */
		double WayTime(const Man& M, double Since)
		{
			double T = Since - M.Out - OnGroundOf(M);
			if (M.FlinchAt >= 0.0 && Since > M.FlinchAt)
			{
				T -= std::min(Since - M.FlinchAt, M.FlinchS);
			}
			return T;
		}
	}

	double OnGroundOf(const Man& M)
	{
		return M.bClips ? ClipOnGroundS : OnGroundS;
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
				// (with the bail clips: where bail_jump_down lands him, 1.70 m out from her edge; and he climbs out facing
				// her way or back from it, so that his right, the clip's way, is the side he drops from)
				M.bClips = W.bClips;
				M.Face = M.Side > 0 ? W.Heading : W.Heading + Pi;
				const double Off = M.bClips ? Half + 0.05 + JumpOut : Half + 1.1;
				M.Down = V3{ W.Middle.X + Fwd.X * Along + Right.X * M.Side * Off, W.Middle.Y + Fwd.Y * Along + Right.Y * M.Side * Off, 0.0 };
				M.Down.Z = Ground(M.Down.X, M.Down.Y);
				M.Lee = M.Down;
				if (M.To == "beside")
				{
					// (in her lee: a pace or two out from her side, toward her back)
					M.Goal = V3{ M.Down.X + Right.X * M.Side * D.U(0.8, 1.8) - Fwd.X * D.U(0.5, 2.0), M.Down.Y + Right.Y * M.Side * D.U(0.8, 1.8) - Fwd.Y * D.U(0.5, 2.0), 0.0 };
				}
				else
				{
					// First straight back in her lee, her hull between him and the enemy (8 to 12 m running, 3 to 5
					// crawling), kept within the shadow her hull casts from the enemy's side; then he breaks for his
					// cover (the art director, 23 November).
					// (past her rear as the enemy sees her, then within her hull's shadow, as far in toward it as the
					// straight leg keeps clear of her hull)
					const V3 Ld{ std::sin(W.Away), std::cos(W.Away), 0.0 };
					const V3 Across{ Ld.Y, -Ld.X, 0.0 };
					const double Rel = W.Heading - W.Away;
					const double HullL = std::max(3.0, W.Long), HullW = std::max(1.6, W.Wide);
					const double ShadowHalf = 0.5 * (HullW * std::abs(std::cos(Rel)) + HullL * std::abs(std::sin(Rel)));
					const double Rear = 0.5 * (HullL * std::abs(std::cos(Rel)) + HullW * std::abs(std::sin(Rel)));
					const double Leg = M.Go == "crawl" ? D.U(3.0, 5.0) : D.U(8.0, 12.0);
					const double DownBack = (M.Down.X - W.Middle.X) * Ld.X + (M.Down.Y - W.Middle.Y) * Ld.Y;
					const double DownAside = (M.Down.X - W.Middle.X) * Across.X + (M.Down.Y - W.Middle.Y) * Across.Y;
					// (each his own spot in her shadow: two men out of one hatch never make for one point)
					const double PastRear = M.Go == "crawl" ? D.U(0.8, 2.2) : D.U(1.5, 3.5);
					const double Spread = D.U(-1.0, 1.0);
					const double Back_ = std::max(DownBack + Leg, Rear + PastRear);
					const double Keep = std::max(0.6, 0.75 * ShadowHalf);
					const double Want = std::clamp(DownAside + Spread, -Keep, Keep);
					const auto LeeAt = [&](double Aside) { return V3{ W.Middle.X + Ld.X * Back_ + Across.X * Aside, W.Middle.Y + Ld.Y * Back_ + Across.Y * Aside, 0.0 }; };
					const auto Clear = [&](const V3& To)
					{
						for (int Q = 0; Q <= 24; ++Q)
						{
							const double U = Q / 24.0;
							const double Px = M.Down.X + (To.X - M.Down.X) * U - W.Middle.X, Py = M.Down.Y + (To.Y - M.Down.Y) * U - W.Middle.Y;
							if (std::abs(Px * Fwd.X + Py * Fwd.Y) < 0.5 * HullL + 0.3 && std::abs(Px * Right.X + Py * Right.Y) < 0.5 * HullW + 0.3)
							{
								return false;
							}
						}
						return true;
					};
					double Aside = DownAside;
					for (const double Pull : { 1.0, 0.75, 0.5, 0.25 })
					{
						if (Clear(LeeAt(DownAside + (Want - DownAside) * Pull)))
						{
							Aside = DownAside + (Want - DownAside) * Pull;
							break;
						}
					}
					M.Lee = LeeAt(Aside);
					M.Lee.Z = Ground(M.Lee.X, M.Lee.Y);
					M.Goal = V3{ M.Lee.X + Dir.X * Far, M.Lee.Y + Dir.Y * Far, 0.0 };
				}
				M.Goal.Z = Ground(M.Goal.X, M.Goal.Y);
				M.Speed = M.Go == "crawl" ? D.U(0.45, 0.7) : M.Go == "crouch" ? D.U(2.2, 3.0) : D.U(4.2, 5.4);
				M.Fall = D.P(0.6) ? 0 : 1;
				const double Run = Legs(M).Long() / M.Speed + (M.Go == "crawl" ? ProneS : 0.0);
				// How it goes for him.
				if (W.Blast >= 0.0 && M.Out >= W.Blast - 0.3)
				{
					break; // (still inside when her ammunition goes: the rest of her crew are lost with her)
				}
				// (the bail clips take 1.48 s longer to the ground: he starts up out of her that much sooner, no sooner
				// than 0.3 s after her kill, so each man is on the ground when he was without them, and goes as he did)
				if (M.bClips)
				{
					M.Out = std::max(0.3, M.Out - (ClipOnGroundS - OnGroundS));
				}
				if (W.Blast >= 0.0 && M.Out + OnGroundOf(M) > W.Blast)
				{
					M.How = End::Caught;
					M.FallAt = W.Blast;
					M.Thrown = D.U(2.0, 5.0); // (the art director: a man on her deck thrown 2 to 5 m)
				}
				else if (W.bBurns && Burning == 0 && K == 0 && D.P(0.12))
				{
					// (the first out of a burning tank, on fire, at most about one in eight: he runs a few steps and
					// falls; his flames die to a smoulder over 10 to 15 s, or, one in three, he rolls them out in a few
					// seconds and lives, lying wounded and smoking)
					M.How = End::Burning;
					M.Go = "run";
					M.Speed = D.U(3.4, 4.4);
					M.FallAt = M.Out + OnGroundOf(M) + D.U(2.0, 5.0);
					M.bLives = D.P(1.0 / 3.0);
					M.FireOut = M.FallAt + (M.bLives ? D.U(2.5, 4.0) : D.U(10.0, 15.0));
					M.Crawl = M.bLives ? D.U(0.4, 1.0) : 0.0;
					M.Fall = 0;
					++Burning;
				}
				else if (W.bUnderFire && M.To != "beside")
				{
					// (by the enemy who can see him: 1 in 6 running with the enemy close, a crawler half that; a little
					// over half of those killed, the rest hit and left crawling)
					const double Hit = (M.Go == "crawl" ? 1.0 / 12.0 : 1.0 / 6.0) * std::clamp(W.Threat, 0.0, 1.0);
					const double Shot = 0.55 * Hit;
					const double Roll = D.U();
					if (Roll < Hit)
					{
						M.How = Roll < Shot ? End::Shot : End::Wounded;
						M.FallAt = M.Out + OnGroundOf(M) + (M.Go == "crawl" ? ProneS : 0.0) + Run * D.U(0.2, 0.85);
						M.Crawl = M.How == End::Wounded ? D.U(1.5, 6.0) : 0.0;
					}
				}
				Out.Men.push_back(M);
			}
		}
		// Her ammunition going: every man on the ground within 15 m of her flinches flat for a second or two, then
		// goes on (the art director, 23 November); a man hit later on his way is hit that much later.
		if (W.Blast >= 0.0)
		{
			for (Man& M : Out.Men)
			{
				if (M.How == End::Caught || M.How == End::Burning || M.Out + OnGroundOf(M) > W.Blast || (M.FallAt >= 0.0 && M.FallAt <= W.Blast))
				{
					continue;
				}
				V3 Way;
				const double Lead = M.Go == "crawl" ? ProneS : 0.0;
				const V3 At = Legs(M).At(M.Speed * (W.Blast - M.Out - OnGroundOf(M) - Lead), Way);
				if (std::hypot(At.X - W.Middle.X, At.Y - W.Middle.Y) <= 15.0)
				{
					M.FlinchAt = W.Blast;
					M.FlinchS = D.U(1.2, 2.2);
					if (M.FallAt > W.Blast)
					{
						M.FallAt += M.FlinchS;
					}
				}
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
					M.BackAt = std::max(M.Out + OnGroundOf(M) + 2.0, Back + 0.7 * Nth++);
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

	static Pose PoseOn(const Man& M, double Since, const std::function<double(double, double)>& Ground)
	{
		Pose P;
		const double T = Since - M.Out;
		if (!(T >= 0.0))
		{
			return P;
		}
		auto G = [&](double X, double Y) { return Ground ? Ground(X, Y) : M.Down.Z; };
		const double Out = YawOf(M.Down.X - M.Hatch.X, M.Down.Y - M.Hatch.Y, 0.0);
		const Legs L(M);
		const double Long = L.Long();
		const double Go = YawOf(L.D2.X, L.D2.Y, Out); // (his last leg's way: where he faces once there)
		const double Lead = M.Go == "crawl" ? ProneS : 0.0;
		// (where he is on his way at a time on it, on the ground, and his way there: a burning man's run weaves)
		auto OnWay = [&](double Tg, V3& Way) -> V3
		{
			const double D = std::clamp(M.Speed * (Tg - Lead), 0.0, Long);
			V3 At = L.At(D, Way);
			if (M.How == End::Burning)
			{
				const double Weave = std::sin(2.0 * Pi * 0.55 * std::max(0.0, Tg)) * std::min(1.0, D * 0.4);
				At.X += Way.Y * Weave;
				At.Y -= Way.X * Weave;
			}
			At.Z = G(At.X, At.Y);
			return At;
		};
		// (on his way out: up out of the hatch, along her deck, down to the ground)
		auto Climbing = [&](double Tc, Pose& Q, bool bClips)
		{
			Q.bShown = true;
			Q.Facing = Out;
			if (bClips)
			{
				// The soldiers' clips, their root moving him: up out of the hatch (his boots under its point, facing so
				// his right is the side he drops from), at a run along her deck to its edge, and off it, landed by the
				// jump's own fall where she is higher or lower than its 1.5 m.
				const V3 FwdF{ std::sin(M.Face), std::cos(M.Face), 0.0 }, RightF{ std::cos(M.Face), -std::sin(M.Face), 0.0 };
				const V3 Up{ M.Hatch.X + RightF.X * ClimbRight + FwdF.X * ClimbFwd, M.Hatch.Y + RightF.Y * ClimbRight + FwdF.Y * ClimbFwd, M.Hatch.Z - BootsUnder + ClimbUp };
				if (Tc < ClipClimbS)
				{
					const double E = Ease(Tc / ClipClimbS);
					Q.At = V3{ M.Hatch.X, M.Hatch.Y, M.Hatch.Z - BootsUnder };
					Q.Facing = M.Face;
					Q.Clip = "bail_climb_out";
					Q.Time = Tc;
					Q.Root = Lerp(Q.At, Up, E);
				}
				else if (Tc < ClipClimbS + StepS)
				{
					Q.At = Lerp(Up, M.Deck, Ease((Tc - ClipClimbS) / StepS));
					Q.Facing = YawOf(M.Deck.X - Up.X, M.Deck.Y - Up.Y, Out);
					Q.Clip = "run";
					Q.Time = Tc - ClipClimbS;
					Q.bMoving = true;
					Q.Root = Q.At;
				}
				else
				{
					const double Tj = Tc - ClipClimbS - StepS;
					const double Drop = M.Deck.Z - M.Down.Z;
					const double Fit = std::clamp((Tj - JumpOff) / (JumpDown - JumpOff), 0.0, 1.0);
					Q.At = V3{ M.Deck.X, M.Deck.Y, M.Deck.Z + (JumpDrop - Drop) * Fit };
					Q.Facing = YawOf(M.Down.X - M.Deck.X, M.Down.Y - M.Deck.Y, Out);
					Q.Clip = "bail_jump_down";
					Q.Time = Tj;
					Q.bMoving = true;
					// (his body: out with the clip, 0.1 m by 0.40 s, 1.70 m at its end; down as it falls, 0.40 to 1.05 s)
					const double Ahead = std::clamp((Tj - 0.3) / (ClipJumpS - 0.3), 0.0, 1.0);
					Q.Root = V3{ M.Deck.X + (M.Down.X - M.Deck.X) * Ahead, M.Deck.Y + (M.Down.Y - M.Deck.Y) * Ahead, M.Deck.Z + (M.Down.Z - M.Deck.Z) * Ease(Fit) };
				}
				return;
			}
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
			Q.Root = Q.At;
		};
		P.bOnFire = M.How == End::Burning && Since < M.FireOut;
		// Thrown down by her ammunition going, wherever he was on his way out: 2 to 5 m out from her, over an arc.
		if (M.How == End::Caught && Since >= M.FallAt)
		{
			Pose Was;
			Climbing(std::max(0.0, M.FallAt - M.Out), Was, M.bClips);
			Was.At = Was.Root; // (from where his body was: the clips' root moves him)
			const double Dx = M.Down.X - M.Hatch.X, Dy = M.Down.Y - M.Hatch.Y;
			const double Dl = std::max(1e-6, std::hypot(Dx, Dy));
			const double Thrown = M.Thrown > 0.0 ? M.Thrown : 0.5 * Dl;
			const V3 Land{ Was.At.X + Dx / Dl * Thrown, Was.At.Y + Dy / Dl * Thrown, 0.0 };
			const double Flight = 0.5 + 0.08 * Thrown;
			const double U = std::clamp((Since - M.FallAt) / Flight, 0.0, 1.0);
			V3 At = Lerp(Was.At, V3{ Land.X, Land.Y, G(Land.X, Land.Y) }, Ease(U));
			At.Z += (0.8 + 0.15 * Thrown) * std::sin(Pi * U);
			P.bShown = true;
			P.At = At;
			P.Facing = Out + Pi;
			P.Clip = "die_back";
			P.Time = Since - M.FallAt;
			P.Root = P.At;
			return P;
		}
		if (T < OnGroundOf(M))
		{
			Climbing(T, P, M.bClips);
			return P;
		}
		const double Tg = WayTime(M, Since);
		P.bShown = true;
		P.Facing = Go;
		// Hit on his way (or a burning man falling).
		if (M.FallAt >= 0.0 && Since >= M.FallAt && M.How != End::Caught)
		{
			V3 Way;
			const V3 Fell = OnWay(WayTime(M, M.FallAt), Way);
			const double Tf = Since - M.FallAt;
			P.Facing = YawOf(Way.X, Way.Y, Go);
			if (M.bClips && M.How == End::Burning)
			{
				// (the soldiers' roll: down and over to his right, his flames rolled out or burning on; still from 3.3 s,
				// whether he lives or not, lying where it leaves him)
				const double E = Ease(Tf / RollStill);
				const double Fx = std::sin(P.Facing), Fy = std::cos(P.Facing);
				P.At = Fell;
				P.Clip = "burning_fall_roll";
				P.Time = Tf;
				P.Root = V3{ Fell.X + (Fx * RollFwd + Fy * RollRight) * E, Fell.Y + (Fy * RollFwd - Fx * RollRight) * E, 0.0 };
				P.Root.Z = G(P.Root.X, P.Root.Y);
				return P;
			}
			if ((M.How == End::Wounded || (M.How == End::Burning && M.bLives)) && Tf >= ProneS)
			{
				// (he crawls on a few metres, then lies writhing where he stopped; a man on fire who lives rolls a little
				// and lies there smoking)
				const double D = std::min(M.Crawl, 0.4 * (Tf - ProneS));
				P.At = V3{ Fell.X + Way.X * D, Fell.Y + Way.Y * D, 0.0 };
				P.At.Z = G(P.At.X, P.At.Y);
				P.Clip = "crawl";
				P.Time = Tf - ProneS;
				P.bMoving = D < M.Crawl;
				return P;
			}
			P.At = Fell;
			P.Clip = M.How == End::Wounded || (M.How == End::Burning && M.bLives) ? "go_prone" : M.Fall == 0 ? "die_forward" : "die_back";
			P.Time = Tf;
			return P;
		}
		// Flat on the ground as her ammunition goes, for a second or two.
		if (M.FlinchAt >= 0.0 && Since >= M.FlinchAt && Since < M.FlinchAt + M.FlinchS)
		{
			V3 Way;
			P.At = OnWay(WayTime(M, M.FlinchAt), Way);
			P.Facing = YawOf(Way.X, Way.Y, Go);
			P.Clip = "go_prone";
			P.Time = Since - M.FlinchAt;
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
			// (the way out played backwards: up onto her deck, along it, down into the hatch; the infantry's, the bail
			// clips being one way only)
			Climbing(OnGroundS - Tc, P, false);
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
			V3 Way;
			P.At = OnWay(Tg, Way);
			P.Facing = YawOf(Way.X, Way.Y, Go);
			P.Clip = M.Go == "crawl" ? "crawl" : M.Go == "crouch" ? "advance" : M.How == End::Burning ? (M.bClips ? "burning_run" : "flee") : "run";
			P.Time = Tg - Lead;
			if (M.bClips && M.How == End::Burning)
			{
				// (his stride the clip's: a loop for each 7.35 m he goes)
				P.Time = std::clamp(M.Speed * (Tg - Lead), 0.0, Long) / BurnStride * BurnLoop;
			}
			P.bMoving = true;
			if (M.How == End::Burning)
			{
				V3 Ahead;
				const V3 Next = OnWay(Tg + 0.1, Ahead);
				P.Facing = YawOf(Next.X - P.At.X, Next.Y - P.At.Y, P.Facing);
			}
			return P;
		}
		V3 Way;
		P.At = OnWay(Arrive, Way);
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

	Pose PoseAt(const Man& M, double Since, const std::function<double(double, double)>& Ground)
	{
		Pose P = PoseOn(M, Since, Ground);
		// (his body where the clip leaves it: At itself but for the clips whose root moves him)
		if (P.Clip != "bail_climb_out" && P.Clip != "bail_jump_down" && P.Clip != "burning_fall_roll")
		{
			P.Root = P.At;
		}
		return P;
	}
}
