#include "Battle/DeckCrewCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>

namespace DeckCrew
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Never = 1e300;

		double Rad(double Degrees)
		{
			return Degrees * Pi / 180.0;
		}

		/** 0 to 1 from a ship's key and a man's role (his own: no two of a crew move as one). */
		double Hash01(int Key, int Salt)
		{
			uint32_t H = static_cast<uint32_t>(Key) * 2654435761u ^ (static_cast<uint32_t>(Salt) * 0x9E3779B9u);
			H ^= H >> 15;
			H *= 0x85EBCA6Bu;
			H ^= H >> 13;
			return static_cast<double>(H & 0xFFFFFFu) / 16777216.0;
		}

		double Wrap(double T, double Long)
		{
			if (Long <= 0.0 || !std::isfinite(T))
			{
				return 0.0;
			}
			const double W = std::fmod(T, Long);
			return W < 0.0 ? W + Long : W;
		}

		/** A place along the bore (A metres ahead of the mount's middle, B to the gun's right) at a train, in her frame. */
		void FromBore(double A, double B, double Train, double& OutFore, double& OutRight)
		{
			const double C = std::cos(Rad(Train)), S = std::sin(Rad(Train));
			OutFore = A * C - B * S;
			OutRight = A * S + B * C;
		}

		/** A man's footprint about his root, in his own axes (the gun clips' feet: from his heels to his toes, either side). */
		constexpr double Heels = 0.20, Toes = 0.33, Feet = 0.21;

		/** The bearing of a place from the mount's middle (degrees off her bow). */
		double BearingOf(double Fore, double Right)
		{
			return std::atan2(Right, Fore) * 180.0 / Pi;
		}

		/** How far round the mount's middle a deck man's way runs: AroundR, on a boat's casing narrower than that Inboard
		 * inside her edge where it crosses her beam (over the gun's raised foot, DeckUnder's). */
		double AroundOf(const GunShape& G)
		{
			return G.bBoat ? std::min(AroundR, (G.DeckHalf - Inboard) / std::hypot(1.0, G.DeckTaper)) : AroundR;
		}

		/**
		 * A deck man's way from the hatch to his post (fore and right pairs): out from the hatch to AroundOf about the
		 * mount's middle, round it at that the way that does not pass under the barrel (its bearing Train), and in or out
		 * to his post.
		 */
		std::vector<double> WayUp(const GunShape& G, const ConvoyLook::GunMount& M, const Post& To, double Train)
		{
			std::vector<double> Way;
			const Spot Hatch = HatchOf(G, M);
			const double HatchFore = Hatch.Fore, HatchRight = Hatch.Right, Around = AroundOf(G);
			Way.push_back(HatchFore);
			Way.push_back(HatchRight);
			const double From = BearingOf(HatchFore, HatchRight), Onto = BearingOf(To.Root.Fore, To.Root.Right);
			double Sweep = std::fmod(Onto - From + 720.0, 360.0);
			const double Barrel = std::fmod(Train - From + 720.0, 360.0);
			if (Barrel < Sweep)
			{
				Sweep -= 360.0;
			}
			const int Steps = std::max(1, static_cast<int>(std::ceil(std::fabs(Sweep) / 10.0)));
			for (int K = 0; K <= Steps; ++K)
			{
				const double B = Rad(From + Sweep * K / Steps);
				Way.push_back(Around * std::cos(B));
				Way.push_back(Around * std::sin(B));
			}
			Way.push_back(To.Root.Fore);
			Way.push_back(To.Root.Right);
			return Way;
		}

		double LengthOf(const std::vector<double>& Way)
		{
			double L = 0.0;
			for (size_t K = 2; K + 1 < Way.size(); K += 2)
			{
				L += std::hypot(Way[K] - Way[K - 2], Way[K + 1] - Way[K - 1]);
			}
			return L;
		}

		/** The place Ran metres along a way, and the way it runs there (degrees off her bow). */
		void Along(const std::vector<double>& Way, double Ran, double& OutFore, double& OutRight, double& OutFacing)
		{
			OutFore = Way.size() >= 2 ? Way[0] : 0.0;
			OutRight = Way.size() >= 2 ? Way[1] : 0.0;
			OutFacing = 0.0;
			double Left = std::max(0.0, Ran);
			for (size_t K = 2; K + 1 < Way.size(); K += 2)
			{
				const double DF = Way[K] - Way[K - 2], DR = Way[K + 1] - Way[K - 1], L = std::hypot(DF, DR);
				if (L < 1e-9)
				{
					continue;
				}
				OutFacing = BearingOf(DF, DR);
				if (Left <= L)
				{
					OutFore = Way[K - 2] + DF * Left / L;
					OutRight = Way[K - 1] + DR * Left / L;
					return;
				}
				Left -= L;
				OutFore = Way[K];
				OutRight = Way[K + 1];
			}
		}

		/** The way back from Ran metres along a way to its start, reversed. */
		std::vector<double> Back(const std::vector<double>& Way, double Ran)
		{
			std::vector<double> Out;
			double Fore = 0.0, Right = 0.0, Facing = 0.0;
			Along(Way, Ran, Fore, Right, Facing);
			Out.push_back(Fore);
			Out.push_back(Right);
			double Gone = 0.0;
			std::vector<double> Before;
			for (size_t K = 2; K + 1 < Way.size(); K += 2)
			{
				Gone += std::hypot(Way[K] - Way[K - 2], Way[K + 1] - Way[K - 1]);
				if (Gone >= Ran)
				{
					break;
				}
				Before.push_back(Way[K]);
				Before.push_back(Way[K + 1]);
			}
			for (size_t K = Before.size(); K >= 2; K -= 2)
			{
				Out.push_back(Before[K - 2]);
				Out.push_back(Before[K - 1]);
			}
			Out.push_back(Way[0]);
			Out.push_back(Way[1]);
			return Out;
		}

		/** How far along a way a man waiting short of its end (her hatch) stops: the furthest place along it at least
		 * Short from its end straight across her deck (a way round the mount comes back near the hatch). */
		double StopShort(const std::vector<double>& Way, double Short)
		{
			if (Way.size() < 2)
			{
				return 0.0;
			}
			const double EndFore = Way[Way.size() - 2], EndRight = Way[Way.size() - 1];
			for (double Ran = LengthOf(Way); Ran > 0.0; Ran -= 0.05)
			{
				double Fore = 0.0, Right = 0.0, Facing = 0.0;
				Along(Way, Ran, Fore, Right, Facing);
				if (std::hypot(Fore - EndFore, Right - EndRight) >= Short)
				{
					return Ran;
				}
			}
			return 0.0;
		}

		/** The cues of the loader's work between two times in his clip (and the passer's with it): the round handed over
		 * only where the passer hands it up (bHanded; from the rack, none). */
		void Cues(const Blocks& K, double From, double To, bool bHanded, std::vector<Cue>* Out)
		{
			if (!Out)
			{
				return;
			}
			auto Heard = [&](float At, const std::string& Name, Role Who)
			{
				if (!Name.empty() && At > From && At <= To)
				{
					Out->push_back({ Who, Name });
				}
			};
			if (bHanded)
			{
				Heard(K.Taken, K.TakenCue, Role::Loader);
			}
			Heard(K.Ram, K.RamCue, Role::Loader);
			Heard(K.Shut, K.ShutCue, Role::Loader);
			if (bHanded)
			{
				Heard(K.Lift, K.LiftCue, Role::Passer);
			}
		}

		/** Her casing's half width at its narrowest from A to B metres toward her bow from the mount's middle (DeckHalfAt's
		 * each way: each slice's flats at their narrowest within it, widening DeckEdgeSlope a metre beyond it, the least of
		 * those; else the line under them). */
		double DeckHalfWithin(const GunShape& G, double A, double B)
		{
			const double Lo = std::clamp(std::min(A, B), DeckEdgeFrom, -DeckEdgeFrom), Hi = std::clamp(std::max(A, B), DeckEdgeFrom, -DeckEdgeFrom);
			if (!G.bDeckEdges)
			{
				return std::min(G.DeckHalf - G.DeckTaper * Lo, G.DeckHalf - G.DeckTaper * Hi);
			}
			double Least = Never;
			for (int Slice = 0; Slice < DeckEdgeCount; ++Slice)
			{
				const double Mid = DeckEdgeFrom + DeckEdgeStep * Slice;
				const double Gap = Mid < Lo ? Lo - Mid : Mid > Hi ? Mid - Hi : 0.0;
				Least = std::min(Least, G.DeckEdges[static_cast<size_t>(Slice)] + DeckEdgeSlope * std::max(0.0, Gap - 0.5 * DeckEdgeStep));
			}
			return Least;
		}

		/** Each of a man's boots' corners at a root and facing: how far it is past her casing's edge across her beam (or her
		 * platform's rim, out from the mount's middle) (Out, metres: at or under 0, on it), and how much of a step from it
		 * straight toward his root goes that way in (Toward, at least TowardLeast): a corner Out past it is Out / Toward
		 * from her deck that way, as far as a boot goes over a flat's edge. */
		constexpr double TowardLeast = 0.35;
		template <typename FEach>
		void Corners(const GunShape& G, double RootFore, double RootRight, double Facing, const FEach& Each)
		{
			for (const double X : { -Heels, Toes })
			{
				for (const double Y : { -Feet, Feet })
				{
					double StepFore = 0.0, StepRight = 0.0;
					FromBore(X, Y, Facing, StepFore, StepRight);
					const double CornerFore = RootFore + StepFore, CornerRight = RootRight + StepRight, Reach = std::hypot(X, Y);
					if (G.bBoat)
					{
						const double Way = CornerRight >= 0.0 ? 1.0 : -1.0;
						Each(std::fabs(CornerRight) - DeckHalfWithin(G, CornerFore, RootFore), std::max(TowardLeast, Way * StepRight / Reach));
					}
					else
					{
						const double Out = std::hypot(CornerFore, CornerRight);
						const double Outward = Out > 1e-9 ? (StepFore * CornerFore + StepRight * CornerRight) / (Out * Reach) : 1.0;
						Each(Out - G.Platform, std::max(TowardLeast, Outward));
					}
				}
			}
		}

		/** How far a man's boots' corners go past her casing's edge or her platform's rim at a root and facing (metres, each
		 * toward his root: at or under 0, on it). */
		double OverAt(const GunShape& G, double RootFore, double RootRight, double Facing)
		{
			double Most = -Never;
			Corners(G, RootFore, RootRight, Facing, [&Most](double Out, double Toward) { Most = std::max(Most, Out > 0.0 ? Out / Toward : Out); });
			return Most;
		}

		/** A loader's root moved in where his boots go past her casing's edge by more than EdgeMost, or past her platform's
		 * rim (across her beam toward her middle line; a merchant's toward the mount's middle) by as much; how far (metres). */
		double ShiftIn(const GunShape& G, double& RootFore, double& RootRight, double Facing)
		{
			double Shift = 0.0;
			for (int Pass = 0; Pass < 4; ++Pass)
			{
				// (moved in by Over, a corner Out past it comes Out - Over: at most EdgeMost from it toward his root)
				const double Allowed = G.bBoat ? EdgeMost : 0.0;
				double Over = 0.0;
				Corners(G, RootFore, RootRight, Facing, [&Over, Allowed](double Out, double Toward) { Over = std::max(Over, Out - Allowed * Toward); });
				if (Over <= 1e-6)
				{
					break;
				}
				if (G.bBoat)
				{
					RootRight -= RootRight >= 0.0 ? Over : -Over;
				}
				else
				{
					const double Out = std::hypot(RootFore, RootRight);
					const double Keep = Out > 1e-6 ? std::max(0.0, Out - Over) / Out : 0.0;
					RootFore *= Keep;
					RootRight *= Keep;
				}
				Shift += Over;
			}
			return Shift;
		}

		/** Whether the loader's skin ahead of him through his clip (Blocks' Hands) is clear of a boat's gun's breech's ring
		 * at a root and facing: none nearer its middle line than RingHalf and HandsPad, from RingBack behind the chamber's
		 * mouth (Mouth, the bore's way Way) to the barrel's pivot. A merchant's: always. */
		bool HandsOffRing(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, const Spot& Mouth, const double Way[3], double Facing, double RootFore, double RootRight)
		{
			if (G.RingHalf <= 0.0)
			{
				return true;
			}
			double Side[3] = { -Way[1], Way[0], 0.0 };
			const double SideLong = std::max(1e-9, std::hypot(Side[0], Side[1]));
			Side[0] /= SideLong;
			Side[1] /= SideLong;
			const double Over3[3] = { Way[1] * Side[2] - Way[2] * Side[1], Way[2] * Side[0] - Way[0] * Side[2], Way[0] * Side[1] - Way[1] * Side[0] };
			Post At;
			At.Facing = Facing;
			At.Root = { RootFore, RootRight, DeckUnder(G, M, RootFore, RootRight) };
			for (const LandMen::Vec3& Hand : K.Hands)
			{
				const Spot Her = ToHer(At, Hand.X, Hand.Y, Hand.Z);
				const double D[3] = { Her.Fore - Mouth.Fore, Her.Right - Mouth.Right, Her.Up - Mouth.Up };
				const double U = D[0] * Way[0] + D[1] * Way[1] + D[2] * Way[2];
				if (U < -G.RingBack - HandsPad || U > G.BreechBack)
				{
					continue;
				}
				if (std::fabs(D[0] * Side[0] + D[1] * Side[1]) < G.RingHalf + HandsPad && std::fabs(D[0] * Over3[0] + D[1] * Over3[1] + D[2] * Over3[2]) < G.RingHalf + HandsPad)
				{
					return false;
				}
			}
			return true;
		}

		/** The first key of the loader's track at or after the round is taken that shows it in his hands. */
		float TakeAt(const Blocks& K)
		{
			for (size_t I = 0; I + 7 < K.Loading.Track.size(); I += 8)
			{
				if (K.Loading.Track[I] >= K.Taken - 1e-4f && K.Loading.Track[I + 1] > 0.5f)
				{
					return K.Loading.Track[I];
				}
			}
			return K.Taken;
		}
	}

	SeaSailors::Act ActOf(Role R)
	{
		switch (R)
		{
		case Role::Layer: return SeaSailors::Act::Layer;
		case Role::Trainer: return SeaSailors::Act::Trainer;
		case Role::Loader: return SeaSailors::Act::Loader;
		default: return SeaSailors::Act::Passer;
		}
	}

	GunShape ShapeOf(const ConvoyLook::GunMount& M)
	{
		GunShape G;
		if (M.File.empty())
		{
			return G;
		}
		// [MODEL] Tools/battle_ships_in.py's boats (Content/Models/Ships/Submarine*.json): the same gun on each, its mount on
		// a raised foot 1.2 m about (its top 0.45 m under the mount's pivot), the barrel's pivot 0.95 m over that foot and
		// its chamber's mouth 0.65 m behind it, the barrel built lying 2 degrees up (its muzzle's socket's way); her
		// casing by the gun as wide as the deck flats either side of it run at their narrowest each half metre along her
		// within 3 m of it, a line under them (a Type VII's 1.33 m at the mount's middle, narrowing ahead of it); and as
		// wide as they run unbroken out from her middle line on both sides at their narrowest within each quarter metre
		// along her (DeckEdges, from 3 m behind the mount's middle to 3 m ahead of it: the review of 24 November, a
		// German's loader abeam her gun stands on the flats wider than that line)
		static const struct
		{
			const char* File;
			double Foot, PivotUp, DeckHalf, DeckTaper;
			std::array<double, DeckEdgeCount> Edges;
		} Boats[] = {
			{ "Submarine_GER", 1.770, 2.7177, 1.330, 0.026,
			  { 1.410, 1.545, 1.510, 1.390, 1.455, 1.525, 1.410, 1.375, 1.500, 1.475, 1.355, 1.375, 1.445, 1.375, 1.335, 1.385, 1.430, 1.310, 1.300, 1.295, 1.285,
			    1.280, 1.275, 1.265, 1.260 } },
			{ "Submarine_USA", 2.577, 3.5250, 2.295, 0.0,
			  { 2.290, 2.295, 2.295, 2.295, 2.295, 2.300, 2.300, 2.300, 2.300, 2.305, 2.305, 2.305, 2.305, 2.310, 2.310, 2.310, 2.310, 2.310, 2.310, 2.315, 2.315,
			    2.315, 2.315, 2.315, 2.315 } },
			{ "Submarine", 2.085, 3.0331, 1.562, 0.026,
			  { 1.635, 1.650, 1.800, 1.795, 1.650, 1.615, 1.745, 1.765, 1.660, 1.590, 1.580, 1.575, 1.570, 1.565, 1.560, 1.550, 1.545, 1.540, 1.530, 1.525, 1.520,
			    1.510, 1.505, 1.500, 1.490 } },
		};
		for (const auto& Row : Boats)
		{
			if (M.File != Row.File)
			{
				continue;
			}
			G.File = M.File;
			G.Part = M.Part;
			G.bBoat = true;
			G.Foot = Row.Foot;
			G.FootR = 1.2;
			G.PivotAhead = 0.0;
			G.PivotUp = Row.PivotUp;
			G.BreechBack = 0.652;
			G.RingBack = 0.048;
			G.RingHalf = 0.207;
			G.Built = 2.0;
			G.LiftLeast = -5.0;
			G.LiftMost = 35.0;
			// (the wheels at either side of its cradle, a quarter metre behind the mount's middle, their shafts 0.33 m in to
			// its side plates)
			G.HubBack = 0.25;
			G.HubOut = 0.76;
			G.ShaftAhead = 0.0;
			G.ShaftIn = 0.33;
			G.DeckHalf = Row.DeckHalf;
			G.DeckTaper = Row.DeckTaper;
			G.DeckEdges = Row.Edges;
			G.bDeckEdges = true;
			G.Boat = ConvoyLook::SculptedOf(M.File);
			G.HatchAlong = std::fabs(G.Boat.ForeHatch - M.Along) <= std::fabs(G.Boat.AftHatch - M.Along) ? G.Boat.ForeHatch : G.Boat.AftHatch;
			G.bScuttle = std::fabs(G.HatchAlong - M.Along) <= ScuttleNear;
			return G;
		}
		// [MODEL] the merchants' guns (Content/Models/Ships/Freighter, Tanker, Troopship .json): a gunhouse roofed and open at
		// its back, the barrel's pivot ahead of the mount's middle and its breech inside the gunhouse, the barrel built 5
		// degrees up; its crew on the platform (MountOf's Deck) behind it, inside the tub's wall
		static const struct
		{
			const char* File;
			const char* Part;
			double PivotAhead, PivotUp, Back, Platform, LiftMost;
		} Merchants[] = {
			{ "Freighter", "turret.A", 0.44, 8.8259, 0.92, 2.14, 80.0 },
			{ "Freighter", "turret.Y", 0.56, 9.1422, 1.18, 2.44, 35.0 },
			{ "Tanker", "turret.A", 0.44, 8.3795, 0.92, 2.14, 80.0 },
			{ "Tanker", "turret.Y", 0.56, 8.5267, 1.18, 2.54, 35.0 },
			{ "Troopship", "turret.A", 0.44, 13.5961, 0.92, 2.24, 80.0 },
			{ "Troopship", "turret.Y", 0.60, 12.9327, 1.26, 2.54, 35.0 },
		};
		for (const auto& Row : Merchants)
		{
			if (M.File != Row.File || M.Part != Row.Part)
			{
				continue;
			}
			G.File = M.File;
			G.Part = M.Part;
			G.Foot = M.Deck;
			G.PivotAhead = Row.PivotAhead;
			G.PivotUp = Row.PivotUp;
			G.Built = 5.0;
			G.LiftLeast = -5.0;
			G.LiftMost = Row.LiftMost;
			G.Back = Row.Back;
			G.Platform = Row.Platform;
			// (the wheels just behind its open back, either side of the breech, their shafts into the gunhouse)
			G.HubBack = Row.Back + 0.05;
			G.HubOut = 0.55;
			G.ShaftAhead = 0.12;
			G.ShaftIn = 0.0;
			return G;
		}
		return G;
	}

	Blocks BlocksOf(const SeaSailors::Crew& C, int File)
	{
		using SeaSailors::Act;
		Blocks K;
		if (File < 0 || File >= C.Files())
		{
			return K;
		}
		for (int R = 0; R < Roles; ++R)
		{
			if (!C.Has(File, ActOf(static_cast<Role>(R))))
			{
				return K;
			}
			K.Long[R] = C.Long(File, ActOf(static_cast<Role>(R)));
		}
		const SeaSailors::Wheel* Layer = C.WheelOf(File, Act::Layer);
		const SeaSailors::Wheel* Trainer = C.WheelOf(File, Act::Trainer);
		const SeaSailors::Round* Loading = C.RoundOf(File, Act::Loader);
		const SeaSailors::Round* Passing = C.RoundOf(File, Act::Passer);
		if (!Layer || !Trainer || !Loading || !Passing || !Loading->bBreech || !C.PairOf(File, K.Pair))
		{
			return K;
		}
		K.Wheels[0] = *Layer;
		K.Wheels[1] = *Trainer;
		K.Loading = *Loading;
		K.Passing = *Passing;
		for (const LandMen::ClipEvent& E : C.ClipOf(File, Act::Loader)->Events)
		{
			if (E.Fx == "round_taken")
			{
				K.Taken = E.T;
				K.TakenCue = E.Cue;
			}
			else if (E.Fx == "ram")
			{
				K.Ram = E.T;
				K.RamCue = E.Cue;
			}
			else if (E.Fx == "breech_close")
			{
				K.Shut = E.T;
				K.ShutCue = E.Cue;
			}
		}
		for (const LandMen::ClipEvent& E : C.ClipOf(File, Act::Passer)->Events)
		{
			if (E.Fx == "round_up")
			{
				K.Lift = E.T;
				K.LiftCue = E.Cue;
			}
		}
		// (laid in the tray: the first key after he takes it that has the round along his clip's bore, within 3 degrees)
		K.Laid = K.Ram;
		const float BoreLong = std::sqrt(K.Loading.Bore[0] * K.Loading.Bore[0] + K.Loading.Bore[1] * K.Loading.Bore[1] + K.Loading.Bore[2] * K.Loading.Bore[2]);
		for (size_t I = 0; I + 7 < K.Loading.Track.size() && BoreLong > 1e-6f; I += 8)
		{
			const float* Key = &K.Loading.Track[I];
			const float Along = (Key[5] * K.Loading.Bore[0] + Key[6] * K.Loading.Bore[1] + Key[7] * K.Loading.Bore[2]) / BoreLong;
			if (Key[0] > K.Taken && Key[0] < K.Ram && Key[1] > 0.5f && Along >= std::cos(static_cast<float>(Rad(3.0))))
			{
				K.Laid = Key[0];
				break;
			}
		}
		// (the loader's skin ahead of him through his clip, the near mesh's, one point to a HandsCell cube: his hands,
		// arms and head clear of the breech's ring at his post)
		std::vector<float> SkinAt, SkinFacing;
		std::set<int64_t> Cells;
		for (float T = 0.f; T < K.Long[static_cast<int>(Role::Loader)]; T += HandsEvery)
		{
			const int Count = C.Skin(File, Act::Loader, T, LandMen::Near, SkinAt, SkinFacing);
			for (int I = 0; I < Count; ++I)
			{
				const LandMen::Vec3 At = { SkinAt[static_cast<size_t>(I) * 3], SkinAt[static_cast<size_t>(I) * 3 + 1], SkinAt[static_cast<size_t>(I) * 3 + 2] };
				if (At.X < HandsFrom)
				{
					continue;
				}
				const int64_t Cell = (static_cast<int64_t>(std::floor(At.X / HandsCell)) * 4096 + static_cast<int64_t>(std::floor(At.Y / HandsCell)) + 2048) * 4096 +
				                     static_cast<int64_t>(std::floor(At.Z / HandsCell)) + 2048;
				if (Cells.insert(Cell).second)
				{
					K.Hands.push_back(At);
				}
			}
		}
		K.ClimbLong = C.Long(File, Act::Climb);
		K.RunLong = C.Long(File, Act::Run);
		K.StandLong = C.Long(File, Act::Stand);
		K.DuckLong = C.Long(File, Act::Duck);
		// (his wait, hands out for the next round, must come before he takes it, and his ram after)
		K.bGun = K.Ram > K.Taken && K.Taken > HoldTo && K.Shut >= K.Ram && K.Long[static_cast<int>(Role::Loader)] > K.Shut;
		return K;
	}

	double MountTrain(const ConvoyLook::GunMount& M, double Train)
	{
		const double Off = std::remainder(Train - M.Rest, 360.0);
		return M.Rest + std::clamp(Off, -M.Arc, M.Arc);
	}

	double BarrelLie(const GunShape& G, double Elevation)
	{
		return G.Built + std::clamp(Elevation, G.LiftLeast, G.LiftMost);
	}

	Spot BreechOf(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, double Train, double Elevation, double Way[3])
	{
		const double Bears = MountTrain(M, Train), Lie = Rad(BarrelLie(G, Elevation));
		Spot Out;
		double Ahead = 0.0;
		if (G.bBoat)
		{
			Ahead = G.PivotAhead - G.BreechBack * std::cos(Lie);
			Out.Up = G.PivotUp - G.BreechBack * std::sin(Lie);
		}
		else
		{
			// (a merchant's: its gunhouse's open back, on the bore's line, at the height his clip puts the round in)
			Ahead = -G.Back;
			Out.Up = G.Foot + K.Loading.Breech[2];
		}
		FromBore(Ahead, 0.0, Bears, Out.Fore, Out.Right);
		FromBore(std::cos(Lie), 0.0, Bears, Way[0], Way[1]);
		Way[2] = std::sin(Lie);
		return Out;
	}

	double DeckHalfAt(const GunShape& G, double Fore)
	{
		return DeckHalfWithin(G, Fore, Fore);
	}

	double DeckUnder(const GunShape& G, const ConvoyLook::GunMount& M, double Fore, double Right)
	{
		if (!G.bBoat)
		{
			return G.Foot;
		}
		return std::hypot(Fore, Right) <= G.FootR ? G.Foot : ConvoyLook::CasingAt(G.Boat, M.Along + Fore, Right);
	}

	Spot ToHer(const Post& P, double X, double Y, double Z)
	{
		const double C = std::cos(Rad(P.Facing)), S = std::sin(Rad(P.Facing));
		return { P.Root.Fore + X * C - Y * S, P.Root.Right + X * S + Y * C, P.Root.Up + Z };
	}

	Spot ToHim(const Post& P, const Spot& Her)
	{
		const double C = std::cos(Rad(P.Facing)), S = std::sin(Rad(P.Facing));
		const double DF = Her.Fore - P.Root.Fore, DR = Her.Right - P.Root.Right;
		return { DF * C + DR * S, -DF * S + DR * C, Her.Up - P.Root.Up };
	}

	double OverEdge(const GunShape& G, const Post& P)
	{
		return OverAt(G, P.Root.Fore, P.Root.Right, P.Facing);
	}

	Post LoaderAt(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, double Train, double Elevation, double Turned, double Slid)
	{
		Post P;
		double Way[3];
		const Spot Mouth = BreechOf(G, K, M, Train, Elevation, Way);
		P.Facing = MountTrain(M, Train) + Turned;
		P.Slid = Slid;
		P.Turned = Turned;
		FromBore(K.Loading.Breech[0] - Slid, K.Loading.Breech[1], P.Facing, P.Root.Fore, P.Root.Right);
		P.Root.Fore = Mouth.Fore - P.Root.Fore;
		P.Root.Right = Mouth.Right - P.Root.Right;
		// (still over her edge by more than EdgeMost, or her platform's rim: moved in by as much, across her beam toward her
		// middle line, or toward the mount's middle; his hands that much off the breech, the round eased home along its
		// bore all the same)
		P.Shift = ShiftIn(G, P.Root.Fore, P.Root.Right, P.Facing);
		P.Root.Up = DeckUnder(G, M, P.Root.Fore, P.Root.Right);
		return P;
	}

	Spot HatchOf(const GunShape& G, const ConvoyLook::GunMount& M)
	{
		Spot S;
		S.Fore = G.HatchAlong - M.Along;
		S.Right = 0.0;
		S.Up = DeckUnder(G, M, S.Fore, S.Right);
		return S;
	}

	Post PasserAt(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, const Post& Loading, double& OutGap)
	{
		Post P;
		P.Root = HatchOf(G, M);
		// (the round in his hands just as the loader takes it, and in the loader's as he has it first: the pair's moment)
		SeaSailors::RoundAt Handed, Taken;
		const bool bHanded = SeaSailors::RoundAlong(K.Passing, K.Long[static_cast<int>(Role::Passer)], K.Taken - 0.01f, Handed);
		const bool bTaken = SeaSailors::RoundAlong(K.Loading, K.Long[static_cast<int>(Role::Loader)], TakeAt(K), Taken);
		const Spot Into = ToHer(Loading, Taken.Base.X, Taken.Base.Y, Taken.Base.Z);
		// (turned so his hands reach toward the loader's: in her hatch, his root where it is)
		P.Facing = BearingOf(Into.Fore - P.Root.Fore, Into.Right - P.Root.Right) - BearingOf(Handed.Base.X, Handed.Base.Y);
		const Spot Hands = ToHer(P, Handed.Base.X, Handed.Base.Y, Handed.Base.Z);
		OutGap = bHanded && bTaken ? std::sqrt((Hands.Fore - Into.Fore) * (Hands.Fore - Into.Fore) + (Hands.Right - Into.Right) * (Hands.Right - Into.Right) + (Hands.Up - Into.Up) * (Hands.Up - Into.Up))
		                           : Never;
		return P;
	}

	bool PasserHolds(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, const Post& Loading)
	{
		if (!G.bBoat || !G.bScuttle || !K.bGun)
		{
			return false;
		}
		double Gap = Never;
		PasserAt(G, K, M, Loading, Gap);
		return Gap <= HandOffNear;
	}

	bool PasserEver(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M)
	{
		if (!G.bBoat || !G.bScuttle || !K.bGun)
		{
			return false;
		}
		for (double Turn = -M.Arc; Turn <= M.Arc + 1e-9; Turn += 2.5)
		{
			for (const double Lift : { 0.0, 5.0, 10.0 })
			{
				if (PasserHolds(G, K, M, PostOf(G, K, M, Role::Loader, M.Rest + Turn, Lift)))
				{
					return true;
				}
			}
		}
		return false;
	}

	Post PostOf(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, Role R, double Train, double Elevation, int Prefer)
	{
		if (R == Role::Passer)
		{
			double Gap = Never;
			return PasserAt(G, K, M, PostOf(G, K, M, Role::Loader, Train, Elevation, Prefer), Gap);
		}
		Post P;
		const double Bears = MountTrain(M, Train);
		P.Facing = Bears;
		if (R == Role::Layer || R == Role::Trainer)
		{
			// (his wheel's hub where the gun's is: the layer's at its left, the trainer's at its right)
			const SeaSailors::Wheel& W = K.Wheels[R == Role::Layer ? 0 : 1];
			const double A = -G.HubBack - W.Hub[0];
			const double B = (R == Role::Layer ? -G.HubOut : G.HubOut) - W.Hub[1];
			FromBore(A, B, Bears, P.Root.Fore, P.Root.Right);
			P.Root.Up = DeckUnder(G, M, P.Root.Fore, P.Root.Right);
			return P;
		}
		// (the loader with the chamber's mouth of his clip at the gun's, his feet on her deck)
		double Way[3];
		const Spot Mouth = BreechOf(G, K, M, Train, Elevation, Way);
		auto RootAt = [&](double Facing, double Slid, double& OutFore, double& OutRight)
		{
			FromBore(K.Loading.Breech[0] - Slid, K.Loading.Breech[1], Facing, OutFore, OutRight);
			OutFore = Mouth.Fore - OutFore;
			OutRight = Mouth.Right - OutRight;
		};
		// (the layer's and the trainer's roots, as above)
		double WheelFore[2], WheelRight[2];
		for (int Hub = 0; Hub < 2; ++Hub)
		{
			const SeaSailors::Wheel& W = K.Wheels[Hub];
			FromBore(-G.HubBack - W.Hub[0], (Hub == 0 ? -G.HubOut : G.HubOut) - W.Hub[1], Bears, WheelFore[Hub], WheelRight[Hub]);
		}
		// (his hands clear of the breech's ring through his clip, at a root and facing: HandsOffRing)
		auto HandsClear = [&](double Facing, double RootFore, double RootRight) { return HandsOffRing(G, K, M, Mouth, Way, Facing, RootFore, RootRight); };
		auto Roomy = [&](double RootFore, double RootRight)
		{
			return std::hypot(RootFore - WheelFore[0], RootRight - WheelRight[0]) >= Room && std::hypot(RootFore - WheelFore[1], RootRight - WheelRight[1]) >= RoomRight;
		};
		// (nearer the breech, the least that has him on her deck, at most SlideMost, his hands clear of its ring; where
		// that is not enough, turned about it either way round (beyond TurnNear less near it, by SlideTo not at all), no
		// further than TurnMost nor nearer the layer than Room or the trainer than RoomRight: the least turned that has
		// him on her deck with his hands clear, else the least costly (CostOff); of the two ways round the less costly,
		// the way he is turned now kept unless the other costs SideMargin less)
		auto Edge = [](double Lo, double Hi, const auto& Good)
		{
			// (between Lo, where Good is false, and Hi, where it holds: where it begins to)
			for (int Halving = 0; Halving < 24; ++Halving)
			{
				const double Mid = 0.5 * (Lo + Hi);
				(Good(Mid) ? Hi : Lo) = Mid;
			}
			return Hi;
		};
		auto NearAt = [](double Off) { return SlideMost * std::clamp((SlideTo - std::fabs(Off)) / (SlideTo - TurnNear), 0.0, 1.0); };
		auto OnDeck = [&](double Near, double Off)
		{
			double OnFore = 0.0, OnRight = 0.0;
			RootAt(Bears + Off, Near, OnFore, OnRight);
			return OverAt(G, OnFore, OnRight, Bears + Off) <= 0.0;
		};
		auto Spaced = [&](double Off, double Near)
		{
			// (where he stands moved in, ShiftIn's)
			double OnFore = 0.0, OnRight = 0.0;
			RootAt(Bears + Off, Near, OnFore, OnRight);
			ShiftIn(G, OnFore, OnRight, Bears + Off);
			return Roomy(OnFore, OnRight);
		};
		auto OverOff = [&](double Off, double Near)
		{
			double OnFore = 0.0, OnRight = 0.0;
			RootAt(Bears + Off, Near, OnFore, OnRight);
			return OverAt(G, OnFore, OnRight, Bears + Off);
		};
		auto ClearAt = [&](double Near, double Off)
		{
			double OnFore = 0.0, OnRight = 0.0;
			RootAt(Bears + Off, Near, OnFore, OnRight);
			return HandsClear(Bears + Off, OnFore, OnRight);
		};
		if (OnDeck(0.0, 0.0) && ClearAt(0.0, 0.0))
		{
			return LoaderAt(G, K, M, Train, Elevation, 0.0, 0.0);
		}
		if (OnDeck(SlideMost, 0.0))
		{
			const double Near = Edge(0.0, SlideMost, [&](double X) { return OnDeck(X, 0.0); });
			if (ClearAt(Near, 0.0))
			{
				return LoaderAt(G, K, M, Train, Elevation, 0.0, Near);
			}
		}
		struct Swung
		{
			double Off = 0.0, Near = 0.0, Cost = Never;
		};
		// (what a turn costs him: on her deck, or within EdgeMost of its edge, how far over it; moved in (ShiftIn's) more
		// than any of those: how much nearer the breech than his clip has them that brings his hands (as near as he is
		// slid, and the part of his shift along his way), his hands clear of its face the less it is)
		auto CostOff = [&](double Off, double Near)
		{
			const double Over = OverOff(Off, Near);
			Post Moved;
			Moved.Facing = Bears + Off;
			RootAt(Moved.Facing, Near, Moved.Root.Fore, Moved.Root.Right);
			ShiftIn(G, Moved.Root.Fore, Moved.Root.Right, Moved.Facing);
			const double Into = HandsClear(Moved.Facing, Moved.Root.Fore, Moved.Root.Right) ? 0.0 : HandsInto;
			if (Over <= EdgeMost)
			{
				return Into + std::max(0.0, Over);
			}
			return Into + EdgeMost + K.Loading.Breech[0] - ToHim(Moved, Mouth).Fore;
		};
		auto Search = [&](double Swing)
		{
			// (each TurnStep round, as near the breech as NearAt has him there or less (half that, or not at all: his hands
			// clear of its ring), of those that leave him room by the layer and the trainer: the least turned that has him
			// on her deck with his hands clear of the breech, from where it begins to; else the least costly; none with room,
			// as he stands behind it)
			Swung Found;
			double Prev = -1.0, PrevNear = 0.0;
			for (double To = 0.0; To <= TurnMost + 1e-9; To += TurnStep)
			{
				bool bAny = false;
				for (const double Near : { NearAt(To), 0.5 * NearAt(To), 0.0 })
				{
					if ((Near < NearAt(To) && NearAt(To) <= 0.0) || !Spaced(Swing * To, Near))
					{
						continue;
					}
					bAny = true;
					const double Cost = CostOff(Swing * To, Near);
					if (Cost <= 0.0)
					{
						double At = To;
						if (Prev >= 0.0 && OverOff(Swing * Prev, PrevNear) > 0.0)
						{
							const double OnAt = Edge(Prev, To, [&](double X) { return OverOff(Swing * X, Near) <= 0.0; });
							if (Spaced(Swing * OnAt, Near) && CostOff(Swing * OnAt, Near) <= 0.0)
							{
								At = OnAt;
							}
						}
						Found.Off = Swing * At;
						Found.Near = Near;
						Found.Cost = 0.0;
						return Found;
					}
					if (Cost < Found.Cost)
					{
						Found.Cost = Cost;
						Found.Off = Swing * To;
						Found.Near = Near;
					}
					Prev = To;
					PrevNear = Near;
				}
				if (!bAny)
				{
					Prev = -1.0;
				}
			}
			if (Found.Cost >= Never)
			{
				Found.Off = 0.0;
				Found.Near = NearAt(0.0);
				Found.Cost = CostOff(0.0, Found.Near);
			}
			return Found;
		};
		// (of the two ways round the less costly; the way he is turned now kept unless the other is SideMargin less)
		const Swung Port = Search(-1.0), Starboard = Search(1.0);
		const Swung* Pick = &Starboard;
		if (Port.Cost < Starboard.Cost - 1e-9 || (std::fabs(Port.Cost - Starboard.Cost) <= 1e-9 && std::fabs(Port.Off) < std::fabs(Starboard.Off)))
		{
			Pick = &Port;
		}
		if (Prefer != 0)
		{
			const Swung& Held = Prefer > 0 ? Starboard : Port;
			const Swung& Other = Prefer > 0 ? Port : Starboard;
			if (Held.Cost <= Other.Cost + SideMargin)
			{
				Pick = &Held;
			}
		}
		return LoaderAt(G, K, M, Train, Elevation, Pick->Off, Pick->Near);
	}

	double BreechHigh(const GunShape& G, const Blocks& K, const ConvoyLook::GunMount& M, const Post& Loading, double Train, double Elevation)
	{
		double Way[3];
		const Spot Mouth = BreechOf(G, K, M, Train, Elevation, Way);
		return ToHer(Loading, K.Loading.Breech[0], K.Loading.Breech[1], K.Loading.Breech[2]).Up - Mouth.Up;
	}

	Told TellOf(const SeaShow::ShipOut& Ship)
	{
		Told T;
		T.bBoat = Ship.Class == "ss";
		T.bLaid = Ship.bGunLaid;
		T.bInLine = Ship.Doing == SeaShow::Fate::Line;
		T.bStruck = Ship.Struck >= 0.0;
		T.bAft = Ship.Class == "mv" && Ship.bAftGun;
		T.Dive = T.bBoat ? Ship.Dive : std::string();
		T.Dived = Ship.Dived;
		T.Train = Ship.Train;
		T.Elevation = Ship.Elevation;
		T.FiredAt = Ship.FiredAt;
		T.ShakenAt = Ship.ShakenAt;
		T.Key = Ship.Key;
		return T;
	}

	double FastestReload(const Blocks& K, bool bBoat)
	{
		return K.Long[static_cast<int>(Role::Loader)] / (bBoat ? BoatRate : MerchantRate);
	}

	bool Unrammed(const Watch& W, const Blocks& K)
	{
		return W.Hands[static_cast<int>(Role::Loader)].Now == Phase::AtGun && !W.bWaiting && W.Clip >= K.Taken && W.Clip < K.Shut;
	}

	float LastShown(const Blocks& K)
	{
		float Last = 0.f;
		for (size_t I = 0; I + 7 < K.Loading.Track.size(); I += 8)
		{
			if (K.Loading.Track[I + 1] > 0.5f)
			{
				Last = K.Loading.Track[I];
			}
		}
		return Last;
	}

	bool LoaderRound(const Blocks& K, float Time, bool bOpenGun, const LandMen::Vec3& Mouth, const LandMen::Vec3& Bore, SeaSailors::RoundAt& Out)
	{
		if (!K.bGun || !SeaSailors::RoundAlong(K.Loading, K.Long[static_cast<int>(Role::Loader)], Time, Out) || !Out.bShown)
		{
			return false;
		}
		if (!bOpenGun)
		{
			return true;
		}
		// (no further in than the gun's chamber's mouth; from when he lays it in the tray (Laid) eased onto its bore's line
		// by the time it is home, as far behind the mouth along it as his clip has it behind along his way (turned about
		// it, he rams across it), its nose its way: the gun's tray lies lower than his clip's, and the round settles into it
		// as he slides it in and rams it, never dropping out of his fist at the ram)
		Out.Base.X = std::min(Out.Base.X, Mouth.X);
		const float Last = LastShown(K);
		const float From = std::min(K.Laid, K.Ram);
		const float Span = std::max(1e-3f, Last - From);
		const float U = std::clamp((Time - From) / Span, 0.f, 1.f), W = U * U * (3.f - 2.f * U);
		if (Time < From || Time > K.Long[static_cast<int>(Role::Loader)])
		{
			return true;
		}
		const float Along = Out.Base.X - Mouth.X;
		const LandMen::Vec3 Line = { Mouth.X + Bore.X * Along, Mouth.Y + Bore.Y * Along, Mouth.Z + Bore.Z * Along };
		Out.Base.X += (Line.X - Out.Base.X) * W;
		Out.Base.Y += (Line.Y - Out.Base.Y) * W;
		Out.Base.Z += (Line.Z - Out.Base.Z) * W;
		LandMen::Vec3 Way = { Out.Way.X + (Bore.X - Out.Way.X) * W, Out.Way.Y + (Bore.Y - Out.Way.Y) * W, Out.Way.Z + (Bore.Z - Out.Way.Z) * W };
		const float L = std::sqrt(Way.X * Way.X + Way.Y * Way.Y + Way.Z * Way.Z);
		Out.Way = L > 1e-6f ? LandMen::Vec3{ Way.X / L, Way.Y / L, Way.Z / L } : Bore;
		return true;
	}

	bool RackRound(const Blocks& K, float Time, SeaSailors::RoundAt& Out)
	{
		Out.bShown = false;
		if (!K.bGun || Time < 0.f || Time >= TakeAt(K))
		{
			return false;
		}
		// (where the passer's clip holds it up for him as he takes it, in his axes by their pair)
		SeaSailors::RoundAt Held;
		if (!SeaSailors::RoundAlong(K.Passing, K.Long[static_cast<int>(Role::Passer)], K.Taken - 0.01f, Held) || !Held.bShown)
		{
			return false;
		}
		Out.bShown = true;
		Out.Base = { Held.Base.X - K.Pair.X, Held.Base.Y - K.Pair.Y, Held.Base.Z - K.Pair.Z };
		Out.Way = Held.Way;
		return true;
	}

	void Step(Watch& W, const Told& In, const GunShape& G, const ConvoyLook::GunMount& M, const Blocks& K, double Now, std::vector<Cue>* OutCues)
	{
		if (W.LastT >= 0.0 && Now < W.LastT - 1e-6)
		{
			// (a step back, a replay begun again: afresh)
			W = Watch();
		}
		const double Dt = W.LastT < 0.0 ? 0.0 : std::max(0.0, Now - W.LastT);
		// (a watch begun this step: the round the show last fired is from before it, never one of this step's)
		const bool bFresh = W.LastT < 0.0;
		// (a skip: a step of more than MaxStep, the scene's SkipTo; no cues across it, and each man where he would be by now)
		const bool bSkip = Dt > MaxStep;
		std::vector<Cue>* Heard = OutCues != nullptr && Dt > 0.0 && !bSkip ? OutCues : nullptr;
		W.LastT = Now;
		auto AllBelow = [&W]()
		{
			for (Hand& H : W.Hands)
			{
				H = Hand();
				H.Since = Never;
			}
			W.bManned = false;
			W.SentAt = -1.0;
			W.bWaiting = false;
			W.Pending = 0;
			W.HatchFor = -1;
			W.bHanding = false;
		};
		auto NoneShown = [&W]()
		{
			for (Shown& S : W.Men)
			{
				S = Shown();
			}
			W.bLoaderWas = false;
		};
		if (G.File.empty() || !K.bGun)
		{
			AllBelow();
			NoneShown();
			return;
		}
		// None once she leaves her line (sinking: her men are the show's, over her side); none the moment a boat strikes (her
		// crew on her casing are the show's own from then, SeaShow::Hands, out of her bridge and hatches in that same second:
		// hers are not drawn twice, nor sent down the hatch the show's men come up).
		if (!In.bInLine || In.bStruck)
		{
			AllBelow();
			NoneShown();
			W.SeenFired = In.FiredAt;
			return;
		}
		if (!G.bBoat && W.bManned && In.bAft != W.bAft)
		{
			// (a merchant firing her other gun: each gun its own crew, the old one's standing down as the new one's close up
			// at it, in this same frame, called afresh below)
			AllBelow();
		}
		const double Bears = MountTrain(M, In.Train);
		const int Loader = static_cast<int>(Role::Loader), Passer = static_cast<int>(Role::Passer);
		const bool bAlarm = G.bBoat && (In.Dive != "surfaced" || In.Dived > 1.0);
		if (!W.bManned && In.bLaid && (!G.bBoat || !bAlarm))
		{
			// Called to the gun: a merchant's at it at once; a boat's up her hatch one after another (the layer, the
			// trainer, the loader; the passer last, where he can hand the rounds up to the loader at all), the first at once.
			// (a round the show fires as her gun is told, in this same step, is one her crew see: a hit the battle tells
			// is fired at once)
			const double SeenWas = W.SeenFired;
			W = Watch();
			W.bManned = true;
			W.CalledAt = Now;
			W.bAft = In.bAft;
			W.TrainSent = Bears;
			W.LastT = Now;
			W.SeenFired = bFresh ? In.FiredAt : SeenWas;
			W.bPasser = G.bBoat && PasserEver(G, K, M);
			for (int R = 0; R < Roles; ++R)
			{
				Hand& H = W.Hands[R];
				H = Hand();
				H.Since = Now;
				if (R == Passer && !W.bPasser)
				{
					H.Since = Never;
				}
				else if (!G.bBoat)
				{
					H.Now = Phase::AtGun;
				}
			}
			if (!G.bBoat)
			{
				// (a merchant's gunners closed up at a gun kept loaded: the loader waits with a round rammed and the breech
				// shut, his hands out for the next, so her first round fires from a shut breech)
				W.Clip = 0.0;
				W.bWaiting = true;
				W.HoldWay = 1.0;
				W.Pending = 0;
			}
		}
		if (!W.bManned)
		{
			// (none called, all below: nothing to step, and a ship whose gun is never told costs next to nothing a frame)
			W.SeenFired = std::max(W.SeenFired, In.FiredAt);
			NoneShown();
			return;
		}
		if (bAlarm && W.SentAt < 0.0 && bSkip)
		{
			// (told down across a skip: gone below in the seconds skipped)
			AllBelow();
			NoneShown();
			W.SeenFired = In.FiredAt;
			return;
		}
		// (the loader's post: at the gun he steps round the breech to a new one, as the gun trains, at TurnRate and
		// SlideRate, the way round he is on kept unless the other is SideMargin better; elsewhere he makes for it as it is)
		Hand& Loads = W.Hands[Loader];
		const int Prefer = W.Turned > 1e-6 ? 1 : W.Turned < -1e-6 ? -1 : 0;
		const Post Aim = PostOf(G, K, M, Role::Loader, In.Train, In.Elevation, Prefer);
		if (Loads.Now == Phase::AtGun && Dt > 0.0 && !bSkip)
		{
			const double TurnedWas = W.Turned, SlidWas = W.Slid;
			W.Turned += std::clamp(Aim.Turned - W.Turned, -TurnRate * Dt, TurnRate * Dt);
			W.Slid += std::clamp(Aim.Slid - W.Slid, -SlideRate * Dt, SlideRate * Dt);
			if (W.bLoaderWas)
			{
				// (his root no quicker than SwingReach swung with the train and his own steps: else as far along them as that)
				const double Budget = SwingReach * std::fabs(Rad(std::remainder(Bears - W.BearsWas, 360.0))) + (Rad(TurnRate) * K.Loading.Breech[0] + SlideRate) * Dt;
				const double TurnStepped = W.Turned - TurnedWas, SlidStepped = W.Slid - SlidWas;
				auto MovedBy = [&](double Part)
				{
					const Post Then = LoaderAt(G, K, M, In.Train, In.Elevation, TurnedWas + Part * TurnStepped, SlidWas + Part * SlidStepped);
					return std::hypot(Then.Root.Fore - W.LoaderWas.Fore, Then.Root.Right - W.LoaderWas.Right);
				};
				if (MovedBy(1.0) > Budget)
				{
					double Lo = 0.0, Hi = 1.0;
					for (int Halving = 0; Halving < 12; ++Halving)
					{
						const double Mid = 0.5 * (Lo + Hi);
						(MovedBy(Mid) <= Budget ? Lo : Hi) = Mid;
					}
					W.Turned = TurnedWas + Lo * TurnStepped;
					W.Slid = SlidWas + Lo * SlidStepped;
				}
			}
		}
		else
		{
			W.Turned = Aim.Turned;
			W.Slid = Aim.Slid;
		}
		const Post LoaderNow = LoaderAt(G, K, M, In.Train, In.Elevation, W.Turned, W.Slid);
		// (his hands clear of the breech's ring there: else he stands clear, stepping round behind it)
		double BoreWay[3];
		const Spot MouthNow = BreechOf(G, K, M, In.Train, In.Elevation, BoreWay);
		const bool bLoaderClear = HandsOffRing(G, K, M, MouthNow, BoreWay, LoaderNow.Facing, LoaderNow.Root.Fore, LoaderNow.Root.Right);
		auto PostNow = [&](int Who)
		{
			if (Who == Loader)
			{
				return LoaderNow;
			}
			if (Who == Passer)
			{
				double Apart = Never;
				return PasserAt(G, K, M, LoaderNow, Apart);
			}
			return PostOf(G, K, M, static_cast<Role>(Who), In.Train, In.Elevation);
		};
		// (each man at his post at once: the passer on his rung)
		auto ToPost = [&](int Who)
		{
			Hand& H = W.Hands[Who];
			H = Hand();
			H.Since = Now;
			H.Now = Phase::AtGun;
			H.Height = Who == Passer ? PasserRung : 0.0;
		};
		// A round the show fires. Before her crew are at the gun (a hit the battle tells is fired at once, her first round
		// as soon as 1.5 s after she is laid: they are still coming up her hatch): the layer, the trainer and the loader at
		// their posts at once, as across a skip, the round it fires rammed and the breech shut. The loader with a round in
		// his hands short of the breech shut (from taking it to shutting the breech on it): he has it home and the breech
		// shut as it fires (his clip on to breech_close). Never once she is told down (her crew below), nor across a skip.
		const bool bFired = In.FiredAt > W.SeenFired + 1e-9;
		W.SeenFired = std::max(W.SeenFired, In.FiredAt);
		if (bFired && !bAlarm && W.SentAt < 0.0 && !bSkip)
		{
			bool bLoaderUp = false;
			for (int R = 0; R < Roles; ++R)
			{
				if (R != Passer && W.Hands[R].Now != Phase::AtGun && W.Hands[R].Since < Never)
				{
					ToPost(R);
					bLoaderUp = bLoaderUp || R == Loader;
				}
			}
			if (bLoaderUp)
			{
				W.Clip = 0.0;
				W.bWaiting = true;
				W.HoldWay = 1.0;
				W.Pending = 0;
			}
			else if (Unrammed(W, K))
			{
				W.Clip = K.Shut;
				W.Pending = 0;
			}
		}
		// (the passer up on his rung only while the loader at his post can take the rounds from his hands)
		const bool bPasserWanted = W.bPasser && W.SentAt < 0.0 && !bAlarm && Loads.Now == Phase::AtGun && bLoaderClear && PasserHolds(G, K, M, LoaderNow);
		if (bAlarm && W.SentAt < 0.0)
		{
			// The diving alarm: all below before her casing is awash, each by the way he came (as it lay as she was told
			// down), into the hatch one at a time; a man still climbing up goes down again at once, the first, and the
			// passer from his rung.
			W.SentAt = Now;
			W.TrainSent = Bears;
			W.HatchFor = -1;
			for (int R = 0; R < Roles; ++R)
			{
				Hand& H = W.Hands[R];
				const Post To = PostNow(R);
				switch (H.Now)
				{
				case Phase::Below:
					H.Since = Never;
					break;
				case Phase::ClimbUp:
					H.Now = Phase::ClimbDown;
					H.Since = Now;
					W.HatchFor = R;
					break;
				case Phase::ClimbDown:
					W.HatchFor = R;
					break;
				case Phase::RunUp:
					H.Way = Back(WayUp(G, M, To, Bears), H.Ran);
					H.Now = Phase::RunBack;
					H.Ran = 0.0;
					H.Since = Now;
					break;
				case Phase::AtGun:
					if (R == Passer)
					{
						H.Now = Phase::ClimbDown;
						H.Height = PasserRung;
						W.HatchFor = R;
					}
					else
					{
						const std::vector<double> Up = WayUp(G, M, To, Bears);
						H.Way = Back(Up, LengthOf(Up));
						H.Now = Phase::RunBack;
						H.Ran = 0.0;
					}
					H.Since = Now;
					break;
				default:
					break;
				}
			}
		}
		else if (bSkip && W.SentAt < 0.0)
		{
			// (called and at the gun across a skip: each at his post by now, the passer on his rung where he hands up)
			for (int R = 0; R < Roles; ++R)
			{
				Hand& H = W.Hands[R];
				if (H.Since >= Never)
				{
					continue;
				}
				if (R != Passer || bPasserWanted)
				{
					ToPost(R);
				}
				else
				{
					H = Hand();
					H.Since = Now;
				}
			}
		}
		// (the round fired: the loader, waiting with one rammed, takes the next at once; one fired after he rammed the one
		// he has, he takes the next as soon as he is round again)
		const bool bLoaderAt = Loads.Now == Phase::AtGun && W.SentAt < 0.0;
		if (bFired && bLoaderAt && !bSkip)
		{
			if (W.bWaiting)
			{
				W.bWaiting = false;
			}
			else if (W.Clip >= K.Ram)
			{
				W.Pending = 1;
			}
		}
		// The hatch: one man in it at a time (two men's feet no nearer than HatchApart), going up each once the one before
		// is out of it, going down each once the one before is gone below it, the rest in a line on their way short of it.
		auto InHatch = [&](int Who)
		{
			const Hand& H = W.Hands[Who];
			return (H.Now == Phase::ClimbUp || H.Now == Phase::ClimbDown || (Who == Passer && H.Now == Phase::AtGun)) && H.Height > -HatchApart;
		};
		auto HatchBusy = [&](int Except)
		{
			for (int Q = 0; Q < Roles; ++Q)
			{
				if (Q != Except && InHatch(Q))
				{
					return true;
				}
			}
			return false;
		};
		auto LeftOf = [&W](int Who) { return LengthOf(W.Hands[Who].Way) - W.Hands[Who].Ran; };
		auto InLine = [&W](int Who) { return (W.Hands[Who].Now == Phase::RunBack || W.Hands[Who].Now == Phase::Wait) && W.HatchFor != Who; };
		auto Ahead = [&](int Who)
		{
			int Count = 0;
			for (int Q = 0; Q < Roles; ++Q)
			{
				if (Q != Who && InLine(Q) && (LeftOf(Q) < LeftOf(Who) - 1e-9 || (std::fabs(LeftOf(Q) - LeftOf(Who)) <= 1e-9 && Q < Who)))
				{
					++Count;
				}
			}
			return Count;
		};
		if (W.HatchFor >= 0)
		{
			// (the hatch his who has it until he is HatchApart down it, or gone)
			const Hand& Holds = W.Hands[W.HatchFor];
			const bool bStepping = Holds.Now == Phase::RunBack || Holds.Now == Phase::Wait;
			if (!bStepping && !InHatch(W.HatchFor))
			{
				W.HatchFor = -1;
			}
		}
		if (W.SentAt >= 0.0 && W.HatchFor < 0 && !HatchBusy(-1))
		{
			for (int R = 0; R < Roles && W.HatchFor < 0; ++R)
			{
				if (InLine(R) && Ahead(R) == 0)
				{
					W.HatchFor = R;
				}
			}
		}
		// Each man on his way.
		for (int R = 0; R < Roles; ++R)
		{
			Hand& H = W.Hands[R];
			switch (H.Now)
			{
			case Phase::Below:
			{
				// (up once the man before him is out of the hatch; the passer last, while he can hand up)
				const Hand* Before = R > 0 ? &W.Hands[R - 1] : nullptr;
				const bool bOut = Before == nullptr || (Before->Now == Phase::RunUp && Before->Ran >= OutOf) || Before->Now == Phase::AtGun;
				const bool bWanted = R == Passer ? bPasserWanted : bOut;
				if (W.SentAt < 0.0 && Now >= H.Since && bWanted && !HatchBusy(R))
				{
					H.Now = Phase::ClimbUp;
					H.Height = R == Passer ? -PasserDepth : -Depth;
					H.Since = Now;
				}
				break;
			}
			case Phase::ClimbUp:
				H.Height += ClimbUpRate * Dt;
				if (H.Height >= (R == Passer ? PasserRung : 0.0))
				{
					H.Height = R == Passer ? PasserRung : 0.0;
					H.Now = R == Passer ? Phase::AtGun : Phase::RunUp;
					H.Ran = 0.0;
					H.Since = Now;
				}
				break;
			case Phase::RunUp:
				H.Ran += RunRate * Dt;
				if (H.Ran >= LengthOf(WayUp(G, M, PostNow(R), Bears)))
				{
					H.Now = Phase::AtGun;
					H.Since = Now;
					if (R == Loader)
					{
						// (the round he brought up: loaded first)
						W.Clip = 0.0;
						W.bWaiting = false;
						W.Pending = 0;
					}
				}
				break;
			case Phase::AtGun:
				if (R == Passer && !bPasserWanted && W.SentAt < 0.0)
				{
					// (the loader gone from where he can take the rounds from him: down below his rung until he can again)
					H.Now = Phase::ClimbDown;
					H.Since = Now;
				}
				break;
			case Phase::RunBack:
			case Phase::Wait:
			{
				// (to the hatch where it is his; else to his place in the line short of it, the nearest first, and there
				// waiting)
				const double Long = LengthOf(H.Way);
				const double Stop = W.HatchFor == R ? Long : StopShort(H.Way, WaitShort + WaitApart * Ahead(R));
				if (H.Ran < Stop - 1e-9)
				{
					H.Ran = std::min(Stop, H.Ran + RunRate * Dt);
					if (H.Now != Phase::RunBack)
					{
						H.Now = Phase::RunBack;
						H.Since = Now;
					}
				}
				if (W.HatchFor == R && H.Ran >= Long - 1e-9)
				{
					H.Now = Phase::ClimbDown;
					H.Height = 0.0;
					H.Since = Now;
				}
				else if (H.Ran >= Stop - 1e-9 && H.Now != Phase::Wait)
				{
					H.Now = Phase::Wait;
					H.Since = Now;
				}
				break;
			}
			case Phase::ClimbDown:
				H.Height -= ClimbDownRate * Dt;
				if (H.Height <= (R == Passer ? -PasserDepth : -Depth))
				{
					// (gone below; the passer, below his rung while he cannot hand up, may come up again)
					const bool bAgain = R == Passer && W.SentAt < 0.0;
					H = Hand();
					H.Since = bAgain ? Now : Never;
				}
				break;
			default:
				break;
			}
		}
		// (the passer hands the round up now: on his rung, the loader at his post where it meets his hands)
		W.bHanding = bPasserWanted && W.Hands[Passer].Now == Phase::AtGun;
		// A shell close aboard: the layer and the trainer crouch a moment, each as he hears it; the loader and the passer
		// work on (the gun fires when the show fires it: the round it fires is loaded).
		auto Ducks = [&](int R)
		{
			if (R == Passer || R == Loader || In.ShakenAt < 0.0 || W.Hands[R].Now != Phase::AtGun)
			{
				return -1.0;
			}
			const double From = In.ShakenAt + Hash01(In.Key, R + 11) * ConvoyLook::DuckLate;
			return Now >= From && Now < From + ConvoyLook::DuckFor ? From : -1.0;
		};
		// The loader's work: on at his clip's pace (a boat's gun quicker) through taking the round (from the passer's hands,
		// or the rack), laying and ramming it, round again; then waiting, his hands out for the next, until the gun fires.
		// Across a skip he starts afresh, waiting for the next round (none of his cues heard).
		if (Loads.Now == Phase::AtGun && W.SentAt < 0.0)
		{
			const double Rate = G.bBoat ? BoatRate : MerchantRate, Long = K.Long[Loader];
			if (bSkip || Long <= 0.0)
			{
				W.Clip = 0.0;
				W.bWaiting = true;
				W.HoldWay = 1.0;
				W.Pending = 0;
			}
			else if (W.bWaiting)
			{
				W.Clip += W.HoldWay * Hold * Rate * Dt;
				if (W.Clip >= HoldTo)
				{
					W.Clip = 2.0 * HoldTo - W.Clip;
					W.HoldWay = -1.0;
				}
				if (W.Clip <= 0.0)
				{
					W.Clip = -W.Clip;
					W.HoldWay = 1.0;
				}
				W.Clip = std::clamp(W.Clip, 0.0, HoldTo);
			}
			else
			{
				const double From = W.Clip;
				double To = From + Rate * Dt;
				Cues(K, From, std::min(To, Long), W.bHanding, Heard);
				if (To >= Long)
				{
					To = std::fmod(To, Long);
					Cues(K, -1.0, To, W.bHanding, Heard);
					if (W.Pending > 0)
					{
						W.Pending = 0;
					}
					else if (To <= HoldTo)
					{
						W.bWaiting = true;
						W.HoldWay = 1.0;
					}
				}
				W.Clip = To;
			}
		}
		// How each is shown.
		const Spot Hatch = G.bBoat ? HatchOf(G, M) : Spot();
		for (int R = 0; R < Roles; ++R)
		{
			const Hand& H = W.Hands[R];
			Shown& S = W.Men[R];
			S = Shown();
			const Post To = PostNow(R);
			switch (H.Now)
			{
			case Phase::ClimbUp:
			case Phase::ClimbDown:
			{
				// (up or down the hatch hand over hand, the net's clip in time with how far he has climbed: back as he goes
				// down; facing the gun from the hatch, the passer the way he hands up)
				S.bShown = true;
				S.What = SeaSailors::Act::Climb;
				S.Time = static_cast<float>(Wrap((H.Height + PasserDepth) / 0.45 * K.ClimbLong, K.ClimbLong));
				if (R == Passer)
				{
					S.At = To;
					S.At.Root.Up = Hatch.Up + H.Height;
				}
				else
				{
					S.At.Root = { Hatch.Fore, Hatch.Right, Hatch.Up + H.Height };
					S.At.Facing = BearingOf(-Hatch.Fore, -Hatch.Right);
				}
				break;
			}
			case Phase::RunUp:
			case Phase::RunBack:
			case Phase::Wait:
			{
				// (running round the mount to his post or back to the hatch; waiting in the line short of it, facing it)
				const std::vector<double> Way = H.Now == Phase::RunUp ? WayUp(G, M, To, Bears) : H.Way;
				double Fore = 0.0, Right = 0.0, Facing = 0.0;
				Along(Way, H.Ran, Fore, Right, Facing);
				S.bShown = true;
				S.What = H.Now == Phase::Wait ? SeaSailors::Act::Stand : SeaSailors::Act::Run;
				S.Time = static_cast<float>(Wrap(Now - H.Since, H.Now == Phase::Wait ? K.StandLong : K.RunLong));
				S.At.Root = { Fore, Right, DeckUnder(G, M, Fore, Right) };
				S.At.Facing = Facing;
				break;
			}
			case Phase::AtGun:
			{
				S.bShown = true;
				S.At = To;
				const double Duck = Ducks(R);
				if (Duck >= 0.0)
				{
					S.What = SeaSailors::Act::Duck;
					S.Time = static_cast<float>(std::min(Now - Duck, static_cast<double>(K.DuckLong)));
					break;
				}
				if (R == Loader && !bLoaderClear)
				{
					// (stepping round behind the breech: standing clear of it, his clip's time running on)
					S.What = SeaSailors::Act::Stand;
					S.Time = static_cast<float>(Wrap(Now - H.Since, K.StandLong));
					break;
				}
				S.bAtWork = true;
				S.What = ActOf(static_cast<Role>(R));
				if (R == Loader || R == Passer)
				{
					// (the passer in step with the loader: their pair's clips played from the same moment)
					S.Time = static_cast<float>(W.Clip);
				}
				else
				{
					S.Time = static_cast<float>(Wrap(Now + Hash01(In.Key, R + 3) * K.Long[R], K.Long[R]));
				}
				break;
			}
			default:
				break;
			}
		}
		W.bLoaderWas = W.Hands[Loader].Now == Phase::AtGun && W.Men[Loader].bShown;
		W.LoaderWas = W.Men[Loader].At.Root;
		W.BearsWas = Bears;
		// (all below again after the alarm: called afresh when she is up and her gun told again)
		if (W.SentAt >= 0.0)
		{
			bool bAny = false;
			for (const Hand& H : W.Hands)
			{
				bAny = bAny || H.Now != Phase::Below;
			}
			if (!bAny)
			{
				W.bManned = false;
				W.SentAt = -1.0;
				W.HatchFor = -1;
			}
		}
	}
}
