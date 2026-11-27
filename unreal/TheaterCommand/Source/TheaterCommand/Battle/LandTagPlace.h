#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

/**
 * Where the field's unit tags hang so that none ever covers another, in plain C++ so the cloud test places the same
 * tags the game draws (the UI director's ruling of 27 November 2026, field-tags-crowding: on Brian's Mac at Prokhorovka
 * the far tags of the low free view fell into one row and printed over each other).
 *
 * Her ruling, as built here; it holds in every view and at every tilt, with no threshold:
 *  1. No tag ever covers another.
 *  2. Tags take their places in this order each frame: the block in hand and its group, then the player's own, then
 *     the enemy's; the nearest the eye first in each. The camera's numbers serve only this order: the placing is by
 *     each tag's box on the screen.
 *  3. A tag that would cover one already placed rises one step (its own height and 2 px) on a longer stalk, at most
 *     two steps. (A tag whose own place is pressed against the top of the field steps down instead.)
 *  4. Past that it folds to its short mark, the kind sign alone on its side's paper. The short mark looks for room at
 *     its own foot, then by turns above and below it: one and two steps up, one down, three up, two down, four up,
 *     three down, five up (my reading where she names no place for it: with these nine, sixty tags 8 px apart in one
 *     row all find room; with the first five, a third of them did not).
 *  5. Nothing is ever hidden: a tag with no room at all is drawn as its short mark where it is covered least.
 *  6. Nothing hops: a tag leaves its place at once only when that place is covered; it comes back to a better one
 *     only when a better one has been clear for 0.3 s. A step up or down is eased over 0.15 s where the way between
 *     is clear, and is taken at once where it is not; folding and unfolding are at once.
 *
 * Where no two tags touch, every tag hangs at its own place, full: the screen looks as it did before.
 */
namespace LandTagPlace
{
	/** A box on the screen: its left, its top, its width and its height, in the screen's own pixels. */
	struct Box
	{
		double X = 0.0, Y = 0.0, W = 0.0, H = 0.0;
	};

	/** What one tag asks for this frame. */
	struct Want
	{
		int Block = 0;
		/** 0 the block in hand and its group, 1 the player's own, 2 the enemy's. */
		int Rank = 0;
		/** How far the block is from the eye, in metres: the nearest first in each rank. */
		double Far = 0.0;
		/** Everything the tag draws, as it would hang were it alone on the field. */
		Box Full;
		/** The short mark's width and height. */
		double ShortW = 0.0, ShortH = 0.0;
		/** The block's own place across the screen: the short mark is centred on it. */
		double StalkX = 0.0;
	};

	/** Where one tag hangs this frame. */
	struct Placed
	{
		/** Its place in the Wants handed in. */
		std::size_t Index = 0;
		int Block = 0;
		/** Where it is drawn this frame: a full tag's box as it rises or falls, or its short mark's box. */
		Box Drawn;
		bool bShort = false;
		/** How far the drawn full tag stands from its own place, in pixels down the screen (a rise is negative). */
		double Rise = 0.0;
		/** Its step (0, 1, 2; a short mark's place 0 to 8), and whether it has room (false: drawn where covered least). */
		int Step = 0;
		bool bRoom = true;
	};

	struct Rules
	{
		/** Between a tag and the one a step above it. */
		double Gap = 2.0;
		/** How long a better place must have been clear before a tag comes back to it, in seconds. */
		double HoldFor = 0.3;
		/** How long a step up or down takes, in seconds. */
		double EaseFor = 0.15;
		/** A block not asked for this long is forgotten, and is placed afresh when it comes back, in seconds. */
		double Forget = 0.5;
	};

	inline bool Covers(const Box& A, const Box& B)
	{
		const double Eps = 1.0e-6;
		return A.X < B.X + B.W - Eps && B.X < A.X + A.W - Eps && A.Y < B.Y + B.H - Eps && B.Y < A.Y + A.H - Eps;
	}

	inline double Shared(const Box& A, const Box& B)
	{
		const double Wd = std::min(A.X + A.W, B.X + B.W) - std::max(A.X, B.X);
		const double Ht = std::min(A.Y + A.H, B.Y + B.H) - std::max(A.Y, B.Y);
		return Wd > 0.0 && Ht > 0.0 ? Wd * Ht : 0.0;
	}

	inline Box Both(const Box& A, const Box& B)
	{
		Box U;
		U.X = std::min(A.X, B.X);
		U.Y = std::min(A.Y, B.Y);
		U.W = std::max(A.X + A.W, B.X + B.W) - U.X;
		U.H = std::max(A.Y + A.H, B.Y + B.H) - U.Y;
		return U;
	}

	/** The places a tag may take, in the order it likes them: 0 to 2 full (its own place, one step, two), 3 to 11 short. */
	const int Places = 12;

	/** Whether a full tag steps down the screen and not up: where one step up would leave the field at its top. */
	inline bool StepsDown(const Want& T, const Box& Field, const Rules& R)
	{
		return T.Full.Y - (T.Full.H + R.Gap) < Field.Y;
	}

	/** The box of a tag at one of its places. */
	inline Box PlaceBox(const Want& T, int Place, const Box& Field, const Rules& R)
	{
		if (Place <= 2)
		{
			Box B = T.Full;
			B.Y += (StepsDown(T, Field, R) ? 1.0 : -1.0) * Place * (T.Full.H + R.Gap);
			return B;
		}
		// The short mark: at the tag's own foot, then by turns above and below it, the nearest first.
		static const int Up[Places - 3] = { 0, 1, 2, -1, 3, -2, 4, -3, 5 };
		Box B;
		B.W = T.ShortW;
		B.H = T.ShortH;
		B.X = T.StalkX - T.ShortW * 0.5;
		B.Y = T.Full.Y + T.Full.H - T.ShortH - Up[Place - 3] * (T.ShortH + R.Gap);
		return B;
	}

	/** Whether a box lies within the field from top to bottom (across, the screen has clamped the tag already). */
	inline bool Within(const Box& B, const Box& Field)
	{
		return B.Y >= Field.Y - 1.0e-6 && B.Y + B.H <= Field.Y + Field.H + 1.0e-6;
	}

	class Placer
	{
	public:
		/** Forgets every tag (the screen is hidden, another battle). */
		void Reset()
		{
			Was.clear();
		}

		/**
		 * Places every tag asked for. Now is the screen's own time in real seconds; Field is the box the tags live in.
		 * Returned in the order the places were taken (the first is the tag nobody may cover: draw it last).
		 */
		std::vector<Placed> Place(const std::vector<Want>& Wants, double Now, const Box& Field, const Rules& R = Rules())
		{
			std::vector<std::size_t> Order(Wants.size());
			for (std::size_t I = 0; I < Order.size(); ++I)
			{
				Order[I] = I;
			}
			std::sort(Order.begin(), Order.end(), [&Wants](std::size_t A, std::size_t B)
			{
				const Want& P = Wants[A];
				const Want& Q = Wants[B];
				if (P.Rank != Q.Rank)
				{
					return P.Rank < Q.Rank;
				}
				if (P.Far != Q.Far)
				{
					return P.Far < Q.Far;
				}
				if (P.Block != Q.Block)
				{
					return P.Block < Q.Block;
				}
				return A < B;
			});

			std::vector<Placed> Out;
			Out.reserve(Wants.size());
			// What is taken so far this frame: each placed tag's box, with the way it is still moving over.
			std::vector<Box> Taken;
			Taken.reserve(Wants.size());
			auto Free = [&Taken](const Box& B)
			{
				for (const Box& T : Taken)
				{
					if (Covers(B, T))
					{
						return false;
					}
				}
				return true;
			};

			for (const std::size_t I : Order)
			{
				const Want& T = Wants[I];
				auto Found = Was.find(T.Block);
				const bool bKnown = Found != Was.end() && Now - Found->second.Seen <= R.Forget && Now >= Found->second.Seen;
				// The place it likes best of those with room, and the one it holds.
				int Best = -1;
				for (int P = 0; P < Places; ++P)
				{
					const Box B = PlaceBox(T, P, Field, R);
					if ((P == 0 || P == 3 || Within(B, Field)) && Free(B))
					{
						Best = P;
						break;
					}
				}
				bool bRoom = true;
				if (Best < 0)
				{
					// No room anywhere: its short mark where it is covered least (the lowest place among equals).
					bRoom = false;
					double Least = 0.0;
					for (int P = 3; P < Places; ++P)
					{
						const Box B = PlaceBox(T, P, Field, R);
						if (P != 3 && !Within(B, Field))
						{
							continue;
						}
						double Sum = 0.0;
						for (const Box& Other : Taken)
						{
							Sum += Shared(B, Other);
						}
						if (Best < 0 || Sum < Least - 1.0e-9)
						{
							Best = P;
							Least = Sum;
						}
					}
				}
				Kept K;
				int Place = Best;
				if (bKnown)
				{
					K = Found->second;
					const Box Held = PlaceBox(T, K.Place, Field, R);
					const bool bHeldGood = (K.Place == 0 || K.Place == 3 || Within(Held, Field)) && Free(Held);
					if (bHeldGood && bRoom && Best < K.Place)
					{
						// A better place has room: it is taken only once one has had room for long enough.
						if (K.ClearSince < 0.0)
						{
							K.ClearSince = Now;
						}
						if (Now - K.ClearSince < R.HoldFor)
						{
							Place = K.Place;
						}
					}
					else
					{
						K.ClearSince = -1.0;
						if (bHeldGood)
						{
							Place = K.Place;
						}
					}
				}
				else
				{
					K.ClearSince = -1.0;
					K.Place = -1;
				}

				const Box Target = PlaceBox(T, Place, Field, R);
				Placed Put;
				Put.Index = I;
				Put.Block = T.Block;
				Put.bShort = Place >= 3;
				Put.Step = Place >= 3 ? Place - 3 : Place;
				Put.bRoom = bRoom;
				const double Rise = Put.bShort ? 0.0 : Target.Y - T.Full.Y;
				if (Put.bShort || !bKnown || K.Place < 0 || K.Place >= 3)
				{
					// Folding, unfolding and a first showing are at once.
					K.Rise = Rise;
					K.From = Rise;
					K.To = Rise;
					K.Start = Now;
				}
				else
				{
					if (Rise != K.To)
					{
						K.From = K.Rise;
						K.To = Rise;
						K.Start = Now;
					}
					const double U = R.EaseFor > 0.0 ? std::min(std::max((Now - K.Start) / R.EaseFor, 0.0), 1.0) : 1.0;
					const double Smooth = U * U * (3.0 - 2.0 * U);
					double At = K.From + (K.To - K.From) * Smooth;
					if (At != K.To)
					{
						// On its way: only where the whole way between is clear, else it is there at once.
						Box Moving = T.Full;
						Moving.Y = T.Full.Y + At;
						if (!Free(Both(Moving, Target)))
						{
							At = K.To;
							K.From = K.To;
						}
					}
					K.Rise = At;
				}
				K.Place = Place;
				K.Seen = Now;
				Put.Rise = K.Rise;
				Put.Drawn = Target;
				if (!Put.bShort)
				{
					Put.Drawn.Y = T.Full.Y + K.Rise;
				}
				Taken.push_back(Put.bShort ? Target : Both(Put.Drawn, Target));
				Was[T.Block] = K;
				Out.push_back(Put);
			}
			// Tags not asked for are forgotten once they have been gone long enough.
			for (auto It = Was.begin(); It != Was.end();)
			{
				if (Now - It->second.Seen > R.Forget || Now < It->second.Seen)
				{
					It = Was.erase(It);
				}
				else
				{
					++It;
				}
			}
			return Out;
		}

	private:
		struct Kept
		{
			/** The place it holds (0 to 11), or -1 before its first. */
			int Place = -1;
			/** Since when a better place has had room, or under 0. */
			double ClearSince = -1.0;
			/** A full tag's rise as drawn, and the step it is easing over. */
			double Rise = 0.0, From = 0.0, To = 0.0, Start = 0.0;
			double Seen = 0.0;
		};
		std::unordered_map<int, Kept> Was;
	};
}
