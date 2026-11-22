#pragma once

// The grass, corn, flowers and stones round the eye laid once, in squares, and never laid again while they stay in
// reach (the graphics audit's item 4, the battle-scale plan's GS7). Until now the whole box round the eye was cleared and
// laid again, made afresh from the field, every time the eye moved 12 m: 2.1 to 2.9 ms on the Mac each time, a stutter
// in every moving shot. Now the ground round the eye is cut into 16 m squares (two of the field's own 8 m clutter squares
// each way, so a square lays exactly what any bigger box would). A square coming into reach is made once and its things
// put into free places in the instanced meshes; a square going out of reach gives its places back (shrunk to nothing
// where they stood) for the next square to use. A shell hole or a tank's tracks lay only the squares they touch again,
// from what the square already holds, without making it afresh.
//
// Plain C++ (no engine): the land field test drives it along a moving eye and checks it against the old box. The
// scene (LandBattle) turns its steps into the instanced meshes' places.

#include "LandField.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace LandCarpet
{
	/** One change to the meshes: Thing put into a mesh's place (bHide false), or the place hidden, shrunk to nothing at
	 * Thing's own spot (bHide true). A place at or past the mesh's count before the step is a new one, added at the end,
	 * in order. Copy -1: every mesh emptied (a cut that leaves nothing held), its places counted again from 0. */
	struct Op
	{
		int32_t Copy = -1, Slot = -1;
		bool bHide = false;
		LandField::Placed Thing;
	};

	/** What one Update did. */
	struct Tally
	{
		int Made = 0;    // squares made from the field (the dear part)
		int Relaid = 0;  // squares laid again from what they hold (a hole, a track, the eye nearer or farther)
		int Dropped = 0; // squares out of reach, their places given back
		int Waiting = 0; // squares wanted but left for a later frame
		int Put = 0, Hidden = 0;
	};

	class Squares
	{
	public:
		static constexpr double Side = 16.0;
		/** The box laid round the eye (as the old one: 45 m each way) and round what it looks at (25 m, when that is 40 m
		 * or more from the eye: the long lens's men in the grass). */
		static constexpr double EyeHalf = 45.0, LookHalf = 25.0, LookApart = 40.0;
		/** A square is let go only 8 m past where it is wanted, so an eye that sways at a square's edge does not make it
		 * and drop it over and over. */
		static constexpr double Keep = 8.0;
		/** Squares this near the eye or what it looks at are made at once, however many; the others a few a frame. On a
		 * cut, those within CutAtOnce (the rest, farther than 32 m, are small in the picture and follow within a few
		 * frames): a cut then costs no more than the old box's whole re-lay did. */
		static constexpr double AtOnce = 20.0, CutAtOnce = 32.0;
		/** A kind drawn only near the eye (the short sward) is laid in a square while the square's nearest point is in
		 * its reach, and the square laid again only once it is this far over the line, either way. */
		static constexpr double ReachSway = 4.0;
		/** A square marked by a hole or a track is laid again at most this often. */
		static constexpr double MarkEvery = 1.0;

		/** Per kind (LandField::Base), how near the eye it is laid (none: very far). */
		std::vector<double> Reach;
		/** Which instanced mesh a thing goes in (-1: none). Asked once a thing, when its square is made. */
		std::function<int32_t(const LandField::Placed&)> CopyOf;
		/** Whether it still stands (not in a shell hole's bare earth, not flattened under a tank). Asked each time a
		 * square is laid. */
		std::function<bool(const LandField::Placed&)> Stands;

		/** Brings the squares round the eye and what it looks at up to date: those within AtOnce made now (CutAtOnce on
		 * a cut, or the first frame), and at most MakeMost others, nearest first. Now: the battle's real clock, for the
		 * marks. The meshes' changes are added to Out. */
		Tally Update(const LandField::Ground& G, double EyeX, double EyeY, double LookX, double LookY, bool bCut, int MakeMost, double Now, std::vector<Op>& Out)
		{
			Tally T;
			const bool bLook = std::hypot(LookX - EyeX, LookY - EyeY) >= LookApart;
			// A cut far away lets go of every square: the meshes emptied whole, not hidden place by place.
			bool bAny = false;
			for (const auto& [K, Sq] : Held)
			{
				if (Within(Sq.I, Sq.J, EyeX, EyeY, EyeHalf + Keep) || (bLook && Within(Sq.I, Sq.J, LookX, LookY, LookHalf + Keep)))
				{
					bAny = true;
					break;
				}
			}
			if (!bAny && !Held.empty())
			{
				T.Dropped += static_cast<int>(Held.size());
				Held.clear();
				Size.clear();
				Free.clear();
				Out.push_back({ -1, -1, true, LandField::Placed() });
			}
			// Let go of the squares no longer wanted.
			for (auto It = Held.begin(); It != Held.end();)
			{
				const Square& Sq = It->second;
				if (Within(Sq.I, Sq.J, EyeX, EyeY, EyeHalf + Keep) || (bLook && Within(Sq.I, Sq.J, LookX, LookY, LookHalf + Keep)))
				{
					++It;
					continue;
				}
				Unlay(It->second, Out, T);
				It = Held.erase(It);
				++T.Dropped;
			}
			// Lay again those held whose marks are due, or whose nearness to the eye crossed a kind's reach.
			for (auto& [Key, Sq] : Held)
			{
				const double Near = Nearest(Sq.I, Sq.J, EyeX, EyeY);
				const bool bReach = Crossed(Sq.LaidNear, Near);
				const bool bMark = Sq.bMarked && Now - Sq.LaidAt >= MarkEvery;
				if (!bReach && !bMark)
				{
					continue;
				}
				Unlay(Sq, Out, T);
				Sq.LaidNear = Near;
				Sq.LaidAt = Now;
				Sq.bMarked = false;
				Lay(Sq, Out, T);
				++T.Relaid;
			}
			// The squares wanted and not held yet, nearest first.
			Wanted.clear();
			Want(EyeX, EyeY, EyeHalf);
			if (bLook)
			{
				Want(LookX, LookY, LookHalf);
			}
			Fresh.clear();
			for (const std::pair<int, int>& IJ : Wanted)
			{
				if (Held.count(Key(IJ.first, IJ.second)) != 0)
				{
					continue;
				}
				double D = Nearest(IJ.first, IJ.second, EyeX, EyeY);
				if (bLook)
				{
					D = std::min(D, Nearest(IJ.first, IJ.second, LookX, LookY));
				}
				Fresh.push_back({ D, IJ });
			}
			std::sort(Fresh.begin(), Fresh.end(), [](const FreshOne& A, const FreshOne& B) { return A.D < B.D || (A.D == B.D && A.IJ < B.IJ); });
			int Others = 0;
			const double OnceWithin = bCut ? CutAtOnce : AtOnce;
			for (const FreshOne& F : Fresh)
			{
				if (F.D >= OnceWithin && Others >= MakeMost)
				{
					++T.Waiting;
					continue;
				}
				if (F.D >= OnceWithin)
				{
					++Others;
				}
				Square& Sq = Held[Key(F.IJ.first, F.IJ.second)];
				Sq.I = F.IJ.first;
				Sq.J = F.IJ.second;
				Made.clear();
				G.Clutter(Sq.I * Side, Sq.J * Side, (Sq.I + 1) * Side, (Sq.J + 1) * Side, Made);
				Sq.Things.swap(Made);
				Sq.Copy.assign(Sq.Things.size(), -1);
				Sq.Slot.assign(Sq.Things.size(), -1);
				for (size_t K = 0; K < Sq.Things.size(); ++K)
				{
					Sq.Copy[K] = CopyOf ? CopyOf(Sq.Things[K]) : -1;
				}
				Sq.LaidNear = Nearest(Sq.I, Sq.J, EyeX, EyeY);
				Sq.LaidAt = Now;
				Lay(Sq, Out, T);
				++T.Made;
			}
			return T;
		}

		/** A shell hole torn, or a tank's tracks laid, at X, Y within Radius: the held squares there laid again (at most
		 * once in MarkEvery), without what is gone. */
		void Mark(double X, double Y, double Radius)
		{
			const int I0 = static_cast<int>(std::floor((X - Radius) / Side)), I1 = static_cast<int>(std::floor((X + Radius) / Side));
			const int J0 = static_cast<int>(std::floor((Y - Radius) / Side)), J1 = static_cast<int>(std::floor((Y + Radius) / Side));
			for (int J = J0; J <= J1; ++J)
			{
				for (int I = I0; I <= I1; ++I)
				{
					const auto Found = Held.find(Key(I, J));
					if (Found != Held.end())
					{
						Found->second.bMarked = true;
					}
				}
			}
		}

		/** The places a mesh has, laid and hidden. */
		int32_t Count(int32_t Copy) const { return Copy >= 0 && static_cast<size_t>(Copy) < Size.size() ? Size[static_cast<size_t>(Copy)] : 0; }

		/** Every laid place of a mesh and its thing (for a mesh built again whole). */
		void Each(int32_t Copy, const std::function<void(int32_t, const LandField::Placed&)>& Fn) const
		{
			for (const auto& [K, Sq] : Held)
			{
				for (size_t N = 0; N < Sq.Things.size(); ++N)
				{
					if (Sq.Slot[N] >= 0 && Sq.Copy[N] == Copy)
					{
						Fn(Sq.Slot[N], Sq.Things[N]);
					}
				}
			}
		}

		/** Things laid now, and squares held. */
		size_t Laid() const
		{
			size_t N = 0;
			for (const auto& [K, Sq] : Held)
			{
				for (const int32_t S : Sq.Slot)
				{
					N += S >= 0 ? 1 : 0;
				}
			}
			return N;
		}
		size_t SquaresHeld() const { return Held.size(); }

		/** Whether a square is held (for the test). */
		bool Holds(int I, int J) const { return Held.count(Key(I, J)) != 0; }

	private:
		struct Square
		{
			int I = 0, J = 0;
			std::vector<LandField::Placed> Things;
			std::vector<int32_t> Copy; // a thing's mesh (-1: none)
			std::vector<int32_t> Slot; // its place in the mesh (-1: not laid)
			double LaidNear = 0.0;     // the square's nearest point to the eye when last laid
			double LaidAt = 0.0;
			bool bMarked = false;
		};
		struct FreshOne
		{
			double D = 0.0;
			std::pair<int, int> IJ;
		};

		std::unordered_map<uint64_t, Square> Held;
		std::vector<int32_t> Size;
		std::vector<std::vector<int32_t>> Free;
		std::vector<std::pair<int, int>> Wanted;
		std::vector<FreshOne> Fresh;
		std::vector<LandField::Placed> Made;

		static uint64_t Key(int I, int J) { return (static_cast<uint64_t>(static_cast<uint32_t>(I)) << 32) | static_cast<uint32_t>(J); }

		/** The distance from X, Y to a square's nearest point (0 inside it). */
		static double Nearest(int I, int J, double X, double Y)
		{
			const double DX = std::max({ I * Side - X, 0.0, X - (I + 1) * Side });
			const double DY = std::max({ J * Side - Y, 0.0, Y - (J + 1) * Side });
			return std::hypot(DX, DY);
		}

		/** Whether a square meets the box Half each way round X, Y. */
		static bool Within(int I, int J, double X, double Y, double Half)
		{
			return (I + 1) * Side > X - Half && I * Side < X + Half && (J + 1) * Side > Y - Half && J * Side < Y + Half;
		}

		void Want(double X, double Y, double Half)
		{
			const int I0 = static_cast<int>(std::floor((X - Half) / Side)), I1 = static_cast<int>(std::floor((X + Half) / Side));
			const int J0 = static_cast<int>(std::floor((Y - Half) / Side)), J1 = static_cast<int>(std::floor((Y + Half) / Side));
			for (int J = J0; J <= J1; ++J)
			{
				for (int I = I0; I <= I1; ++I)
				{
					if (std::find(Wanted.begin(), Wanted.end(), std::make_pair(I, J)) == Wanted.end())
					{
						Wanted.push_back({ I, J });
					}
				}
			}
		}

		/** Whether the eye has come into, or gone out of, a kind's reach of a square since it was laid (by ReachSway). */
		bool Crossed(double Was, double Now) const
		{
			for (const double R : Reach)
			{
				if (R > 1e6)
				{
					continue;
				}
				if ((Was < R) != (Now < R) && std::fabs(Now - R) > ReachSway)
				{
					return true;
				}
			}
			return false;
		}

		int32_t Take(int32_t Copy)
		{
			const size_t C = static_cast<size_t>(Copy);
			if (Size.size() <= C)
			{
				Size.resize(C + 1, 0);
				Free.resize(C + 1);
			}
			if (!Free[C].empty())
			{
				const int32_t S = Free[C].back();
				Free[C].pop_back();
				return S;
			}
			return Size[C]++;
		}

		void Lay(Square& Sq, std::vector<Op>& Out, Tally& T)
		{
			for (size_t N = 0; N < Sq.Things.size(); ++N)
			{
				const LandField::Placed& P = Sq.Things[N];
				const int32_t Copy = Sq.Copy[N];
				if (Copy < 0)
				{
					continue;
				}
				const size_t Kind = static_cast<size_t>(P.Kind);
				if (Kind < Reach.size() && Sq.LaidNear >= Reach[Kind])
				{
					continue; // (past where it is drawn: the sward's short grass, laid only near the eye)
				}
				if (Stands && !Stands(P))
				{
					continue; // (blown away, or laid flat under a tank's tracks)
				}
				Sq.Slot[N] = Take(Copy);
				Out.push_back({ Copy, Sq.Slot[N], false, P });
				++T.Put;
			}
		}

		void Unlay(Square& Sq, std::vector<Op>& Out, Tally& T)
		{
			for (size_t N = 0; N < Sq.Things.size(); ++N)
			{
				if (Sq.Slot[N] < 0)
				{
					continue;
				}
				const int32_t Copy = Sq.Copy[N];
				Free[static_cast<size_t>(Copy)].push_back(Sq.Slot[N]);
				Out.push_back({ Copy, Sq.Slot[N], true, Sq.Things[N] });
				Sq.Slot[N] = -1;
				++T.Hidden;
			}
		}
	};
}
