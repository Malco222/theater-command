#include "Battle/LandHulk.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace LandHulk
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Rad = Pi / 180.0;

		using LandKit::Quat;
		using LandKit::V3;
		using LandField::Rgb8;

		/** A small steady random source (splitmix64), the wrecks' own: seeded on its own constants, so no draw of the
		 * battle's, the scars' or the ruts' is moved by it. */
		struct Dice
		{
			uint64_t State = 0;
			explicit Dice(uint64_t Seed) : State(Seed * 0xD1B54A32D192ED03ull + 0x632BE59BD9B4E019ull) {}
			double U()
			{
				State += 0x9E3779B97F4A7C15ull;
				uint64_t Z = State;
				Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
				Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
				Z ^= Z >> 31;
				return static_cast<double>(Z >> 11) * (1.0 / 9007199254740992.0);
			}
			double U(double A, double B) { return A + (B - A) * U(); }
			/** About 0, mostly within -1 to 1 (the sum of three throws). */
			double Bell() { const double A = U(), B = U(), C = U(); return A + B + C - 1.5; }
		};

		uint64_t Mix(uint64_t A, uint64_t B)
		{
			uint64_t Z = A ^ (B + 0x9E3779B97F4A7C15ull + (A << 6) + (A >> 2));
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}

		double Clamp01(double X) { return X < 0.0 ? 0.0 : X > 1.0 ? 1.0 : X; }
		double Smooth(double E0, double E1, double X)
		{
			const double T = Clamp01((X - E0) / (E1 - E0));
			return T * T * (3.0 - 2.0 * T);
		}

		/** A colour as three numbers (8-bit sRGB, mixed as the game's soot mixes them). */
		struct Tone
		{
			double R = 0.0, G = 0.0, B = 0.0;
			Tone() = default;
			Tone(double InR, double InG, double InB) : R(InR), G(InG), B(InB) {}
		};
		Tone Mixed(const Tone& A, const Tone& B, double T)
		{
			T = Clamp01(T);
			return Tone(A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T);
		}
		uint8_t Byte(double V) { return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(V)), 0, 255)); }
		Rgb8 ToRgb(const Tone& T)
		{
			Rgb8 C;
			C.R = Byte(T.R);
			C.G = Byte(T.G);
			C.B = Byte(T.B);
			return C;
		}

		/** Value noise over space, 0 to 1, smooth at the scale of a unit. */
		double Hash3(int64_t X, int64_t Y, int64_t Z, uint64_t S)
		{
			return static_cast<double>(Mix(Mix(Mix(static_cast<uint64_t>(X), static_cast<uint64_t>(Y)), static_cast<uint64_t>(Z)), S) >> 11) * (1.0 / 9007199254740992.0);
		}
		double Noise3(double X, double Y, double Z, uint64_t S)
		{
			const double FX = std::floor(X), FY = std::floor(Y), FZ = std::floor(Z);
			const int64_t IX = static_cast<int64_t>(FX), IY = static_cast<int64_t>(FY), IZ = static_cast<int64_t>(FZ);
			const double U = Smooth(0.0, 1.0, X - FX), V = Smooth(0.0, 1.0, Y - FY), W = Smooth(0.0, 1.0, Z - FZ);
			double Plane[2];
			for (int K = 0; K < 2; ++K)
			{
				const double A = Hash3(IX, IY, IZ + K, S), B = Hash3(IX + 1, IY, IZ + K, S);
				const double C = Hash3(IX, IY + 1, IZ + K, S), D = Hash3(IX + 1, IY + 1, IZ + K, S);
				Plane[K] = (A + (B - A) * U) * (1.0 - V) + (C + (D - C) * U) * V;
			}
			return Plane[0] + (Plane[1] - Plane[0]) * W;
		}

		/** The field's x east, y north from a turn's frame (x north, y east): the same swap both ways (LandKit's). */
		V3 ToField(const V3& P) { return V3(P.Y, P.X, P.Z); }

		/** A heading's ways on the field (0 north, clockwise): forward, and to her right. */
		void Ways(double Heading, double& Fx, double& Fy, double& Rx, double& Ry)
		{
			Fx = std::sin(Heading);
			Fy = std::cos(Heading);
			Rx = std::cos(Heading);
			Ry = -std::sin(Heading);
		}

		/** The scorched patch's make (Patch): its rings, as shares of its reach, its middle first, and its spokes. */
		constexpr int PatchSpokes = 28, PatchRings = 7;
		constexpr double PatchRing[PatchRings] = { 0.0, 0.2, 0.4, 0.6, 0.78, 0.9, 1.0 };

		/** A spoke's reach, as a share of the patch's size (0.78 to 1.12: a ragged edge of her own). */
		double SpokeReach(const Hulk& H, int S)
		{
			const double Ang = 2.0 * Pi * S / PatchSpokes;
			return 0.78 + 0.34 * Noise3(std::cos(Ang) * 1.6, std::sin(Ang) * 1.6, 0.5, H.Seed ^ 0xE5ull);
		}

		/** A point of her patch on the field: ring Ri, spoke S (its reach Reach), her ways Fx, Fy, Rx, Ry. */
		void PatchAt(const Hulk& H, double Fx, double Fy, double Rx, double Ry, int Ri, int S, double Reach, double& OutX, double& OutY)
		{
			const double Ang = 2.0 * Pi * S / PatchSpokes;
			const double F = PatchRing[Ri] * Reach;
			const double A = std::cos(Ang) * F * H.PatchLong, B = std::sin(Ang) * F * H.PatchWide;
			OutX = H.X + Fx * A + Rx * B;
			OutY = H.Y + Fy * A + Ry * B;
		}

		/** The grass kept this far off her hull's footprint (Covers: a blade standing against her side, not through it). */
		constexpr double FootMargin = 0.15;

		/** How far a place on the field lies from her hull's footprint (0 on it). */
		double FromFoot(const Hulk& H, double X, double Y)
		{
			double Fx, Fy, Rx, Ry;
			Ways(H.Heading, Fx, Fy, Rx, Ry);
			const double A = (X - H.X) * Fx + (Y - H.Y) * Fy, B = (X - H.X) * Rx + (Y - H.Y) * Ry;
			return std::hypot(std::max({ H.FootLoX - A, 0.0, A - H.FootHiX }), std::max({ H.FootLoY - B, 0.0, B - H.FootHiY }));
		}

		/** Places on the field in squares, for asking what lies near. */
		class Squares
		{
		public:
			explicit Squares(double InCell) : Cell(InCell) {}
			void Add(double X, double Y, double R = 0.0)
			{
				if (!(X == X) || !(Y == Y)) return;
				Of[KeyOf(Square(X), Square(Y))].push_back({ X, Y, R });
				MostR = std::max(MostR, R);
				++Count;
			}
			/** A line's points every Step metres along it (and its corners). */
			void AddLine(const std::vector<std::pair<double, double>>& L, double Step)
			{
				for (size_t K = 0; K + 1 < L.size(); ++K)
				{
					const double Dx = L[K + 1].first - L[K].first, Dy = L[K + 1].second - L[K].second, Long = std::hypot(Dx, Dy);
					if (!(Long < 1e5)) continue;
					const int Steps = std::max(1, static_cast<int>(std::ceil(Long / Step)));
					for (int I = 0; I < Steps; ++I)
					{
						const double T = static_cast<double>(I) / Steps;
						Add(L[K].first + Dx * T, L[K].second + Dy * T);
					}
				}
				if (!L.empty()) Add(L.back().first, L.back().second);
			}
			/** Whether any lies within Reach of (X, Y), its own reach added. */
			bool Near(double X, double Y, double Reach) const
			{
				if (Count == 0) return false;
				const double Far = Reach + MostR;
				const int I0 = Square(X - Far), I1 = Square(X + Far), J0 = Square(Y - Far), J1 = Square(Y + Far);
				for (int J = J0; J <= J1; ++J)
				{
					for (int I = I0; I <= I1; ++I)
					{
						const auto Found = Of.find(KeyOf(I, J));
						if (Found == Of.end()) continue;
						for (const Ring& S : Found->second)
						{
							const double D = Reach + S.R;
							if ((S.X - X) * (S.X - X) + (S.Y - Y) * (S.Y - Y) < D * D) return true;
						}
					}
				}
				return false;
			}
			/** The nearest within Reach (its own reach taken off), or Reach where none. */
			double Nearest(double X, double Y, double Reach) const
			{
				double Best = Reach;
				if (Count == 0) return Best;
				const double Far = Reach + MostR;
				const int I0 = Square(X - Far), I1 = Square(X + Far), J0 = Square(Y - Far), J1 = Square(Y + Far);
				for (int J = J0; J <= J1; ++J)
				{
					for (int I = I0; I <= I1; ++I)
					{
						const auto Found = Of.find(KeyOf(I, J));
						if (Found == Of.end()) continue;
						for (const Ring& S : Found->second)
						{
							Best = std::min(Best, std::hypot(S.X - X, S.Y - Y) - S.R);
						}
					}
				}
				return Best;
			}
			size_t Size() const { return Count; }

		private:
			double Cell = 8.0, MostR = 0.0;
			size_t Count = 0;
			std::unordered_map<int64_t, std::vector<Ring>> Of;
			int Square(double V) const { return static_cast<int>(std::floor(V / Cell)); }
			static int64_t KeyOf(int I, int J) { return (static_cast<int64_t>(I) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(J)); }
		};

		bool Contains(const std::string& Text, const char* Part) { return Text.find(Part) != std::string::npos; }

		/**
		 * What a model gives a wreck, worked out once a model: the bodies left off (her aerial and roof gun, burnt or
		 * taken), those that go with a thrown turret, her tracks' links laid out flat, her tyres, and what opens.
		 */
		struct Kit
		{
			std::vector<uint8_t> Drop, OnTurret, OnGun;
			/** Her turret may be thrown (a turret turning about her up, a body of its own of some size). */
			bool bTurret = false;
			V3 TurretPivot;
			double TurretR = 1.0, Muzzle = 0.0;
			V3 MuzzleAt;
			bool bMuzzle = false;
			/** A track side's links: each near point's link (-1 none), each link's middle and its way round the run and
			 * out from it; the links' depth (out from the run: BMin to BMax) and width; a link's pitch. */
			struct Track
			{
				bool bOk = false;
				std::vector<int> LinkOf;
				std::vector<V3> Mid, Along, Out;
				double BMin = 0.0, BMax = 0.0, DMin = 0.0, DMax = 0.0, Pitch = 0.15;
			};
			Track Tracks[2];
			/** Per body: 0 no tyre, 1 a road wheel's solid rubber ring (burnt, kept), 2 a lorry's tyre (burnt off to its
			 * rim); the wheel's part, its radius and its rim's. */
			std::vector<uint8_t> Tyre;
			std::vector<double> TyreR, RimR;
			/** How far her front and back sink with their tyres burnt off. */
			double DropFront = 0.0, DropBack = 0.0;
			/** What opens (hatches, doors, the tailboard), and each one's lift part (-1 none). */
			std::vector<int> Opens, Lifts;
			double HalfLong = 3.0, HalfWide = 1.5, MidX = 0.0;
		};

		/** Whether a part turns with Root (Root itself, or one hung under it). */
		bool Under(const LandKit::Model& M, int Part, int Root)
		{
			for (int P = Part; Root >= 0 && P >= 0 && P < static_cast<int>(M.Parts.size()); P = M.Parts[static_cast<size_t>(P)].Parent)
			{
				if (P == Root) return true;
			}
			return false;
		}

		Kit KitOf(const LandKit::Model& M)
		{
			Kit K;
			const size_t Bodies = M.Bodies.size();
			K.Drop.assign(Bodies, 0);
			K.OnTurret.assign(Bodies, 0);
			K.OnGun.assign(Bodies, 0);
			K.Tyre.assign(Bodies, 0);
			K.TyreR.assign(Bodies, 0.0);
			K.RimR.assign(Bodies, 0.0);
			K.HalfLong = std::max(0.5, 0.5 * (M.HullHi.X - M.HullLo.X));
			K.HalfWide = std::max(0.5, 0.5 * (M.HullHi.Y - M.HullLo.Y));
			K.MidX = 0.5 * (M.HullHi.X + M.HullLo.X);
			size_t TurretPoints = 0;
			for (size_t B = 0; B < Bodies; ++B)
			{
				const LandKit::Body& Bo = M.Bodies[B];
				// (her aerial burnt off or snapped, her roof gun and its cradle and a carrier's gun mount burnt or taken)
				if (Bo.Name == "aerial" || Contains(Bo.Name, "roof_mg") || Bo.Name == "mg_mount") K.Drop[B] = 1;
				if (Under(M, Bo.Part, M.Turret))
				{
					K.OnTurret[B] = 1;
					if (Bo.Part == M.Turret) TurretPoints = Bo.Near.Count();
				}
				if (Under(M, Bo.Part, M.Gun)) K.OnGun[B] = 1;
				const LandKit::Part& P = M.Parts[static_cast<size_t>(Bo.Part)];
				if (P.Radius > 0.12 && std::find(M.Wheels.begin(), M.Wheels.end(), Bo.Part) != M.Wheels.end())
				{
					const bool bPneumatic = Contains(P.Name, "wheel_front") || Contains(P.Name, "wheel_back");
					if (bPneumatic || P.Name.rfind("wheel_", 0) == 0)
					{
						K.Tyre[B] = bPneumatic ? 2 : 1;
						K.TyreR[B] = P.Radius;
						K.RimR[B] = bPneumatic ? std::max(0.55 * P.Radius, P.Radius - 0.2) : 0.82 * P.Radius;
						if (bPneumatic)
						{
							(P.Pivot.X > K.MidX ? K.DropFront : K.DropBack) = P.Radius - K.RimR[B];
						}
					}
				}
			}
			if (M.Turret >= 0)
			{
				const LandKit::Part& T = M.Parts[static_cast<size_t>(M.Turret)];
				K.TurretPivot = T.Pivot;
				// (a turret proper: one that turns about her up, not the StuG's gun mount in its casemate)
				K.bTurret = T.Kind == LandKit::PartKind::Turn && std::fabs(T.Axis.Z) > 0.9 && !T.bLimits && TurretPoints >= 1000;
				K.TurretR = 0.5 * std::hypot(M.TurretHi.X - M.TurretLo.X, M.TurretHi.Y - M.TurretLo.Y) + 0.2;
			}
			const int Muzzle = M.SocketIndex("muzzle");
			if (Muzzle >= 0)
			{
				K.bMuzzle = true;
				K.MuzzleAt = M.Sockets[static_cast<size_t>(Muzzle)].At;
				K.Muzzle = std::hypot(K.MuzzleAt.X - K.TurretPivot.X, K.MuzzleAt.Y - K.TurretPivot.Y);
			}
			// What opens: her hatches (the model's own list), then her doors and a lorry's tailboard; a lid's lift.
			for (const int H : M.Hatches) K.Opens.push_back(H);
			for (size_t P = 0; P < M.Parts.size(); ++P)
			{
				const LandKit::Part& Pt = M.Parts[P];
				if (Pt.Kind == LandKit::PartKind::Turn && Pt.bLimits && Pt.Hi > 0.0 && (Pt.Name.rfind("door", 0) == 0 || Pt.Name == "tailboard")
					&& std::find(K.Opens.begin(), K.Opens.end(), static_cast<int>(P)) == K.Opens.end())
				{
					K.Opens.push_back(static_cast<int>(P));
				}
			}
			for (const int O : K.Opens)
			{
				K.Lifts.push_back(M.PartIndex(M.Parts[static_cast<size_t>(O)].Name + "_lift"));
			}
			// Her tracks' links as they run round her wheels (the track body's near level at rest): each link's middle,
			// its way along the run and out of it (from the run's own middle), for laying a run-off track out flat.
			for (int Side = 0; Side < 2; ++Side)
			{
				const int TB = M.TrackBody[Side];
				if (TB < 0) continue;
				const LandKit::Mesh& Me = M.Bodies[static_cast<size_t>(TB)].Near;
				Kit::Track& Tr = K.Tracks[Side];
				const size_t Links = Me.LinkFirst.size();
				if (Links < 8 || Me.LinkCount.size() != Links) continue;
				Tr.LinkOf.assign(Me.Count(), -1);
				Tr.Mid.assign(Links, V3());
				V3 Middle;
				for (size_t L = 0; L < Links; ++L)
				{
					V3 Sum;
					const uint32_t First = Me.LinkFirst[L], Many = Me.LinkCount[L];
					for (uint32_t I = First; I < First + Many && I < Me.Count(); ++I)
					{
						Tr.LinkOf[I] = static_cast<int>(L);
						Sum += V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]);
					}
					Tr.Mid[L] = Many > 0 ? Sum * (1.0 / Many) : V3();
					Middle += Tr.Mid[L];
				}
				Middle = Middle * (1.0 / static_cast<double>(Links));
				Tr.Along.assign(Links, V3());
				Tr.Out.assign(Links, V3());
				double Pitch = 0.0;
				for (size_t L = 0; L < Links; ++L)
				{
					const V3& Next = Tr.Mid[(L + 1) % Links];
					const V3& Before = Tr.Mid[(L + Links - 1) % Links];
					const V3 Along = (Next - Before).Unit(V3(1.0, 0.0, 0.0));
					V3 Out = Along.Cross(V3(0.0, 1.0, 0.0)).Unit();
					if (Out.Dot(Tr.Mid[L] - Middle) < 0.0) Out = -Out;
					Tr.Along[L] = Along;
					Tr.Out[L] = Out;
					Pitch += (Next - Tr.Mid[L]).Length();
				}
				Tr.Pitch = std::clamp(Pitch / static_cast<double>(Links), 0.08, 0.4);
				Tr.BMin = 1e9;
				Tr.BMax = -1e9;
				Tr.DMin = 1e9;
				Tr.DMax = -1e9;
				for (size_t I = 0; I < Me.Count(); ++I)
				{
					const int L = Tr.LinkOf[I];
					if (L < 0) continue;
					const V3 D = V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]) - Tr.Mid[static_cast<size_t>(L)];
					const double Bo = D.Dot(Tr.Out[static_cast<size_t>(L)]), De = D.Y;
					Tr.BMin = std::min(Tr.BMin, Bo);
					Tr.BMax = std::max(Tr.BMax, Bo);
					Tr.DMin = std::min(Tr.DMin, De);
					Tr.DMax = std::max(Tr.DMax, De);
				}
				Tr.bOk = Tr.BMax > Tr.BMin && Tr.DMax > Tr.DMin;
			}
			return K;
		}

		/** The battle's month and the wrecks' (Months before it). */
		void MonthsBefore(int Year, int Month, double Months, int& OutYear, int& OutMonth)
		{
			const int Total = Year * 12 + (std::clamp(Month, 1, 12) - 1) - static_cast<int>(std::lround(std::max(0.0, Months)));
			OutYear = Total / 12;
			OutMonth = Total % 12 + 1;
		}

		/** The turn of a wreck's hull on the field (in Unreal's axes: x north, y east, z up), from her heading and how she
		 * rests: her heading about her up, her nose up by Pitch, her right side up by Roll. */
		Quat HullTurn(double Heading, double Pitch, double Roll)
		{
			return (Quat::Axis(V3(0.0, 0.0, 1.0), Heading) * Quat::Axis(V3(0.0, 1.0, 0.0), -Pitch) * Quat::Axis(V3(1.0, 0.0, 0.0), Roll)).Unit();
		}

		/** A thrown turret's turn: how it lies (on its roof, on its side, tilted), turned on the ground by its yaw, and
		 * tilted with the ground under it (Up). */
		Quat TurretTurn(const Hulk& H, const V3& Up)
		{
			Quat Lay;
			if (H.TurretLie == Lie::Upturned) Lay = Quat::Axis(V3(1.0, 0.0, 0.0), Pi + H.TurretTilt);
			else if (H.TurretLie == Lie::OnSide) Lay = Quat::Axis(V3(1.0, 0.0, 0.0), 0.5 * Pi + H.TurretTilt);
			else Lay = Quat::Axis(V3(std::cos(H.TurretTiltWay), std::sin(H.TurretTiltWay), 0.0), H.TurretTilt);
			// (the ground's up, in Unreal's axes, from vertical: a small turn that stands vertical on it)
			const V3 UpU(Up.Y, Up.X, Up.Z);
			const V3 Axis = V3(0.0, 0.0, 1.0).Cross(UpU);
			const double S = Axis.Length();
			const Quat Tilt = S > 1e-6 ? Quat::Axis(Axis * (1.0 / S), std::atan2(S, UpU.Z)) : Quat();
			return (Tilt * Quat::Axis(V3(0.0, 0.0, 1.0), H.TurretYaw) * Lay).Unit();
		}

		/** A hatch's, door's or tailboard's open share (0.75 to 1 of its swing), by its own place in the list. */
		double OpenShare(const Hulk& H, size_t I)
		{
			return 0.72 + 0.28 * static_cast<double>(Mix(H.Seed, 0x0BE4ull + I) >> 11) * (1.0 / 9007199254740992.0);
		}

		/** Each part's pose for a wreck: her turret turned, her gun drooped, what opens open. */
		void PoseOf(const LandKit::Model& M, const Kit& K, const Hulk& H, std::vector<LandKit::PartState>& S)
		{
			S.assign(M.Parts.size(), LandKit::PartState());
			if (M.Turret >= 0) S[static_cast<size_t>(M.Turret)].Turn = H.Turret;
			if (M.Gun >= 0) S[static_cast<size_t>(M.Gun)].Turn = H.Gun;
			for (size_t I = 0; I < K.Opens.size() && I < 32; ++I)
			{
				if (!((H.Open >> I) & 1u)) continue;
				// (a thrown turret's hatches and doors slammed shut as it went up or jammed as it fell: open, one lying on
				// its side or its roof would stand on a door or a lid, its steel a metre over the grass)
				if (H.bTurretOff && Under(M, K.Opens[I], M.Turret)) continue;
				const LandKit::Part& P = M.Parts[static_cast<size_t>(K.Opens[I])];
				S[static_cast<size_t>(K.Opens[I])].Turn = (P.bLimits && P.Hi > 0.0 ? P.Hi : 90.0) * OpenShare(H, I);
				if (K.Lifts[I] >= 0)
				{
					S[static_cast<size_t>(K.Lifts[I])].Slide = M.Parts[static_cast<size_t>(K.Lifts[I])].SlideHi;
				}
			}
		}

		/** A wheel's point with its tyre burnt off: pulled in to its rim (a lorry's), from its axle. */
		V3 ToRim(const LandKit::Model& M, const LandKit::Body& Bo, const V3& Local, double Rim)
		{
			const LandKit::Part& P = M.Parts[static_cast<size_t>(Bo.Part)];
			const V3 Q = Bo.Pivot + Local - P.Pivot;
			const V3 Axis = P.Axis.Unit(V3(0.0, 1.0, 0.0));
			const V3 Across = Axis * Q.Dot(Axis);
			const V3 Radial = Q - Across;
			const double R = Radial.Length();
			if (R <= Rim || R < 1e-6) return Local;
			return Across + Radial * (Rim / R) + P.Pivot - Bo.Pivot;
		}

		/** A wheel point's distance from its axle, as a share of its radius (0 where not a wheel). */
		double OnTyre(const LandKit::Model& M, const LandKit::Body& Bo, const V3& Local, double Radius)
		{
			const LandKit::Part& P = M.Parts[static_cast<size_t>(Bo.Part)];
			const V3 Q = Bo.Pivot + Local - P.Pivot;
			const V3 Axis = P.Axis.Unit(V3(0.0, 1.0, 0.0));
			return Radius > 0.0 ? (Q - Axis * Q.Dot(Axis)).Length() / Radius : 0.0;
		}

		enum class Stuff : uint8_t { Steel, Rubber, Link, Rim };

		/**
		 * The burnt-out look of one point (the art director's to judge; the proposal, 24 November): Paint her own coat
		 * (LandKit's, worn and dusty as she fought), Soot how far her own weight says the fire blackens it, At her point at
		 * rest (for the patches, model metres), Up its way up as she lies (its z), High its height over her tracks at rest.
		 * - Where the fire was hottest (her soot weight times how fiercely she burnt, broken up in patches a few hand
		 *   spans across) the paint is gone to bare steel; elsewhere it is scorched toward her burnt colour and blistered,
		 *   and flakes off over two years, the bare steel spreading down her sides.
		 * - Bare steel: black-grey from the fire at first, then rust takes it within weeks (half by a month, nearly all
		 *   by three): a bright orange bloom first, browning over a year and more, darker where it is deep, in runs down
		 *   her sides. The soot washes off what faces the sky over the months and stays under her and in her corners.
		 * - The paint left: scorched, faded and chalky with the months, rust bleeding through it.
		 * - Rubber (a road wheel's ring): charred black with grey ash, the ash washed away, the steel under it rusting;
		 *   a lorry's tyre is gone to its rim (ToRim), the rim steel.
		 * - Low on her: the field's own earth splashed up, and after half a year a green stain where the weeds stand.
		 * - Snowed: the snow fallen on her since she burnt (SnowSince): white on what faces up, the steel showing through
		 *   where it lies thin; the earth and the weeds' stain under it.
		 */
		Rgb8 Burn(const uint8_t* Paint, double Soot, const V3& At, double UpZ, double High, const Hulk& H, const Tone& Burnt, const Tone& Earth, Stuff What, bool bRing,
			double Snowed)
		{
			const double M = std::max(0.0, H.Months);
			const uint64_t S = H.Seed;
			const double N1 = Noise3(At.X * 2.2, At.Y * 2.2, At.Z * 2.2, S ^ 0xA1ull);
			const double N2 = Smooth(0.5, 0.85, Noise3(At.X * 0.9, At.Y * 0.9, At.Z * 0.9, S ^ 0xB2ull));
			const double N3 = Noise3(At.X * 5.0, At.Y * 5.0, At.Z * 5.0, S ^ 0xC3ull);
			const double Heat = Clamp01(Soot * H.Fire + 0.3 * (N1 - 0.5));
			// (the scorched paint left flakes off over the next two years, so the bare steel spreads down her sides: the
			// models' soot weights run from 0.4 low on her to 1 on her deck and turret)
			const double Flake = 0.18 * Smooth(2.0, 24.0, M);
			double Bare = What == Stuff::Link || What == Stuff::Rim ? 1.0 : Smooth(0.58 - Flake, 0.8 - Flake, Heat);
			// rust: its onset (half by a month), its age (orange to brown), deep patches, runs down a side
			const double Onset = 1.0 - std::exp(-M / 1.4);
			const double Age = Smooth(1.0, 18.0, M);
			const double Runs = std::fabs(UpZ) < 0.5 ? Noise3(At.X * 4.0, At.Y * 4.0, At.Z * 0.35, S ^ 0xD4ull) : 0.5;
			const double Rusted = Clamp01(Onset * (0.75 + 0.5 * Runs));
			// (orange and brown side by side in patches a stride across, browning as a whole with the months)
			const double N5 = Noise3(At.X * 1.3, At.Y * 1.3, At.Z * 1.3, S ^ 0xF5ull);
			Tone Rust = Mixed(Tone(156, 84, 42), Tone(102, 58, 36), Clamp01(Age + 0.7 * (N5 - 0.5)));
			Rust = Mixed(Rust, Tone(66, 40, 30), 0.45 * N2);
			Rust = Mixed(Rust, Tone(168, 96, 48), 0.35 * (1.0 - Age) * N3);
			// the soot left on it: in sheets and streaks where the flames came out (some of her black, some clean of it),
			// washed off what faces the sky over the months, kept under her
			const double N4 = Smooth(0.3, 0.7, Noise3(At.X * 0.8, At.Y * 0.8, At.Z * 1.6, S ^ 0xE4ull));
			const double Wash = (1.0 - std::exp(-M / 6.0)) * (UpZ > 0.0 ? 0.25 + 0.75 * Clamp01(UpZ) : 0.15);
			const double SootLeft = Clamp01(Heat * (1.0 - Wash) * (0.25 + 1.05 * N4));
			const Tone Fresh = Mixed(Burnt, Tone(88, 82, 76), 0.25 * N3);
			Tone Steel = Mixed(Fresh, Rust, Rusted * (1.0 - 0.55 * SootLeft));
			Steel = Mixed(Steel, Burnt, 0.55 * SootLeft * (1.0 - 0.5 * Rusted));
			Tone Out;
			if (What == Stuff::Rubber)
			{
				// (the rubber charred, its ash grey and soon washed off; the steel under it rusting)
				const Tone Charred = Mixed(Tone(30, 28, 26), Tone(118, 114, 108), 0.4 * N1 * std::exp(-M / 2.0));
				Out = Mixed(Charred, Rust, 0.65 * Rusted);
				Bare = 1.0;
			}
			else
			{
				const Tone Own(Paint[0], Paint[1], Paint[2]);
				Tone Coat = Mixed(Own, Burnt, Clamp01(Heat * 1.25 - 0.15) * 0.9);
				const Tone Chalk(std::min(255.0, Coat.R * 0.8 + 52.0), std::min(255.0, Coat.G * 0.8 + 50.0), std::min(255.0, Coat.B * 0.8 + 46.0));
				Coat = Mixed(Coat, Chalk, 0.3 * (1.0 - std::exp(-M / 10.0)));
				Coat = Mixed(Coat, Rust, 0.3 * Rusted * N2);
				Out = Mixed(Coat, Steel, Bare);
			}
			// her open turret ring (her turret thrown): black, the fire's own chimney
			if (bRing) Out = Mixed(Out, Burnt, 0.8);
			// low on her: the field's earth splashed up, and the weeds' stain once they stand (both under the snow where it
			// has fallen since she burnt: on a winter field she would otherwise stand brown- and green-footed in clean snow)
			if (High < 0.45)
			{
				const double Low = 1.0 - Clamp01(High / 0.45);
				Out = Mixed(Out, Earth, 0.3 * Low * (0.4 + 0.6 * std::min(1.0, M / 4.0)) * (1.0 - Snowed));
				Out = Mixed(Out, Tone(70, 82, 42), 0.18 * Low * Smooth(4.0, 18.0, M) * (1.0 - Snowed));
			}
			// the snow fallen on her since she cooled, on what faces up (her deck, her turret's roof, her track's links
			// lying flat), the steel showing through in patches where it lies thin (LandSnow's new snow)
			if (Snowed > 0.0 && UpZ > 0.55)
			{
				Out = Mixed(Out, Tone(250, 252, 255), Snowed * Smooth(0.55, 0.9, UpZ) * (0.7 + 0.35 * N1));
			}
			return ToRgb(Out);
		}

		/** A triangle added so it faces Up's side (counter-clockwise from its front in the field's axes). */
		void Wound(LandField::Mesh& Out, uint32_t A, uint32_t B, uint32_t C)
		{
			const float* P = Out.Positions.data();
			const float* N = Out.Normals.data();
			const V3 Pa(P[A * 3], P[A * 3 + 1], P[A * 3 + 2]), Pb(P[B * 3], P[B * 3 + 1], P[B * 3 + 2]), Pc(P[C * 3], P[C * 3 + 1], P[C * 3 + 2]);
			const V3 Up(N[A * 3] + N[B * 3] + N[C * 3], N[A * 3 + 1] + N[B * 3 + 1] + N[C * 3 + 1], N[A * 3 + 2] + N[B * 3 + 2] + N[C * 3 + 2]);
			const bool bFront = (Pb - Pa).Cross(Pc - Pa).Dot(Up) >= 0.0;
			Out.Triangles.insert(Out.Triangles.end(), { A, bFront ? B : C, bFront ? C : B });
		}

		/** The snow fallen on a wreck and her patch since she burnt (Snow the field's lying, 0 to 1; white from 0.45, as
		 * LandSnow whitens a roof): none on a fresh one, whose fire melted it round her, all of it within a month. */
		double SnowSince(double Snow, double Months)
		{
			return Smooth(0.03, 0.45, Snow) * Smooth(0.25, 1.0, Months);
		}

		uint32_t Point(LandField::Mesh& Out, const V3& At, const V3& Up, const Rgb8& C)
		{
			const uint32_t I = static_cast<uint32_t>(Out.Colours.size());
			Out.Positions.insert(Out.Positions.end(), { static_cast<float>(At.X), static_cast<float>(At.Y), static_cast<float>(At.Z) });
			const V3 U = Up.Unit();
			Out.Normals.insert(Out.Normals.end(), { static_cast<float>(U.X), static_cast<float>(U.Y), static_cast<float>(U.Z) });
			Out.Colours.push_back(C);
			return I;
		}
	}

	double Left(double Months)
	{
		const double M = std::max(0.0, Months);
		if (M <= 3.0) return 1.0;
		if (M <= 12.0) return 1.0 - 0.5 * (M - 3.0) / 9.0;
		if (M <= 24.0) return 0.5 - 0.35 * (M - 12.0) / 12.0;
		if (M <= 48.0) return 0.15 - 0.09 * (M - 24.0) / 24.0;
		return 0.06;
	}

	namespace
	{
		/** Per as Models reads it: not a number or under 1 is 1, over 1000 is 1000. */
		double PerOf(double Per)
		{
			return Per == Per && Per > 1.0 ? std::min(Per, 1000.0) : 1.0;
		}
	}

	int Models(int Count, double Per)
	{
		const int Burnt = std::clamp(Count, 0, MostCount);
		return Burnt > 0 ? std::max(1, static_cast<int>(std::lround(Burnt / PerOf(Per)))) : 0;
	}

	int Kept(int Count, double Months, double Per)
	{
		return std::min(Most, static_cast<int>(std::lround(Models(Count, Per) * Left(Months))));
	}

	std::string ModelFor(const std::string& Nation, int Year, int N, bool bSoft)
	{
		const size_t K = static_cast<size_t>(std::max(0, N));
		if (bSoft)
		{
			if (Nation == "GER") return "Hanomag";
			if (Nation == "USA" && Year >= 1942) return "M3Halftrack";
			return "Truck";
		}
		const bool bAxis = Nation == "GER" || Nation == "ITA" || Nation == "JAP" || Nation == "HUN" || Nation == "ROM" || Nation == "BUL" || Nation == "FIN";
		std::vector<const char*> Line;
		if (Nation == "GER")
		{
			if (Year >= 1944) Line = { "PanzerIV", "Panther", "PanzerIV", "StuG3", "Tiger" };
			else if (Year >= 1943) Line = { "PanzerIV", "PanzerIV", "Tiger", "StuG3" };
			else if (Year >= 1941) Line = { "PanzerIV", "StuG3", "LightTank" };
			else Line = { "LightTank", "PanzerIV" };
		}
		else if (Nation == "SOV") Line = Year >= 1941 ? std::vector<const char*>{ "T34", "T34", "LightTank" } : std::vector<const char*>{ "LightTank" };
		else if (bAxis) Line = Year >= 1943 && Nation != "JAP" ? std::vector<const char*>{ "StuG3", "LightTank" } : std::vector<const char*>{ "LightTank" };
		else Line = Year >= 1942 ? std::vector<const char*>{ "Sherman", "Sherman", "LightTank" } : std::vector<const char*>{ "LightTank" };
		return Line[K % Line.size()];
	}

	Field FieldOf(const LandFeed::Recording& Rec, const LandField::Ground& G, const LandCrater::Field* Holes,
		const std::vector<std::vector<std::pair<double, double>>>& OldRuts)
	{
		Field F;
		// The fought-over ground and the ways, and the way the machines went: their tracks' start to end as a line (a
		// heading and its reverse alike), the longer the more (LayOldRuts's).
		double Sx = 0.0, Sy = 0.0;
		for (const LandFeed::Track& T : Rec.Tracks)
		{
			for (size_t K = 0; K < T.At.size(); K += 4)
			{
				F.Fought.emplace_back(T.At[K].X, T.At[K].Y);
			}
			std::vector<std::pair<double, double>> Way;
			Way.reserve(T.At.size());
			for (const LandFeed::Point& P : T.At)
			{
				Way.emplace_back(P.X, P.Y);
			}
			if (T.bPinned)
			{
				Way.emplace_back(T.PinX, T.PinY);
			}
			if (T.IsVehicle())
			{
				if (T.At.size() >= 2)
				{
					const double Dx = T.At.back().X - T.At.front().X, Dy = T.At.back().Y - T.At.front().Y, Long = std::hypot(Dx, Dy);
					if (Long > 20.0)
					{
						const double A = 2.0 * std::atan2(Dy, Dx);
						Sx += Long * std::cos(A);
						Sy += Long * std::sin(A);
					}
				}
				F.Vehicles.push_back(std::move(Way));
			}
			else
			{
				F.Men.push_back(std::move(Way));
			}
		}
		F.bAxis = Sx * Sx + Sy * Sy > 0.0;
		F.Axis = F.bAxis ? 0.5 * std::atan2(Sy, Sx) : 0.0;
		// This battle's shells, bombs and grenades, where they land (a rifle's, a machine gun's or a smoke round's
		// tears no hole).
		for (const LandFeed::Round& R : Rec.Rounds)
		{
			if (R.W == "mortar" || R.W == "gun" || R.W == "tank" || R.W == "at" || R.W == "grenade")
			{
				F.Blasts.emplace_back(R.X, R.Y);
			}
		}
		for (const LandFeed::Event& E : Rec.Events)
		{
			if (E.What == "shell")
			{
				F.Blasts.emplace_back(E.X, E.Y);
			}
		}
		for (const LandFeed::AirNote& A : Rec.Air)
		{
			if (A.What == "land:bomb-drop")
			{
				F.Blasts.emplace_back(A.To[0], A.To[1]);
			}
		}
		for (const LandFeed::Crater& C : Rec.Craters)
		{
			F.Standing.push_back({ C.X, C.Y, C.R + 1.0 });
		}
		// The field's lines: the old ones a fight lay along, the lanes, and every bank, wall and wood's edge.
		F.Lines = LandCrater::OldLinesOf(G, Rec.Ground.TownX, Rec.Ground.TownY, Rec.Ground.TownR);
		F.Lanes = G.Lanes();
		for (const LandField::HedgeLine& H : G.Hedges())
		{
			F.Hedges.push_back(H.Points);
		}
		// What stands in the open: every tree, shrub, gate, well, cart, haycock and a region's kit, by its reach.
		for (int B = 0; B < static_cast<int>(LandField::Base::Tuft); ++B)
		{
			const LandField::Base Kind = static_cast<LandField::Base>(B);
			const double Reach = G.BaseOf(Kind).Radius;
			for (const LandField::Placed& P : G.Instances(Kind))
			{
				F.Standing.push_back({ P.X, P.Y, Reach * P.Scale });
			}
		}
		// The old shell holes laid so far (the scars'), each to the reach of its thrown soil: kept off whole, not only
		// where Open's samples fall (the steppe's first old field had a mortar's bowl between them, under a hull).
		if (Holes)
		{
			for (const LandCrater::Field::Hole& H : Holes->All())
			{
				F.Standing.push_back({ H.X, H.Y, std::max({ H.Out, H.BareR, 1.2 * H.R }) });
			}
		}
		F.Ways = OldRuts;
		// The field telephones' wires (release 2: the recording's, or the scene's -TheaterLandPhones made from the battle
		// before the wrecks are laid), each lying on the ground along its points: a lineman lays his round a wreck.
		for (const LandFeed::Wire& W : Rec.Wires)
		{
			if (!W.Pts.empty())
			{
				F.Wires.push_back(W.Pts);
			}
		}
		F.Snow = G.SnowLying();
		// The two sides: their zones' middles and nations.
		const double* A = Rec.Ground.ZoneAtt;
		const double* D = Rec.Ground.ZoneDef;
		F.bSides = (A[2] - A[0]) * (A[3] - A[1]) > 0.0 && (D[2] - D[0]) * (D[3] - D[1]) > 0.0;
		F.AttX = 0.5 * (A[0] + A[2]);
		F.AttY = 0.5 * (A[1] + A[3]);
		F.DefX = 0.5 * (D[0] + D[2]);
		F.DefY = 0.5 * (D[1] + D[3]);
		F.AttNat = Rec.Sides[0].Nat;
		F.DefNat = Rec.Sides[1].Nat;
		const LandField::Ground* Gp = &G;
		F.Open = [Gp, Holes](double X, double Y)
		{
			const LandField::Drawn What = Gp->At(X, Y).What;
			const bool bOpen = What == LandField::Drawn::Pasture || What == LandField::Drawn::Rough || What == LandField::Drawn::Beyond
				|| (What == LandField::Drawn::Crop && !Gp->CornAt(X, Y));
			return bOpen && !(Holes && (Holes->Bare(X, Y) || Holes->InCut(X, Y)));
		};
		F.Ground = [Gp](double X, double Y, double& OutZ, LandKit::V3& OutNormal)
		{
			const LandField::Surface Under = Gp->At(X, Y);
			OutZ = Under.Z;
			OutNormal = LandKit::V3(Under.NX, Under.NY, Under.NZ);
		};
		F.Trodden = [Gp](double X0, double Y0, double X1, double Y1, std::vector<std::pair<double, double>>& Out)
		{
			std::vector<LandField::Placed> Laid;
			Gp->Clutter(X0, Y0, X1, Y1, Laid);
			Out.clear();
			for (const LandField::Placed& P : Laid)
			{
				if (P.Kind == LandField::Base::Trodden)
				{
					Out.emplace_back(P.X, P.Y);
				}
			}
		};
		F.DrawnZ = [Gp](double X, double Y) { return Gp->DrawnZ(X, Y); };
		F.DrawnUp = [Gp](double X, double Y, double& OutX, double& OutY, double& OutZ) { Gp->DrawnUp(X, Y, OutX, OutY, OutZ); };
		F.Colour = [Gp](double X, double Y) { return Gp->ColourAt(X, Y); };
		return F;
	}

	const LandKit::Model* Yard::ModelOf(size_t K) const
	{
		if (K >= KeyOf.size()) return nullptr;
		const auto Found = Models.find(KeyOf[K]);
		return Found == Models.end() ? nullptr : Found->second;
	}

	int Yard::Lay(const Field& F, const Ask& A, const ReadFn& ReadFile, const HaveFn& Have)
	{
		List.clear();
		KeyOf.clear();
		Cover.clear();
		StandIns.clear();
		Wanted = NoRoom = Read = 0;
		std::fill(std::begin(ByWhere), std::end(ByWhere), 0);
		Asked = A;
		Ground = F.Ground;
		DrawnZ = F.DrawnZ;
		DrawnUp = F.DrawnUp;
		Colour = F.Colour;
		Snow = F.Snow;
		const int Burnt = std::clamp(A.Count, 0, MostCount);
		const double Months = std::max(0.0, A.Months);
		// (the Mac's stand-in lays as many as it is asked, at most Most: its months age their look and take none away;
		// the key's count is burnt-out machines, drawn at the battle's scale, Per to a wreck, before the scrap men's)
		Wanted = A.bAsLaid ? Burnt : static_cast<int>(std::lround(LandHulk::Models(A.Count, A.Per) * Left(Months)));
		const int Target = std::min(Wanted, Most);
		if (Target <= 0 || F.Fought.empty() || !F.Ground || !F.Open)
		{
			return 0;
		}
		Dice D(Mix(A.Seed ? A.Seed : 1, 0x01D3EC5ull));

		// Whose they were: the recording's nats, shared out by the largest remainder; else the battle's two sides, the
		// attacker's the odd one (the attacker loses more armour).
		std::vector<std::string> Nation;
		{
			std::vector<std::pair<std::string, int>> Nats;
			int Sum = 0;
			for (const auto& P : A.Nats)
			{
				if (!P.first.empty() && P.second > 0)
				{
					Nats.push_back(P);
					Sum += P.second;
				}
			}
			if (Nats.empty())
			{
				const std::string Att = !F.AttNat.empty() ? F.AttNat : !F.DefNat.empty() ? F.DefNat : std::string("USA");
				const std::string Def = !F.DefNat.empty() ? F.DefNat : Att;
				for (int K = 0; K < Target; ++K) Nation.push_back(K % 2 == 0 ? Att : Def);
			}
			else
			{
				std::vector<int> Share(Nats.size(), 0);
				std::vector<std::pair<double, size_t>> Rest;
				int Given = 0;
				for (size_t K = 0; K < Nats.size(); ++K)
				{
					const double Exact = static_cast<double>(Target) * Nats[K].second / Sum;
					Share[K] = static_cast<int>(std::floor(Exact));
					Given += Share[K];
					Rest.emplace_back(Exact - Share[K], K);
				}
				std::stable_sort(Rest.begin(), Rest.end(), [](const auto& L, const auto& R) { return L.first > R.first; });
				for (size_t K = 0; Given < Target && K < Rest.size(); ++K, ++Given) ++Share[Rest[K].second];
				for (size_t K = 0; K < Nats.size(); ++K)
				{
					for (int I = 0; I < Share[K]; ++I) Nation.push_back(Nats[K].first);
				}
			}
			// (mixed, so a group holds both sides' as a fight leaves them)
			for (size_t K = Nation.size(); K > 1; --K)
			{
				std::swap(Nation[K - 1], Nation[static_cast<size_t>(D.U() * static_cast<double>(K)) % K]);
			}
		}

		// Their machines, as the month they burnt in had them (Months before this battle's), each read once a nation. The
		// battle's own models are dressed for this month's coats and kit: taken only where the wrecks burnt this month,
		// else her own read and dressed for hers (a German wreck of the spring in a summer battle in her spring coat).
		int BurntYear = A.Year, BurntMonth = A.Month, NowYear = A.Year, NowMonth = A.Month;
		MonthsBefore(A.Year, A.Month, Months, BurntYear, BurntMonth);
		MonthsBefore(A.Year, A.Month, 0.0, NowYear, NowMonth);
		const bool bThisMonth = BurntYear == NowYear && BurntMonth == NowMonth;
		std::map<std::string, Kit> Kits;
		std::map<std::string, int> Nth;
		const auto Load = [&](const std::string& Name, const std::string& Nat, bool bSoft) -> std::string
		{
			std::vector<std::string> Try = { Name };
			if (bSoft) Try.push_back("Truck");
			Try.push_back(ModelFor(Nat, BurntYear, 0, false));
			for (const char* Any : { "PanzerIV", "Sherman", "T34", "LightTank" }) Try.push_back(Any);
			for (const std::string& Model : Try)
			{
				const std::string Key = Model + "|" + Nat;
				if (Models.count(Key))
				{
					return Key;
				}
				const LandKit::Model* Got = Have && bThisMonth ? Have(Model, Nat) : nullptr;
				if (!Got && ReadFile)
				{
					std::string Text, Why;
					if (ReadFile(Model + ".json", Text))
					{
						LandKit::Dress Coat;
						Coat.Nation = Nat;
						Coat.Year = BurntYear;
						Coat.Month = BurntMonth;
						Coat.Terrain = A.Terrain;
						auto Made = std::make_shared<LandKit::Model>();
						if (Made->Load(Text, Coat, Why))
						{
							Loaded[Key] = Made;
							Got = Made.get();
							++Read;
						}
					}
				}
				if (Got)
				{
					Models[Key] = Got;
					Kits[Key] = KitOf(*Got);
					if (Model != Name)
					{
						StandIns.push_back("the " + Nat + " " + Name + " drawn as the " + Model + " (no " + Name + ".json read)");
					}
					return Key;
				}
			}
			return std::string();
		};

		// The fought-over ground (as LayScars reads it): its middle four fifths each way, 300 m at least.
		std::vector<double> Xs, Ys;
		for (const auto& P : F.Fought)
		{
			if (P.first == P.first && P.second == P.second)
			{
				Xs.push_back(P.first);
				Ys.push_back(P.second);
			}
		}
		if (Xs.empty()) return 0;
		std::sort(Xs.begin(), Xs.end());
		std::sort(Ys.begin(), Ys.end());
		const auto Quantile = [](const std::vector<double>& V, double Q) { return V[std::min(V.size() - 1, static_cast<size_t>(Q * static_cast<double>(V.size() - 1) + 0.5))]; };
		double X0 = Quantile(Xs, 0.1), X1 = Quantile(Xs, 0.9), Y0 = Quantile(Ys, 0.1), Y1 = Quantile(Ys, 0.9);
		const auto Widen = [](double& Lo, double& Hi) { const double Mid = 0.5 * (Lo + Hi), Half = std::max(150.0, 0.5 * (Hi - Lo)); Lo = Mid - Half; Hi = Mid + Half; };
		Widen(X0, X1);
		Widen(Y0, Y1);
		const auto InBox = [&](double X, double Y) { return X >= X0 && X <= X1 && Y >= Y0 && Y <= Y1; };

		// The old lines' pieces over it, and a few fronts of them (as the scars' fronts: the pieces lying in one heading
		// within 30 m of a first piece's line, 60 m of them at least).
		struct Piece
		{
			double AX, AY, BX, BY, Long, Ux, Uy;
		};
		std::vector<Piece> Pieces;
		for (const auto& Line : F.Lines.Lines)
		{
			for (size_t K = 0; K + 1 < Line.size(); ++K)
			{
				const double AX = Line[K].first, AY = Line[K].second, BX = Line[K + 1].first, BY = Line[K + 1].second;
				const double Long = std::hypot(BX - AX, BY - AY);
				if (Long >= 4.0 && Long < 1e5 && InBox(0.5 * (AX + BX), 0.5 * (AY + BY)))
				{
					Pieces.push_back({ AX, AY, BX, BY, Long, (BX - AX) / Long, (BY - AY) / Long });
				}
			}
		}
		struct Front
		{
			std::vector<size_t> Of;
			double Long = 0.0;
		};
		std::vector<Front> Fronts;
		if (!Pieces.empty())
		{
			double Total = 0.0;
			for (const Piece& Pc : Pieces) Total += Pc.Long;
			const int Want = std::min(4, 1 + Target / 5);
			for (int Tries = 0; Tries < Want * 3 && static_cast<int>(Fronts.size()) < Want; ++Tries)
			{
				double Pick = D.U() * Total;
				size_t First = 0;
				while (First + 1 < Pieces.size() && Pick > Pieces[First].Long) Pick -= Pieces[First++].Long;
				const Piece& Pa = Pieces[First];
				Front Fr;
				for (size_t K = 0; K < Pieces.size(); ++K)
				{
					const Piece& Pb = Pieces[K];
					const double Mx = 0.5 * (Pb.AX + Pb.BX) - Pa.AX, My = 0.5 * (Pb.AY + Pb.BY) - Pa.AY;
					if (std::fabs(Pa.Ux * Pb.Ux + Pa.Uy * Pb.Uy) >= 0.85 && std::fabs(Mx * Pa.Uy - My * Pa.Ux) <= 30.0)
					{
						Fr.Of.push_back(K);
						Fr.Long += Pb.Long;
					}
				}
				if (Fr.Long >= 60.0) Fronts.push_back(std::move(Fr));
			}
		}
		std::vector<std::pair<double, double>> Spots;
		for (const auto& P : F.Lines.Spots)
		{
			if (InBox(P.first, P.second)) Spots.push_back(P);
		}
		// The battle's axis: through the middle of where its machines went, along the way they went.
		double AxisX = 0.5 * (X0 + X1), AxisY = 0.5 * (Y0 + Y1), AxisUx = 1.0, AxisUy = 0.0;
		{
			double Sx = 0.0, Sy = 0.0;
			size_t N = 0;
			for (const auto& Way : F.Vehicles)
			{
				for (const auto& P : Way)
				{
					if (InBox(P.first, P.second))
					{
						Sx += P.first;
						Sy += P.second;
						++N;
					}
				}
			}
			if (N > 0)
			{
				AxisX = Sx / static_cast<double>(N);
				AxisY = Sy / static_cast<double>(N);
			}
			if (F.bAxis && F.Axis == F.Axis)
			{
				AxisUx = std::cos(F.Axis);
				AxisUy = std::sin(F.Axis);
			}
			else if (F.bSides)
			{
				const double Dx = F.DefX - F.AttX, Dy = F.DefY - F.AttY, L = std::hypot(Dx, Dy);
				if (L > 1.0)
				{
					AxisUx = Dx / L;
					AxisUy = Dy / L;
				}
			}
		}
		const double AxisHalf = 0.5 * std::min(std::fabs((X1 - X0) / (std::fabs(AxisUx) > 1e-6 ? AxisUx : 1e-6)), std::fabs((Y1 - Y0) / (std::fabs(AxisUy) > 1e-6 ? AxisUy : 1e-6)));

		// What each is kept clear of, by squares.
		Squares LaneAt(8.0), HedgeAt(8.0), VehicleAt(8.0), ManAt(6.0), BlastAt(8.0), StandAt(8.0), WayAt(8.0), HulkAt(8.0), WireAt(8.0);
		for (const auto& L : F.Lanes) LaneAt.AddLine(L, 1.0);
		for (const auto& L : F.Hedges) HedgeAt.AddLine(L, 1.0);
		for (const auto& L : F.Vehicles)
		{
			if (L.size() == 1) VehicleAt.Add(L[0].first, L[0].second);
			VehicleAt.AddLine(L, 1.0);
		}
		for (const auto& L : F.Men)
		{
			if (L.size() == 1) ManAt.Add(L[0].first, L[0].second);
			ManAt.AddLine(L, 0.75);
		}
		for (const auto& P : F.Blasts) BlastAt.Add(P.first, P.second);
		for (const Ring& R : F.Standing) StandAt.Add(R.X, R.Y, R.R);
		for (const auto& L : F.Ways) WayAt.AddLine(L, 1.0);
		for (const auto& L : F.Wires)
		{
			if (L.size() == 1) WireAt.Add(L[0].first, L[0].second);
			WireAt.AddLine(L, 1.0);
		}

		// (her scorched patch reaches about two metres past her outline: it is kept off the old ruts too, whose drawn
		// ruts would show through it)
		constexpr double PatchReach = 2.0;
		std::vector<std::pair<double, double>> Paths;
		// Whether a wreck's outline lies clear: on open ground all round each ring, off the lanes and hedges, the
		// recorded ways, this battle's blasts, what stands in the open, the old ruts, the field telephones' wires and the
		// other wrecks.
		const auto Clear = [&](const std::vector<Ring>& Rings, double PatchMargin) -> bool
		{
			for (const Ring& R : Rings)
			{
				if (!F.Open(R.X, R.Y)) return false;
				for (int K = 0; K < 8; ++K)
				{
					const double Ang = K * Pi / 4.0;
					for (const double Share : { 0.6, 1.0 })
					{
						if (!F.Open(R.X + std::cos(Ang) * R.R * Share, R.Y + std::sin(Ang) * R.R * Share)) return false;
					}
				}
				if (LaneAt.Near(R.X, R.Y, LaneClear + R.R + 0.5)) return false;
				if (HedgeAt.Near(R.X, R.Y, HedgeClear + R.R + 0.5)) return false;
				if (VehicleAt.Near(R.X, R.Y, VehicleClear + R.R + 0.5)) return false;
				if (ManAt.Near(R.X, R.Y, ManClear + R.R + 0.4)) return false;
				if (BlastAt.Near(R.X, R.Y, BlastClear + R.R)) return false;
				if (StandAt.Near(R.X, R.Y, R.R + 1.0)) return false;
				if (WayAt.Near(R.X, R.Y, WayClear + R.R + PatchMargin + 0.5)) return false;
				if (WireAt.Near(R.X, R.Y, WireClear + R.R + 0.5)) return false;
				if (HulkAt.Near(R.X, R.Y, R.R + Apart)) return false;
			}
			// (the cattle's paths last: their pieces are laid as the grass is, dearer to ask than all the rest)
			if (F.Trodden && !Rings.empty())
			{
				double Bx0 = 1e18, By0 = 1e18, Bx1 = -1e18, By1 = -1e18;
				for (const Ring& R : Rings)
				{
					Bx0 = std::min(Bx0, R.X - R.R - PathClear);
					By0 = std::min(By0, R.Y - R.R - PathClear);
					Bx1 = std::max(Bx1, R.X + R.R + PathClear);
					By1 = std::max(By1, R.Y + R.R + PathClear);
				}
				F.Trodden(Bx0, By0, Bx1, By1, Paths);
				for (const auto& P : Paths)
				{
					for (const Ring& R : Rings)
					{
						if (std::hypot(P.first - R.X, P.second - R.Y) < R.R + PathClear) return false;
					}
				}
			}
			return true;
		};

		// Whether her scorched patch lies on open ground, every point of it as Patch lays it (its ragged edge reaches some
		// 2.4 m past her outline, which Clear reads: on the steppe's first lay three patches crossed a trench's works and
		// one a garden), the outer rings first.
		const auto PatchClear = [&](const Hulk& H) -> bool
		{
			double Fx, Fy, Rx, Ry;
			Ways(H.Heading, Fx, Fy, Rx, Ry);
			double Reach[PatchSpokes];
			for (int S = 0; S < PatchSpokes; ++S) Reach[S] = SpokeReach(H, S);
			for (int Ri = PatchRings - 1; Ri >= 1; --Ri)
			{
				for (int S = 0; S < PatchSpokes; ++S)
				{
					double X = 0.0, Y = 0.0;
					PatchAt(H, Fx, Fy, Rx, Ry, Ri, S, Reach[S], X, Y);
					if (!F.Open(X, Y)) return false;
				}
			}
			return F.Open(H.X, H.Y);
		};

		// A wreck's shape on the field at a place: how she rests (her foot's corners on the ground, sunk where her
		// tyres burnt off or a track ran off), her outline, her thrown turret's and her run-off track's places.
		const auto Shape = [&](Hulk& H, const LandKit::Model& M, const Kit& K, Dice& Own) -> bool
		{
			double Fx, Fy, Rx, Ry;
			Ways(H.Heading, Fx, Fy, Rx, Ry);
			const auto At = [&](double Ax, double Ay, double& OutX, double& OutY)
			{
				OutX = H.X + Fx * Ax + Rx * Ay;
				OutY = H.Y + Fy * Ax + Ry * Ay;
			};
			const Kit::Track* Off = H.TrackOff != 0 ? &K.Tracks[H.TrackOff < 0 ? 0 : 1] : nullptr;
			double Z[4], Lo = 1e9, Hi = -1e9;
			for (int C = 0; C < 4; ++C)
			{
				double Cx, Cy;
				At(M.Foot[C].X, M.Foot[C].Y, Cx, Cy);
				V3 N;
				Ground(Cx, Cy, Z[C], N);
				Lo = std::min(Lo, Z[C]);
				Hi = std::max(Hi, Z[C]);
				Z[C] -= M.Foot[C].X > K.MidX ? K.DropFront : K.DropBack;
				if (Off && ((M.Foot[C].Y < 0.0) == (H.TrackOff < 0))) Z[C] -= Off->BMax - Off->BMin;
			}
			if (Hi - Lo > 1.2) return false;
			const double Long = std::max(0.5, 0.5 * ((M.Foot[0].X + M.Foot[1].X) - (M.Foot[2].X + M.Foot[3].X)));
			const double Wide = std::max(0.5, 0.5 * ((M.Foot[1].Y + M.Foot[3].Y) - (M.Foot[0].Y + M.Foot[2].Y)));
			H.Pitch = std::atan2(0.5 * (Z[0] + Z[1]) - 0.5 * (Z[2] + Z[3]), Long);
			H.Roll = std::atan2(0.5 * (Z[1] + Z[3]) - 0.5 * (Z[0] + Z[2]), Wide);
			double MeanX = 0.0, MeanY = 0.0, MeanZ = 0.0;
			for (int C = 0; C < 4; ++C)
			{
				MeanX += 0.25 * M.Foot[C].X;
				MeanY += 0.25 * M.Foot[C].Y;
				MeanZ += 0.25 * Z[C];
			}
			H.Z = MeanZ - MeanX * std::tan(H.Pitch) - MeanY * std::tan(H.Roll) - H.Sink;
			{
				V3 Up;
				double Mid = 0.0;
				Ground(H.X, H.Y, Mid, Up);
				if (Up.Z < 0.96) return false;
			}
			// Her outline: her hull in a few rings nose to tail, and her gun where it reaches past it.
			H.Rings.clear();
			const double R = K.HalfWide + 0.2;
			const double From = M.HullLo.X + 0.8 * R, To = M.HullHi.X - 0.8 * R;
			const int Rings = std::max(2, static_cast<int>(std::ceil((To - From) / R)) + 1);
			for (int I = 0; I < Rings; ++I)
			{
				Ring Ri;
				At(From + (To - From) * I / (Rings - 1), 0.0, Ri.X, Ri.Y);
				Ri.R = R;
				H.Rings.push_back(Ri);
			}
			if (!H.bTurretOff && K.bMuzzle)
			{
				const double C = std::cos(H.Turret * Rad), S = std::sin(H.Turret * Rad);
				const double Mx = K.MuzzleAt.X - K.TurretPivot.X, My = K.MuzzleAt.Y - K.TurretPivot.Y;
				for (const double Share : { 0.55, 0.8, 1.0 })
				{
					Ring Ri;
					At(K.TurretPivot.X + Share * (Mx * C - My * S), K.TurretPivot.Y + Share * (Mx * S + My * C), Ri.X, Ri.Y);
					Ri.R = 0.45;
					H.Rings.push_back(Ri);
				}
			}
			// Her turret thrown: off her side, her tail or her bow quarter, a metre or a few off her hull, lying on its
			// roof, its side or tilted, its gun along its yaw.
			std::vector<Ring> Thrown;
			if (H.bTurretOff)
			{
				const double Pick = Own.U();
				double Side = Own.U() < 0.5 ? -1.0 : 1.0;
				// (to her side away from her run-off track where she has one, not onto its links)
				if (H.TrackOff != 0) Side = H.TrackOff < 0 ? 1.0 : -1.0;
				const double Bearing = Pick < 0.5 ? Side * Own.U(70.0, 110.0) : Pick < 0.8 ? Own.U(150.0, 210.0) : Side * Own.U(20.0, 50.0);
				const double Cb = std::cos(Bearing * Rad), Sb = std::sin(Bearing * Rad);
				const double Edge = 1.0 / std::sqrt((Cb / K.HalfLong) * (Cb / K.HalfLong) + (Sb / K.HalfWide) * (Sb / K.HalfWide));
				double Off2 = Edge + K.TurretR + Own.U(0.6, 4.0);
				At(K.MidX + Cb * Off2, Sb * Off2, H.TurretX, H.TurretY);
				// (and 0.6 m clear of her hull's corners, which the ellipse above cuts inside: pushed out along its bearing,
				// a bow quarter's throw that had lain through her front corner)
				for (int Push = 0; Push < 40 && FromFoot(H, H.TurretX, H.TurretY) < K.TurretR + 0.4; ++Push)
				{
					Off2 += 0.2;
					At(K.MidX + Cb * Off2, Sb * Off2, H.TurretX, H.TurretY);
				}
				H.TurretYaw = Own.U(0.0, 2.0 * Pi);
				// (its gun turned away from her where it would lie through her hull)
				const auto GunIn = [&]()
				{
					for (const double Share : { 0.6, 1.0 })
					{
						if (K.Muzzle <= K.TurretR) break;
						const double Gx = H.TurretX + std::sin(H.TurretYaw) * K.Muzzle * Share, Gy = H.TurretY + std::cos(H.TurretYaw) * K.Muzzle * Share;
						if (FromFoot(H, Gx, Gy) < 0.55) return true;
					}
					return false;
				};
				if (GunIn()) H.TurretYaw = std::fmod(H.TurretYaw + Pi, 2.0 * Pi);
				if (GunIn()) return false;
				const double How = Own.U();
				H.TurretLie = How < 0.35 ? Lie::Upturned : How < 0.6 ? Lie::OnSide : Lie::Tilted;
				H.TurretTilt = H.TurretLie == Lie::Tilted ? Own.U(8.0, 22.0) * Rad : Own.Bell() * 10.0 * Rad;
				// (tilted back onto its rear edge or a little to one side, its gun in the air: tilted toward its gun it
				// would stand on its muzzle, its ring a metre over the grass, as the steppe's first lay had it)
				H.TurretTiltWay = -0.5 * Pi + Own.Bell() * 0.9;
				Ring Ri;
				Ri.X = H.TurretX;
				Ri.Y = H.TurretY;
				Ri.R = K.TurretR;
				Thrown.push_back(Ri);
				for (const double Share : { 0.6, 1.0 })
				{
					if (K.Muzzle <= K.TurretR) break;
					Ring Gun;
					Gun.X = H.TurretX + std::sin(H.TurretYaw) * K.Muzzle * Share;
					Gun.Y = H.TurretY + std::cos(H.TurretYaw) * K.Muzzle * Share;
					Gun.R = 0.45;
					Thrown.push_back(Gun);
				}
				H.Rings.insert(H.Rings.end(), Thrown.begin(), Thrown.end());
			}
			// A track run off: its links laid out flat from her bow along her side and on past her tail, bowing out a
			// little, as she drove off it.
			H.Strip.clear();
			if (Off && Off->bOk)
			{
				const double S = H.TrackOff < 0 ? -1.0 : 1.0;
				double Px, Py;
				At(M.HullHi.X - 0.3, S * (M.TrackHalf + 0.55), Px, Py);
				const size_t Links = Off->Mid.size();
				const double Long2 = Off->Pitch * static_cast<double>(Links);
				const double Drift = Own.U(0.03, 0.12), Bend = Own.U(0.1, 0.35), Wobble = Own.U(0.0, 2.0 * Pi);
				for (size_t L = 0; L < Links; ++L)
				{
					H.Strip.emplace_back(Px, Py);
					const double U = static_cast<double>(L) * Off->Pitch / Long2;
					const double Way = H.Heading + Pi - S * (Drift + Bend * std::pow(U, 1.5)) + 0.03 * std::sin(Wobble + 9.0 * U);
					Px += std::sin(Way) * Off->Pitch;
					Py += std::cos(Way) * Off->Pitch;
				}
				const double Half = 0.5 * (Off->DMax - Off->DMin) + 0.15;
				for (size_t L = 0; L < Links; L += 3)
				{
					H.Rings.push_back({ H.Strip[L].first, H.Strip[L].second, Half });
				}
				// (her thrown turret and its gun not on the links: a throw over her tail can reach the strip's far end, and
				// the turret is settled on the ground, not on them)
				for (const Ring& T : Thrown)
				{
					for (const auto& P : H.Strip)
					{
						if (std::hypot(P.first - T.X, P.second - T.Y) < T.R + Half) return false;
					}
				}
			}
			for (const Ring& Ri : H.Rings)
			{
				if (!InBox(Ri.X, Ri.Y) && !InBox(H.X, H.Y)) return false;
			}
			return true;
		};

		// The thrown turret's height: dropped straight down onto the ground under it until its first point touches (on a
		// slope its uphill edge), then settled a little; none where the ground under it falls away by more than half a
		// metre (a balka's side, a bank: the steppe's first lay had one 0.85 m over the grass on one).
		const auto SettleTurret = [&](Hulk& H, const LandKit::Model& M, const Kit& K) -> bool
		{
			std::vector<LandKit::PartState> S;
			PoseOf(M, K, H, S);
			std::vector<LandKit::Xf> Xf;
			M.Solve(LandKit::Xf(), S, Xf);
			double GroundZ = 0.0;
			V3 Up;
			Ground(H.TurretX, H.TurretY, GroundZ, Up);
			const Quat T = TurretTurn(H, Up);
			double Rest = -1e9, Lo = 1e9, Hi = -1e9;
			// (and its reach for the camera, in its own axes along its gun's way: the turret's own bodies a box, its gun's
			// farthest point the muzzle)
			double Fx, Fy, Rx, Ry;
			Ways(H.TurretYaw, Fx, Fy, Rx, Ry);
			V3 BoxLo(1e9, 1e9, 1e9), BoxHi(-1e9, -1e9, -1e9), AllLo = BoxLo, AllHi = BoxHi, Muzzle;
			double Reach = -1.0;
			for (size_t B = 0; B < M.Bodies.size(); ++B)
			{
				if (!K.OnTurret[B] || K.Drop[B]) continue;
				const LandKit::Body& Bo = M.Bodies[B];
				const LandKit::Xf& X = Xf[static_cast<size_t>(Bo.Part)];
				const LandKit::Mesh& Me = Bo.Near;
				for (size_t I = 0; I < Me.Count(); I += 4)
				{
					const V3 P = X.Apply(Bo.Pivot + V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]));
					const V3 Q = ToField(T.Rotate(P - K.TurretPivot));
					double Under = 0.0;
					V3 N;
					Ground(H.TurretX + Q.X, H.TurretY + Q.Y, Under, N);
					Rest = std::max(Rest, Under - Q.Z);
					Lo = std::min(Lo, Under);
					Hi = std::max(Hi, Under);
					const V3 L(Q.X * Fx + Q.Y * Fy, Q.X * Rx + Q.Y * Ry, Q.Z);
					AllLo = V3(std::min(AllLo.X, L.X), std::min(AllLo.Y, L.Y), std::min(AllLo.Z, L.Z));
					AllHi = V3(std::max(AllHi.X, L.X), std::max(AllHi.Y, L.Y), std::max(AllHi.Z, L.Z));
					if (K.OnGun[B])
					{
						if (std::hypot(Q.X, Q.Y) > Reach)
						{
							Reach = std::hypot(Q.X, Q.Y);
							Muzzle = Q;
						}
						continue;
					}
					BoxLo = V3(std::min(BoxLo.X, L.X), std::min(BoxLo.Y, L.Y), std::min(BoxLo.Z, L.Z));
					BoxHi = V3(std::max(BoxHi.X, L.X), std::max(BoxHi.Y, L.Y), std::max(BoxHi.Z, L.Z));
				}
			}
			if (Rest < -1e8 || Hi - Lo > 0.5) return false;
			H.TurretZ = Rest - std::min(0.15, 0.05 + 0.6 * H.Sink);
			// (a few centimetres more each way: every fourth point read; a turret that is all gun one box)
			const bool bOwn = BoxHi.X >= BoxLo.X;
			H.TurretBoxLo = (bOwn ? BoxLo : AllLo) - V3(0.05, 0.05, 0.05);
			H.TurretBoxHi = (bOwn ? BoxHi : AllHi) + V3(0.05, 0.05, 0.05);
			H.bTurretGun = bOwn && Reach > 0.0;
			H.TurretMuzzle = H.bTurretGun ? V3(H.TurretX + Muzzle.X, H.TurretY + Muzzle.Y, H.TurretZ + Muzzle.Z) : V3();
			return true;
		};

		// The groups, one fight each, until all are laid or the field has no more room.
		std::vector<size_t> Order(static_cast<size_t>(Target));
		for (size_t K = 0; K < Order.size(); ++K) Order[K] = K;
		size_t Next = 0;
		int Group = 0;
		int Tries = 0;
		const int MostTries = 40 * Target + 60;
		std::vector<uint8_t> Used(static_cast<size_t>(Target), 0);
		while (Next < Order.size() && Tries < MostTries)
		{
			++Tries;
			// Where the fight lay: along a front of the old lines (most), at a junction or the village's edge, along the
			// battle's axis, or anywhere over the fought-over ground.
			const double Pick = D.U();
			// (a field with no old lines over its fought-over ground, the open steppe's, gives their share to the axis, not
			// to the village's edge: the scene's first steppe lay had eleven groups of fifteen round the village)
			Where W = Where::Strewn;
			if (Pick < 0.45) W = Fronts.empty() ? Where::Axis : Where::OldLine;
			else if (Pick < 0.65) W = Spots.empty() ? Where::Axis : Where::Spot;
			else if (Pick < 0.92) W = Where::Axis;
			double Cx = X0 + (X1 - X0) * D.U(), Cy = Y0 + (Y1 - Y0) * D.U();
			double Ux = AxisUy, Uy = -AxisUx; // (the way the group lies along: abreast, across the axis)
			if (W == Where::OldLine)
			{
				const Front& Fr = Fronts[std::min(Fronts.size() - 1, static_cast<size_t>(D.U() * static_cast<double>(Fronts.size())))];
				double Along = D.U() * Fr.Long;
				size_t K = 0;
				while (K + 1 < Fr.Of.size() && Along > Pieces[Fr.Of[K]].Long) Along -= Pieces[Fr.Of[K++]].Long;
				const Piece& Pc = Pieces[Fr.Of[K]];
				const double T = std::clamp(Along / Pc.Long, 0.0, 1.0);
				const double Lx = Pc.AX + (Pc.BX - Pc.AX) * T, Ly = Pc.AY + (Pc.BY - Pc.AY) * T;
				// (the side toward the attacker's ground mostly, where his armour was stopped in front of the line)
				double Held = D.U() < 0.5 ? 1.0 : -1.0;
				if (F.bSides)
				{
					const double Toward = (F.AttX - Lx) * -Pc.Uy + (F.AttY - Ly) * Pc.Ux;
					Held = (Toward >= 0.0 ? 1.0 : -1.0) * (D.U() < 0.75 ? 1.0 : -1.0);
				}
				const double Off = Held * D.U(12.0, 34.0);
				Cx = Lx - Pc.Uy * Off;
				Cy = Ly + Pc.Ux * Off;
				Ux = Pc.Ux;
				Uy = Pc.Uy;
			}
			else if (W == Where::Spot)
			{
				const auto& P = Spots[std::min(Spots.size() - 1, static_cast<size_t>(D.U() * static_cast<double>(Spots.size())))];
				const double Ang = D.U(0.0, 2.0 * Pi), Off = D.U(12.0, 28.0);
				Cx = P.first + std::cos(Ang) * Off;
				Cy = P.second + std::sin(Ang) * Off;
				Ux = std::cos(Ang + 0.5 * Pi);
				Uy = std::sin(Ang + 0.5 * Pi);
			}
			else if (W == Where::Axis)
			{
				const double T = D.U(-1.0, 1.0) * AxisHalf * 0.85, Across = D.Bell() * 45.0;
				Cx = AxisX + AxisUx * T - AxisUy * Across;
				Cy = AxisY + AxisUy * T + AxisUx * Across;
			}
			if (!InBox(Cx, Cy)) continue;
			// (a platoon's few, a lone one now and then)
			const int Size = std::min(static_cast<int>(Order.size() - Next), 1 + static_cast<int>(std::pow(D.U(), 1.4) * 3.6));
			const double Spacing = D.U(13.0, 28.0);
			int Laid = 0;
			for (int Member = 0; Member < Size && Next < Order.size(); ++Member)
			{
				const size_t Slot = Order[Next];
				const std::string& Nat = Nation[Slot];
				Dice Own(Mix(A.Seed ? A.Seed : 1, 0x01D3EC5000ull + Slot));
				const bool bSoft = Own.U() < 0.2;
				const std::string Key = Load(ModelFor(Nat, BurntYear, Nth[Nat + (bSoft ? "|soft" : "")]++, bSoft), Nat, bSoft);
				if (Key.empty())
				{
					// (no model to draw her by: none laid)
					Used[Slot] = 1;
					++Next;
					continue;
				}
				const LandKit::Model& M = *Models[Key];
				const Kit& K = Kits[Key];
				Hulk H;
				H.Nation = Nat;
				H.Model = Key.substr(0, Key.find('|'));
				H.bSoft = M.Kind != "tank";
				H.Group = Group;
				H.At = W;
				H.Seed = Mix(A.Seed ? A.Seed : 1, 0x5EED01Dull + Slot);
				// Her footprint (her model's hull box) and her scorched patch: her length and width and two metres more
				// (Patch).
				H.FootLoX = M.HullLo.X;
				H.FootHiX = M.HullHi.X;
				H.FootLoY = M.HullLo.Y;
				H.FootHiY = M.HullHi.Y;
				H.PatchLong = K.HalfLong + 2.2;
				H.PatchWide = K.HalfWide + 1.8;
				// (the last battle's mostly; some an earlier one's, a few months older)
				H.Months = Own.U() < 0.7 ? Months + Own.U(0.0, 0.5) : Months + Own.U(2.0, 8.0);
				H.Fire = Own.U(0.8, 1.0);
				if (M.Turret >= 0)
				{
					const LandKit::Part& T = M.Parts[static_cast<size_t>(M.Turret)];
					H.Turret = Own.U() < 0.7 ? Own.Bell() * 50.0 : Own.U(-175.0, 175.0);
					if (T.bLimits) H.Turret = std::clamp(H.Turret, T.Lo, T.Hi);
				}
				if (M.Gun >= 0)
				{
					// (her gun drooped to its stop, or onto her deck where it lies over her engine)
					const LandKit::Part& G = M.Parts[static_cast<size_t>(M.Gun)];
					const double Lo = G.bLimits ? G.Lo : -5.0;
					H.Gun = std::fabs(H.Turret) < 100.0 ? Lo : std::max(Lo, 0.0) + Own.U(0.0, 2.0);
				}
				H.bTurretOff = K.bTurret && Own.U() < 0.2;
				if (H.bTurretOff)
				{
					// (thrown, its gun lies along it as it was built: how it lies is the throw's, TurretTurn)
					H.Turret = 0.0;
				}
				const bool bTracks = K.Tracks[0].bOk && K.Tracks[1].bOk;
				H.TrackOff = bTracks && Own.U() < (H.bSoft ? 0.2 : 0.3) ? (Own.U() < 0.5 ? -1 : 1) : 0;
				for (size_t I = 0; I < K.Opens.size() && I < 32; ++I)
				{
					if (Own.U() < 0.75) H.Open |= 1u << I;
				}
				// (settled: a few centimetres at once, more as the months go and the rain softens the ground under her)
				H.Sink = (0.04 + 0.08 * std::min(1.0, H.Months / 12.0) + 0.05 * Clamp01((H.Months - 12.0) / 24.0)) * (H.bSoft ? 0.6 : 1.0);
				// Her facing: her side's way toward the other, a little off; some turned as they were hit, a few going back.
				double Face = Own.U(0.0, 2.0 * Pi);
				if (F.bSides && (Nat == F.AttNat || Nat == F.DefNat))
				{
					const bool bAtt = Nat == F.AttNat && !(Nat == F.DefNat && Own.U() < 0.5);
					Face = bAtt ? std::atan2(F.DefX - F.AttX, F.DefY - F.AttY) : std::atan2(F.AttX - F.DefX, F.AttY - F.DefY);
				}
				else if (W == Where::Axis)
				{
					Face = std::atan2(AxisUx, AxisUy) + (Own.U() < 0.5 ? Pi : 0.0);
				}
				const double Turned = Own.U();
				// (each throw its own statement: the order a compiler takes an expression's operands in is its own, and
				// g++ and clang differ)
				const double Aside = Own.Bell() * 35.0 * Rad;
				double Swung = 0.0;
				if (Turned < 0.15)
				{
					const double Way = Own.U() < 0.5 ? -1.0 : 1.0;
					Swung = Way * Own.U(50.0, 110.0) * Rad;
				}
				else if (Turned < 0.23)
				{
					Swung = Pi;
				}
				Face += Aside + Swung;
				bool bLaid = false;
				const double Mid = static_cast<double>(Member) - 0.5 * (Size - 1);
				for (int Try = 0; Try < 14 && !bLaid; ++Try)
				{
					const double Wander = 2.0 + 1.2 * Try;
					const double AlongX = Own.Bell() * Wander;
					const double OutX = Own.Bell() * Wander;
					const double AlongY = Own.Bell() * Wander;
					const double OutY = Own.Bell() * Wander;
					H.X = Cx + Ux * (Mid * Spacing + AlongX) - Uy * OutX;
					H.Y = Cy + Uy * (Mid * Spacing + AlongY) + Ux * OutY;
					H.Heading = Face + Own.Bell() * 0.15;
					// (a turret thrown or a track run off that finds no room: the second half of her tries without them)
					Hulk Tried = H;
					if (Try >= 9)
					{
						Tried.bTurretOff = false;
						Tried.TrackOff = 0;
					}
					if (!Shape(Tried, M, K, Own)) continue;
					if (!Clear(Tried.Rings, PatchReach)) continue;
					if (!PatchClear(Tried)) continue;
					if (Tried.bTurretOff && !SettleTurret(Tried, M, K)) continue;
					H = Tried;
					bLaid = true;
				}
				if (!bLaid) break;
				for (const Ring& Ri : H.Rings) HulkAt.Add(Ri.X, Ri.Y, Ri.R);
				List.push_back(H);
				KeyOf.push_back(Key);
				Used[Slot] = 1;
				++Next;
				++Laid;
			}
			if (Laid > 0)
			{
				++ByWhere[static_cast<int>(W)];
				++Group;
			}
		}
		NoRoom = Target - static_cast<int>(List.size());

		// The squares Covers reads: each wreck's rings, her hull's footprint (her rings leave its corners: the grass stood
		// through 2 to 4 in a hundred of each footprint), and her patch while it is fresh (the grass burnt there).
		for (size_t K = 0; K < List.size(); ++K)
		{
			const Hulk& H = List[K];
			const auto Mark = [&](double X, double Y, double R, int What)
			{
				for (int J = static_cast<int>(std::floor((Y - R) / 4.0)); J <= static_cast<int>(std::floor((Y + R) / 4.0)); ++J)
				{
					for (int I = static_cast<int>(std::floor((X - R) / 4.0)); I <= static_cast<int>(std::floor((X + R) / 4.0)); ++I)
					{
						Cover[(static_cast<int64_t>(I) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(J))].emplace_back(K, What);
					}
				}
			};
			for (size_t R = 0; R < H.Rings.size(); ++R) Mark(H.Rings[R].X, H.Rings[R].Y, H.Rings[R].R, static_cast<int>(R));
			Mark(H.X, H.Y, std::hypot(std::max(-H.FootLoX, H.FootHiX), std::max(-H.FootLoY, H.FootHiY)) + FootMargin, -2);
			if (H.Months < 3.0) Mark(H.X, H.Y, std::max(H.PatchLong, H.PatchWide), -1);
		}
		return static_cast<int>(List.size());
	}

	bool Yard::Covers(double X, double Y) const
	{
		if (Cover.empty()) return false;
		const int I = static_cast<int>(std::floor(X / 4.0)), J = static_cast<int>(std::floor(Y / 4.0));
		const auto Found = Cover.find((static_cast<int64_t>(I) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(J)));
		if (Found == Cover.end()) return false;
		for (const auto& [K, What] : Found->second)
		{
			const Hulk& H = List[K];
			if (What >= 0)
			{
				const Ring& R = H.Rings[static_cast<size_t>(What)];
				if ((X - R.X) * (X - R.X) + (Y - R.Y) * (Y - R.Y) <= R.R * R.R) return true;
			}
			else if (What == -2)
			{
				if (FromFoot(H, X, Y) <= FootMargin) return true;
			}
			else
			{
				double Fx, Fy, Rx, Ry;
				Ways(H.Heading, Fx, Fy, Rx, Ry);
				const double A = ((X - H.X) * Fx + (Y - H.Y) * Fy) / H.PatchLong, B = ((X - H.X) * Rx + (Y - H.Y) * Ry) / H.PatchWide;
				if (A * A + B * B <= 0.85 * 0.85) return true;
			}
		}
		return false;
	}

	void Yard::Dress(size_t K, bool bNear, LandField::Mesh& Out) const
	{
		Out = LandField::Mesh();
		const LandKit::Model* Mp = ModelOf(K);
		if (!Mp || !Ground) return;
		const LandKit::Model& M = *Mp;
		const Hulk& H = List[K];
		const Kit Ki = KitOf(M);
		std::vector<LandKit::PartState> S;
		PoseOf(M, Ki, H, S);
		std::vector<LandKit::Xf> Xf;
		M.Solve(LandKit::Xf(), S, Xf);
		const Quat Hull = HullTurn(H.Heading, H.Pitch, H.Roll);
		V3 TurretUp(0.0, 0.0, 1.0);
		double TurretGround = 0.0;
		if (H.bTurretOff) Ground(H.TurretX, H.TurretY, TurretGround, TurretUp);
		const Quat Thrown = TurretTurn(H, TurretUp);
		const V3 TurretAt(H.TurretX - H.X, H.TurretY - H.Y, H.TurretZ - H.Z);
		const Tone Burnt(M.BurntColour[0], M.BurntColour[1], M.BurntColour[2]);
		const double Snowed = SnowSince(Snow, H.Months);
		Tone Earth(112, 96, 72);
		if (Colour)
		{
			const Rgb8 E = Colour(H.X, H.Y);
			Earth = Mixed(Tone(E.R, E.G, E.B), Tone(104, 88, 66), 0.5);
		}
		const int OffBody = H.TrackOff != 0 ? M.TrackBody[H.TrackOff < 0 ? 0 : 1] : -1;
		for (size_t B = 0; B < M.Bodies.size(); ++B)
		{
			const LandKit::Body& Bo = M.Bodies[B];
			if (Ki.Drop[B] || static_cast<int>(B) == OffBody) continue;
			const LandKit::Mesh& Me = bNear ? Bo.Near : Bo.Far;
			const size_t Count = Me.Count();
			if (Count == 0 || Me.Points.size() < Count * 3 || Me.Normals.size() < Count * 3 || Me.Colours.size() < Count * 4) continue;
			const bool bThrown = H.bTurretOff && Ki.OnTurret[B];
			const LandKit::Xf& X = Xf[static_cast<size_t>(Bo.Part)];
			const uint32_t First = static_cast<uint32_t>(Out.Colours.size());
			const Stuff What = Bo.bTrack ? Stuff::Link : Stuff::Steel;
			for (size_t I = 0; I < Count; ++I)
			{
				V3 Local(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]);
				Stuff Is = What;
				if (Ki.Tyre[B] == 2)
				{
					// (a lorry's tyre burnt off: its rubber gone to its rim, the rim steel)
					if (OnTyre(M, Bo, Local, Ki.TyreR[B]) > Ki.RimR[B] / Ki.TyreR[B] + 0.02) Is = Stuff::Rim;
					Local = ToRim(M, Bo, Local, Ki.RimR[B]);
				}
				else if (Ki.Tyre[B] == 1 && OnTyre(M, Bo, Local, Ki.TyreR[B]) > Ki.RimR[B] / Ki.TyreR[B])
				{
					Is = Stuff::Rubber;
				}
				const V3 Rest = Bo.Pivot + Local;
				const V3 Posed = X.Apply(Rest);
				const V3 Way = X.Q.Rotate(V3(Me.Normals[I * 3], Me.Normals[I * 3 + 1], Me.Normals[I * 3 + 2]));
				V3 At, Up;
				if (bThrown)
				{
					At = ToField(Thrown.Rotate(Posed - Ki.TurretPivot)) + TurretAt;
					Up = ToField(Thrown.Rotate(Way));
				}
				else
				{
					At = ToField(Hull.Rotate(Posed));
					Up = ToField(Hull.Rotate(Way));
				}
				// (her open ring, where the turret sat: the hull's roof inside its ring)
				const bool bRing = H.bTurretOff && !bThrown && Posed.Z > M.TurretLo.Z - 0.3
					&& std::hypot(Posed.X - Ki.TurretPivot.X, Posed.Y - Ki.TurretPivot.Y) < 0.8 * (Ki.TurretR - 0.2);
				const double Soot = I < Me.Soot.size() ? Me.Soot[I] : 0.8;
				const Rgb8 C = Burn(&Me.Colours[I * 4], Soot, Rest, Up.Unit().Z, bThrown ? 1.0 : Rest.Z, H, Burnt, Earth, Is, bRing, Snowed);
				Point(Out, At, Up, C);
			}
			// (the files wind a face counter-clockwise from its front in her own axes, which turn into the field's with a
			// mirror on the way: each triangle's last two corners swapped, as the scene's KitFront does for a body)
			for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
			{
				Out.Triangles.insert(Out.Triangles.end(), { First + Me.Triangles[T], First + Me.Triangles[T + 2], First + Me.Triangles[T + 1] });
			}
		}
		// Her run-off track, laid out flat beside her: each link as it ran under her wheels (its outer face down), along
		// the strip; the far level a flat band of its width.
		if (H.TrackOff != 0 && OffBody >= 0 && !H.Strip.empty())
		{
			const Kit::Track& Tr = Ki.Tracks[H.TrackOff < 0 ? 0 : 1];
			const LandKit::Body& Bo = M.Bodies[static_cast<size_t>(OffBody)];
			const size_t Links = std::min(Tr.Mid.size(), H.Strip.size());
			std::vector<V3> At(Links), Along(Links), Across(Links), Up(Links);
			for (size_t L = 0; L < Links; ++L)
			{
				const auto& P = H.Strip[L];
				const auto& Before = H.Strip[L > 0 ? L - 1 : L];
				const auto& After = H.Strip[L + 1 < Links ? L + 1 : L];
				double Gz = 0.0;
				V3 N;
				Ground(P.first, P.second, Gz, N);
				const V3 Way = V3(After.first - Before.first, After.second - Before.second, 0.0).Unit(V3(0.0, 1.0, 0.0));
				Up[L] = N.Unit();
				Along[L] = (Way - Up[L] * Way.Dot(Up[L])).Unit(Way);
				Across[L] = Up[L].Cross(Along[L]).Unit(V3(1.0, 0.0, 0.0));
				At[L] = V3(P.first - H.X, P.second - H.Y, Gz - H.Z - 0.02);
			}
			if (bNear)
			{
				const LandKit::Mesh& Me = Bo.Near;
				std::vector<uint32_t> Index(Me.Count(), UINT32_MAX);
				for (size_t I = 0; I < Me.Count() && I < Tr.LinkOf.size(); ++I)
				{
					const int L = Tr.LinkOf[I];
					if (L < 0 || static_cast<size_t>(L) >= Links) continue;
					const size_t Li = static_cast<size_t>(L);
					const V3 P(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]);
					const V3 N(Me.Normals[I * 3], Me.Normals[I * 3 + 1], Me.Normals[I * 3 + 2]);
					const V3 Dp = P - Tr.Mid[Li];
					const V3 Side(0.0, 1.0, 0.0);
					const V3 Laid = At[Li] + Along[Li] * Dp.Dot(Tr.Along[Li]) + Up[Li] * (Tr.BMax - Dp.Dot(Tr.Out[Li])) + Across[Li] * Dp.Dot(Side);
					const V3 Facing = Along[Li] * N.Dot(Tr.Along[Li]) - Up[Li] * N.Dot(Tr.Out[Li]) + Across[Li] * N.Dot(Side);
					const double Soot = I < Me.Soot.size() ? Me.Soot[I] : 0.8;
					Index[I] = Point(Out, Laid, Facing, Burn(&Me.Colours[I * 4], Soot, Bo.Pivot + P, Facing.Unit().Z, 0.1, H, Burnt, Earth, Stuff::Link, false, Snowed));
				}
				for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
				{
					const uint32_t A = Index[Me.Triangles[T]], B2 = Index[Me.Triangles[T + 1]], C = Index[Me.Triangles[T + 2]];
					if (A == UINT32_MAX || B2 == UINT32_MAX || C == UINT32_MAX) continue;
					Wound(Out, A, B2, C);
				}
			}
			else
			{
				const double Half = 0.5 * (Tr.DMax - Tr.DMin), Thick = Tr.BMax - Tr.BMin;
				const uint8_t Steel[4] = { 80, 70, 60, 255 };
				uint32_t Was[2] = { 0, 0 };
				for (size_t L = 0; L < Links; ++L)
				{
					uint32_t Now[2];
					for (int E = 0; E < 2; ++E)
					{
						const V3 P = At[L] + Across[L] * (E == 0 ? -Half : Half) + Up[L] * Thick;
						Now[E] = Point(Out, P, Up[L], Burn(Steel, 0.9, V3(static_cast<double>(L) * 0.15, E * 0.4, 0.0), 1.0, 0.1, H, Burnt, Earth, Stuff::Link, false, Snowed));
					}
					if (L > 0)
					{
						Wound(Out, Was[0], Was[1], Now[1]);
						Wound(Out, Was[0], Now[1], Now[0]);
					}
					Was[0] = Now[0];
					Was[1] = Now[1];
				}
			}
		}
	}

	void Yard::Patch(size_t K, LandField::Mesh& Out) const
	{
		Out = LandField::Mesh();
		if (K >= List.size() || !DrawnZ) return;
		const Hulk& H = List[K];
		double Fx, Fy, Rx, Ry;
		Ways(H.Heading, Fx, Fy, Rx, Ry);
		const double M = std::max(0.0, H.Months);
		// How it goes with the months: scorched black at first (the burnt grass, her oil and fuel soaked in), the ash
		// washed into a burnt brown within a few months, then the weeds come up lusher and darker than the field round
		// it, fading into it over the second year. Under the snow fallen since she burnt, none of it: the field's own
		// snow (Colour) lies over her patch as over the field round it, and only a fresh one shows black through it.
		const double Snowed = SnowSince(Snow, M);
		const double Scorch = (M < 1.0 ? 0.9 : 0.9 * std::exp(-(M - 1.0) / 2.5)) * (1.0 - Snowed);
		const double Brown = Smooth(0.5, 3.0, M) * (1.0 - Smooth(6.0, 14.0, M)) * (1.0 - Snowed);
		const double Lush = Smooth(2.0, 8.0, M) * (1.0 - 0.6 * Smooth(18.0, 36.0, M)) * (1.0 - Snowed);
		constexpr int Spokes = PatchSpokes;
		const double* Rings = PatchRing;
		constexpr int RingCount = PatchRings;
		double Reach[Spokes];
		for (int S = 0; S < Spokes; ++S)
		{
			Reach[S] = SpokeReach(H, S);
		}
		std::vector<uint32_t> Index(static_cast<size_t>(Spokes * RingCount));
		for (int Ri = 0; Ri < RingCount; ++Ri)
		{
			for (int S = 0; S < Spokes; ++S)
			{
				double X = 0.0, Y = 0.0;
				PatchAt(H, Fx, Fy, Rx, Ry, Ri, S, Reach[S], X, Y);
				double Nx = 0.0, Ny = 0.0, Nz = 1.0;
				if (DrawnUp) DrawnUp(X, Y, Nx, Ny, Nz);
				Tone Own(96, 108, 60);
				if (Colour)
				{
					const Rgb8 C = Colour(X, Y);
					Own = Tone(C.R, C.G, C.B);
				}
				// (strongest under her and close round her, feathered out to the field's own at its edge)
				const double N = Noise3(X * 0.6, Y * 0.6, 1.5, H.Seed ^ 0xF6ull);
				const double In = 1.0 - Smooth(0.5, 1.0, Rings[Ri] + 0.16 * (N - 0.5));
				Tone C = Own;
				C = Mixed(C, Tone(32, 28, 24), Scorch * In * (Rings[Ri] < 0.45 ? 1.0 : 0.85));
				C = Mixed(C, Tone(84, 66, 46), 0.55 * Brown * In);
				const double Ring = Smooth(0.25, 0.6, Rings[Ri]) * (1.0 - Smooth(0.85, 1.0, Rings[Ri]));
				C = Mixed(C, Tone(Own.R * 0.72, Own.G * 0.86, Own.B * 0.6), Lush * (0.35 + 0.4 * Ring) * In);
				const double Z = DrawnZ(X, Y) + 0.02 - H.Z;
				Index[static_cast<size_t>(Ri * Spokes + S)] = Point(Out, V3(X - H.X, Y - H.Y, Z), V3(Nx, Ny, Nz), ToRgb(C));
				if (Ri == 0) break;
			}
		}
		// (the middle point once; its ring's others point at it)
		for (int S = 1; S < Spokes; ++S) Index[static_cast<size_t>(S)] = Index[0];
		for (int Ri = 0; Ri + 1 < RingCount; ++Ri)
		{
			for (int S = 0; S < Spokes; ++S)
			{
				const int S1 = (S + 1) % Spokes;
				const uint32_t A = Index[static_cast<size_t>(Ri * Spokes + S)], B = Index[static_cast<size_t>(Ri * Spokes + S1)];
				const uint32_t C = Index[static_cast<size_t>((Ri + 1) * Spokes + S)], D = Index[static_cast<size_t>((Ri + 1) * Spokes + S1)];
				if (Ri > 0) Wound(Out, A, B, D);
				Wound(Out, A, D, C);
			}
		}
	}

	std::string Yard::Said() const
	{
		std::map<std::string, std::map<std::string, int>> ByNation;
		int Thrown = 0, Tracks = 0;
		for (const Hulk& H : List)
		{
			++ByNation[H.Nation][H.Model];
			Thrown += H.bTurretOff ? 1 : 0;
			Tracks += H.TrackOff != 0 ? 1 : 0;
		}
		// (the months as a person would say them: a battle a week back is "under a month", not "0 months")
		const long Whole = std::lround(Asked.Months);
		const std::string When = Whole < 1 ? std::string("under a month") : std::to_string(Whole) + (Whole == 1 ? " month" : " months");
		char Head[256];
		if (Asked.bAsLaid)
		{
			// (the Mac's stand-in: as many as it asked, their months their look's alone)
			std::snprintf(Head, sizeof(Head), "%d laid of %d asked (the stand-in: none taken by the scrap men), %s since they burnt (at most %d on a field)",
				static_cast<int>(List.size()), Wanted, When.c_str(), Most);
		}
		else if (PerOf(Asked.Per) > 1.0)
		{
			// (the battle's scale: each drawn wreck as many real machines as a drawn tank)
			std::snprintf(Head, sizeof(Head), "%d laid of %d the scrap men left of %d drawn for %d burnt out here (one for every %.1f, the battle's scale), %s since the last (%.0f%% left; at most %d on a field)",
				static_cast<int>(List.size()), Wanted, LandHulk::Models(Asked.Count, Asked.Per), std::clamp(Asked.Count, 0, MostCount), PerOf(Asked.Per), When.c_str(),
				100.0 * Left(Asked.Months), Most);
		}
		else
		{
			std::snprintf(Head, sizeof(Head), "%d laid of %d the scrap men left of %d burnt out here, %s since the last (%.0f%% left; at most %d on a field)",
				static_cast<int>(List.size()), Wanted, std::clamp(Asked.Count, 0, MostCount), When.c_str(), 100.0 * Left(Asked.Months), Most);
		}
		std::string Out = Head;
		std::string Of;
		for (const auto& [Nat, Kinds] : ByNation)
		{
			int Sum = 0;
			std::string Each;
			for (const auto& [Name, N] : Kinds)
			{
				Sum += N;
				Each += (Each.empty() ? "" : ", ") + std::to_string(N) + " " + Name;
			}
			Of += (Of.empty() ? "" : "; ") + std::to_string(Sum) + " " + Nat + " (" + Each + ")";
		}
		if (!Of.empty()) Out += ": " + Of;
		char Tail[256];
		std::snprintf(Tail, sizeof(Tail), "; %d %s thrown, %d %s run off; %d %s along the old lines, %d at a junction or the village's edge, %d on the battle's axis, %d strewn",
			Thrown, Thrown == 1 ? "turret" : "turrets", Tracks, Tracks == 1 ? "track" : "tracks", ByWhere[0], ByWhere[0] == 1 ? "group" : "groups",
			ByWhere[1], ByWhere[2], ByWhere[3]);
		Out += Tail;
		if (NoRoom > 0) Out += "; " + std::to_string(NoRoom) + " found no room clear of the lanes, hedges and the battle's own ways";
		return Out;
	}
}
