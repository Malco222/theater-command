#pragma once

// The grass, corn, flowers and stones round the eye laid once, in squares, and never laid again while they stay in
// reach (the graphics audit's item 4, the battle-scale plan's GS7). Until now the whole box round the eye was cleared and
// laid again, made afresh from the field, every time the eye moved 12 m: 2.1 to 2.9 ms on the Mac each time, a stutter
// in every moving shot. Now the ground round the eye is cut into 16 m squares (two of the field's own 8 m clutter squares
// each way, so a square lays exactly what any bigger box would). A square coming into reach is made once and its things
// added at the end of the instanced meshes; a square going out of reach gives its places back, each filled with its
// mesh's last thing and the last place removed, so a mesh draws only what is laid (GS7; the old way, bCompact false,
// left each given-back place shrunk to nothing where it stood for the next square to use, and the meshes drew them all).
// A shell hole or a tank's tracks lay only the squares they touch again, from what the square already holds, without
// making it afresh, and only what changed (a thing gone, one come into a kind's reach) moves in the meshes.
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
	 * Thing's own spot (bHide true: the old way only). A place at the mesh's count is a new one, added at the end. bPop:
	 * the mesh's last place (Slot) removed (kept packed, GS7: a place given back is first filled with the last place's
	 * thing, a put, unless it is the last). Copy -1: every mesh emptied (a cut that leaves nothing held), its places
	 * counted again from 0. */
	struct Op
	{
		int32_t Copy = -1, Slot = -1;
		bool bHide = false;
		LandField::Placed Thing;
		bool bPop = false;
	};

	/** What one Update did. */
	struct Tally
	{
		int Made = 0;    // squares made from the field (the dear part)
		int Relaid = 0;  // squares laid again from what they hold (a hole, a track, the eye nearer or farther)
		int Dropped = 0; // squares out of reach, their places given back
		int Waiting = 0; // squares wanted but left for a later frame
		int Put = 0, Hidden = 0; // things laid, things given back
		int Moved = 0;           // places given back filled with their mesh's last thing (kept packed, GS7)
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
		/** Squares this near the eye or what it looks at are made at once, however many; the others a few a frame. A
		 * cut makes only these too (32 m round both the eye and a long lens's mark was some 45 squares, 6.5 ms on a
		 * busy box: a hitch at every cut), and the frames after it make CutFollow a frame, not the caller's few, until
		 * none wait: the far squares, small in the picture, follow within a few frames. */
		static constexpr double AtOnce = 20.0;
		static constexpr int CutFollow = 8;
		/** A kind drawn only near the eye (the short sward) is laid in a square while the square's nearest point is in
		 * its reach, and the square laid again only once it is this far over the line, either way. */
		static constexpr double ReachSway = 4.0;
		/** A square marked by a hole or a track is laid again at most this often. */
		static constexpr double MarkEvery = 1.0;

		/** Per kind (LandField::Base), how near the eye it is laid (none: very far). */
		std::vector<double> Reach;
		/** Which instanced mesh a thing goes in (-1: none). Asked once a thing, when its square is made. */
		std::function<int32_t(const LandField::Placed&)> CopyOf;
		/** Per kind (LandField::Base), how near the eye its things cast a shadow, metres (none, or 0: wherever its mesh
		 * does): the corn's within some 25 m (the battle-scale plan's GS9). A square whose nearest point is nearer the
		 * eye than this lays such things in their mesh's shadowed twin (Twin) instead, and is laid again once the eye
		 * crosses the line by ReachSway, either way, as for Reach: so every such thing within ShadowTo - ReachSway of the
		 * eye casts its shadow, and none past ShadowTo + ReachSway and a square's width. */
		std::vector<double> ShadowTo;
		/** Per mesh, its shadowed twin (-1: none): the same thing in a mesh that casts a shadow. */
		std::vector<int32_t> Twin;
		/** Whether it still stands (not in a shell hole's bare earth, not flattened under a tank). Asked each time a
		 * square is laid. */
		std::function<bool(const LandField::Placed&)> Stands;
		/** Per kind (LandField::Base), how near the eye a thing is cleared away, metres (none, or 0: never): the corn's
		 * bundles round the lens, where its stalks are drawn one by one (the art director's cut 4, fix 4). Kept as the
		 * eye moves, a thing at a time, and given back as it leaves the clearing. The caller may change it between
		 * updates (the scene clears the corn only while the eye is low, where the stalks are drawn): the next update
		 * hides or gives back what the change takes in or lets out. */
		std::vector<double> Clear;
		/** Whether each mesh is kept packed (the battle-scale plan's GS7): a place given back is filled with its mesh's
		 * last thing (a put) and the last place removed (a pop), so the meshes hold only what is laid and draw nothing
		 * hidden. False: a place given back stays, shrunk to nothing where it stood, for the next thing to take (the old
		 * way; the scene's "-TheaterLandClutterShrunk"). Set before the first Update. */
		bool bCompact = true;

		/** Brings the squares round the eye and what it looks at up to date: those within AtOnce made now, and at most
		 * MakeMost others, nearest first (CutFollow while a cut's, or the first frame's, are still being made). Now:
		 * the battle's real clock, for the marks. The meshes' changes are added to Out. */
		Tally Update(const LandField::Ground& G, double EyeX, double EyeY, double LookX, double LookY, bool bCut, int MakeMost, double Now, std::vector<Op>& Out)
		{
			Tally T;
			const bool bLook = std::hypot(LookX - EyeX, LookY - EyeY) >= LookApart;
			// (the clearing round the eye moves with it: squares laid this frame lay round where it is now; those held
			// from before are mended at the end)
			const double WasX = ClearX, WasY = ClearY, WasMost = ClearMost;
			const bool bClearChanged = Clear != ClearWas;
			ClearWas = Clear;
			ClearX = EyeX;
			ClearY = EyeY;
			ClearMost = 0.0;
			for (const double C : Clear)
			{
				ClearMost = std::max(ClearMost, C);
			}
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
				Owner.clear();
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
				Sq.LaidNear = Near;
				Sq.LaidAt = Now;
				Sq.bMarked = false;
				Relay(Sq, Out, T);
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
			bFollowing = bFollowing || bCut;
			const int Most = bFollowing ? std::max(MakeMost, CutFollow) : MakeMost;
			for (const FreshOne& F : Fresh)
			{
				if (F.D >= AtOnce && Others >= Most)
				{
					++T.Waiting;
					continue;
				}
				if (F.D >= AtOnce)
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
				Sq.In.assign(Sq.Things.size(), -1);
				Sq.Slot.assign(Sq.Things.size(), -1);
				Sq.Cleared.assign(Sq.Things.size(), 0);
				for (size_t K = 0; K < Sq.Things.size(); ++K)
				{
					Sq.Copy[K] = CopyOf ? CopyOf(Sq.Things[K]) : -1;
				}
				Sq.LaidNear = Nearest(Sq.I, Sq.J, EyeX, EyeY);
				Sq.LaidAt = Now;
				Lay(Sq, Out, T);
				++T.Made;
			}
			bFollowing = bFollowing && T.Waiting > 0;
			Clearing(WasX, WasY, WasMost, bClearChanged, Out, T);
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

		/** The places a mesh has, laid and hidden (kept packed: laid only). */
		int32_t Count(int32_t Copy) const { return Copy >= 0 && static_cast<size_t>(Copy) < Size.size() ? Size[static_cast<size_t>(Copy)] : 0; }

		/** Every laid place of a mesh and its thing (for a mesh built again whole). */
		void Each(int32_t Copy, const std::function<void(int32_t, const LandField::Placed&)>& Fn) const
		{
			for (const auto& [K, Sq] : Held)
			{
				for (size_t N = 0; N < Sq.Things.size(); ++N)
				{
					if (Sq.Slot[N] >= 0 && Sq.In[N] == Copy)
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
			std::vector<int32_t> In;   // the mesh it is laid in now (its own, or that mesh's shadowed twin)
			std::vector<int32_t> Slot; // its place in that mesh (-1: not laid)
			std::vector<uint8_t> Cleared; // 1: not laid for being in the clearing round the eye
			double LaidNear = 0.0;     // the square's nearest point to the eye when last laid
			double LaidAt = 0.0;
			bool bMarked = false;
		};
		struct FreshOne
		{
			double D = 0.0;
			std::pair<int, int> IJ;
		};
		/** Kept packed: which square's thing a mesh's place holds. (A held square stays where it is in Held, whose
		 * elements never move as it grows; a square gives back every place before it is let go.) */
		struct Owned
		{
			Square* Sq = nullptr;
			uint32_t N = 0;
		};

		std::unordered_map<uint64_t, Square> Held;
		/** The clearing's middle (the eye), the widest kind's reach of it, and the reaches as last used. */
		double ClearX = 1e30, ClearY = 1e30, ClearMost = 0.0;
		std::vector<double> ClearWas;
		std::vector<int32_t> Size;
		std::vector<std::vector<int32_t>> Free;  // the old way: places given back, for the next thing
		std::vector<std::vector<Owned>> Owner;   // kept packed: each place's thing
		std::vector<int32_t> WantIn;             // (Relay's: the mesh each thing goes in now)
		std::vector<std::pair<int, int>> Wanted;
		std::vector<FreshOne> Fresh;
		/** A cut's far squares still being made, CutFollow a frame. */
		bool bFollowing = false;
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

		/** Whether the eye has come into, or gone out of, a kind's reach (or its shadows' reach) of a square since it was
		 * laid (by ReachSway). */
		bool Crossed(double Was, double Now) const
		{
			for (const std::vector<double>* Lines : { &Reach, &ShadowTo })
			{
				for (const double R : *Lines)
				{
					if (R <= 0.0 || R > 1e6)
					{
						continue;
					}
					if ((Was < R) != (Now < R) && std::fabs(Now - R) > ReachSway)
					{
						return true;
					}
				}
			}
			return false;
		}

		/** The mesh thing N of a square is laid in: its own, or its shadowed twin while the square was laid within its
		 * kind's ShadowTo of the eye. */
		int32_t Into(const Square& Sq, size_t N) const
		{
			const int32_t Copy = Sq.Copy[N];
			const size_t Kind = static_cast<size_t>(Sq.Things[N].Kind);
			if (Copy >= 0 && Kind < ShadowTo.size() && ShadowTo[Kind] > 0.0 && Sq.LaidNear < ShadowTo[Kind] && static_cast<size_t>(Copy) < Twin.size()
				&& Twin[static_cast<size_t>(Copy)] >= 0)
			{
				return Twin[static_cast<size_t>(Copy)];
			}
			return Copy;
		}

		/** A place in mesh Copy for thing N of Sq: kept packed, a new one at the end; the old way, one given back if any. */
		int32_t Take(int32_t Copy, Square& Sq, size_t N)
		{
			const size_t C = static_cast<size_t>(Copy);
			if (Size.size() <= C)
			{
				Size.resize(C + 1, 0);
				Free.resize(C + 1);
				Owner.resize(C + 1);
			}
			if (bCompact)
			{
				Owner[C].push_back({ &Sq, static_cast<uint32_t>(N) });
				return Size[C]++;
			}
			if (!Free[C].empty())
			{
				const int32_t S = Free[C].back();
				Free[C].pop_back();
				return S;
			}
			return Size[C]++;
		}

		/** Thing N of Sq laid in mesh In: a place taken and put. */
		void Put(Square& Sq, size_t N, int32_t In, std::vector<Op>& Out, Tally& T)
		{
			Sq.In[N] = In;
			Sq.Slot[N] = Take(In, Sq, N);
			Out.push_back({ In, Sq.Slot[N], false, Sq.Things[N] });
			++T.Put;
		}

		/** Thing N of Sq's place given back. Kept packed: the mesh's last thing put into it (unless it is the last) and the
		 * last place removed. The old way: the place hidden, shrunk to nothing where it stood, for the next thing. */
		void GiveBack(Square& Sq, size_t N, std::vector<Op>& Out, Tally& T)
		{
			const int32_t Copy = Sq.In[N], Slot = Sq.Slot[N];
			const size_t C = static_cast<size_t>(Copy);
			Sq.Slot[N] = -1;
			++T.Hidden;
			if (!bCompact)
			{
				Free[C].push_back(Slot);
				Out.push_back({ Copy, Slot, true, Sq.Things[N] });
				return;
			}
			const int32_t Last = Size[C] - 1;
			if (Slot != Last)
			{
				const Owned Moved = Owner[C][static_cast<size_t>(Last)];
				Owner[C][static_cast<size_t>(Slot)] = Moved;
				Moved.Sq->Slot[Moved.N] = Slot;
				Out.push_back({ Copy, Slot, false, Moved.Sq->Things[Moved.N] });
				++T.Moved;
			}
			Owner[C].pop_back();
			--Size[C];
			Out.push_back({ Copy, Last, false, LandField::Placed(), true });
		}

		/** The mesh thing N of a square goes in, laid now (-1: not laid: no mesh, past its kind's reach, fallen, or in the
		 * clearing round the lens, which it marks in Cleared). */
		int32_t Wants(Square& Sq, size_t N) const
		{
			const LandField::Placed& P = Sq.Things[N];
			Sq.Cleared[N] = 0;
			if (Sq.Copy[N] < 0)
			{
				return -1;
			}
			const size_t Kind = static_cast<size_t>(P.Kind);
			if (Kind < Reach.size() && Sq.LaidNear >= Reach[Kind])
			{
				return -1; // (past where it is drawn: the sward's short grass, laid only near the eye)
			}
			if (Stands && !Stands(P))
			{
				return -1; // (blown away, or laid flat under a tank's tracks)
			}
			if (InClearing(P))
			{
				Sq.Cleared[N] = 1; // (round the lens, where its stalks are drawn one by one)
				return -1;
			}
			return Into(Sq, N);
		}

		/** Whether a thing is in the clearing round the eye (its kind's Clear). */
		bool InClearing(const LandField::Placed& P) const
		{
			const size_t Kind = static_cast<size_t>(P.Kind);
			return Kind < Clear.size() && Clear[Kind] > 0.0 && std::hypot(P.X - ClearX, P.Y - ClearY) < Clear[Kind];
		}

		/** The squares held from before round where the clearing was and where it is: each clearable thing come into it
		 * hidden, each gone out of it laid again (if it still stands and is in its reach). WasMost: the widest reach
		 * before; bChanged: the reaches changed since. */
		void Clearing(double WasX, double WasY, double WasMost, bool bChanged, std::vector<Op>& Out, Tally& T)
		{
			const double Most = std::max(ClearMost, WasMost);
			const bool bMoved = std::fabs(WasX - ClearX) >= 1e-3 || std::fabs(WasY - ClearY) >= 1e-3;
			if (Most <= 0.0 || (!bMoved && !bChanged))
			{
				return;
			}
			if (WasX >= 1e29)
			{
				Clearing(Out, T, ClearX - Most, ClearY - Most, ClearX + Most, ClearY + Most);
				return;
			}
			// (a jump, a cut: where the clearing was and where it is now mended each on its own, not all the ground
			// between them; what it left behind is given back even where its squares are still held)
			if (std::fabs(WasX - ClearX) > 2.0 * Most + Side || std::fabs(WasY - ClearY) > 2.0 * Most + Side)
			{
				Clearing(Out, T, WasX - Most, WasY - Most, WasX + Most, WasY + Most);
				Clearing(Out, T, ClearX - Most, ClearY - Most, ClearX + Most, ClearY + Most);
				return;
			}
			Clearing(Out, T, std::min(ClearX, WasX) - Most, std::min(ClearY, WasY) - Most, std::max(ClearX, WasX) + Most, std::max(ClearY, WasY) + Most);
		}

		void Clearing(std::vector<Op>& Out, Tally& T, double X0, double Y0, double X1, double Y1)
		{
			const int I0 = static_cast<int>(std::floor(X0 / Side)), I1 = static_cast<int>(std::floor(X1 / Side));
			const int J0 = static_cast<int>(std::floor(Y0 / Side)), J1 = static_cast<int>(std::floor(Y1 / Side));
			for (int J = J0; J <= J1; ++J)
			{
				for (int I = I0; I <= I1; ++I)
				{
					const auto Found = Held.find(Key(I, J));
					if (Found == Held.end())
					{
						continue;
					}
					Square& Sq = Found->second;
					for (size_t N = 0; N < Sq.Things.size(); ++N)
					{
						const LandField::Placed& P = Sq.Things[N];
						const size_t Kind = static_cast<size_t>(P.Kind);
						if (Sq.Copy[N] < 0 || (!Sq.Cleared[N] && (Kind >= Clear.size() || Clear[Kind] <= 0.0)))
						{
							continue;
						}
						const bool bIn = InClearing(P);
						if (bIn && Sq.Slot[N] >= 0)
						{
							GiveBack(Sq, N, Out, T);
							Sq.Cleared[N] = 1;
						}
						else if (!bIn && Sq.Cleared[N])
						{
							Sq.Cleared[N] = 0;
							if ((Kind < Reach.size() && Sq.LaidNear >= Reach[Kind]) || (Stands && !Stands(P)))
							{
								continue;
							}
							Put(Sq, N, Into(Sq, N), Out, T);
						}
					}
				}
			}
		}

		/** A square made: each of its things laid that should be. */
		void Lay(Square& Sq, std::vector<Op>& Out, Tally& T)
		{
			for (size_t N = 0; N < Sq.Things.size(); ++N)
			{
				const int32_t In = Wants(Sq, N);
				if (In >= 0)
				{
					Put(Sq, N, In, Out, T);
				}
			}
		}

		/** A held square laid again (a hole, a track, the eye come nearer or gone farther): what stays in the same mesh
		 * keeps its place, nothing sent for it; what goes, or changes mesh, is given back first, then what comes is laid. */
		void Relay(Square& Sq, std::vector<Op>& Out, Tally& T)
		{
			WantIn.resize(Sq.Things.size());
			for (size_t N = 0; N < Sq.Things.size(); ++N)
			{
				WantIn[N] = Wants(Sq, N);
			}
			for (size_t N = 0; N < Sq.Things.size(); ++N)
			{
				if (Sq.Slot[N] >= 0 && Sq.In[N] != WantIn[N])
				{
					GiveBack(Sq, N, Out, T);
				}
			}
			for (size_t N = 0; N < Sq.Things.size(); ++N)
			{
				if (Sq.Slot[N] < 0 && WantIn[N] >= 0)
				{
					Put(Sq, N, WantIn[N], Out, T);
				}
			}
		}

		/** A square let go: every place it holds given back. */
		void Unlay(Square& Sq, std::vector<Op>& Out, Tally& T)
		{
			for (size_t N = 0; N < Sq.Things.size(); ++N)
			{
				if (Sq.Slot[N] >= 0)
				{
					GiveBack(Sq, N, Out, T);
				}
			}
		}
	};
}
