// The field tags' placing in the cloud (Battle/LandTagPlace.h; the UI director's ruling of 27 November 2026,
// field-tags-crowding): no tag ever covers another, the order of taking places, the steps, the short mark, the hold
// and the easing. Usage: land_tags_test (no arguments). Prints each check and ends "ALL CHECKS PASSED" with exit 0.
#include "Battle/LandTagPlace.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	int Failed = 0;
	int Checked = 0;

	void Check(bool bGood, const std::string& What)
	{
		++Checked;
		if (!bGood)
		{
			++Failed;
			std::printf("FAIL: %s\n", What.c_str());
		}
	}

	const LandTagPlace::Box Field{ 0.0, 60.0, 1440.0, 700.0 };

	LandTagPlace::Want Tag(int Block, int Rank, double Far, double X, double Y, double W = 120.0, double H = 22.0)
	{
		LandTagPlace::Want T;
		T.Block = Block;
		T.Rank = Rank;
		T.Far = Far;
		T.Full = LandTagPlace::Box{ X, Y, W, H };
		T.ShortW = 21.0;
		T.ShortH = 19.0;
		T.StalkX = X + W * 0.5;
		return T;
	}

	const LandTagPlace::Placed& Of(const std::vector<LandTagPlace::Placed>& Out, int Block)
	{
		for (const LandTagPlace::Placed& P : Out)
		{
			if (P.Block == Block)
			{
				return P;
			}
		}
		static const LandTagPlace::Placed None;
		return None;
	}

	/** How many pairs of drawn boxes cover each other, among the tags that found room. */
	int Covering(const std::vector<LandTagPlace::Placed>& Out)
	{
		int N = 0;
		for (std::size_t I = 0; I < Out.size(); ++I)
		{
			for (std::size_t J = I + 1; J < Out.size(); ++J)
			{
				if (Out[I].bRoom && Out[J].bRoom && LandTagPlace::Covers(Out[I].Drawn, Out[J].Drawn))
				{
					++N;
				}
			}
		}
		return N;
	}

	bool Same(const LandTagPlace::Box& A, const LandTagPlace::Box& B)
	{
		return std::fabs(A.X - B.X) < 1.0e-9 && std::fabs(A.Y - B.Y) < 1.0e-9 && std::fabs(A.W - B.W) < 1.0e-9 && std::fabs(A.H - B.H) < 1.0e-9;
	}

	/** A small generator of its own, so the soak is the same on every machine. */
	struct Dice
	{
		std::uint64_t S;
		double Next()
		{
			S = S * 6364136223846793005ULL + 1442695040888963407ULL;
			return static_cast<double>((S >> 11) & 0xFFFFFFFFFFFFFULL) / 4503599627370496.0;
		}
	};
}

int main()
{
	using namespace LandTagPlace;
	const Rules R;

	// 1. Where no two tags touch, every tag hangs at its own place, full: the screen as it was.
	{
		Placer P;
		std::vector<Want> W;
		for (int K = 0; K < 12; ++K)
		{
			W.push_back(Tag(K, K % 3, 500.0 - K, 40.0 + 130.0 * (K % 6), 200.0 + 60.0 * (K / 6)));
		}
		bool bAll = true;
		for (int Frame = 0; Frame < 30; ++Frame)
		{
			const std::vector<Placed> Out = P.Place(W, Frame / 60.0, Field, R);
			bAll = bAll && Out.size() == W.size();
			for (const Placed& Put : Out)
			{
				bAll = bAll && !Put.bShort && Put.Step == 0 && Put.Rise == 0.0 && Put.bRoom && Same(Put.Drawn, W[Put.Index].Full);
			}
		}
		Check(bAll, "tags that do not touch all hang at their own place, full, for 30 frames");
	}

	// 2. Two that would cover each other: the nearer keeps its place, the farther rises its own height and 2 px.
	{
		Placer P;
		const std::vector<Want> W = { Tag(1, 1, 300.0, 400.0, 300.0), Tag(2, 1, 200.0, 440.0, 305.0) };
		const std::vector<Placed> Out = P.Place(W, 0.0, Field, R);
		Check(Out.size() == 2 && Out[0].Block == 2, "the nearer of two takes its place first");
		Check(Of(Out, 2).Step == 0 && Same(Of(Out, 2).Drawn, W[1].Full), "the nearer of two keeps its own place");
		Check(Of(Out, 1).Step == 1 && !Of(Out, 1).bShort && std::fabs(Of(Out, 1).Drawn.Y - (300.0 - 24.0)) < 1.0e-9, "the farther rises one step: its own height and 2 px");
		Check(Covering(Out) == 0, "two tags: neither covers the other");
	}

	// 3. The order: the block in hand and its group, then the player's own, then the enemy's; distance only within one.
	{
		Placer P;
		const std::vector<Want> W = { Tag(1, 2, 50.0, 400.0, 300.0), Tag(2, 1, 400.0, 410.0, 302.0), Tag(3, 0, 900.0, 420.0, 304.0) };
		const std::vector<Placed> Out = P.Place(W, 0.0, Field, R);
		Check(Out.size() == 3 && Out[0].Block == 3 && Out[1].Block == 2 && Out[2].Block == 1, "in hand first, then his own, then the enemy's, whatever their distance");
		Check(Of(Out, 3).Step == 0 && Of(Out, 2).Step == 1 && Of(Out, 1).Step == 2, "the three stand at steps 0, 1 and 2 in that order");
		Check(Covering(Out) == 0, "three tags: none covers another");
	}

	// 4. Past two steps the farther tag folds to its short mark; nothing is hidden.
	{
		Placer P;
		std::vector<Want> W;
		for (int K = 0; K < 4; ++K)
		{
			W.push_back(Tag(K, 1, 100.0 + K, 400.0 + 4.0 * K, 300.0));
		}
		const std::vector<Placed> Out = P.Place(W, 0.0, Field, R);
		Check(Out.size() == 4, "four tags in one place: all four are drawn");
		Check(!Of(Out, 0).bShort && !Of(Out, 1).bShort && !Of(Out, 2).bShort && Of(Out, 3).bShort, "the fourth, the farthest, folds to its short mark");
		Check(std::fabs(Of(Out, 3).Drawn.W - 21.0) < 1.0e-9 && std::fabs(Of(Out, 3).Drawn.H - 19.0) < 1.0e-9, "the short mark is the short mark's size");
		Check(std::fabs(Of(Out, 3).Drawn.X + 10.5 - W[3].StalkX) < 1.0e-9, "the short mark is centred on its block");
		Check(Covering(Out) == 0, "four tags: none covers another");
	}

	// 5. A crowd in one row (the Mac's still): all are drawn, and none with room covers another.
	{
		Placer P;
		std::vector<Want> W;
		for (int K = 0; K < 40; ++K)
		{
			W.push_back(Tag(K, 1 + K % 2, 900.0 + 3.0 * K, 300.0 + 17.0 * K, 240.0 + (K % 3)));
		}
		const std::vector<Placed> Out = P.Place(W, 0.0, Field, R);
		int Room = 0, Short = 0;
		for (const Placed& Put : Out)
		{
			Room += Put.bRoom ? 1 : 0;
			Short += Put.bShort ? 1 : 0;
		}
		Check(Out.size() == 40, "forty tags in one row: all forty are drawn");
		Check(Covering(Out) == 0, "forty tags in one row: none with room covers another");
		Check(Room == 40, "forty tags 17 px apart: every one finds room (full, stepped or short)");
		Check(Short > 0 && Short < 40, "forty tags in one row: some stay full, some fold");
		std::printf("  (forty in one row: %d full, %d short)\n", 40 - Short, Short);
	}

	// 5b. The same row far tighter, sixty tags 8 px apart: the short mark's nine places give every one room.
	{
		Placer P;
		std::vector<Want> W;
		for (int K = 0; K < 60; ++K)
		{
			W.push_back(Tag(K, 1 + K % 2, 900.0 + 3.0 * K, 200.0 + 8.0 * K, 240.0 + (K % 3)));
		}
		const std::vector<Placed> Out = P.Place(W, 0.0, Field, R);
		int Room = 0, Short = 0, Highest = 0;
		for (const Placed& Put : Out)
		{
			Room += Put.bRoom ? 1 : 0;
			Short += Put.bShort ? 1 : 0;
			Highest = Put.bShort && Put.Step > Highest ? Put.Step : Highest;
		}
		Check(Out.size() == 60 && Covering(Out) == 0, "sixty tags 8 px apart: all drawn, none with room covers another");
		Check(Room == 60, "sixty tags 8 px apart: every one finds room");
		Check(Highest > 4, "sixty tags 8 px apart: the short mark's farther places are used");
		std::printf("  (sixty 8 px apart: %d full, %d short, the farthest short place %d)\n", 60 - Short, Short, Highest);
	}

	// 6. No room at all (sixty on one point): each is still drawn, as its short mark, and says it has no room.
	{
		Placer P;
		std::vector<Want> W;
		for (int K = 0; K < 60; ++K)
		{
			W.push_back(Tag(K, 1, 100.0 + K, 500.0, 300.0));
		}
		const std::vector<Placed> Out = P.Place(W, 0.0, Field, R);
		int NoRoom = 0;
		bool bShortAll = true;
		for (const Placed& Put : Out)
		{
			NoRoom += Put.bRoom ? 0 : 1;
			bShortAll = bShortAll && (Put.bRoom || Put.bShort);
		}
		Check(Out.size() == 60 && NoRoom > 0 && bShortAll, "sixty on one point: all drawn, those without room as short marks");
		Check(Covering(Out) == 0, "sixty on one point: none with room covers another with room");
	}

	// 7. Pressed against the top of the field, a tag steps down, and stays within the field.
	{
		Placer P;
		const std::vector<Want> W = { Tag(1, 1, 100.0, 400.0, 62.0), Tag(2, 1, 200.0, 420.0, 62.0), Tag(3, 1, 300.0, 440.0, 62.0) };
		const std::vector<Placed> Out = P.Place(W, 0.0, Field, R);
		Check(Of(Out, 2).Step == 1 && Of(Out, 2).Drawn.Y > 62.0 && Of(Out, 3).Step == 2 && Of(Out, 3).Drawn.Y > Of(Out, 2).Drawn.Y, "at the top of the field the steps go down");
		Check(Covering(Out) == 0 && Within(Of(Out, 3).Drawn, Field), "at the top: none covers another and all stay in the field");
	}

	// 8. The hold and the easing: the farther tag stays up 0.3 s after its place clears, then comes down over 0.15 s.
	{
		Placer P;
		const Want A = Tag(1, 1, 100.0, 400.0, 300.0), B = Tag(2, 1, 200.0, 440.0, 300.0);
		const Want Away = Tag(1, 1, 100.0, 900.0, 300.0);
		std::vector<Placed> Out = P.Place({ A, B }, 0.0, Field, R);
		Check(Of(Out, 2).Step == 1, "(hold) the farther starts one step up");
		bool bHeld = true, bDown = true, bNoCover = true;
		double Last = Of(Out, 2).Rise;
		for (int Frame = 1; Frame <= 60; ++Frame)
		{
			const double Now = Frame / 100.0;
			Out = P.Place({ Away, B }, Now, Field, R);
			const Placed& Put = Of(Out, 2);
			bNoCover = bNoCover && Covering(Out) == 0;
			if (Now < 0.3 - 1.0e-9)
			{
				bHeld = bHeld && Put.Step == 1 && Put.Rise == -24.0;
			}
			bDown = bDown && Put.Rise >= Last - 1.0e-9;
			Last = Put.Rise;
		}
		Check(bHeld, "(hold) it keeps its step while its own place has been clear under 0.3 s");
		Check(bDown && Last == 0.0, "(ease) it then comes down without a step back, and is home by 0.45 s");
		Check(bNoCover, "(ease) nothing is covered on the way");
		Placer Q;
		Q.Place({ A, B }, 0.0, Field, R);
		Q.Place({ Away, B }, 0.01, Field, R);
		Check(Of(Q.Place({ Away, B }, 0.31, Field, R), 2).Rise == -24.0, "(ease) the step down begins in the frame the hold ends: it is still up in that frame");
		const double Mid = Of(Q.Place({ Away, B }, 0.31 + 0.075, Field, R), 2).Rise;
		Check(Mid < -1.0 && Mid > -23.0, "(ease) half way through the 0.15 s it is part of the way down, not at either end");
	}

	// 8b. Where another tag stands in the way of a step, the step is taken at once: nothing is eased through a tag.
	{
		Placer P;
		const Want B = Tag(2, 1, 300.0, 440.0, 300.0), Between = Tag(3, 1, 150.0, 500.0, 278.0, 120.0, 18.0);
		std::vector<Placed> Out = P.Place({ Tag(1, 1, 100.0, 400.0, 300.0), Between, B }, 0.0, Field, R);
		Check(Of(Out, 2).Step == 2 && Of(Out, 3).Step == 0 && Covering(Out) == 0, "(way barred) with one tag on its place and one on its first step, it stands two steps up");
		bool bNoCover = true, bAtOnce = true;
		for (int Frame = 1; Frame <= 60; ++Frame)
		{
			Out = P.Place({ Tag(1, 1, 100.0, 900.0, 300.0), Between, B }, Frame / 100.0, Field, R);
			bNoCover = bNoCover && Covering(Out) == 0;
			bAtOnce = bAtOnce && (Of(Out, 2).Rise == -48.0 || Of(Out, 2).Rise == 0.0);
		}
		Check(Of(Out, 2).Step == 0 && Of(Out, 2).Rise == 0.0, "(way barred) its own place cleared, it comes home past the tag between");
		Check(bAtOnce, "(way barred) and comes at once: it is never drawn part of the way, over the tag between");
		Check(bNoCover, "(way barred) nothing is covered in any frame");
	}

	// 9. A tag whose place is taken moves at once: nothing is covered in that very frame.
	{
		Placer P;
		const Want B = Tag(2, 1, 200.0, 440.0, 300.0);
		std::vector<Placed> Out = P.Place({ Tag(1, 1, 100.0, 900.0, 300.0), B }, 0.0, Field, R);
		Check(Of(Out, 2).Step == 0, "(at once) alone at first, it hangs at its own place");
		Out = P.Place({ Tag(1, 1, 100.0, 400.0, 300.0), B }, 0.016, Field, R);
		Check(Covering(Out) == 0 && Of(Out, 2).Step == 1, "(at once) the frame a nearer tag takes its place, it is clear of it");
	}

	// 10. Nothing hops: a nearer tag that comes and goes every 0.1 s leaves the farther one up, not hopping.
	{
		Placer P;
		const Want B = Tag(2, 1, 200.0, 440.0, 300.0);
		int Changes = 0, Was = -1;
		bool bNoCover = true;
		for (int Frame = 0; Frame < 300; ++Frame)
		{
			const bool bNear = (Frame / 6) % 2 == 0;
			const std::vector<Placed> Out = P.Place({ Tag(1, 1, 100.0, bNear ? 400.0 : 900.0, 300.0), B }, Frame / 60.0, Field, R);
			bNoCover = bNoCover && Covering(Out) == 0;
			const int Step = Of(Out, 2).Step;
			if (Was >= 0 && Step != Was)
			{
				++Changes;
			}
			Was = Step;
		}
		Check(Changes == 0 && Was == 1, "(no hopping) five seconds of a tag coming and going each 0.1 s: the farther never leaves its step");
		Check(bNoCover, "(no hopping) and nothing is covered meanwhile");
	}

	// 11. A tag gone longer than the forgetting time is placed afresh when it comes back.
	{
		Placer P;
		const Want A = Tag(1, 1, 100.0, 400.0, 300.0), B = Tag(2, 1, 200.0, 440.0, 300.0);
		P.Place({ A, B }, 0.0, Field, R);
		P.Place({ A }, 0.1, Field, R);
		const std::vector<Placed> Out = P.Place({ Tag(1, 1, 100.0, 900.0, 300.0), B }, 1.0, Field, R);
		Check(Of(Out, 2).Step == 0 && Of(Out, 2).Rise == 0.0, "a tag back after 0.9 s away hangs at its own place at once");
	}

	// 12. The soak: forty tags drifting for twenty seconds. All drawn, none with room covering another, the same twice.
	{
		std::vector<double> First;
		int MostChanges = 0;
		for (int Run = 0; Run < 2; ++Run)
		{
			Placer P;
			Dice D{ 20261127ULL };
			std::vector<Want> W;
			std::vector<double> Vx, Vy;
			for (int K = 0; K < 40; ++K)
			{
				// One draw to a line: the order of draws inside one call is the compiler's to choose.
				const double Far = 100.0 + 900.0 * D.Next();
				const double X = 100.0 + 1100.0 * D.Next();
				const double Y = 150.0 + 300.0 * D.Next();
				const double Wd = 90.0 + 60.0 * D.Next();
				W.push_back(Tag(K, K < 4 ? 0 : (K < 22 ? 1 : 2), Far, X, Y, Wd));
				const double DriftX = 40.0 * (D.Next() - 0.5);
				Vx.push_back(DriftX);
				const double DriftY = 14.0 * (D.Next() - 0.5);
				Vy.push_back(DriftY);
			}
			int Bad = 0, Missing = 0, Changes = 0;
			std::vector<int> Step(40, -1);
			std::vector<double> All;
			for (int Frame = 0; Frame < 1200; ++Frame)
			{
				for (int K = 0; K < 40; ++K)
				{
					W[K].Full.X += Vx[K] / 60.0;
					W[K].Full.Y += Vy[K] / 60.0;
					W[K].StalkX = W[K].Full.X + W[K].Full.W * 0.5;
					W[K].Far += Vy[K] / 30.0;
				}
				const std::vector<Placed> Out = P.Place(W, Frame / 60.0, Field, R);
				Missing += Out.size() == 40 ? 0 : 1;
				Bad += Covering(Out);
				for (const Placed& Put : Out)
				{
					const int Now = Put.bShort ? 10 + Put.Step : Put.Step;
					if (Step[Put.Block] >= 0 && Step[Put.Block] != Now)
					{
						++Changes;
					}
					Step[Put.Block] = Now;
					All.push_back(Put.Drawn.Y);
				}
			}
			if (Run == 0)
			{
				First = All;
				Check(Missing == 0, "(soak) forty drifting tags, 1,200 frames: all forty drawn in every frame");
				Check(Bad == 0, "(soak) no tag with room covers another in any frame");
				MostChanges = Changes;
				std::printf("  (soak: %d changes of place among 40 tags in 20 s)\n", Changes);
				Check(Changes < 40 * 20, "(soak) fewer than one change of place a tag a second");
			}
			else
			{
				Check(All == First && Changes == MostChanges, "(soak) the same tags give the same places twice");
			}
		}
	}

	std::printf("%d checks, %d failed\n", Checked, Failed);
	if (Failed == 0)
	{
		std::printf("ALL CHECKS PASSED\n");
	}
	return Failed == 0 ? 0 : 1;
}
