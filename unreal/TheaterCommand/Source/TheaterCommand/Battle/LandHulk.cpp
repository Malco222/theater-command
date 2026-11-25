#include "Battle/LandHulk.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

		/** How far a place lies from a straight piece of line (A to B; a point where they are one). */
		double FromSegment(double Ax, double Ay, double Bx, double By, double X, double Y)
		{
			const double Dx = Bx - Ax, Dy = By - Ay, Long2 = Dx * Dx + Dy * Dy;
			const double T = Long2 > 0.0 ? std::clamp(((X - Ax) * Dx + (Y - Ay) * Dy) / Long2, 0.0, 1.0) : 0.0;
			return std::hypot(X - Ax - T * Dx, Y - Ay - T * Dy);
		}

		/** Where a track is drawn at battle time T, as LandKit's Vehicle::PlaceAt draws a machine: her kept points joined
		 * straight, her first before it and her last after it, and a step faster than 25 m/s (the recording putting her
		 * somewhere, not her driving there) held at its start. False where none of her points there is a number. */
		bool PlaceOf(const LandFeed::Track& Tr, double T, double& X, double& Y)
		{
			const auto Ok = [](const LandFeed::Point& P) { return P.X == P.X && P.Y == P.Y && P.T == P.T; };
			const std::vector<LandFeed::Point>& A = Tr.At;
			if (A.empty()) return false;
			size_t Hi = 0;
			while (Hi < A.size() && A[Hi].T <= T) ++Hi;
			if (Hi == 0 || Hi == A.size())
			{
				const LandFeed::Point& P = A[Hi == 0 ? 0 : A.size() - 1];
				if (!Ok(P)) return false;
				X = P.X;
				Y = P.Y;
				return true;
			}
			const LandFeed::Point& L = A[Hi - 1];
			const LandFeed::Point& H = A[Hi];
			if (!Ok(L) || !Ok(H))
			{
				if (!Ok(L) && !Ok(H)) return false;
				X = Ok(L) ? L.X : H.X;
				Y = Ok(L) ? L.Y : H.Y;
				return true;
			}
			const double Span = H.T - L.T;
			if (Span <= 1e-9 || std::hypot(H.X - L.X, H.Y - L.Y) > 25.0 * Span)
			{
				X = L.X;
				Y = L.Y;
				return true;
			}
			const double U = (T - L.T) / Span;
			X = L.X + (H.X - L.X) * U;
			Y = L.Y + (H.Y - L.Y) * U;
			return true;
		}

		/** How far a place lies from a convex outline's edge (its corners in order, either way round): under 0 inside. */
		double FromConvex(const std::vector<std::pair<double, double>>& P, double X, double Y)
		{
			double Edge = 1e18;
			int Left = 0, Right = 0;
			for (size_t K = 0; K < P.size(); ++K)
			{
				const std::pair<double, double>& A = P[K];
				const std::pair<double, double>& B = P[(K + 1) % P.size()];
				Edge = std::min(Edge, FromSegment(A.first, A.second, B.first, B.second, X, Y));
				const double Cross = (B.first - A.first) * (Y - A.second) - (B.second - A.second) * (X - A.first);
				if (Cross > 1e-9) ++Left;
				else if (Cross < -1e-9) ++Right;
			}
			return Left > 0 && Right > 0 ? Edge : -Edge;
		}

		/** How far a place lies from a box's edge ([x0, y0, x1, y1]): under 0 inside (1e18 for a box of fewer numbers). */
		double FromBox(const std::vector<double>& B, double X, double Y)
		{
			if (B.size() < 4) return 1e18;
			const double Ox = std::max({ B[0] - X, 0.0, X - B[2] }), Oy = std::max({ B[1] - Y, 0.0, Y - B[3] });
			return Ox > 0.0 || Oy > 0.0 ? std::hypot(Ox, Oy) : -std::min({ X - B[0], B[2] - X, Y - B[1], B[3] - Y });
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
			/** Her turret ring's open hole, its radius about TurretPivot (the turret's own base, the collar it sat in, less
			 * the hull's ring round it: some 0.75 m on a Panzer IV, 0.88 on a Tiger), and the hull's roof height there. */
			double RingR = 0.0;
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
			/** What the fire made of each paint (Model::PaintNames' numbers): her coat and steel burnt, her canvas, leather,
			 * rope and cloth burnt away, her glass, wood and rubber each their own way. */
			enum class Matter : uint8_t { Coat, Gone, Glass, Wood, Rubber };
			std::vector<Matter> PaintIs;
			/** The fire's places at rest (her engine, the hatches the flames came out of, her turret's ring: the sockets
			 * LandKit's soot reads), her height, and her engine's end along her (+1 her bow, -1 her tail). */
			std::vector<V3> Fires;
			/** Each of the fire's places on her turret (a turret hatch's), which a thrown turret takes with it. */
			std::vector<uint8_t> FireOnTurret;
			double Top = 2.0, EngineEnd = -1.0;
			/** Which of the fire's places is her turret's ring (-1 none): the fire inside her heats her turret from there,
			 * but its smoke comes out of her hatches unless her turret was thrown and left the ring open. */
			int RingFire = -1;
			/** A lorry's tilt (its canvas burnt off its hoops): the part (-1 none). */
			int Tilt = -1;
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
			const LandKit::Body* TurretBody = nullptr;
			for (size_t B = 0; B < Bodies; ++B)
			{
				const LandKit::Body& Bo = M.Bodies[B];
				// (her aerial burnt off or snapped, her roof gun and its cradle and a carrier's gun mount burnt or taken)
				if (Bo.Name == "aerial" || Contains(Bo.Name, "roof_mg") || Bo.Name == "mg_mount") K.Drop[B] = 1;
				if (Under(M, Bo.Part, M.Turret))
				{
					K.OnTurret[B] = 1;
					if (Bo.Part == M.Turret)
					{
						TurretPoints = Bo.Near.Count();
						TurretBody = &Bo;
					}
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
				// (the ring: the turret's own base, its lowest points within 4 cm, reaches out to the collar it turns in;
				// the hull's ring round it some 9 cm inside that. Round 2 blacked a disc of 0.8 of her turret box's half
				// diagonal, 1.1 m on a Panzer IV, over the roof round the ring too)
				if (TurretBody && TurretBody->Near.Count() > 0)
				{
					const LandKit::Mesh& Me = TurretBody->Near;
					double Low = 1e9, Reach = 0.0;
					for (size_t I = 0; I < Me.Count(); ++I) Low = std::min(Low, TurretBody->Pivot.Z + Me.Points[I * 3 + 2]);
					for (size_t I = 0; I < Me.Count(); ++I)
					{
						if (TurretBody->Pivot.Z + Me.Points[I * 3 + 2] > Low + 0.04) continue;
						Reach = std::max(Reach, std::hypot(TurretBody->Pivot.X + Me.Points[I * 3] - T.Pivot.X, TurretBody->Pivot.Y + Me.Points[I * 3 + 1] - T.Pivot.Y));
					}
					K.RingR = std::clamp(Reach - 0.09, 0.35, std::max(0.35, K.TurretR - 0.3));
				}
			}
			// What the fire made of each paint, by its name before any dot (a half-track's "canvas.seat", a T-34's unditching
			// log's "wood.end": round 2 matched the whole name, and dressed the M3's seats and the T-34's logs as steel).
			for (const std::string& Whole : M.PaintNames)
			{
				const std::string Name = Whole.substr(0, Whole.find('.'));
				Kit::Matter Is = Kit::Matter::Coat;
				if (Name == "canvas" || Name == "leather" || Name == "rope" || Name == "cloth" || Name == "torn" || Name == "skin") Is = Kit::Matter::Gone;
				else if (Name == "glass" || Name == "lamp") Is = Kit::Matter::Glass;
				else if (Name == "wood") Is = Kit::Matter::Wood;
				else if (Name == "rubber") Is = Kit::Matter::Rubber;
				K.PaintIs.push_back(Is);
			}
			// The fire's places (as LandKit's soot reads them), her height and her engine's end.
			for (const LandKit::Socket& So : M.Sockets)
			{
				if (So.Name == "ring") K.RingFire = static_cast<int>(K.Fires.size());
				if (So.Name == "engine" || So.Name.compare(0, 5, "fire.") == 0 || So.Name == "ring")
				{
					K.Fires.push_back(So.At);
					K.FireOnTurret.push_back(M.Turret >= 0 && Under(M, So.Part, M.Turret) ? 1 : 0);
				}
				if (So.Name == "engine") K.EngineEnd = So.At.X >= K.MidX ? 1.0 : -1.0;
			}
			K.Top = std::max({ 1.0, M.HullHi.Z, M.Turret >= 0 ? M.TurretHi.Z : 0.0 });
			K.Tilt = M.PartIndex("tilt");
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

		enum class Stuff : uint8_t { Steel, Rubber, Link, Rim, Wood, Glass };

		/** Two noises, a stride and a hand across, 0 to 1 (about 0.5, mostly within 0.3 to 0.7). */
		double Fbm(const V3& P, double Scale, uint64_t S)
		{
			const double A = Noise3(P.X * Scale, P.Y * Scale, P.Z * Scale, S);
			const double B = Noise3(P.X * Scale * 2.3, P.Y * Scale * 2.3, P.Z * Scale * 2.3, S ^ 0x9Bull);
			return 0.65 * A + 0.35 * B;
		}
		/** A noise spread evenly over 0 to 1 (Noise3's own bunch about 0.5): what share of a surface lies under a value. */
		double Even(double N) { return Smooth(0.1, 0.9, N); }

		/**
		 * Where the fire and the months left each point of her (FateOf). The last round (the art director's notes of 24
		 * November: round 2's patchwork read as camouflage, a week on hard black blotches over pale chalky grey, after
		 * months pale blotches on rust; every boundary a cut line, the patches anywhere, the noise outweighing the fire):
		 * the fire decides where, the noise only roughens the edges and breaks the coats up a little. Two fields over her,
		 * each smooth across her plates: her heat, hottest round where the flames came out (her engine deck's grilles, her
		 * hatches, her turret's ring), leaning downwind, high on her and a little more on what faces up; and her soot,
		 * densest round those openings, streaking downwind and up from them (not down the outside of a plate below an
		 * opening: the smoke rose past it), burnt off the hottest steel. Each coat blends into the next across a band of
		 * its field: her own paint; a narrow fringe of it heat-tinted brown; the paint scorched chalky grey-buff, then
		 * blistered off to the bare steel in the hottest centres; the soot over them all, feathered through grey at its
		 * edges and grained along the wind. (Each point's coat for the cloud test's measure, Coat, is the one on its side
		 * of each band's middle.) Set by what the cloud test measures of her lit faces in its look sheet against the art
		 * director's numbers: soot over about half of what faces the sun a week on, some 20 in 100 scorched grey-buff and
		 * 10 bare grey steel, her own paint a few in 100; the bare steel rusted, some 15 to 25 in 100 of her, a month on.
		 */
		constexpr double BareAt = 0.6, ScorchAt = 0.4, SootAt = 0.45;
		/** Each band's half width in its field's units (her heat falls some 0.25 a metre near the fire's places: a band of
		 * 0.05 either way is some 0.4 m across, more where her heat lies flatter): the bare steel's edge into the
		 * blistered paint, the scorched paint's into her own, the heat tint's fringe under the scorched paint's edge, the
		 * soot's feathered edge. */
		constexpr double BareBand = 0.05, ScorchBand = 0.045, TintBand = 0.05, SootBand = 0.09;
		/** How far the steel's edge eats back into the scorched paint by a year and a half (a share of the way from the
		 * bare steel's edge to the scorched paint's outer one): the paint flaking from the hottest first, more of it each
		 * season (round 3's first try a quarter of the way: six months, a year and a year and a half looked alike). */
		constexpr double Flaking = 0.5;
		/** Her coat's plates halved down to these (metres) before she is dressed (Halve): the near level so her coats'
		 * bands lie where her heat puts them where the camera sees her close (round 2 halved to a quarter metre: 57 in 100
		 * more triangles than round 1; at 0.35 some 22 in 100 more, her measured look the same), the far one so its few
		 * big plates carry the same coats at a coarser grain (her heat's lie is metres across). */
		constexpr double NearHalve = 0.35, FarHalve = 0.7;

		/** Her height over each spot of her at rest (a hand and a half square): what lies above a point of her side is
		 * her own plate up to her top there, then her top, whose rust runs over the edge and down her. */
		struct Tops
		{
			static constexpr double Cell = 0.15;
			double X0 = 0.0, Y0 = 0.0;
			int Nx = 0, Ny = 0;
			std::vector<float> Z;
			/** Each cell's top's rust, read as her sides under it first ask it (-1 not yet: TopRust). */
			mutable std::vector<float> Rusted;
			size_t CellOf(double X, double Y) const
			{
				const int I = std::clamp(static_cast<int>(std::floor((X - X0) / Cell)), 0, Nx - 1);
				const int J = std::clamp(static_cast<int>(std::floor((Y - Y0) / Cell)), 0, Ny - 1);
				return static_cast<size_t>(J) * static_cast<size_t>(Nx) + static_cast<size_t>(I);
			}
			double At(double X, double Y) const { return Nx > 0 && Ny > 0 ? Z[CellOf(X, Y)] : -1e9; }
			/** Her height over a spot, none (-1e9) off her (At reads the nearest cell's). */
			double Within(double X, double Y) const
			{
				const double I = (X - X0) / Cell, J = (Y - Y0) / Cell;
				return I >= 0.0 && J >= 0.0 && I < static_cast<double>(Nx) && J < static_cast<double>(Ny) ? Z[CellOf(X, Y)] : -1e9;
			}
		};

		/** What dressing one wreck reads for every point: her, her age, the fire's places (at rest) and her height, the way
		 * the wind blew as she burnt (across her, at rest), her burnt colour, the earth under her, the snow fallen on her
		 * since. */
		struct Fire
		{
			const Hulk* H = nullptr;
			double Months = 0.0, Snowed = 0.0, Top = 2.0;
			/** How much hotter her thin plates burnt (a lorry's, a half-track's: their paint burnt off, their soot burnt
			 * off it), on her heat. */
			double Hotter = 0.0;
			/** Her own thresholds: her bare steel's and her scorched paint's (BareAt and ScorchAt, moved for her by
			 * ThresholdsFor). */
			double Bare = BareAt, Scorch = ScorchAt;
			/** How far her heat's lie is drawn out for her (ThresholdsFor: a wreck that burnt cool all over keeps the
			 * fire's shape on her, not the noise's). */
			double Gain = 1.0;
			std::vector<V3> At;
			/** Which of At are on her turret (a turret hatch's), her turret's ring among them (-1 none), and whether its
			 * smoke came out of it (her turret thrown: the hull's fire then never came out of the turret's hatches). */
			std::vector<uint8_t> OnTurret;
			int Ring = -1;
			bool bRingOpen = false;
			/** Her thrown turret's own dressing: blown off her as the fire began, it burnt only from its own hatches,
			 * neither the ring under it nor her engine deck reaching it (round 3's first try heated it from her ring as
			 * if it still sat there: it went wholly bare grey a week on, wholly orange a month on). */
			bool bThrown = false;
			V3 Wind = V3(1.0, 0.0, 0.0);
			Tone Burnt, Earth;
			/** Her height over each spot of her (for the runs of rust down her sides; none: no runs). */
			const Tops* Over = nullptr;
		};

		/** Where a point of her stands as the fire and the months left it (her place at rest), each coat's share over the
		 * one under it, 0 to 1 across its band: its heat; the heat-tinted fringe, the scorched paint, the steel showing
		 * (burnt bare, or the paint flaked off it) and how far it has rusted, and the rust's stain on the paint round it; the
		 * soot left on it after the rain, the soot there was before it, and the soot's grain (0 its greyest, 1 its blackest). */
		struct Fate
		{
			double Heat = 0.0;
			double Tint = 0.0, Scorch = 0.0, Steel = 0.0, Rust = 0.0, Stain = 0.0, Soot = 0.0, SootWas = 0.0, Grain = 0.5;
		};

		/** How near a point (N its way out, at rest) lies to where the flames came out, 0 to 1, for her heat and for her
		 * soot: each fire place's reach leaning a little downwind (the flames bent on the wind), the smoke's stretched far
		 * downwind and up (it streaked her plates as it went) and cut short upwind and below the opening, shorter still on
		 * a plate below the opening that faces away from it (the outside of a half-track's body under the fire in it: round
		 * 3's first try laid a black band along it, straight-edged, as a painted stripe); the places' reaches run together
		 * (one minus each one's share left out), so no crease where two meet. */
		void NearFire(const Fire& W, const V3& At, const V3& N, double& Heat, double& Smoke)
		{
			if (W.At.empty())
			{
				Heat = Smoke = 0.5;
				return;
			}
			double ColdH = 1.0, ColdS = 1.0;
			for (size_t I = 0; I < W.At.size(); ++I)
			{
				// (a thrown turret burnt from its own hatches only; the hull left behind from all but those)
				const bool bOnTurret = I < W.OnTurret.size() && W.OnTurret[I] != 0;
				if (W.bThrown ? !bOnTurret : W.bRingOpen && bOnTurret) continue;
				const V3 D = At - W.At[I];
				const double Down = D.X * W.Wind.X + D.Y * W.Wind.Y, Across = D.Y * W.Wind.X - D.X * W.Wind.Y;
				const double Hd = Down > 0.0 ? Down / 1.3 : Down / 0.9, Hz = D.Z > 0.0 ? D.Z / 1.2 : D.Z / 0.5;
				ColdH *= 1.0 - Smooth(2.1, 0.3, std::sqrt(Hd * Hd + Across * Across + Hz * Hz));
				if (static_cast<int>(I) == W.Ring && !W.bRingOpen) continue;
				const double Dl = D.Length();
				const double Away = Dl > 1e-6 ? Clamp01(N.Dot(D) / Dl) : 0.0;
				const double Shield = 1.0 + 1.4 * Away * Smooth(0.0, 0.6, -D.Z);
				const double Sd = Down > 0.0 ? Down / 2.2 : Down / 0.7, Sa = Across / 0.9, Sz = D.Z > 0.0 ? D.Z / 1.4 : D.Z / 0.7;
				ColdS *= 1.0 - Smooth(2.0, 0.25, Shield * std::sqrt(Sd * Sd + Sa * Sa + Sz * Sz));
			}
			Heat = 1.0 - ColdH;
			Smoke = 1.0 - ColdS;
		}

		/** Her heat's noise's reach either way (its noises 0 to 1). */
		constexpr double HeatPatches = 0.3, HeatRag = 0.12, HeatNoise = 0.5 * (HeatPatches + HeatRag);
		/** Her heat at a point before its noise (NearH its nearness to the fire for the heat): the fire's places leading,
		 * her height, what faces up and her model's own soot after them (round 3's first try counted what faces up some
		 * half as much again: her top and her sides split at the plate's edge, a rust lid on her years on). */
		double HeatBuilt(const Fire& W, const V3& At, double RestUp, double SootW, double NearH)
		{
			return (0.48 * NearH + 0.18 * Clamp01(At.Z / W.Top) + 0.05 * Clamp01(RestUp) + 0.05 * Clamp01((SootW - 0.5) / 0.5)) * (0.92 + 0.4 * (W.H->Fire - 0.8))
				* W.Gain + W.Hotter;
		}
		/** Her heat at a point: Built and the noise a little, in patches a stride and more across, ragged at a hand or two. */
		double HeatAt(const Fire& W, const V3& At, double Built)
		{
			const uint64_t S = W.H->Seed;
			const double Patches = Fbm(At, 0.9, S ^ 0x11ull);
			const double Rag = Noise3(At.X * 1.7, At.Y * 1.7, At.Z * 1.7, S ^ 0x22ull);
			return Built + HeatPatches * (Patches - 0.5) + HeatRag * (Rag - 0.5);
		}

		/** The steel's edge after the months: the heat a point needs to show its steel (the bare steel's at first, then
		 * eating back into the scorched paint, the hottest flaking first), and a point's heat as that edge reads it,
		 * ragged where the paint flakes (FlakeRag either way at most). */
		double SteelEdge(const Fire& W) { return W.Bare - Flaking * Smooth(0.5, 15.0, W.Months) * (W.Bare - W.Scorch); }
		constexpr double FlakeRag = 0.12;
		/** How far out from the steel's band the rust's stain reaches into the paint round it, in her heat (some 0.25 m). */
		constexpr double StainBand = 0.06;
		double FlakedHeat(const Fire& W, const V3& At, double Heat)
		{
			const double Ragged = Smooth(0.5, 6.0, W.Months);
			if (!(Ragged > 0.0)) return Heat;
			return Heat + FlakeRag * Ragged * (Noise3(At.X * 1.8, At.Y * 1.8, At.Z * 1.8, W.H->Seed ^ 0x55ull) - 0.5);
		}
		/** The steel showing at a point of that heat, 0 to 1 (its flaking noise read only where it could lift it there). */
		double SteelOf(const Fire& W, const V3& At, double Heat, double Edge)
		{
			if (!(Heat + 0.5 * FlakeRag > Edge - BareBand)) return 0.0;
			return Smooth(Edge - BareBand, Edge + BareBand, FlakedHeat(W, At, Heat));
		}

		/** The soot on a point: left after the rain, there before it, and its grain. */
		struct Sooted
		{
			double Left = 0.0, Was = 0.0, Grain = 0.5;
		};
		constexpr double SootStreak = 0.22, SootPatches = 0.16, SootBurn = 0.3;
		/** How much of her soot's score the hottest steel burnt off (Heat its heat). */
		double SootBurnt(const Fire& W, double Heat) { return SootBurn * Smooth(W.Bare - 0.02, W.Bare + 0.12, Heat); }
		/** Her soot's score at a point before the hottest steel burnt it off (Smoke its nearness to the openings for the
		 * smoke): densest round where the flames came out and streaking downwind and up from there (its grain long down
		 * the wind on her deck, up her sides), a little more on what faces up; -1 where its noise could not lift it to its
		 * band with Burnt off it. */
		double SootScore(const Fire& W, const V3& At, double RestUp, double SootW, double Smoke, double Burnt)
		{
			const uint64_t S = W.H->Seed;
			const double Built = 0.7 * Smoke + 0.1 * Clamp01(RestUp) + 0.06 * Clamp01((SootW - 0.5) / 0.5);
			if (!(Built - Burnt + 0.5 * (SootStreak + SootPatches) > SootAt - SootBand)) return -1.0;
			const double Along = At.X * W.Wind.X + At.Y * W.Wind.Y, Across = At.Y * W.Wind.X - At.X * W.Wind.Y;
			const double Streak = Noise3(Along * 0.7, Across * 2.4, At.Z * 0.6, S ^ 0x3Cull);
			const double Patches = Noise3(At.X * 1.1, At.Y * 1.1, At.Z * 1.1, S ^ 0x33ull);
			return Built + SootStreak * (Streak - 0.5) + SootPatches * (Patches - 0.5);
		}
		/** The soot on a point after the months (Heat its heat): its score less what the hottest steel burnt off,
		 * feathered across SootBand, its grain blackest deep in it and along its streaks (round 3's first try one flat
		 * black: a cap of black paint). The rain washes it off what faces the sky within half a year, its edge soft and
		 * ragged, less off her sides, little from under her. */
		Sooted SootOf(const Fire& W, const V3& At, double RestUp, double SootW, double UpZ, double Smoke, double Heat)
		{
			Sooted O;
			const uint64_t S = W.H->Seed;
			const double Burnt = SootBurnt(W, Heat);
			const double Score = SootScore(W, At, RestUp, SootW, Smoke, Burnt) - Burnt;
			O.Was = Smooth(SootAt - SootBand, SootAt + SootBand, Score);
			if (!(O.Was > 0.0)) return O;
			// (its grain: a finer streak along the wind, and how deep in it a point lies)
			const double Along = At.X * W.Wind.X + At.Y * W.Wind.Y, Across = At.Y * W.Wind.X - At.X * W.Wind.Y;
			const double Fine = Noise3(Along * 1.3, Across * 5.0, At.Z * 1.2, S ^ 0x3Dull);
			O.Grain = Clamp01(0.3 + 1.1 * (Score - SootAt) + 1.3 * (Fine - 0.5));
			O.Left = O.Was;
			const double Face = 0.1 + 0.3 * Smooth(-0.5, 0.0, UpZ) + 0.55 * Smooth(0.3, 0.7, UpZ);
			const double Washed = Smooth(0.7, 7.0, W.Months) * Face;
			if (!(Washed > 0.0)) return O;
			const double Rain = Even(Noise3(At.X * 2.0, At.Y * 2.0, At.Z * 0.8, S ^ 0x44ull));
			O.Left = O.Was * (1.0 - Smooth(Rain - 0.12, Rain + 0.12, 1.1 * Washed));
			return O;
		}

		/** How far the steel showing at a point has rusted, 0 to 1: within a month, a little ragged on its way. */
		double RustsBy(double Months) { return Smooth(0.3, 0.95, Months); }
		double RustOf(const Fire& W, const V3& At)
		{
			const double Rusts = RustsBy(W.Months);
			if (!(Rusts > 0.0)) return 0.0;
			return Smooth(0.0, 1.0, (1.25 * Rusts - Even(Noise3(At.X * 3.4, At.Y * 3.4, At.Z * 3.4, W.H->Seed ^ 0x66ull))) / 0.25);
		}

		Fate FateOf(const Fire& W, const V3& At, const V3& N, double SootW, double UpZ)
		{
			Fate F;
			double NearH = 0.5, Smoke = 0.5;
			NearFire(W, At, N, NearH, Smoke);
			const double Built = HeatBuilt(W, At, N.Z, SootW, NearH);
			const double Edge = SteelEdge(W);
			// (where the steel's band begins, or as it rusts the rust's stain's round it)
			const double Rusts = RustsBy(W.Months);
			const double From = Edge - BareBand - (Rusts > 0.0 ? StainBand : 0.0);
			// (its noise read only where it could lift her heat to a band: the tint's, the steel's or its stain's as the
			// flaking reads it, the soot's burning off; below them all every share it sets is none)
			const double Least = std::min({ W.Scorch - ScorchBand - TintBand, From - 0.5 * FlakeRag, W.Bare - 0.02 });
			F.Heat = Built + HeatNoise > Least ? HeatAt(W, At, Built) : Built;
			F.Tint = Smooth(W.Scorch - ScorchBand - TintBand, W.Scorch - 0.3 * ScorchBand, F.Heat);
			F.Scorch = Smooth(W.Scorch - ScorchBand, W.Scorch + ScorchBand, F.Heat);
			// (the steel showing, as SteelOf; and as it rusts its stain: its flaking noise read only where it could lift it to
			// either)
			if (F.Heat + 0.5 * FlakeRag > From)
			{
				const double Flaked = FlakedHeat(W, At, F.Heat);
				F.Steel = Smooth(Edge - BareBand, Edge + BareBand, Flaked);
				if (Rusts > 0.0) F.Stain = Rusts * Smooth(From, Edge, Flaked);
			}
			const Sooted So = SootOf(W, At, N.Z, SootW, UpZ, Smoke, F.Heat);
			F.Soot = So.Left;
			F.SootWas = So.Was;
			F.Grain = So.Grain;
			F.Rust = F.Steel > 0.0 ? RustOf(W, At) : 0.0;
			return F;
		}

		double RustAt(const Fire& W, const V3& At, const V3& N, double SootW, double UpZ);
		/** The rust showing on her top over a point of her side (Tops: its cell's highest point, facing up), read once a
		 * cell for all her side under it. */
		double TopRust(const Fire& W, const V3& At)
		{
			const Tops& T = *W.Over;
			const size_t C = T.CellOf(At.X, At.Y);
			if (!(T.Rusted[C] >= 0.0f))
			{
				const V3 Mid(T.X0 + (static_cast<double>(C % static_cast<size_t>(T.Nx)) + 0.5) * Tops::Cell, T.Y0 + (static_cast<double>(C / static_cast<size_t>(T.Nx)) + 0.5) * Tops::Cell, T.Z[C]);
				T.Rusted[C] = static_cast<float>(RustAt(W, Mid, V3(0.0, 0.0, 1.0), 0.5, 0.9));
			}
			return T.Rusted[C];
		}

		/** The rust showing at a point to run down her side from it (FateOf's steel, rusted, out from under the soot), read
		 * in the order that settles it soonest (a run of rust down her side asks it of up to three points above each point
		 * of her sides). */
		double RustAt(const Fire& W, const V3& At, const V3& N, double SootW, double UpZ)
		{
			if (!(RustsBy(W.Months) > 0.0)) return 0.0;
			double NearH = 0.5, Smoke = 0.5;
			NearFire(W, At, N, NearH, Smoke);
			const double Built = HeatBuilt(W, At, N.Z, SootW, NearH);
			const double Edge = SteelEdge(W);
			// (none where her heat's noise and the flaking's could not lift it to the steel's band)
			if (!(Built + HeatNoise + 0.5 * FlakeRag > Edge - BareBand)) return 0.0;
			const double Heat = HeatAt(W, At, Built);
			const double Steel = SteelOf(W, At, Heat, Edge);
			if (!(Steel > 0.0)) return 0.0;
			const double Rust = RustOf(W, At);
			if (!(Rust > 0.0)) return 0.0;
			return Steel * Rust * (1.0 - SootOf(W, At, N.Z, SootW, UpZ, Smoke, Heat).Left);
		}

		/**
		 * Her own thresholds, so each wreck (not only the look sheet's four together) comes to the art director's shares:
		 * her flash rust a month on some 15 to 25 in 100 of her (round 2: its Tiger 37 in 100, its Panzer IV 6, the Soviet
		 * lorry 7, at the one threshold for all), and her scorched paint round it a week on some 15 to 25. Read on what
		 * faces up on her upper works, as the look's sun finds them (her coat's near level at rest from UpperFrom of her
		 * height up: her deck, her turret's roof and what stands on them; by area, a big plate's triangle read at points a
		 * hand or so apart over it, a small one's every third; a thrown turret, blown off her as the fire began, not
		 * counted) a month on: the share whose steel shows past the middle of its band and not under soot held within 15
		 * to 22 in 100 (the edges' orange, the runs and a thrown track's links come on top), the soot over it burnt off as
		 * her steel there grows hotter. Too much of her: BareAt moved up for her, 0.15 at most. Too little (a wreck that
		 * burnt cool all over, or whose hottest steel lies under her soot): her heat's lie drawn out (Gain, up to 2.5
		 * times) till it reaches there, so that the fire's places still say where on her, not the noise; BareAt moved
		 * down for her, 0.15 at most, only past that. Then the heat over which her scorched paint begins set where the
		 * share between the two, not under soot, is some 34 in 100 (42 where she burnt fiercest), less where her soot
		 * leaves too little. (Round 3's first try moved BareAt alone, 0.15 down at most, and kept her scorched paint's
		 * over 0.2: a third of the field's wrecks stopped there and the field's scorched paint halved.) The same for her
		 * near and far levels.
		 */
		constexpr double UpperFrom = 0.4;
		void ThresholdsFor(const LandKit::Model& M, const Kit& Ki, const Hulk& H, Fire W, double& Bare, double& Scorch, double& Gain)
		{
			W.Months = 1.0;
			W.Bare = BareAt;
			W.Scorch = ScorchAt;
			W.Gain = 1.0;
			Bare = BareAt;
			Scorch = ScorchAt;
			Gain = 1.0;
			// (each point read, not under soot: its heat's lie as Gain draws it out, the rest of its heat (her thin plates'
			// Hotter, the noise), its area)
			struct Read
			{
				double Lie = 0.0, Rest = 0.0, Soot = -1.0, Area = 0.0;
			};
			std::vector<Read> Open;
			double All = 0.0;
			const int OffBody = H.TrackOff != 0 ? M.TrackBody[H.TrackOff < 0 ? 0 : 1] : -1;
			for (size_t B = 0; B < M.Bodies.size(); ++B)
			{
				const LandKit::Body& Bo = M.Bodies[B];
				if (Ki.Drop[B] || static_cast<int>(B) == OffBody || Bo.bTrack || Ki.Tyre[B] != 0 || (H.bTurretOff && Ki.OnTurret[B])) continue;
				const LandKit::Mesh& Me = Bo.Near;
				const size_t N = Me.Count();
				if (Me.Points.size() < N * 3 || Me.Normals.size() < N * 3) continue;
				const auto IsCoat = [&](uint32_t I) { return I >= Me.Paints.size() || Me.Paints[I] >= Ki.PaintIs.size() || Ki.PaintIs[Me.Paints[I]] == Kit::Matter::Coat; };
				for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
				{
					const uint32_t A = Me.Triangles[T], Bv = Me.Triangles[T + 1], C = Me.Triangles[T + 2];
					if (A >= N || Bv >= N || C >= N || !IsCoat(A) || !IsCoat(Bv) || !IsCoat(C)) continue;
					const V3 Way = V3(static_cast<double>(Me.Normals[A * 3]) + Me.Normals[Bv * 3] + Me.Normals[C * 3],
						static_cast<double>(Me.Normals[A * 3 + 1]) + Me.Normals[Bv * 3 + 1] + Me.Normals[C * 3 + 1],
						static_cast<double>(Me.Normals[A * 3 + 2]) + Me.Normals[Bv * 3 + 2] + Me.Normals[C * 3 + 2]) * (1.0 / 3.0);
					const double Up = Way.Z;
					if (Up < 0.5) continue;
					const V3 Pa(Me.Points[A * 3], Me.Points[A * 3 + 1], Me.Points[A * 3 + 2]), Pb(Me.Points[Bv * 3], Me.Points[Bv * 3 + 1], Me.Points[Bv * 3 + 2]);
					const V3 Pc(Me.Points[C * 3], Me.Points[C * 3 + 1], Me.Points[C * 3 + 2]);
					const double Area = 0.5 * (Pb - Pa).Cross(Pc - Pa).Length();
					if (!(Area > 0.0)) continue;
					// (a small triangle every third, standing for three; a big one at points over it)
					const bool bSmall = Area < 0.02;
					if (bSmall && (T / 3) % 3 != 0) continue;
					const int Cut = bSmall ? 1 : std::min(12, static_cast<int>(std::ceil(std::sqrt(Area / 0.02))));
					const double Each = (bSmall ? 3.0 * Area : Area) / (0.5 * Cut * (Cut + 1));
					const double SootW = (static_cast<double>(Me.Soot[A]) + Me.Soot[Bv] + Me.Soot[C]) / 3.0;
					for (int I = 0; I < Cut; ++I)
					{
						for (int J = 0; I + J < Cut; ++J)
						{
							const V3 At = Bo.Pivot + Pa + (Pb - Pa) * ((I + 1.0 / 3.0) / Cut) + (Pc - Pa) * ((J + 1.0 / 3.0) / Cut);
							// (her upper works: her deck, her turret, what stands on them; not her track guards, nor her floor
							// and its like inside her)
							if (At.Z < UpperFrom * W.Top) continue;
							double NearH = 0.5, Smoke = 0.5;
							NearFire(W, At, Way, NearH, Smoke);
							const double Built = HeatBuilt(W, At, Up, SootW, NearH);
							Read R;
							R.Lie = Built - W.Hotter;
							R.Rest = FlakedHeat(W, At, HeatAt(W, At, Built)) - R.Lie;
							R.Soot = SootScore(W, At, Up, SootW, Smoke, 0.0);
							R.Area = Each;
							All += Each;
							Open.push_back(R);
						}
					}
				}
			}
			if (!(All > 0.0) || Open.empty()) return;
			const double Edge = SteelEdge(W), Eaten = Flaking * Smooth(0.5, 15.0, W.Months);
			// (whether a point is under soot at a gain: the hotter her steel the more of her soot burnt off it; the rain
			// a month on as good as none)
			const auto Sooty = [&](const Read& R, double Heat) { return R.Soot - SootBurnt(W, Heat) > SootAt; };
			const auto ShareOver = [&](double G, double Above)
			{
				double Sum = 0.0;
				for (const Read& R : Open)
				{
					const double Heat = G * R.Lie + R.Rest;
					Sum += Heat > Above && !Sooty(R, Heat) ? R.Area : 0.0;
				}
				return Sum / All;
			};
			const double Share = ShareOver(1.0, Edge);
			if (Share < 0.15)
			{
				// (the least gain that brings her there, halving the way to it)
				double Lo = 1.0, Hi = 2.5;
				if (ShareOver(Hi, Edge) >= 0.15)
				{
					for (int K = 0; K < 20; ++K)
					{
						const double Mid = 0.5 * (Lo + Hi);
						(ShareOver(Mid, Edge) < 0.15 ? Lo : Hi) = Mid;
					}
				}
				Gain = Hi;
			}
			// (her heat at her gain, hottest first; the heat above which a share of her lies, not under soot)
			std::vector<std::pair<double, double>> Hot;
			Hot.reserve(Open.size());
			for (const Read& R : Open)
			{
				const double Heat = Gain * R.Lie + R.Rest;
				if (!Sooty(R, Heat)) Hot.emplace_back(Heat, R.Area);
			}
			if (Hot.empty()) return;
			std::sort(Hot.begin(), Hot.end(), [](const std::pair<double, double>& P, const std::pair<double, double>& Q) { return P.first > Q.first; });
			const auto HeatOver = [&](double Want)
			{
				double Sum = 0.0, To = Hot.front().first;
				for (const auto& [Heat, Area] : Hot)
				{
					Sum += Area;
					To = Heat;
					if (Sum >= Want * All) break;
				}
				return To;
			};
			// (the threshold whose edge a month on lies there)
			if (Share > 0.22) Bare = std::clamp((HeatOver(0.22) - Eaten * ScorchAt) / (1.0 - Eaten), BareAt, BareAt + 0.15);
			else if (ShareOver(Gain, Edge) < 0.15) Bare = std::clamp((HeatOver(0.15) - Eaten * ScorchAt) / (1.0 - Eaten), BareAt - 0.15, BareAt);
			double Clear = 0.0, Steel = 0.0;
			for (const auto& [Heat, Area] : Hot)
			{
				Clear += Area;
				Steel += Heat > Bare ? Area : 0.0;
			}
			const double Scorched = std::min(0.34 + 0.08 * Clamp01((H.Fire - 0.8) / 0.2), 0.9 * (Clear - Steel) / All);
			Scorch = std::clamp(HeatOver(Steel / All + Scorched), 0.05, Bare - 0.08);
		}

		/** Her height over each spot of her at rest (Tops), from her coat's near level: her hull and what stands on it
		 * (bTurret false; her turret too unless it was thrown), or a thrown turret's own (bTurret true). */
		Tops TopsOf(const LandKit::Model& M, const Kit& Ki, const Hulk& H, bool bTurret)
		{
			Tops T;
			const int OffBody = H.TrackOff != 0 ? M.TrackBody[H.TrackOff < 0 ? 0 : 1] : -1;
			const auto Kept = [&](size_t B)
			{
				const LandKit::Body& Bo = M.Bodies[B];
				if (Ki.Drop[B] || static_cast<int>(B) == OffBody || Bo.bTrack || Bo.Near.Points.size() < Bo.Near.Count() * 3) return false;
				return bTurret ? Ki.OnTurret[B] != 0 : !(H.bTurretOff && Ki.OnTurret[B]);
			};
			double X0 = 1e9, Y0 = 1e9, X1 = -1e9, Y1 = -1e9;
			for (size_t B = 0; B < M.Bodies.size(); ++B)
			{
				if (!Kept(B)) continue;
				const LandKit::Body& Bo = M.Bodies[B];
				for (size_t I = 0; I < Bo.Near.Count(); ++I)
				{
					const double X = Bo.Pivot.X + Bo.Near.Points[I * 3], Y = Bo.Pivot.Y + Bo.Near.Points[I * 3 + 1];
					X0 = std::min(X0, X);
					Y0 = std::min(Y0, Y);
					X1 = std::max(X1, X);
					Y1 = std::max(Y1, Y);
				}
			}
			if (!(X1 >= X0 && Y1 >= Y0)) return T;
			T.X0 = X0;
			T.Y0 = Y0;
			T.Nx = static_cast<int>(std::floor((X1 - X0) / Tops::Cell)) + 1;
			T.Ny = static_cast<int>(std::floor((Y1 - Y0) / Tops::Cell)) + 1;
			T.Z.assign(static_cast<size_t>(T.Nx) * static_cast<size_t>(T.Ny), -1e9f);
			T.Rusted.assign(T.Z.size(), -1.0f);
			for (size_t B = 0; B < M.Bodies.size(); ++B)
			{
				if (!Kept(B)) continue;
				const LandKit::Body& Bo = M.Bodies[B];
				for (size_t I = 0; I < Bo.Near.Count(); ++I)
				{
					const int Ix = std::clamp(static_cast<int>(std::floor((Bo.Pivot.X + Bo.Near.Points[I * 3] - X0) / Tops::Cell)), 0, T.Nx - 1);
					const int Iy = std::clamp(static_cast<int>(std::floor((Bo.Pivot.Y + Bo.Near.Points[I * 3 + 1] - Y0) / Tops::Cell)), 0, T.Ny - 1);
					float& Z = T.Z[static_cast<size_t>(Iy) * static_cast<size_t>(T.Nx) + static_cast<size_t>(Ix)];
					Z = std::max(Z, static_cast<float>(Bo.Pivot.Z + Bo.Near.Points[I * 3 + 2]));
				}
			}
			return T;
		}

		/** From when her old rust weathers on her: her own paint kept where the weather reaches least, and darker runs weeping
		 * from her seams and hatches (from some three months, full by six). */
		double OldBy(double Months) { return Smooth(2.5, 6.0, Months); }

		/**
		 * How sheltered a point of her is from the weather, 0 to 1 (the art director's carry of 24 November 21:01Z: from
		 * half a year her old rust read as one even brown; a little of her own paint wanted low and on her sheltered faces):
		 * turned down, wholly; a side open to the weather, a little (0.25: a patch here and there; the fix round's first try
		 * gave every side 0.45, paint on a fifth of each open plate, high on her as much as low); more low on her (wholly
		 * under 0.5 m over her tracks, none from 1.4 m) or under something of her that stands out over it (her track guards
		 * over her hull's sides: what lies a hand or so out from the face reaches over it). N its way out at rest, UpZ its
		 * way up as she lies, High its height over her tracks at rest.
		 */
		double ShelterOf(const Fire& W, const V3& At, const V3& N, double UpZ, double High)
		{
			const double Side = Smooth(0.5, 0.15, UpZ);
			if (!(Side > 0.0)) return 0.0;
			double Under = Smooth(1.4, 0.5, High);
			const double Nh = std::hypot(N.X, N.Y);
			if (Under < 1.0 && Nh > 0.3 && W.Over && W.Over->Nx > 0 && W.Over->Ny > 0)
			{
				const double Out = W.Over->Within(At.X + 0.3 * N.X / Nh, At.Y + 0.3 * N.Y / Nh);
				Under = std::max(Under, Smooth(0.08, 0.25, Out - At.Z));
			}
			return std::max(Smooth(-0.05, -0.5, UpZ), Side * (0.25 + 0.75 * Under));
		}

		/**
		 * Where a darker run of rust weeps down her side from above a point (the same carry: a few darker rust runs below
		 * her hatches and joints), 0 to 1: under the seam where her top meets her side, under a ledge of her that stands out
		 * over it (her track guards), and under a hatch or grille the flames came out of (the fire's places: her hatches,
		 * her engine deck's grilles; her turret's only while it is on her: Burn runs none on a thrown turret), a stride to either
		 * side; each fading as the run goes down, out within about a metre. Top her top over the point (Tops), -1e9 where her map of
		 * them has none: no seam there (the fix round's first try read none as her top at the point, a seam at every
		 * height: dark runs cut off at the map's cells).
		 */
		double WeepFrom(const Fire& W, const V3& At, const V3& N, double Top)
		{
			double From = Top > -1e8 ? Smooth(0.9, 0.05, std::max(0.0, Top - At.Z)) : 0.0;
			const double Nh = std::hypot(N.X, N.Y);
			if (Nh > 0.3 && W.Over && W.Over->Nx > 0 && W.Over->Ny > 0)
			{
				const double Over = W.Over->Within(At.X + 0.3 * N.X / Nh, At.Y + 0.3 * N.Y / Nh) - At.Z;
				if (Over > 0.05) From = std::max(From, Smooth(0.9, 0.05, Over));
			}
			for (size_t I = 0; I < W.At.size(); ++I)
			{
				const bool bOnTurret = I < W.OnTurret.size() && W.OnTurret[I] != 0;
				if (W.bThrown ? !bOnTurret : W.bRingOpen && bOnTurret) continue;
				const double Down = W.At[I].Z - At.Z;
				if (!(Down > -0.05)) continue;
				From = std::max(From, 1.2 * Smooth(0.7, 0.2, std::hypot(At.X - W.At[I].X, At.Y - W.At[I].Y)) * Smooth(1.4, 0.1, Down));
			}
			return From;
		}

		/** A point burnt: its colour and what it shows. */
		struct Burnt
		{
			Rgb8 C;
			Coat Is = Coat::Paint;
		};

		/**
		 * The burnt-out look of one point (the last round: the art director's notes of 24 November, round 2's patchwork
		 * read as camouflage; now a burn, its coats laid by where the fire was and blending into one another, FateOf):
		 * Paint her own coat (LandKit's, worn and dusty as she fought), Soot the model's own soot weight, At her point at
		 * rest (model metres: the coats are hers, coherent over her hull), N its way out at rest, UpZ its way up as she
		 * lies, High its height over her tracks at rest, bEdge on a plate's edge.
		 * - A week on: soot black (25 to 40 in the picture) over about half of what faces the sun, densest round where the
		 *   flames came out (her engine deck's grilles, her hatches, her turret's ring when it was thrown) and streaking
		 *   downwind and up from there, feathered grey at its edges and grained along the wind; round it, by her heat, the
		 *   bare grey steel (85 to 95), smudged darker, in the hottest centres, the paint scorched chalky grey-buff (100 to
		 *   110) round that, a narrow fringe of it heat-tinted brown, and her own paint low on her cooler sides; a fresh
		 *   orange only along the edges where the paint burnt off, a few in 100.
		 * - A month on: the bare steel gone to flash rust (some 120, 69, 36 in the picture), a dull orange-brown mottled at
		 *   a hand orange and a browner ochre and darker in brown patches, its brightest in the sun, on her tops and on her
		 *   sunlit sides, no brighter than some 150, 85, 45 (the art director's carry of 24 November 21:01Z: round 3's read
		 *   hot, almost glowing, on a sunlit turret top), coming on across a soft edge and staining the paint round it
		 *   rust-brown (never more than half), on some 15 to 25 in 100 of her, and running down her sides in streaks soft
		 *   at their edges and fading as they run, a softer wash of it under the rusting plates above.
		 * - Six months to a year and a half: the rust old, darker and duller (80 to 90, 52 to 56, 38 to 42: red no more than
		 *   2.2 times blue), mottled deep brown and lighter at a stride, browning darker as the months go; the soot washed
		 *   off what faces the sky (the steel under it rusted, a dark stain of it fading), kept on her sides and under her;
		 *   the scorched paint flaking, the steel's edge eating back into it from the hottest, what is left of it weathering
		 *   duller and darker (some 95, 85, 73 by a year and a half), stained by the rust round it, the stain easing to
		 *   under a third; her own paint flaking in flecks to the rust. And (the same carry: her old rust read as one even
		 *   brown) a little of her paint as the fire left it, her own or scorched, kept where the weather reaches least
		 *   (ShelterOf: low on her sides, under her guards and what stands out over her, on what turns down; hardly any
		 *   high on an open side, and none where the fire burnt her bare), in soft patches a stride or so across that her
		 *   runs of rust pass by, lighter than the rust round it; and a few darker runs weeping from her seams, ledges and
		 *   hatches (WeepFrom), on her upright faces only, in some stretches of her and not others, narrowing and fading as
		 *   they run (none on a thrown turret).
		 * - Rubber (a road wheel's ring) charred black with grey ash, greying; wood charred, ember brown and ash grey in
		 *   its grain, silvering; glass blackened in its port (a lorry's is gone: Dress). A track's links and a lorry's
		 *   rims: steel through the fire, rusting within weeks across a soft edge, some of them still black-grey from the
		 *   fire at a month.
		 * - Her turret ring's open hole (Ring 1, her turret thrown): the dark of her burnt-out inside, its own tag (not her
		 *   soot); its lip (Ring 2), the bare steel the turret turned on, black-grey from the fire, rusting as her bare
		 *   steel does, duller.
		 * - Low on her: the field's own earth splashed up, and after half a year a green stain where the weeds stand.
		 * - Snowed: the snow fallen on her since she burnt (SnowSince): white on what faces up, the steel showing through
		 *   where it lies thin; the earth and the weeds' stain under it.
		 */
		Burnt Burn(const Fire& W, const uint8_t* Paint, double Soot, const V3& At, const V3& N, double UpZ, double High, bool bEdge, Stuff What, int Ring)
		{
			const Hulk& H = *W.H;
			const double M = W.Months;
			const uint64_t S = H.Seed;
			// (each stuff a little uneven, a hand's breadth across)
			const double Nv = Noise3(At.X * 6.0, At.Y * 6.0, At.Z * 6.0, S ^ 0x77ull);
			const double Vary = 0.9 + 0.2 * Nv;
			const auto Toned = [Vary](const Tone& T) { return Tone(T.R * Vary, T.G * Vary, T.B * Vary); };
			// Rust: flash orange in its first weeks, mottled bright and brown at a hand; old and dull brown by half a year,
			// mottled deep and lighter at a stride, darker as the months go; darker still where it is deep.
			const double Age = Smooth(1.2, 6.0, M);
			// (rust's and bare steel's tones made only where a point shows them)
			Tone RustTone, SteelTone;
			bool bRustTone = false, bSteelTone = false;
			const auto Rust = [&]() -> const Tone&
			{
				if (!bRustTone)
				{
					Tone Base;
					if (Age < 1.0)
					{
						// (its brightest no brighter than some 150, 85, 45 in the sun: the art director's carry of 24 November
						// 21:01Z, round 3's flash rust on a sunlit turret top reading hot, almost glowing, its bright end 220, 108, 47
						// before the light. Mottled at a hand orange and a browner ochre near its value, so the mottle stays and
						// its brightest is held: the fix round's first try mottled it 162 to 158 only, an even orange-brown coat)
						const double Mottle = Even(Noise3(At.X * 2.6, At.Y * 2.6, At.Z * 2.6, S ^ 0x8Dull));
						Base = Mixed(Tone(159, 84, 40), Tone(136, 77, 42), Smooth(0.5, 1.0, Mottle));
					}
					if (Age > 0.0)
					{
						const double Mottle = Even(Noise3(At.X * 0.8, At.Y * 0.8, At.Z * 1.6, S ^ 0x8Eull));
						const double Older = Smooth(6.0, 18.0, M);
						const Tone Old = Mixed(Mixed(Tone(130, 77, 52), Tone(74, 46, 38), Mottle), Mixed(Tone(124, 74, 51), Tone(68, 44, 37), Mottle), Older);
						Base = Age < 1.0 ? Mixed(Base, Old, Age) : Old;
					}
					// (darker where it is deep, in patches and, down her sides, in streaks the rain drew: the flash rust's patches a
					// fifth as dark as the old's, its mottle's ochre its brown; round 3 darkened both 0.6, the fix round's first try
					// the flash rust's 0.1)
					const double Deep = Noise3(At.X * 1.4, At.Y * 1.4, At.Z * 0.5, S ^ 0x88ull);
					RustTone = Mixed(Base, Tone(70, 45, 36), (0.12 + 0.48 * Age) * Smooth(0.55, 0.78, Deep));
					// (the flash rust even at a hand's breadth, not uneven as its steel is, Vary: its brightest held down, its
					// mottle its own; the old as uneven as the steel)
					const double Steady = (1.0 + (Vary - 1.0) * Age) / Vary;
					RustTone = Tone(RustTone.R * Steady, RustTone.G * Steady, RustTone.B * Steady);
					bRustTone = true;
				}
				return RustTone;
			};
			const Coat RustIs = Age < 0.5 ? Coat::Flash : Coat::OldRust;
			// Bare steel: grey, a little blue in places, smudged darker where the fire's soot and scale stayed on it.
			const auto Steel = [&]() -> const Tone&
			{
				if (!bSteelTone)
				{
					// (one noise for both: bluer where it is low, smudged where it is high)
					const double Smudge = Noise3(At.X * 1.8, At.Y * 1.8, At.Z * 1.8, S ^ 0xACull);
					SteelTone = Mixed(Mixed(Tone(119, 116, 111), Tone(108, 111, 117), Smooth(0.5, 0.25, Smudge)), Tone(74, 72, 69), 0.55 * Smooth(0.55, 0.76, Smudge));
					bSteelTone = true;
				}
				return SteelTone;
			};
			Tone Out;
			Coat Is = Coat::Paint;
			if (What == Stuff::Rubber)
			{
				// (charred black, its grey ash washed off within months, greying)
				Out = Mixed(Tone(31, 29, 27), Tone(112, 108, 102), 0.45 * Smooth(0.55, 0.75, Nv) * std::exp(-M / 2.0));
				Out = Mixed(Out, Tone(60, 56, 52), 0.5 * Smooth(2.0, 18.0, M));
				Is = Coat::Charred;
			}
			else if (What == Stuff::Wood)
			{
				// (charred along its grain, ember brown and ash grey in it while fresh, silvering with the months)
				const double Grain = Noise3(At.X * 1.5, At.Y * 16.0, At.Z * 16.0, S ^ 0xBCull);
				Out = Mixed(Tone(40, 32, 26), Tone(86, 55, 34), 0.6 * Smooth(0.62, 0.8, Grain) * std::exp(-M / 3.0));
				Out = Mixed(Out, Tone(104, 100, 94), 0.5 * Smooth(0.6, 0.8, Nv) * std::exp(-M / 2.0));
				Out = Mixed(Out, Tone(76, 72, 66), 0.6 * Smooth(2.0, 18.0, M));
				Is = Coat::Charred;
			}
			else if (What == Stuff::Glass)
			{
				Out = Tone(28, 28, 30);
				Is = Coat::Charred;
			}
			else if (What == Stuff::Link || What == Stuff::Rim)
			{
				// (steel through the fire, black-grey at first, rusting within weeks: some one in five by a month, every one
				// by half a year; round 2 rusted every link by a month, and a thrown track went wholly orange, its links a
				// fifth of a Tiger's lit faces)
				// (each link's rust coming on across a soft edge, not link by link at a cut; none before its share is)
				const double Share = 0.2 * Smooth(0.15, 0.8, M) + 0.8 * Smooth(1.2, 6.0, M);
				const double Rusted = Smooth(-0.08, 0.08, 1.16 * Share - 0.08 - Even(Noise3(At.X * 3.4, At.Y * 3.4, At.Z * 3.4, S ^ 0x66ull)));
				// (the steel's tone read before the rust's: never two noises' reads as one call's arguments)
				const Tone Black = Mixed(Steel(), W.Burnt, 0.35);
				Out = Toned(Rusted > 0.0 ? Mixed(Black, Rust(), Rusted) : Black);
				Is = Rusted > 0.5 ? RustIs : Coat::Links;
			}
			else
			{
				const Fate F = FateOf(W, At, N, Soot, UpZ);
				const Tone Own(Paint[0], Paint[1], Paint[2]);
				// Her own paint, low on her cooler sides: smoked a little, fading with the months, flaking in flecks to the
				// rust from half a year (some one in eight of it by a year and a half)...
				Out = Mixed(Tone(Own.R * 0.9, Own.G * 0.9, Own.B * 0.9), Tone(Own.R * 0.8 + 50.0, Own.G * 0.8 + 48.0, Own.B * 0.8 + 44.0), 0.25 * (1.0 - std::exp(-M / 10.0)));
				const Tone Kept = Out;
				double Flecks = 0.0;
				const double Flakes = 0.16 * Smooth(3.0, 18.0, M);
				if (Flakes > 0.0 && F.Scorch < 1.0 && F.Steel < 1.0)
				{
					Flecks = Smooth(-0.05, 0.05, 1.1 * Flakes - 0.05 - Even(Noise3(At.X * 2.4, At.Y * 2.4, At.Z * 2.4, S ^ 0x5Bull)));
					if (Flecks > 0.0) Out = Mixed(Out, Toned(Rust()), Flecks);
				}
				// ...a narrow fringe of it heat-tinted brown and straw where the scorch begins, weathering with her...
				// (each kept for her paint left where the weather reaches least, below)
				Tone Tinted, Chalked;
				if (F.Tint > 0.0)
				{
					Tinted = Toned(Mixed(Mixed(Own, Tone(116, 86, 56), 0.65), Tone(92, 80, 66), 0.5 * Smooth(1.0, 18.0, M)));
					Out = Mixed(Out, Tinted, F.Tint);
				}
				// ...the paint scorched and gone chalky, a grey-buff with a little of her own colour in it; within half a year
				// duller and darker and stained by the rust round it, till it reads as faded paint on the rust (round 3's
				// first try weathered it a fifth of the way by six months: pale blotches on rust)...
				if (F.Scorch > 0.0)
				{
					Tone Chalk = Mixed(Own, Tone(128, 121, 108), 0.86);
					const double Weathered = M > 0.5 ? 1.0 - std::exp(-(M - 0.5) / 4.0) : 0.0;
					if (Weathered > 0.0)
					{
						Chalk = Mixed(Chalk, Tone(115, 106, 94), Weathered);
						const double Stain = Even(Noise3(At.X * 2.2, At.Y * 2.2, At.Z * 0.6, S ^ 0x5Aull));
						Chalk = Mixed(Chalk, Rust(), Weathered * (0.04 + 0.18 * Stain));
					}
					Chalked = Toned(Chalk);
					Out = Mixed(Out, Chalked, F.Scorch);
				}
				// ...stained rust-brown round the steel as it rusts (so the rust's edge browns into the scorched paint, not a
				// bright orange against pale grey-buff), never more than half over it (the paint still the coat it shows: the
				// fix round's first try stained it 0.85, a ring of rust tagged scorched paint), easing to under a third by ten
				// months as the paint weathers round it (a stain stays: with none the old rust met the pale paint at a hard line)...
				const double Stained = F.Stain * (0.5 - 0.2 * Smooth(1.0, 10.0, M));
				if (Stained > 0.0) Out = Mixed(Out, Toned(Mixed(Rust(), Tone(84, 56, 40), 0.5)), Stained);
				// ...blistered off to the steel in the hottest centres (and flaked off it after), rusting within a month...
				if (F.Steel > 0.0)
				{
					const Tone Bare = Steel();
					Out = Mixed(Out, Toned(F.Rust > 0.0 ? Mixed(Bare, Rust(), F.Rust) : Bare), F.Steel);
				}
				// From half a year, a little of her paint left where the weather reaches least (low on her sides, under her guards
				// and what stands out over her, on what turns down; on her open sides hardly any), in patches a stride or so
				// across, softly edged, stained a little by the rust round it, her runs of rust passing it by (the art director's
				// carry of 24 November 21:01Z: her old rust read as one even brown). It is her paint as the fire left it, her own
				// or scorched, kept from flaking to the rust there; none where the fire burnt her bare (the fix round's first
				// try laid her own paint back over the burnt steel, and on a fifth of every open side)...
				// (read only where something lies over her paint and the fire left it, and where her shelter could leave a
				// patch: its patches' noise none under 0, over 0.02 / 0.85)
				double Left = 0.0;
				const double Old = OldBy(M);
				if (Old > 0.0 && UpZ < 0.5 && F.Heat < W.Bare + BareBand && (F.Steel > 0.0 || F.Scorch > 0.0 || F.Tint > 0.0 || F.Stain > 0.0 || Flecks > 0.0))
				{
					const double Shelter = ShelterOf(W, At, N, UpZ, High);
					if (Shelter > 0.02 / 0.85)
					{
						const double Patches = Even(Noise3(At.X * 1.9, At.Y * 1.9, At.Z * 1.9, S ^ 0x5Eull));
						Left = Old * Smooth(W.Bare + BareBand, W.Bare - BareBand, F.Heat) * Smooth(-0.1, 0.1, 0.85 * Shelter - 0.12 - Patches);
						if (Left > 0.0)
						{
							Tone Painted = Kept;
							if (F.Tint > 0.0) Painted = Mixed(Painted, Tinted, F.Tint);
							if (F.Scorch > 0.0) Painted = Mixed(Painted, Chalked, F.Scorch);
							Out = Mixed(Out, Mixed(Painted, Toned(Rust()), 0.06), Left);
						}
					}
				}
				// ...and the soot over them all, feathered through grey, grained blacker and greyer along the wind, browning a
				// little as the rust under it comes through; where the rain washed it off, a dark stain of it, fading.
				if (F.Soot > 0.0)
				{
					const Tone Black = Mixed(Tone(47, 44, 41), Tone(22, 21, 19), F.Grain);
					Out = Mixed(Out, Toned(Mixed(Black, Tone(48, 37, 30), 0.6 * Smooth(2.0, 18.0, M))), F.Soot);
				}
				if (F.SootWas > F.Soot) Out = Mixed(Out, Toned(Tone(60, 50, 42)), 0.4 * (F.SootWas - F.Soot) * std::exp(-M / 10.0));
				// (each point's coat for the measure: the one on its side of each band's middle)
				// (her paint left where the weather reaches least: her own, or scorched where the paint left was)
				Is = F.Soot > 0.5 ? Coat::Soot : Left > 0.5 ? (F.Scorch > 0.5 ? Coat::Scorched : Coat::Paint) : F.Steel > 0.5 ? (F.Rust > 0.5 ? RustIs : Coat::Bare) : F.Scorch > 0.5 ? Coat::Scorched : Flecks > 0.5 ? RustIs : Coat::Paint;
				// The first orange: along a plate's edge where the paint burnt off (a week on, the only orange on her, as fresh
				// an orange as round 3's: the fix round's first try dulled it with the flash rust held down; by a month the
				// flash rust's own).
				if (bEdge)
				{
					const double Orange = 0.85 * std::max(F.Scorch, F.Steel) * (1.0 - F.Soot) * (1.0 - Left);
					// (rusting through along it as the months go: not a pale orange line over the scorched paint; the edge her
					// rust's where it shows as much as round 3's edge did, 0.42)
					const double Rusts = RustsBy(M);
					const double Through = std::min(1.0, Orange * (1.0 + 0.5 * Rusts));
					if (Through > 0.0) Out = Mixed(Out, Toned(!(Rusts > 0.0) ? Tone(178, 90, 43) : Rusts < 1.0 ? Mixed(Tone(178, 90, 43), Rust(), Rusts) : Rust()), Through);
					if (Through > 0.42) Is = RustIs;
				}
				// Runs of rust down her sides from the rust above (from a few weeks on): streaks a hand to a stride wide and
				// long down her, soft at their edges and fading as they run, as strong as the rust above them shows, and a
				// softer wash of it between them (round 3's first try ran them only in streaks, few to be seen: her rusted
				// top met her sides at the plate's edge, a two-tone scheme).
				if (M > 0.3 && std::fabs(UpZ) < 0.6 && F.Soot < 0.9 && W.Over && W.Over->Nx > 0 && W.Over->Ny > 0)
				{
					const double Top = W.Over->At(At.X, At.Y), ToTop = std::max(0.0, Top - At.Z);
					// (fading to none over her own paint left where the weather reaches least, 1 - Left: read only where any shows)
					if (Is != RustIs && Left < 1.0)
					{
						// (what lies above her side: her own plate up to her top there, sampled as itself; then her top, facing up,
						// its rust running over the edge, fading out a metre under it; each sample's weight falling with its
						// height, and read only where it could raise what the others found)
						double Above = 0.0;
						for (const double Dz : { 0.3, 0.7 })
						{
							if (At.Z + Dz >= Top - 0.03 || !(Above < 1.0 - 0.3 * Dz)) continue;
							Above = std::max(Above, (1.0 - 0.3 * Dz) * RustAt(W, At + V3(0.0, 0.0, Dz), N, Soot, UpZ));
						}
						const double FromTop = (1.0 - 0.3 * ToTop) * Smooth(1.0, 0.6, ToTop);
						if (Above < FromTop) Above = std::max(Above, FromTop * TopRust(W, At));
						if (Above > 0.0)
						{
							const double Streak = Noise3(At.X * 2.8, At.Y * 2.8, At.Z * 0.45, S ^ 0x99ull);
							const double Run = 0.9 * Smooth(0.3, 1.0, M) * (0.4 + 0.6 * Smooth(0.44, 0.58, Streak)) * Above * (1.0 - F.Soot) * (1.0 - Left);
							if (Run > 0.0)
							{
								Out = Mixed(Out, Toned(Rust()), Run);
								if (Run > 0.45) Is = Coat::Run;
							}
						}
					}
					// From half a year, a few darker runs weeping from her seams and hatches (the art director's carry of 24
					// November 21:01Z: her old rust read as one even brown), over the rust and all: narrow streaks a hand wide, a
					// stride or more apart and in some stretches of her only, long down her and short, each narrowing as it fades
					// (WeepFrom), soft at their edges, passing by her paint left (1 - Left). Only on her upright faces, at rest and
					// as she lies (the streak's noise is drawn out down her at rest), and never on her thrown turret (lying any
					// way, its hatches reaching all of it): the fix round's first try ran them evenly spaced and all one length,
					// stripes over her paint left, as camouflage, and blots on her sloped and curved faces and thrown turrets.
					if (Old > 0.0 && !W.bThrown && std::fabs(UpZ) < 0.35 && std::fabs(N.Z) < 0.35 && Left < 1.0)
					{
						const double From = WeepFrom(W, At, N, Top);
						if (From > 0.0)
						{
							// (the streak's noise turned off her axes, so its peaks do not stand at its lattice's even spacing along her
							// plates; its edge rising as the run fades, so each run ends where its own streak falls short)
							const double U = 0.8 * At.X + 0.6 * At.Y, V = 0.6 * At.X - 0.8 * At.Y;
							const double Streak = Noise3(U * 5.7, V * 7.3, At.Z * 0.35, S ^ 0x9Aull);
							const double Lo = 0.65 + 0.1 * (1.0 - std::min(1.0, From));
							if (Streak > Lo)
							{
								// (a few: in some stretches of her and none in others, read only where a streak stands)
								const double Gate = Noise3(At.X * 1.1, At.Y * 1.1, At.Z * 0.3, S ^ 0x9Cull);
								const double Weep = 0.9 * Old * Smooth(Lo, Lo + 0.07, Streak) * Smooth(0.3, 0.45, Gate) * (1.0 - F.Soot) * (1.0 - Left);
								if (Weep > 0.0)
								{
									Out = Mixed(Out, Toned(Mixed(Rust(), Tone(44, 29, 23), 0.75)), Weep);
									if (Weep > 0.45) Is = Coat::Run;
								}
							}
						}
					}
				}
			}
			// her open turret ring (her turret thrown): the dark of her burnt-out inside, the fire's own chimney; its lip the
			// bare steel the turret turned on, black-grey from the fire, then rusting as her bare steel does, duller (round
			// 3's first try rusted it bright at a month, a drawn orange ring round the hole)
			if (Ring == 1)
			{
				Out = Mixed(Out, W.Burnt, 0.8);
				Is = Coat::Hole;
			}
			else if (Ring == 2)
			{
				const double Share = Smooth(0.3, 0.95, M);
				const double Rusted = Smooth(-0.08, 0.08, 1.16 * Share - 0.08 - Even(Noise3(At.X * 3.4, At.Y * 3.4, At.Z * 3.4, S ^ 0x6Bull)));
				const Tone Lip = Mixed(Steel(), W.Burnt, 0.45);
				Out = Toned(Rusted > 0.0 ? Mixed(Lip, Mixed(Rust(), W.Burnt, 0.3), Rusted) : Lip);
				Is = Rusted > 0.5 ? RustIs : Coat::Bare;
			}
			// low on her: the field's earth splashed up, and the weeds' stain once they stand (both under the snow where it
			// has fallen since she burnt: on a winter field she would otherwise stand brown- and green-footed in clean snow)
			if (High < 0.45)
			{
				const double Low = 1.0 - Clamp01(High / 0.45);
				Out = Mixed(Out, W.Earth, 0.3 * Low * (0.4 + 0.6 * std::min(1.0, M / 4.0)) * (1.0 - W.Snowed));
				Out = Mixed(Out, Tone(70, 82, 42), 0.18 * Low * Smooth(4.0, 18.0, M) * (1.0 - W.Snowed));
			}
			// the snow fallen on her since she cooled, on what faces up (her deck, her turret's roof, her track's links
			// lying flat), the steel showing through in patches where it lies thin (LandSnow's new snow)
			if (W.Snowed > 0.0 && UpZ > 0.55)
			{
				const double White = W.Snowed * Smooth(0.55, 0.9, UpZ) * (0.7 + 0.35 * Noise3(At.X * 2.2, At.Y * 2.2, At.Z * 2.2, S ^ 0xA1ull));
				Out = Mixed(Out, Tone(250, 252, 255), White);
				if (White > 0.5) Is = Coat::Snow;
			}
			Burnt B;
			B.C = ToRgb(Out);
			B.Is = Is;
			return B;
		}

		/**
		 * A body's coat halved down to Most metres (as LandKit halves a camouflaged one's, its Refine), so her patches show
		 * on a lorry's, a half-track's or a Sherman's big plates and not as one colour smeared from corner to corner: each
		 * edge of a triangle all of her coat longer than Most halved, a few rounds, the new point halfway with its
		 * neighbours' facing, paint, soot and stuff. Made grows with it. False when nothing was long enough.
		 */
		bool Halve(LandKit::Mesh& Me, std::vector<Kit::Matter>& Made, double Most)
		{
			const double Most2 = Most * Most;
			const auto Length2 = [&Me](uint32_t A, uint32_t B)
			{
				double S = 0.0;
				for (size_t I = 0; I < 3; ++I)
				{
					const double D = static_cast<double>(Me.Points[A * 3 + I]) - Me.Points[B * 3 + I];
					S += D * D;
				}
				return S;
			};
			const auto EdgeKey = [](uint32_t A, uint32_t B) { return (static_cast<uint64_t>(std::min(A, B)) << 32) | std::max(A, B); };
			bool bAny = false;
			for (int Round = 0; Round < 8; ++Round)
			{
				std::unordered_map<uint64_t, uint32_t> Halves;
				const size_t Before = Me.Triangles.size() / 3;
				for (size_t T = 0; T < Before; ++T)
				{
					const uint32_t V[3] = { Me.Triangles[T * 3], Me.Triangles[T * 3 + 1], Me.Triangles[T * 3 + 2] };
					if (V[0] >= Made.size() || V[1] >= Made.size() || V[2] >= Made.size()) continue;
					if (Made[V[0]] != Kit::Matter::Coat || Made[V[1]] != Kit::Matter::Coat || Made[V[2]] != Kit::Matter::Coat) continue;
					for (size_t E = 0; E < 3; ++E)
					{
						const uint32_t A = V[E], B = V[(E + 1) % 3];
						if (Length2(A, B) <= Most2 || !Halves.emplace(EdgeKey(A, B), static_cast<uint32_t>(Me.Count())).second) continue;
						if (Me.Paints.size() == Me.Count()) Me.Paints.push_back(Me.Paints[A]);
						for (size_t I = 0; I < 3; ++I) Me.Points.push_back(0.5f * (Me.Points[A * 3 + I] + Me.Points[B * 3 + I]));
						const V3 Facing = V3(static_cast<double>(Me.Normals[A * 3]) + Me.Normals[B * 3], static_cast<double>(Me.Normals[A * 3 + 1]) + Me.Normals[B * 3 + 1],
							static_cast<double>(Me.Normals[A * 3 + 2]) + Me.Normals[B * 3 + 2]).Unit(V3(0.0, 0.0, 1.0));
						Me.Normals.push_back(static_cast<float>(Facing.X));
						Me.Normals.push_back(static_cast<float>(Facing.Y));
						Me.Normals.push_back(static_cast<float>(Facing.Z));
						for (size_t I = 0; I < 4; ++I) Me.Colours.push_back(static_cast<uint8_t>((Me.Colours[A * 4 + I] + Me.Colours[B * 4 + I] + 1) / 2));
						Me.Soot.push_back(0.5f * (Me.Soot[A] + Me.Soot[B]));
						Made.push_back(Kit::Matter::Coat);
					}
				}
				if (Halves.empty()) break;
				bAny = true;
				const auto Half = [&Halves, &EdgeKey](uint32_t A, uint32_t B)
				{
					const auto It = Halves.find(EdgeKey(A, B));
					return It == Halves.end() ? UINT32_MAX : It->second;
				};
				std::vector<uint32_t> Cut;
				Cut.reserve(Me.Triangles.size() * 2);
				const auto Add = [&Cut](uint32_t A, uint32_t B, uint32_t C) { Cut.insert(Cut.end(), { A, B, C }); };
				for (size_t T = 0; T < Before; ++T)
				{
					const uint32_t V[3] = { Me.Triangles[T * 3], Me.Triangles[T * 3 + 1], Me.Triangles[T * 3 + 2] };
					const uint32_t Mid[3] = { Half(V[0], V[1]), Half(V[1], V[2]), Half(V[2], V[0]) };
					const int Halved = (Mid[0] != UINT32_MAX) + (Mid[1] != UINT32_MAX) + (Mid[2] != UINT32_MAX);
					if (Halved == 0)
					{
						Add(V[0], V[1], V[2]);
					}
					else if (Halved == 3)
					{
						Add(V[0], Mid[0], Mid[2]);
						Add(Mid[0], V[1], Mid[1]);
						Add(Mid[2], Mid[1], V[2]);
						Add(Mid[0], Mid[1], Mid[2]);
					}
					else if (Halved == 1)
					{
						// (the halved edge runs from V[E] to the next corner; the winding kept)
						const size_t E = Mid[0] != UINT32_MAX ? 0 : (Mid[1] != UINT32_MAX ? 1 : 2);
						Add(V[E], Mid[E], V[(E + 2) % 3]);
						Add(Mid[E], V[(E + 1) % 3], V[(E + 2) % 3]);
					}
					else
					{
						// (two halved: the whole edge U's, from A to B; the far corner C keeps a small triangle)
						const size_t U = Mid[0] == UINT32_MAX ? 0 : (Mid[1] == UINT32_MAX ? 1 : 2);
						const uint32_t A = V[U], B = V[(U + 1) % 3], C = V[(U + 2) % 3];
						const uint32_t BC = Mid[(U + 1) % 3], CA = Mid[(U + 2) % 3];
						Add(A, B, BC);
						Add(A, BC, CA);
						Add(CA, BC, C);
					}
				}
				Me.Triangles.swap(Cut);
			}
			return bAny;
		}

		/** Each point of a body's mesh on a plate's edge: its place shared with a point of another face more than some 35
		 * degrees off it (as LandKit finds its chipped edges). */
		std::vector<uint8_t> EdgesOf(const LandKit::Mesh& Me)
		{
			const size_t N = Me.Count();
			std::vector<uint8_t> Out(N, 0);
			std::unordered_map<uint64_t, uint32_t> Head;
			Head.reserve(N);
			std::vector<uint32_t> Next(N, UINT32_MAX);
			for (size_t I = 0; I < N; ++I)
			{
				const int64_t X = std::llround(Me.Points[I * 3] * 500.0), Y = std::llround(Me.Points[I * 3 + 1] * 500.0), Z = std::llround(Me.Points[I * 3 + 2] * 500.0);
				const uint64_t Key = (static_cast<uint64_t>(X & 0x1FFFFF) << 42) | (static_cast<uint64_t>(Y & 0x1FFFFF) << 21) | static_cast<uint64_t>(Z & 0x1FFFFF);
				const auto Got = Head.emplace(Key, static_cast<uint32_t>(I));
				if (!Got.second)
				{
					Next[I] = Got.first->second;
					Got.first->second = static_cast<uint32_t>(I);
				}
			}
			for (const auto& Group : Head)
			{
				if (Next[Group.second] == UINT32_MAX) continue;
				double Least = 1.0;
				for (uint32_t A = Group.second; A != UINT32_MAX; A = Next[A])
				{
					for (uint32_t B = Next[A]; B != UINT32_MAX; B = Next[B])
					{
						const double D = static_cast<double>(Me.Normals[A * 3]) * Me.Normals[B * 3] + static_cast<double>(Me.Normals[A * 3 + 1]) * Me.Normals[B * 3 + 1]
							+ static_cast<double>(Me.Normals[A * 3 + 2]) * Me.Normals[B * 3 + 2];
						Least = std::min(Least, D);
					}
				}
				if (Least < 0.82)
				{
					for (uint32_t A = Group.second; A != UINT32_MAX; A = Next[A]) Out[A] = 1;
				}
			}
			return Out;
		}

		/** A lorry's wooden bed in a body's mesh (Wood: each point's wood or not): the box of its floor, body metres from its
		 * pivot; bOk false where it has none. The floor is the biggest flat of wood facing up (a square metre and more; on
		 * the lorry her bed's floor, 7 square metres at 1.10 m: round 2 took the highest, her two side benches at 1.55 m,
		 * and laid her planks over the middle of her bed half a metre up, her floor left whole), its underside the flat of
		 * wood facing down within 15 cm under it. */
		struct Bed
		{
			bool bOk = false;
			V3 Lo, Hi;
		};
		Bed BedOf(const LandKit::Mesh& Me, const std::vector<uint8_t>& Wood)
		{
			Bed B;
			// (each flat by its height to the centimetre, facing up and down apart: its area and its box)
			std::map<int64_t, double> Area[2];
			std::map<int64_t, std::pair<V3, V3>> Box[2];
			const auto P = [&Me](uint32_t I) { return V3(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]); };
			for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
			{
				const uint32_t A = Me.Triangles[T], Bv = Me.Triangles[T + 1], C = Me.Triangles[T + 2];
				if (A >= Wood.size() || Bv >= Wood.size() || C >= Wood.size() || !Wood[A] || !Wood[Bv] || !Wood[C]) continue;
				const V3 Pa = P(A), Pb = P(Bv), Pc = P(C);
				const double Twice = (Pb - Pa).Cross(Pc - Pa).Length();
				// (which way it faces by its points' own facings, not its winding)
				const double Nz = static_cast<double>(Me.Normals[A * 3 + 2]) + Me.Normals[Bv * 3 + 2] + Me.Normals[C * 3 + 2];
				if (Twice < 1e-9 || std::fabs(Nz) < 2.85 || std::fabs(Pa.Z - Pb.Z) > 0.005 || std::fabs(Pa.Z - Pc.Z) > 0.005) continue;
				const int Up = Nz > 0.0 ? 1 : 0;
				const int64_t Key = std::llround((Pa.Z + Pb.Z + Pc.Z) / 3.0 * 100.0);
				Area[Up][Key] += 0.5 * Twice;
				auto Found = Box[Up].find(Key);
				if (Found == Box[Up].end()) Found = Box[Up].emplace(Key, std::make_pair(V3(1e9, 1e9, 1e9), V3(-1e9, -1e9, -1e9))).first;
				for (const V3& Q : { Pa, Pb, Pc })
				{
					Found->second.first = V3(std::min(Found->second.first.X, Q.X), std::min(Found->second.first.Y, Q.Y), std::min(Found->second.first.Z, Q.Z));
					Found->second.second = V3(std::max(Found->second.second.X, Q.X), std::max(Found->second.second.Y, Q.Y), std::max(Found->second.second.Z, Q.Z));
				}
			}
			int64_t Top = INT64_MIN;
			double Most = 1.0;
			for (const auto& [Key, A] : Area[1])
			{
				if (A >= Most)
				{
					Most = A;
					Top = Key;
				}
			}
			if (Top == INT64_MIN) return B;
			B.bOk = true;
			B.Lo = Box[1][Top].first;
			B.Hi = Box[1][Top].second;
			double Under = B.Hi.Z - 0.05;
			for (const auto& [Key, A] : Area[0])
			{
				if (Key <= Top && Key >= Top - 15 && A >= 0.8 * Most) Under = std::min(Under, Box[0][Key].first.Z);
			}
			B.Lo.Z = Under;
			return B;
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

		/** Her run-off track as it lies (the art director's carry, 24 November: a thrown track lay dead straight like a
		 * ladder): the placement's strip (Shape's, which every clearance and the thrown turret's place read) snaking from
		 * side to side from where it starts, a wave of 2.6 to 4.2 m, 14 to 22 cm either way past her tail and beside her
		 * the same wave all outward (off her wheels: 0 to 28 to 44 cm out), and folded back on itself once: its last
		 * quarter or third turned over and lying doubled back on top of the rest, upside down, its free end angled out off
		 * the links under it by a third of its width or so, so the fold shows from above. (Round 2 began the snake only past
		 * her hull's length, and a half-track's short track, which never passes her tail, lay straight; it laid the doubled
		 * links exactly over the flat ones.) Beside her hull its links' inner edges are kept 3 cm off her footprint (Shape's
		 * strip ran up to 20 cm under her outer road wheels), its snaking there on top of that: up to some 45 cm past the
		 * strip's rings, so each flat link's edges are read against Clear (open ground, and off her thrown turret) and the
		 * snaking beside her eased, then dropped, then the wheels' room too, until they are clear; the doubled end's angle
		 * likewise. Its own dice (her look's seed), so no throw of the laying moves. Sets Strip, Fold and Curl. */
		void LieTrack(Hulk& H, const Kit::Track& Tr, const LandKit::Model& M, const std::function<bool(double X, double Y)>& Clear)
		{
			const int N = static_cast<int>(H.Strip.size());
			H.Fold = H.Curl = 0;
			if (N < 16 || !Tr.bOk) return;
			Dice D(Mix(H.Seed, 0x7EAC0FFull));
			const double Swing = D.U(0.14, 0.22);
			const double Wave = D.U(2.6, 4.2);
			const double Phase = D.U(0.0, 2.0 * Pi);
			const double Share = D.U(0.62, 0.78);
			const double Thick = std::max(0.02, Tr.BMax - Tr.BMin);
			const int Curl = std::max(1, static_cast<int>(std::lround(Pi * Thick / Tr.Pitch)));
			// (the doubled end no longer than what it lies on)
			const int Fold = std::clamp(static_cast<int>(std::lround(Share * N)), (N - Curl + 2) / 2, N - Curl - 1);
			const double Out = H.TrackOff < 0 ? -1.0 : 1.0;
			const double HullLong = M.HullHi.X - M.HullLo.X, HalfY = std::max(-M.HullLo.Y, M.HullHi.Y), HalfW = 0.5 * (Tr.DMax - Tr.DMin);
			double Fx, Fy, Rx, Ry;
			Ways(H.Heading, Fx, Fy, Rx, Ry);
			const std::vector<std::pair<double, double>> Was = H.Strip;
			std::vector<std::pair<double, double>> Now(Was.size());
			// The flat links, Beside the share of the snaking beside her, Room the share of her wheels' room: whether each
			// one's edges lie clear.
			const auto LayFlat = [&](double Beside, double Room)
			{
				bool bClear = true;
				for (int L = 0; L < Fold; ++L)
				{
					const auto& Before = Was[static_cast<size_t>(L > 0 ? L - 1 : L)];
					const auto& After = Was[static_cast<size_t>(L + 1 < N ? L + 1 : L)];
					const double Dx = After.first - Before.first, Dy = After.second - Before.second, Long = std::hypot(Dx, Dy);
					const double Ux = Long > 1e-9 ? Dx / Long : 0.0, Uy = Long > 1e-9 ? Dy / Long : 0.0;
					// (the strip runs back from her bow: its left is her right, so Out says which way is away from her)
					const double S = static_cast<double>(L) * Tr.Pitch;
					const double Wavy = std::sin(2.0 * Pi * S / Wave + Phase);
					const double Past = Smooth(HullLong - 0.5, HullLong + 0.5, S);
					// (beside her, off her footprint: what it takes to keep the link's inner edge 3 cm clear, fading out over
					// a metre past her tail)
					const double A = (Was[static_cast<size_t>(L)].first - H.X) * Fx + (Was[static_cast<size_t>(L)].second - H.Y) * Fy;
					const double B = (Was[static_cast<size_t>(L)].first - H.X) * Rx + (Was[static_cast<size_t>(L)].second - H.Y) * Ry;
					const double Need = std::max(0.0, HalfW + 0.03 - (std::fabs(B) - HalfY)) * Smooth(M.HullLo.X - HalfW - 1.0, M.HullLo.X - HalfW, A);
					const double Off = Out * (Room * Need + Swing * (Beside * (1.0 - Past) * (1.0 + Wavy) + Past * Wavy));
					const double Px = Was[static_cast<size_t>(L)].first - Uy * Off, Py = Was[static_cast<size_t>(L)].second + Ux * Off;
					Now[static_cast<size_t>(L)] = { Px, Py };
					const double Edge = HalfW + 0.05;
					if (Clear && bClear && !(Clear(Px - Uy * Edge, Py + Ux * Edge) && Clear(Px + Uy * Edge, Py - Ux * Edge))) bClear = false;
				}
				return bClear;
			};
			if (!LayFlat(1.0, 1.0) && !LayFlat(0.7, 1.0) && !LayFlat(0.4, 1.0) && !LayFlat(0.0, 1.0)) LayFlat(0.0, 0.0);
			const auto& E = Now[static_cast<size_t>(Fold - 1)];
			const auto& E0 = Now[static_cast<size_t>(Fold - 2)];
			const double Ex = E.first - E0.first, Ey = E.second - E0.second, El = std::max(1e-9, std::hypot(Ex, Ey));
			for (int J = 0; J < Curl; ++J)
			{
				const double Phi = Pi * (J + 0.5) / Curl;
				const double Ahead = 0.5 * Tr.Pitch + Thick * std::sin(Phi);
				Now[static_cast<size_t>(Fold + J)] = { E.first + Ex / El * Ahead, E.second + Ey / El * Ahead };
			}
			// The doubled end: over the flat links it lies on, angled out from them toward its free end (Angle: a share of
			// her track's width at its end), eased where its outer edge would leave open ground.
			const int Doubled = N - Fold - Curl;
			const auto LayDoubled = [&](double Angle)
			{
				bool bClear = true;
				for (int L = Fold + Curl; L < N; ++L)
				{
					const int U = std::max(0, Fold - 1 - (L - Fold - Curl));
					const auto& Under = Now[static_cast<size_t>(U)];
					const auto& Before = Now[static_cast<size_t>(std::max(0, U - 1))];
					const auto& After = Now[static_cast<size_t>(std::min(Fold - 1, U + 1))];
					const double Dx = After.first - Before.first, Dy = After.second - Before.second, Long = std::max(1e-9, std::hypot(Dx, Dy));
					const double T = Doubled > 0 ? static_cast<double>(L - Fold - Curl + 1) / Doubled : 0.0;
					const double Off = Out * 2.0 * HalfW * Angle * (0.15 + 0.85 * T);
					const double Px = Under.first - Dy / Long * Off, Py = Under.second + Dx / Long * Off;
					Now[static_cast<size_t>(L)] = { Px, Py };
					const double Edge = Out * (HalfW + 0.05);
					if (Clear && bClear && !Clear(Px - Dy / Long * Edge, Py + Dx / Long * Edge)) bClear = false;
				}
				return bClear;
			};
			if (!LayDoubled(0.4) && !LayDoubled(0.2)) LayDoubled(0.0);
			H.Strip = Now;
			H.Fold = Fold;
			H.Curl = Curl;
		}

		/** Her bed's floor burnt through (a lorry's): laid again as planks along her, at the floor's own height and thickness,
		 * a quarter to two fifths of them burnt through and gone (round 2 drew each plank's fate alone, and every lorry it
		 * logged lost seven of ten), some burnt short at an end or through the middle, all charred; each plank a box of its
		 * own points. Floor in her body's metres from Pivot; X her body's place, Hull her turn on the field. */
		void LayPlanks(LandField::Mesh& Out, Dressing* Note, const Bed& Floor, const V3& Pivot, const LandKit::Xf& X, const Quat& Hull, const Fire& W)
		{
			const double Wide = Floor.Hi.Y - Floor.Lo.Y, Long = Floor.Hi.X - Floor.Lo.X;
			if (Wide <= 0.1 || Long <= 0.1) return;
			const int N = std::max(4, static_cast<int>(std::lround(Wide / 0.2)));
			const double Each = Wide / N, Z0 = Floor.Lo.Z, Z1 = Floor.Hi.Z;
			// (how many burnt through, her own throw; which, those whose own fate came lowest)
			Dice Share(Mix(W.H->Seed, 0xBED0ull));
			const int GoneN = std::clamp(static_cast<int>(std::lround(N * Share.U(0.25, 0.4))), 1, N - 1);
			std::vector<double> Fates(static_cast<size_t>(N));
			for (int K = 0; K < N; ++K)
			{
				Dice D(Mix(W.H->Seed, 0xBED5ull + static_cast<uint64_t>(K)));
				Fates[static_cast<size_t>(K)] = D.U();
			}
			std::vector<double> Ranked = Fates;
			std::nth_element(Ranked.begin(), Ranked.begin() + (GoneN - 1), Ranked.end());
			const double GoneTo = Ranked[static_cast<size_t>(GoneN - 1)];
			if (Note) Note->PlankTop = Pivot.Z + Z1;
			const uint8_t None[4] = { 0, 0, 0, 255 };
			const auto Box = [&](double X0, double X1, double Y0, double Y1)
			{
				const V3 Ways[6] = { V3(0, 0, 1), V3(0, 0, -1), V3(1, 0, 0), V3(-1, 0, 0), V3(0, 1, 0), V3(0, -1, 0) };
				for (const V3& Nm : Ways)
				{
					// (the face's four corners: the box's corners on its side)
					V3 C[4];
					int Got = 0;
					for (int Cz = 0; Cz < 2; ++Cz)
						for (int Cy = 0; Cy < 2; ++Cy)
							for (int Cx = 0; Cx < 2; ++Cx)
							{
								const V3 P(Cx ? X1 : X0, Cy ? Y1 : Y0, Cz ? Z1 : Z0);
								const double Side = Nm.X != 0.0 ? (Cx ? 1.0 : -1.0) * Nm.X : Nm.Y != 0.0 ? (Cy ? 1.0 : -1.0) * Nm.Y : (Cz ? 1.0 : -1.0) * Nm.Z;
								if (Side > 0.0 && Got < 4) C[Got++] = P;
							}
					// (round the face: 0, 1, 3, 2 of the corners as found)
					uint32_t I[4];
					const int Order[4] = { 0, 1, 3, 2 };
					for (int K = 0; K < 4; ++K)
					{
						const V3 Rest = Pivot + C[Order[K]];
						const V3 At = ToField(Hull.Rotate(X.Apply(Rest)));
						const V3 Up = ToField(Hull.Rotate(X.Q.Rotate(Nm)));
						const Burnt B = Burn(W, None, 1.0, Rest, Nm, Up.Z, Rest.Z, false, Stuff::Wood, 0);
						I[K] = Point(Out, At, Up, B.C);
						if (Note) Note->Coats.push_back(static_cast<uint8_t>(B.Is));
					}
					Wound(Out, I[0], I[1], I[2]);
					Wound(Out, I[0], I[2], I[3]);
				}
			};
			for (int K = 0; K < N; ++K)
			{
				Dice D(Mix(W.H->Seed, 0xBED5ull + static_cast<uint64_t>(K)));
				// (each throw its own statement: g++ and clang take an expression's operands in their own orders)
				const double Fate = D.U();
				const double CutFrom = D.U();
				const double CutTo = D.U();
				const double Break = D.U(0.3, 0.7);
				const double Hole = D.U(0.12, 0.3);
				const bool bGone = Fate <= GoneTo;
				if (Note)
				{
					++Note->Planks;
					Note->PlanksGone += bGone ? 1 : 0;
				}
				if (bGone) continue;
				const double Y0 = Floor.Lo.Y + K * Each + 0.012, Y1 = Floor.Lo.Y + (K + 1) * Each - 0.012;
				const double X0 = Floor.Lo.X + Long * (CutFrom < 0.45 ? 0.0 : 0.5 * (CutFrom - 0.45));
				const double X1 = Floor.Hi.X - Long * (CutTo < 0.45 ? 0.0 : 0.5 * (CutTo - 0.45));
				if (Fate > 0.8)
				{
					// (burnt through in the middle)
					const double Mid = X0 + (X1 - X0) * Break, Half = 0.5 * Hole * (X1 - X0);
					Box(X0, Mid - Half, Y0, Y1);
					Box(Mid + Half, X1, Y0, Y1);
				}
				else
				{
					Box(X0, X1, Y0, Y1);
				}
			}
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
		// A live battle (release 2): it opens with nothing fought (no tracks, rounds or events yet), so no ways to keep
		// off and no ground fought over. Kept off instead: the ground its men and machines are likely to cross (Likely,
		// FromLikely, FromCore); and the ground between the two zones' back edges off its core stands for the fought-over
		// (every 25 m; the axis is then the line between the zones' middles, Lay's own where there are no tracks).
		if (Rec.bLive && F.bSides)
		{
			Field::Likely& L = F.Live;
			L.bOn = true;
			const double W = Rec.Ground.W > 0.0 ? Rec.Ground.W : std::max(A[2], D[2]) + 300.0;
			const double H = Rec.Ground.H > 0.0 ? Rec.Ground.H : std::max(A[3], D[3]) + 300.0;
			L.Inside[0] = EdgeClear;
			L.Inside[1] = EdgeClear;
			L.Inside[2] = W - EdgeClear;
			L.Inside[3] = H - EdgeClear;
			const auto Grown = [](const double* Box, double By) { return std::vector<double>{ Box[0] - By, Box[1] - By, Box[2] + By, Box[3] + By }; };
			// Where the rules draw each side up (units.ts deploy, its plain drawUp: the foot in the front line across its
			// zone 40 m in from the side facing the enemy, the tanks and lorries 110 m behind it on its wings, a defence of
			// four or more keeping a quarter back as a reserve, the guns 230 m behind; GAP between blocks in a row; the
			// attacker's about its zone's middle, the defender's about its town kept 300 m inside its zone's ends). A
			// river or a city's streets on the field draw a side up otherwise (the bend's and the river's lanes, the
			// second river, the street edge): there each whole zone is kept, as the rules' blocks may stand anywhere in it.
			bool bPlain = Rec.Ground.TownSize != "city" && Rec.Ground.TownSize != "capital" && Rec.Ground.Terrain != "urban";
			{
				std::vector<uint8_t> Used(Rec.Ground.Grounds.size(), 0);
				for (const uint8_t Cell : Rec.Ground.Ground)
				{
					if (Cell < Used.size()) Used[Cell] = 1;
				}
				for (size_t K = 0; bPlain && K < Rec.Ground.Grounds.size(); ++K)
				{
					const std::string& Gw = Rec.Ground.Grounds[K];
					if (Used[K] != 0 && (Gw == "water" || Gw == "bridge" || Gw == "blown" || Gw == "suburb" || Gw == "industry" || Gw == "park")) bPlain = false;
				}
			}
			constexpr double Gap = 40.0;
			double Widest = 0.0;
			for (const LandFeed::Block& B : Rec.Blocks) Widest = std::max({ Widest, B.W, B.D });
			double FrontY[2] = { A[3] - 40.0, D[1] + 40.0 };
			double FrontLo[2] = { 1e18, 1e18 }, FrontHi[2] = { -1e18, -1e18 };
			for (int Side = 0; Side < 2; ++Side)
			{
				const double* Z = Side == 0 ? A : D;
				const double Dir = Side == 0 ? 1.0 : -1.0;
				const double Front = FrontY[Side];
				const double Cx = Side == 1 && Rec.Ground.TownR > 0.0 ? std::min(Z[2] - 300.0, std::max(Z[0] + 300.0, Rec.Ground.TownX)) : 0.5 * (Z[0] + Z[2]);
				// (rowsOf: the foot, the tanks and lorries, the guns; half the tanks in front where there is no foot; a corps
				// from next door left out, it walks on later)
				std::vector<const LandFeed::Block*> Line[3];
				for (const LandFeed::Block& B : Rec.Blocks)
				{
					if (B.Side != Side || B.bJoins) continue;
					if (B.Kind == "inf" || B.Kind == "mtn" || B.Kind == "eng" || B.Kind == "fort" || B.Kind == "cav") Line[0].push_back(&B);
					else if (B.Kind == "arm" || B.Kind == "mot") Line[1].push_back(&B);
					else if (B.Kind == "art") Line[2].push_back(&B);
				}
				if (Line[0].empty() && !Line[1].empty())
				{
					const size_t Half = (Line[1].size() + 1) / 2;
					Line[0].assign(Line[1].begin(), Line[1].begin() + static_cast<std::ptrdiff_t>(Half));
					Line[1].erase(Line[1].begin(), Line[1].begin() + static_cast<std::ptrdiff_t>(Half));
				}
				// (a defence's reserve: the last division of whichever corps has most in the line, a quarter of it)
				std::vector<const LandFeed::Block*> Held;
				if (Side == 1 && Line[0].size() >= 4)
				{
					for (size_t N = std::max<size_t>(1, Line[0].size() / 4); N > 0; --N)
					{
						const auto Count = [&](int Corps)
						{
							size_t C = 0;
							for (const LandFeed::Block* B : Line[0]) C += B->Corps == Corps ? 1 : 0;
							return C;
						};
						size_t At = Line[0].size() - 1;
						for (size_t I = Line[0].size(); I-- > 0;)
						{
							if (Count(Line[0][I]->Corps) > Count(Line[0][At]->Corps)) At = I;
						}
						Held.insert(Held.begin(), Line[0][At]);
						Line[0].erase(Line[0].begin() + static_cast<std::ptrdiff_t>(At));
					}
				}
				const auto Put = [&](const LandFeed::Block* B, double X, double Y)
				{
					X = std::min(W - 0.5 * B->W - 10.0, std::max(0.5 * B->W + 10.0, X));
					Y = std::min(H - 40.0, std::max(40.0, Y));
					L.Stands.push_back({ X - 0.5 * B->W, Y - 0.5 * B->D, X + 0.5 * B->W, Y + 0.5 * B->D, static_cast<double>(Side) });
				};
				const auto Place = [&](const std::vector<const LandFeed::Block*>& Row, double Y, bool bWings) -> bool
				{
					if (bWings)
					{
						// (half on each wing beyond the front line's ends; none where a wing would run off the field)
						const double Span = static_cast<double>(Line[0].size()) * (150.0 + Gap) / 2.0;
						std::vector<double> At;
						bool bFits = true;
						for (size_t I = 0; I < Row.size(); ++I)
						{
							const double Way = I % 2 == 0 ? -1.0 : 1.0;
							At.push_back(Cx + Way * (Span + 60.0 + static_cast<double>(I / 2) * (Row[I]->W + Gap)));
							bFits = bFits && At[I] - 0.5 * Row[I]->W >= 10.0 && At[I] + 0.5 * Row[I]->W <= W - 10.0;
						}
						if (bFits)
						{
							for (size_t I = 0; I < Row.size(); ++I) Put(Row[I], At[I], Y);
							return true;
						}
					}
					double Width = -Gap;
					for (const LandFeed::Block* B : Row) Width += B->W + Gap;
					double X = Cx - 0.5 * Width;
					for (const LandFeed::Block* B : Row)
					{
						Put(B, X + 0.5 * B->W, Y);
						X += B->W + Gap;
					}
					return false;
				};
				const size_t First = L.Stands.size();
				Place(Line[0], Front, false);
				const bool bWide = Place(Line[1], Front - Dir * 110.0, true);
				if (!Held.empty()) Place(Held, Front - Dir * (bWide || Line[1].empty() ? 110.0 : 170.0), false);
				Place(Line[2], Front - Dir * 230.0, false);
				L.bDrawnUp[Side] = bPlain;
				for (size_t K = First; K < L.Stands.size(); ++K)
				{
					FrontLo[Side] = std::min(FrontLo[Side], L.Stands[K][0]);
					FrontHi[Side] = std::max(FrontHi[Side], L.Stands[K][2]);
					if (bPlain) L.Core.push_back(Grown(L.Stands[K].data(), StartClear));
				}
				if (!bPlain) L.Core.push_back(Grown(Z, StartClear));
				if (FrontLo[Side] > FrontHi[Side])
				{
					FrontLo[Side] = FrontHi[Side] = 0.5 * (Z[0] + Z[2]);
				}
			}
			L.FrontAtt = FrontHi[0] - FrontLo[0];
			L.FrontDef = FrontHi[1] - FrontLo[1];
			// (the setup step's room, where the player stands his blocks before the clock runs, read as the header gives
			// it: grown as the rest, and by its widest block's half more, which may stand over its edge)
			if (Rec.bSetup && (Rec.Room[2] - Rec.Room[0]) * (Rec.Room[3] - Rec.Room[1]) > 0.0) L.Core.push_back(Grown(Rec.Room, StartClear + 0.5 * Widest));
			for (const LandFeed::Block& B : Rec.Blocks) L.bJoins = L.bJoins || B.bJoins;
			// (a corps from next door walks on by the edge facing where it comes from, or its own back edge, some 200 m in:
			// sim.ts joinEdge and walkOn, LAND.join; any edge but the enemy's back one, so every edge kept)
			if (L.bJoins)
			{
				L.Core.push_back({ -1e6, -1e6, JoinClear, 1e6 });
				L.Core.push_back({ W - JoinClear, -1e6, 1e6, 1e6 });
				L.Core.push_back({ -1e6, -1e6, 1e6, JoinClear });
				L.Core.push_back({ -1e6, H - JoinClear, 1e6, 1e6 });
			}
			// (the advance: between the two fronts as drawn up, each from its westmost block to its eastmost, wings and all)
			L.Advance = { { FrontLo[0], FrontY[0] }, { FrontHi[0], FrontY[0] }, { FrontHi[1], FrontY[1] }, { FrontLo[1], FrontY[1] } };
			// Round the core: each side's whole zone (its blocks shift in it), and behind its blocks and its advance to its
			// own back edge (a block falls back, or runs, straight back: sim.ts withdraw and rout).
			const double AdvLo = std::min(FrontLo[0], FrontLo[1]) - AdvanceMargin, AdvHi = std::max(FrontHi[0], FrontHi[1]) + AdvanceMargin;
			L.Areas.push_back(Grown(A, StartClear));
			L.Areas.push_back(Grown(D, StartClear));
			L.Areas.push_back({ std::min(AdvLo, FrontLo[0] - StartClear), -1e6, std::max(AdvHi, FrontHi[0] + StartClear), FrontY[0] + StartClear });
			L.Areas.push_back({ std::min(AdvLo, FrontLo[1] - StartClear), FrontY[1] - StartClear, std::max(AdvHi, FrontHi[1] + StartClear), 1e6 });
			// The stand-in for the fought-over ground: between the zones' back edges along the line between their middles,
			// off the core (the groups along the old lines and at the junctions and the village's edge may lie round it).
			double Ux = F.DefX - F.AttX, Uy = F.DefY - F.AttY;
			const double Apart2 = std::hypot(Ux, Uy);
			if (Apart2 > 1.0)
			{
				Ux /= Apart2;
				Uy /= Apart2;
				double After = 1e18, Before = -1e18;
				for (int C = 0; C < 4; ++C)
				{
					const double Cx = C % 2 == 0 ? A[0] : A[2], Cy = C < 2 ? A[1] : A[3];
					After = std::min(After, (Cx - F.AttX) * Ux + (Cy - F.AttY) * Uy);
					const double Ex = C % 2 == 0 ? D[0] : D[2], Ey = C < 2 ? D[1] : D[3];
					Before = std::max(Before, (Ex - F.AttX) * Ux + (Ey - F.AttY) * Uy);
				}
				for (double Y = L.Inside[1]; Y <= L.Inside[3]; Y += 25.0)
				{
					for (double X = L.Inside[0]; X <= L.Inside[2]; X += 25.0)
					{
						const double T = (X - F.AttX) * Ux + (Y - F.AttY) * Uy;
						if (T >= After && T <= Before && FromCore(F, X, Y) > 0.0)
						{
							F.Fought.emplace_back(X, Y);
						}
					}
				}
			}
		}
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

	double FromCore(const Field& F, double X, double Y)
	{
		const Field::Likely& L = F.Live;
		if (!L.bOn) return 1e9;
		// (the field's edges, each block's box and the room and side strips, the advance and its margin: the nearest)
		double Most2 = std::min({ X - L.Inside[0], L.Inside[2] - X, Y - L.Inside[1], L.Inside[3] - Y });
		for (const std::vector<double>& B : L.Core) Most2 = std::min(Most2, FromBox(B, X, Y));
		if (L.Advance.size() >= 3) Most2 = std::min(Most2, FromConvex(L.Advance, X, Y) - AdvanceMargin);
		return Most2;
	}

	double FromLikely(const Field& F, double X, double Y)
	{
		const Field::Likely& L = F.Live;
		if (!L.bOn) return 1e9;
		double Most2 = FromCore(F, X, Y);
		for (const std::vector<double>& B : L.Areas) Most2 = std::min(Most2, FromBox(B, X, Y));
		return Most2;
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
		Wanted = NoRoom = Read = OnLikely = 0;
		Swept.clear();
		DueAt.clear();
		SoonAt.clear();
		bLive = F.Live.bOn;
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
				// (a live battle's: none over its core, where its blocks are drawn up and between its fronts; round it, where
				// the rest of its start areas lie and its blocks fall back over, they may)
				if (Long >= 4.0 && Long < 1e5 && InBox(0.5 * (AX + BX), 0.5 * (AY + BY)) && (!F.Live.bOn || FromCore(F, 0.5 * (AX + BX), 0.5 * (AY + BY)) > 0.0))
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
			if (InBox(P.first, P.second) && (!F.Live.bOn || FromCore(F, P.first, P.second) > 0.0)) Spots.push_back(P);
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
		// (a live battle's group along an old line or at a junction or the village's edge, laid now: off the likely
		// ground's core only; any other, off all of it)
		bool bStoryNow = false;
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
				if (LaneAt.Near(R.X, R.Y, (F.Live.bOn ? LiveLaneClear : LaneClear) + R.R + 0.5)) return false;
				if (HedgeAt.Near(R.X, R.Y, HedgeClear + R.R + 0.5)) return false;
				if (VehicleAt.Near(R.X, R.Y, VehicleClear + R.R + 0.5)) return false;
				if (ManAt.Near(R.X, R.Y, ManClear + R.R + 0.4)) return false;
				if (BlastAt.Near(R.X, R.Y, BlastClear + R.R)) return false;
				if (StandAt.Near(R.X, R.Y, R.R + 1.0)) return false;
				if (WayAt.Near(R.X, R.Y, WayClear + R.R + PatchMargin + 0.5)) return false;
				if (WireAt.Near(R.X, R.Y, WireClear + R.R + 0.5)) return false;
				if (HulkAt.Near(R.X, R.Y, R.R + Apart)) return false;
				// (a live battle's: off the ground its men and machines are likely to cross, every ring of her; along an old
				// line or at a junction or the village's edge, off its core)
				if (F.Live.bOn && (bStoryNow ? FromCore(F, R.X, R.Y) : FromLikely(F, R.X, R.Y)) < R.R + 0.5) return false;
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
		// (a live battle's groups' middles, kept LiveGroupApart apart)
		std::vector<std::pair<double, double>> Middles;
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
				const double T = D.U(-1.0, 1.0) * AxisHalf * 0.85;
				double Across = D.Bell() * 45.0;
				if (F.Live.bOn)
				{
					// (a live battle's axis runs through the middle of its advance, which is kept clear: the group lies
					// beside it instead, out on one flank, 15 to 250 m past the likely ground, as an earlier battle's
					// advance over the same field)
					const double Flank = D.U() < 0.5 ? -1.0 : 1.0;
					const double Past = D.U(15.0, 250.0);
					double Out = 0.0;
					for (int Step = 0; Step < 400 && FromLikely(F, AxisX + AxisUx * T - AxisUy * Flank * Out, AxisY + AxisUy * T + AxisUx * Flank * Out) <= 0.0; ++Step) Out += 10.0;
					Across = Flank * (Out + Past);
				}
				Cx = AxisX + AxisUx * T - AxisUy * Across;
				Cy = AxisY + AxisUy * T + AxisUx * Across;
			}
			if (!InBox(Cx, Cy)) continue;
			bStoryNow = F.Live.bOn && (W == Where::OldLine || W == Where::Spot);
			if (F.Live.bOn)
			{
				bool bCrowded = false;
				for (const auto& M : Middles) bCrowded = bCrowded || std::hypot(M.first - Cx, M.second - Cy) < LiveGroupApart;
				if (bCrowded) continue;
			}
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
				// Her run-off track as it lies, within the strip the placement read (LieTrack): her outline's rings along it
				// as it lies, the grass's and the carpet's (the next wreck kept off the strip as read, as before).
				if (H.TrackOff != 0 && !H.Strip.empty())
				{
					const Kit::Track& Tr = K.Tracks[H.TrackOff < 0 ? 0 : 1];
					const size_t Was = (H.Strip.size() + 2) / 3;
					H.Rings.resize(H.Rings.size() - std::min(Was, H.Rings.size()));
					// (its flat links on open ground and off her thrown turret, as the strip's rings were read)
					const double TurretHalf = 0.5 * std::hypot(M.TurretHi.X - M.TurretLo.X, M.TurretHi.Y - M.TurretLo.Y);
					LieTrack(H, Tr, M, [&F, &H, TurretHalf](double X, double Y)
						{ return (!F.Open || F.Open(X, Y)) && (!H.bTurretOff || std::hypot(X - H.TurretX, Y - H.TurretY) >= TurretHalf); });
					const double Half = 0.5 * (Tr.DMax - Tr.DMin) + 0.15;
					const size_t Flat = H.Fold > 0 ? static_cast<size_t>(H.Fold) : H.Strip.size();
					for (size_t L = 0; L < Flat; L += 3)
					{
						H.Rings.push_back({ H.Strip[L].first, H.Strip[L].second, Half });
					}
					if (Flat < H.Strip.size()) H.Rings.push_back({ H.Strip[Flat].first, H.Strip[Flat].second, Half });
				}
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
				if (F.Live.bOn) Middles.emplace_back(Cx, Cy);
				if (bStoryNow)
				{
					// (a group along an old line or at a junction or the village's edge on the likely ground round its core)
					bool bOn = false;
					for (size_t K = List.size() - static_cast<size_t>(Laid); K < List.size() && !bOn; ++K)
					{
						for (const Ring& R : List[K].Rings) bOn = bOn || FromLikely(F, R.X, R.Y) < R.R + 0.5;
					}
					OnLikely += bOn ? 1 : 0;
				}
			}
		}
		NoRoom = Target - static_cast<int>(List.size());
		Swept.assign(List.size(), 0);
		DueAt.assign(List.size(), 1e18);
		SoonAt.assign(List.size(), 1e18);

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
			// (a wreck the live battle's guard cleared away: the grass stands where she lay)
			if (K < Swept.size() && Swept[K] != 0) continue;
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

	std::vector<size_t> Yard::Reached(const LandFeed::Recording& Rec, std::vector<size_t>& Seen, double Crews, double Blend)
	{
		std::vector<size_t> Out;
		if (List.empty()) return Out;
		if (Swept.size() != List.size()) Swept.assign(List.size(), 0);
		if (DueAt.size() != List.size()) DueAt.assign(List.size(), 1e18);
		SoonAt.assign(List.size(), 1e18);
		// (each wreck's reach round her anchor: her rings' farthest, for a quick look before her rings are read)
		std::vector<double> Round(List.size(), 0.0);
		for (size_t K = 0; K < List.size(); ++K)
		{
			for (const Ring& R : List[K].Rings) Round[K] = std::max(Round[K], std::hypot(R.X - List[K].X, R.Y - List[K].Y) + R.R);
		}
		// (a piece of a way, A at battle time Ta to B at Tb, as it is drawn between two kept points: when it first comes
		// within Far of wreck K's outline, 1e18 never)
		const auto When = [&](size_t K, double Ax, double Ay, double Ta, double Bx, double By, double Tb, double Far) -> double
		{
			const Hulk& H = List[K];
			if (FromSegment(Ax, Ay, Bx, By, H.X, H.Y) >= Round[K] + Far) return 1e18;
			const double Dx = Bx - Ax, Dy = By - Ay, Long2 = Dx * Dx + Dy * Dy;
			double First = 1e18;
			for (const Ring& R : H.Rings)
			{
				const double Reach = R.R + Far;
				if (FromSegment(Ax, Ay, Bx, By, R.X, R.Y) >= Reach) continue;
				const double Fx = Ax - R.X, Fy = Ay - R.Y, C = Fx * Fx + Fy * Fy - Reach * Reach;
				double U = 0.0;
				if (C > 0.0 && Long2 > 0.0)
				{
					// (where the line first crosses her ring's reach, the nearer of its two crossings)
					const double B = Fx * Dx + Fy * Dy, Disc = std::max(0.0, B * B - Long2 * C);
					U = std::clamp((-B - std::sqrt(Disc)) / Long2, 0.0, 1.0);
				}
				First = std::min(First, Ta + U * (Tb - Ta));
			}
			return First;
		};
		const auto Mark = [&](size_t K, double T)
		{
			if (Swept[K] != 0 || T >= 1e17) return;
			if (DueAt[K] >= 1e17) Out.push_back(K);
			DueAt[K] = std::min(DueAt[K], T);
		};
		const auto Touch = [&](double Ax, double Ay, double Ta, double Bx, double By, double Tb, double Far)
		{
			for (size_t K = 0; K < List.size(); ++K)
			{
				if (Swept[K] == 0) Mark(K, When(K, Ax, Ay, Ta, Bx, By, Tb, Far));
			}
		};
		if (Seen.size() > Rec.Tracks.size()) Seen.clear();
		Seen.resize(Rec.Tracks.size(), 0);
		double Latest = -1e18;
		for (const LandFeed::Track& Tr : Rec.Tracks)
		{
			if (!Tr.At.empty()) Latest = std::max(Latest, Tr.At.back().T);
		}
		for (size_t T = 0; T < Rec.Tracks.size(); ++T)
		{
			const LandFeed::Track& Tr = Rec.Tracks[T];
			const double Far = Tr.IsVehicle() ? VehicleReach : ManReach;
			const size_t N = Tr.At.size();
			const auto Ok = [](const LandFeed::Point& P) { return P.X == P.X && P.Y == P.Y && P.T == P.T; };
			// (a tank drawn rolling smoothly, Blend: her reach along a piece of her way BlendLead Blend her fastest more,
			// over the kept points within the bell's reach of it and a step, 2.5 Blend and 0.1 s; a step faster than 25 m/s
			// not counted, the screen putting her there unblended)
			const bool bBlend = Blend > 0.0 && Blend == Blend && Tr.Role == "tank";
			const double Bell = bBlend ? 2.5 * Blend + 0.1 : 0.0;
			const auto Wide = [&](size_t I) -> double
			{
				if (!bBlend || I == 0 || I >= N) return Far;
				const double Lo = Tr.At[I - 1].T - Bell, Hi = Tr.At[I].T + Bell;
				size_t A = I, B = I;
				while (A > 1 && Tr.At[A - 1].T > Lo) --A;
				while (B + 1 < N && Tr.At[B].T < Hi) ++B;
				double Fastest = 0.0;
				for (size_t J = A; J <= B; ++J)
				{
					const LandFeed::Point& P = Tr.At[J - 1];
					const LandFeed::Point& Q = Tr.At[J];
					const double Dt = Q.T - P.T;
					if (!Ok(P) || !Ok(Q) || Dt <= 1e-9) continue;
					const double Speed = std::hypot(Q.X - P.X, Q.Y - P.Y) / Dt;
					if (Speed <= 25.0) Fastest = std::max(Fastest, Speed);
				}
				return Far + BlendLead * Blend * Fastest;
			};
			// (a track shorter than read before was made again: read it whole; a smoothly rolling tank's way read again back
			// over the bell's reach of her new points, as they widen it)
			size_t From = Seen[T] <= N ? Seen[T] : 0;
			if (bBlend && From > 1 && From < N)
			{
				const double Back = Tr.At[From - 1].T - Bell;
				while (From > 1 && Tr.At[From - 1].T > Back) --From;
			}
			for (size_t I = From; I < N; ++I)
			{
				const LandFeed::Point& P = Tr.At[I];
				const LandFeed::Point& Was = Tr.At[I > 0 ? I - 1 : 0];
				if (Ok(P) && Ok(Was)) Touch(Was.X, Was.Y, Was.T, P.X, P.Y, P.T, Wide(I));
			}
			// (a knocked-out machine is drawn where she was at her fall from then on: LandKit's PlaceAt holds her a hair
			// before it, on her kept points joined straight, and a live battle's machine is never pinned, the live drill
			// stopping her where she is hit; and at her pin from its time where the reading pinned her (a recorded battle's
			// jump to her formation place). Where the scene's crews bail out, a tank's run from her as far as Crews from her
			// kill on.)
			const bool bFell = Tr.IsVehicle() && Tr.Fell >= 0.0 && Tr.Fell == Tr.Fell;
			const bool bPin = Tr.bPinned && Tr.PinX == Tr.PinX && Tr.PinY == Tr.PinY && Tr.PinFrom == Tr.PinFrom;
			if (bFell || bPin)
			{
				const double Dead = Crews > 0.0 && Tr.Role == "tank" ? std::max(Far, Crews) : Far;
				const double Since = bFell && bPin ? std::min(Tr.Fell, Tr.PinFrom) : bFell ? Tr.Fell : Tr.PinFrom;
				if (bPin) Touch(Tr.PinX, Tr.PinY, Since, Tr.PinX, Tr.PinY, Since, Dead);
				double Sx = 0.0, Sy = 0.0;
				if (bFell && PlaceOf(Tr, Tr.Fell - 1e-3, Sx, Sy)) Touch(Sx, Sy, Since, Sx, Sy, Since, Dead);
			}
			// (a man who left the field with his block is drawn on at his last speed and facing for a second as he fades:
			// read as reaching LeaveDrift more from where he left, whichever way he faced)
			if (Tr.IsMan() && Tr.Out >= 0.0 && Tr.Out == Tr.Out && N > 0)
			{
				size_t I = N - 1;
				while (I > 0 && Tr.At[I].T > Tr.Out) --I;
				const LandFeed::Point& P = Tr.At[I];
				const LandFeed::Point& Q = Tr.At[I + 1 < N ? I + 1 : I];
				const double U = Q.T > P.T ? std::clamp((Tr.Out - P.T) / (Q.T - P.T), 0.0, 1.0) : 0.0;
				const double Lx = P.X + (Q.X - P.X) * U, Ly = P.Y + (Q.Y - P.Y) * U;
				if (Ok(P) && Ok(Q)) Touch(Lx, Ly, Tr.Out, Lx, Ly, Tr.Out, Far + LeaveDrift);
			}
			// (his provisional last point, where he stands now, is replaced with the next piece: read from it again)
			Seen[T] = Tr.bTail && N > 0 ? N - 1 : N;
			// Heading for one: a man or machine moving as the piece ends (his last kept point the latest), his last way
			// straight on SoonAhead of battle (SoonFar at most), his reach and SoonWiden round it.
			if (N >= 2 && Tr.At[N - 1].T >= Latest - 2.0 && !(Tr.bPinned && Tr.PinFrom <= Tr.At[N - 1].T) && !(Tr.Fell >= 0.0 && Tr.Fell <= Tr.At[N - 1].T)
				&& !(Tr.Out >= 0.0 && Tr.Out <= Tr.At[N - 1].T))
			{
				const LandFeed::Point& A = Tr.At[N - 2];
				const LandFeed::Point& B = Tr.At[N - 1];
				const double Dt = B.T - A.T;
				if (Ok(A) && Ok(B) && Dt > 1e-3)
				{
					const double Vx = (B.X - A.X) / Dt, Vy = (B.Y - A.Y) / Dt, Speed = std::hypot(Vx, Vy);
					if (Speed > 0.5)
					{
						const double Ahead = std::min(SoonAhead, SoonFar / Speed);
						for (size_t K = 0; K < List.size(); ++K)
						{
							if (Swept[K] != 0) continue;
							SoonAt[K] = std::min(SoonAt[K], When(K, B.X, B.Y, B.T, B.X + Vx * Ahead, B.Y + Vy * Ahead, B.T + Ahead, Wide(N - 1) + SoonWiden));
						}
					}
				}
			}
		}
		return Out;
	}

	Sight SightAlong(double EyeX, double EyeY, double EyeZ, double AheadX, double AheadY, double AheadZ, double WideFov, double Aspect, double Lines, bool bCut)
	{
		Sight Out;
		Out.EyeX = EyeX;
		Out.EyeY = EyeY;
		Out.EyeZ = EyeZ;
		const double Long = std::sqrt(AheadX * AheadX + AheadY * AheadY + AheadZ * AheadZ);
		const double Scale = Long > 1e-9 && Long == Long ? 100.0 / Long : 0.0;
		Out.LookX = EyeX + AheadX * Scale;
		Out.LookY = EyeY + AheadY * Scale;
		Out.LookZ = EyeZ + AheadZ * Scale;
		if (Aspect > 0.0 && Aspect == Aspect) Out.Aspect = Aspect;
		if (Lines > 0.0 && Lines == Lines) Out.Lines = Lines;
		// (the lens across the picture to the lens from its top to its bottom, by the picture's shape)
		if (WideFov > 0.0 && WideFov < 180.0)
		{
			const double HalfWide = 0.5 * WideFov * Pi / 180.0;
			Out.Fov = 2.0 * std::atan(std::tan(HalfWide) / std::max(0.1, Out.Aspect)) * 180.0 / Pi;
		}
		Out.bCut = bCut;
		return Out;
	}

	bool Yard::InSight(size_t K, const Sight& Eye) const
	{
		if (K >= List.size()) return false;
		const Hulk& H = List[K];
		// (her reach: her rings and her scorched patch at its ragged edge's widest, about her anchor a metre up)
		double Reach = 1.12 * std::max(H.PatchLong, H.PatchWide);
		for (const Ring& R : H.Rings) Reach = std::max(Reach, std::hypot(R.X - H.X, R.Y - H.Y) + R.R);
		const double Wx = Eye.LookX - Eye.EyeX, Wy = Eye.LookY - Eye.EyeY, Wz = Eye.LookZ - Eye.EyeZ, Wl = std::sqrt(Wx * Wx + Wy * Wy + Wz * Wz);
		const double Tx = H.X - Eye.EyeX, Ty = H.Y - Eye.EyeY, Tz = H.Z + 1.0 - Eye.EyeZ;
		if (std::sqrt(Tx * Tx + Ty * Ty + Tz * Tz) <= Reach + 0.5) return true;
		if (Wl < 1e-9) return true;
		const double Fx = Wx / Wl, Fy = Wy / Wl, Fz = Wz / Wl;
		double Rx = Fy, Ry = -Fx;
		const double Rl = std::hypot(Rx, Ry);
		if (Rl < 1e-9)
		{
			Rx = 1.0;
			Ry = 0.0;
		}
		else
		{
			Rx /= Rl;
			Ry /= Rl;
		}
		// (up: her right crossed with forward, as LandEye's InPicture)
		const double Upx = Ry * Fz, Upy = -Rx * Fz, Upz = Rx * Fy - Ry * Fx;
		const double Depth = Tx * Fx + Ty * Fy + Tz * Fz, Across = Tx * Rx + Ty * Ry, Up = Tx * Upx + Ty * Upy + Tz * Upz;
		if (Depth <= -Reach) return false;
		// (the picture's edges a tenth wider, for an eye that turns; her reach a sphere against each edge's plane)
		const double TanV = std::tan(std::clamp(Eye.Fov, 1.0, 170.0) * 0.5 * Pi / 180.0) * 1.1, TanH = TanV * std::max(0.1, Eye.Aspect);
		if (std::fabs(Across) > TanH * Depth + Reach * std::sqrt(1.0 + TanH * TanH)) return false;
		if (std::fabs(Up) > TanV * Depth + Reach * std::sqrt(1.0 + TanV * TanV)) return false;
		// (and big enough to be seen go: her reach SightPx of the picture's lines or more)
		const double Px = Reach / (std::max(Depth, 0.1) * TanV / 1.1) * 0.5 * Eye.Lines;
		return Px >= SightPx;
	}

	std::vector<size_t> Yard::Clear(double Now, const Sight& Eye, std::vector<int>* How)
	{
		std::vector<size_t> Out;
		for (size_t K = 0; K < List.size() && K < Swept.size(); ++K)
		{
			if (Swept[K] != 0) continue;
			const double Due = DueOf(K);
			if (Due >= 1e17 && SoonOf(K) >= 1e17) continue;
			int Why = -1;
			if (Eye.bCut) Why = 1;
			else if (!InSight(K, Eye)) Why = 0;
			else if (Due < 1e17 && Now >= Due - ClearLead) Why = 2;
			if (Why < 0) continue;
			Swept[K] = 1;
			Out.push_back(K);
			if (How) How->push_back(Why);
		}
		return Out;
	}

	bool Yard::Over(double X, double Y, double Margin) const
	{
		for (size_t K = 0; K < List.size(); ++K)
		{
			if (K < Swept.size() && Swept[K] != 0) continue;
			const Hulk& H = List[K];
			// (her patch's ragged edge reaches 1.12 of its size at its widest, SpokeReach)
			const double PatchLong = 1.12 * H.PatchLong + Margin, PatchWide = 1.12 * H.PatchWide + Margin;
			double Reach = std::max(PatchLong, PatchWide);
			for (const Ring& R : H.Rings) Reach = std::max(Reach, std::hypot(R.X - H.X, R.Y - H.Y) + R.R + Margin);
			if (std::hypot(X - H.X, Y - H.Y) > Reach) continue;
			for (const Ring& R : H.Rings)
			{
				if (std::hypot(X - R.X, Y - R.Y) < R.R + Margin) return true;
			}
			if (FromFoot(H, X, Y) <= Margin) return true;
			double Fx, Fy, Rx, Ry;
			Ways(H.Heading, Fx, Fy, Rx, Ry);
			const double A = ((X - H.X) * Fx + (Y - H.Y) * Fy) / PatchLong, B = ((X - H.X) * Rx + (Y - H.Y) * Ry) / PatchWide;
			if (A * A + B * B <= 1.0) return true;
		}
		return false;
	}

	void Yard::Dress(size_t K, bool bNear, LandField::Mesh& Out, Dressing* Note) const
	{
		Out = LandField::Mesh();
		if (Note)
		{
			Note->Coats.clear();
			Note->Burnt = Note->Glass = Note->Hoops = Note->Planks = Note->PlanksGone = Note->Rims = Note->Wood = Note->Hole = 0;
			Note->BedTop = Note->PlankTop = Note->BedArea = Note->BedDropped = Note->WoodKept = Note->RingR = 0.0;
		}
		const LandKit::Model* Mp = ModelOf(K);
		if (!Mp || !Ground) return;
		const LandKit::Model& M = *Mp;
		const Hulk& H = List[K];
		const Kit Ki = KitOf(M);
		if (Note && H.bTurretOff) Note->RingR = Ki.RingR;
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
		Fire W;
		W.H = &H;
		W.Months = Note && Note->Months >= 0.0 ? Note->Months : std::max(0.0, H.Months);
		W.Snowed = SnowSince(Snow, W.Months);
		W.Top = Ki.Top;
		W.At = Ki.Fires;
		W.OnTurret = Ki.FireOnTurret;
		W.Ring = Ki.RingFire;
		W.bRingOpen = H.bTurretOff;
		W.Hotter = M.Kind != "tank" ? 0.08 : 0.0;
		W.Burnt = Tone(M.BurntColour[0], M.BurntColour[1], M.BurntColour[2]);
		W.Earth = Tone(112, 96, 72);
		if (Colour)
		{
			const Rgb8 E = Colour(H.X, H.Y);
			W.Earth = Mixed(Tone(E.R, E.G, E.B), Tone(104, 88, 66), 0.5);
		}
		// (the way the wind blew as she burnt, across her at rest: her own, so her soot streaks one way)
		const double Blew = 2.0 * Pi * static_cast<double>(Mix(H.Seed, 0x3A1Dull) >> 11) * (1.0 / 9007199254740992.0);
		W.Wind = V3(std::cos(Blew), std::sin(Blew), 0.0);
		// (her thrown turret was gone from over her ring when the fire took hold: it burnt from its own hatches only)
		ThresholdsFor(M, Ki, H, W, W.Bare, W.Scorch, W.Gain);
		Fire Gone = W;
		Gone.bRingOpen = false;
		Gone.bThrown = true;
		// (her height over each spot of her, for the runs of rust down her sides: her hull's and what stays on it, and a
		// thrown turret's own)
		const Tops HullTops = TopsOf(M, Ki, H, false), TurretTops = H.bTurretOff ? TopsOf(M, Ki, H, true) : Tops();
		W.Over = &HullTops;
		Gone.Over = &TurretTops;
		// (a lorry's and a half-track's glass gone, its frames left empty; a tank's vision blocks blackened in their ports)
		const bool bSoft = M.Kind != "tank";
		const int OffBody = H.TrackOff != 0 ? M.TrackBody[H.TrackOff < 0 ? 0 : 1] : -1;
		for (size_t B = 0; B < M.Bodies.size(); ++B)
		{
			const LandKit::Body& Bo = M.Bodies[B];
			if (Ki.Drop[B] || static_cast<int>(B) == OffBody) continue;
			const LandKit::Mesh& Drawn = bNear ? Bo.Near : Bo.Far;
			if (Drawn.Count() == 0 || Drawn.Points.size() < Drawn.Count() * 3 || Drawn.Normals.size() < Drawn.Count() * 3 || Drawn.Colours.size() < Drawn.Count() * 4) continue;
			const bool bThrown = H.bTurretOff && Ki.OnTurret[B];
			const LandKit::Xf& X = Xf[static_cast<size_t>(Bo.Part)];
			const Stuff What = Bo.bTrack ? Stuff::Link : Stuff::Steel;
			// What each point is made of, by its paint (a point with none, as a captor's mark, is her coat).
			std::vector<Kit::Matter> Made(Drawn.Count(), Kit::Matter::Coat);
			for (size_t I = 0; I < Made.size() && I < Drawn.Paints.size(); ++I)
			{
				if (Drawn.Paints[I] < Ki.PaintIs.size()) Made[I] = Ki.PaintIs[Drawn.Paints[I]];
			}
			// (her coat's big plates halved, so her patches show on them: near down to NearHalve, far to FarHalve. Round 2
			// halved only the near level, and the far one, seen beyond 140 m where the strategy camera mostly sits, turned
			// one coat again: its few big plates took one fate a corner, and nearly every corner, on a hard crease, the
			// edges' orange)
			LandKit::Mesh Fine;
			bool bFine = false;
			if (!Bo.bTrack)
			{
				Fine = Drawn;
				bFine = Halve(Fine, Made, bNear ? NearHalve : FarHalve);
			}
			const LandKit::Mesh& Me = bFine ? Fine : Drawn;
			const size_t Count = Me.Count();
			std::vector<uint8_t> Wood(Count, 0);
			bool bWood = false;
			for (size_t I = 0; I < Count; ++I)
			{
				Wood[I] = Made[I] == Kit::Matter::Wood ? 1 : 0;
				bWood = bWood || Wood[I] != 0;
			}
			// (a lorry's wooden bed: its floor laid again as burnt planks, LayPlanks)
			const Bed Floor = bWood && bSoft ? BedOf(Me, Wood) : Bed();
			if (Note && Floor.bOk)
			{
				Note->BedTop = Bo.Pivot.Z + Floor.Hi.Z;
				Note->BedArea = (Floor.Hi.X - Floor.Lo.X) * (Floor.Hi.Y - Floor.Lo.Y);
			}
			const auto InBed = [&Me, &Floor](uint32_t I)
			{
				const double Px = Me.Points[I * 3], Py = Me.Points[I * 3 + 1], Pz = Me.Points[I * 3 + 2];
				return Px >= Floor.Lo.X - 0.01 && Px <= Floor.Hi.X + 0.01 && Py >= Floor.Lo.Y - 0.01 && Py <= Floor.Hi.Y + 0.01 && Pz >= Floor.Lo.Z - 0.01
					&& Pz <= Floor.Hi.Z + 0.01;
			};
			// The triangles kept: none with a point burnt away (canvas, leather, rope, cloth), none of a lorry's glass, none
			// of her bed's floor.
			std::vector<uint8_t> Used(Count, 0);
			std::vector<uint32_t> Keep;
			Keep.reserve(Me.Triangles.size());
			for (size_t T = 0; T + 2 < Me.Triangles.size(); T += 3)
			{
				const uint32_t A = Me.Triangles[T], B2 = Me.Triangles[T + 1], C = Me.Triangles[T + 2];
				if (A >= Count || B2 >= Count || C >= Count) continue;
				bool bDrop = false;
				for (const uint32_t V : { A, B2, C })
				{
					bDrop = bDrop || Made[V] == Kit::Matter::Gone || (bSoft && Made[V] == Kit::Matter::Glass);
				}
				const bool bAllWood = Wood[A] && Wood[B2] && Wood[C];
				const bool bFloor = !bDrop && Floor.bOk && bAllWood && InBed(A) && InBed(B2) && InBed(C);
				if (Note && bAllWood && !bDrop)
				{
					// (the wood facing up taken off as her floor, and kept: her benches, her sides' tops)
					const V3 Pa(Me.Points[A * 3], Me.Points[A * 3 + 1], Me.Points[A * 3 + 2]), Pb(Me.Points[B2 * 3], Me.Points[B2 * 3 + 1], Me.Points[B2 * 3 + 2]);
					const V3 Pc(Me.Points[C * 3], Me.Points[C * 3 + 1], Me.Points[C * 3 + 2]);
					const double Nz = static_cast<double>(Me.Normals[A * 3 + 2]) + Me.Normals[B2 * 3 + 2] + Me.Normals[C * 3 + 2];
					if (Nz > 2.85) (bFloor ? Note->BedDropped : Note->WoodKept) += 0.5 * (Pb - Pa).Cross(Pc - Pa).Length();
				}
				if (bFloor) bDrop = true;
				if (bDrop) continue;
				Keep.insert(Keep.end(), { A, B2, C });
				Used[A] = Used[B2] = Used[C] = 1;
			}
			// (a lorry's tilt is one body with her hull, LandKit's fixed parts under the part that moves: her hoops are what
			// still stands just under where the burnt canvas's top was)
			double ClothTop = -1e9;
			if (Ki.Tilt >= 0)
			{
				for (size_t I = 0; I < Count; ++I)
				{
					if (Made[I] == Kit::Matter::Gone) ClothTop = std::max(ClothTop, static_cast<double>(Me.Points[I * 3 + 2]));
				}
			}
			if (Note)
			{
				for (size_t I = 0; I < Count; ++I)
				{
					Note->Burnt += Made[I] == Kit::Matter::Gone ? 1 : 0;
					Note->Glass += Made[I] == Kit::Matter::Glass ? 1 : 0;
					Note->Wood += Made[I] == Kit::Matter::Wood && Used[I] ? 1 : 0;
					Note->Hoops += Ki.Tilt >= 0 && ClothTop > -1e8 && Used[I] && Me.Points[I * 3 + 2] > ClothTop - 0.3 ? 1 : 0;
				}
				Note->Rims += Ki.Tyre[B] == 2 ? 1 : 0;
			}
			// (a plate's edge only on the near level: the far level's few big plates have nearly every corner on a crease)
			const std::vector<uint8_t> Edge = bNear ? EdgesOf(Me) : std::vector<uint8_t>(Count, 0);
			std::vector<uint32_t> Index(Count, UINT32_MAX);
			for (size_t I = 0; I < Count; ++I)
			{
				if (!Used[I]) continue;
				V3 Local(Me.Points[I * 3], Me.Points[I * 3 + 1], Me.Points[I * 3 + 2]);
				Stuff Is = What;
				if (Ki.Tyre[B] == 2)
				{
					// (a lorry's tyre burnt off: its rubber gone to its rim, the rim steel)
					if (OnTyre(M, Bo, Local, Ki.TyreR[B]) > Ki.RimR[B] / Ki.TyreR[B] + 0.02) Is = Stuff::Rim;
					Local = ToRim(M, Bo, Local, Ki.RimR[B]);
				}
				else if (Ki.Tyre[B] == 1 && (Made[I] == Kit::Matter::Rubber || OnTyre(M, Bo, Local, Ki.TyreR[B]) > Ki.RimR[B] / Ki.TyreR[B]))
				{
					Is = Stuff::Rubber;
				}
				else if (What == Stuff::Steel)
				{
					Is = Made[I] == Kit::Matter::Wood ? Stuff::Wood : Made[I] == Kit::Matter::Glass ? Stuff::Glass : Made[I] == Kit::Matter::Rubber ? Stuff::Rubber : Stuff::Steel;
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
				// (her open ring, where the turret sat: the hull's roof inside its ring, and its lip)
				int Ring = 0;
				if (H.bTurretOff && !bThrown && Ki.RingR > 0.0 && Posed.Z > M.TurretLo.Z - 0.3)
				{
					const double From = std::hypot(Posed.X - Ki.TurretPivot.X, Posed.Y - Ki.TurretPivot.Y);
					Ring = From < Ki.RingR ? 1 : From < Ki.RingR + 0.08 ? 2 : 0;
				}
				const double Soot = I < Me.Soot.size() ? Me.Soot[I] : 0.8;
				const V3 RestWay(Me.Normals[I * 3], Me.Normals[I * 3 + 1], Me.Normals[I * 3 + 2]);
				// (her hoops thin bent tube, every point of them on a crease: no edge's orange on them, or they went wholly
				// orange a week on)
				const bool bHoop = ClothTop > -1e8 && Me.Points[I * 3 + 2] > ClothTop - 0.3;
				const Burnt Bu = Burn(bThrown ? Gone : W, &Me.Colours[I * 4], Soot, Rest, RestWay, Up.Unit().Z, bThrown ? 1.0 : Rest.Z, Edge[I] != 0 && !bHoop, Is, Ring);
				if (Note && Ring == 1) ++Note->Hole;
				Index[I] = Point(Out, At, Up, Bu.C);
				if (Note) Note->Coats.push_back(static_cast<uint8_t>(Bu.Is));
			}
			// (the files wind a face counter-clockwise from its front in her own axes, which turn into the field's with a
			// mirror on the way: each triangle's last two corners swapped, as the scene's KitFront does for a body)
			for (size_t T = 0; T + 2 < Keep.size(); T += 3)
			{
				Out.Triangles.insert(Out.Triangles.end(), { Index[Keep[T]], Index[Keep[T + 2]], Index[Keep[T + 1]] });
			}
			if (Floor.bOk)
			{
				LayPlanks(Out, Note, Floor, Bo.Pivot, X, Hull, W);
			}
		}
		// Her run-off track, link by link as it ran under her wheels (its outer face down): flat on the ground along the
		// strip, snaking, then turned over in the fold and lying doubled back on top, upside down (LieTrack); the far level
		// a band of its width.
		if (H.TrackOff != 0 && OffBody >= 0 && !H.Strip.empty())
		{
			const Kit::Track& Tr = Ki.Tracks[H.TrackOff < 0 ? 0 : 1];
			const LandKit::Body& Bo = M.Bodies[static_cast<size_t>(OffBody)];
			const size_t Links = std::min(Tr.Mid.size(), H.Strip.size());
			const size_t Fold = H.Fold > 1 ? std::min(Links, static_cast<size_t>(H.Fold)) : Links;
			const size_t Curl = Fold < Links ? std::min(Links - Fold, static_cast<size_t>(std::max(0, H.Curl))) : 0;
			const double Thick = Tr.BMax - Tr.BMin;
			// Each link's frame: its outer face's middle (Base), its way along (Al), its way in from its outer face (In),
			// and across (Xa).
			std::vector<V3> Base(Links), Al(Links), In(Links), Xa(Links), Lie(Links);
			for (size_t L = 0; L < Fold; ++L)
			{
				const auto& P = H.Strip[L];
				const auto& Before = H.Strip[L > 0 ? L - 1 : L];
				const auto& After = H.Strip[L + 1 < Fold ? L + 1 : L];
				double Gz = 0.0;
				V3 N;
				Ground(P.first, P.second, Gz, N);
				const V3 Way = V3(After.first - Before.first, After.second - Before.second, 0.0).Unit(V3(0.0, 1.0, 0.0));
				In[L] = N.Unit();
				Al[L] = (Way - In[L] * Way.Dot(In[L])).Unit(Way);
				Xa[L] = In[L].Cross(Al[L]).Unit(V3(1.0, 0.0, 0.0));
				// (on whichever is drawn higher there, the ground or her scorched patch 2 cm over it: a lorry's or a
				// half-track's band is barely 3 cm thick, and sunk 2 cm it showed only its guide horns through the patch)
				const double Lies = DrawnZ ? std::max(Gz, DrawnZ(P.first, P.second) + 0.02) : Gz;
				Base[L] = V3(P.first - H.X, P.second - H.Y, Lies - H.Z + 0.005);
				// (the face that looks up: its inner face on the ground)
				Lie[L] = Base[L] + In[L] * Thick;
			}
			if (Curl > 0)
			{
				const size_t E = Fold - 1;
				const V3 Mid = Base[E] + Al[E] * (0.5 * Tr.Pitch) + In[E] * Thick;
				for (size_t J = 0; J < Curl; ++J)
				{
					const double Phi = Pi * (static_cast<double>(J) + 0.5) / static_cast<double>(Curl);
					const V3 OutWay = Al[E] * std::sin(Phi) - In[E] * std::cos(Phi);
					const size_t L = Fold + J;
					Base[L] = Mid + OutWay * Thick;
					Al[L] = Al[E] * std::cos(Phi) + In[E] * std::sin(Phi);
					In[L] = -OutWay;
					Xa[L] = Xa[E];
					Lie[L] = Base[L];
				}
				for (size_t L = Fold + Curl; L < Links; ++L)
				{
					const size_t U = Fold - 1 - std::min(Fold - 1, L - Fold - Curl);
					// (angled out off the link it lies on as LieTrack laid it, so the fold shows from above)
					const V3 Aside(H.Strip[L].first - H.Strip[U].first, H.Strip[L].second - H.Strip[U].second, 0.0);
					Base[L] = Base[U] + In[U] * (2.0 * Thick) + Xa[U] * Aside.Dot(Xa[U]);
					Al[L] = -Al[U];
					In[L] = -In[U];
					Xa[L] = Xa[U];
					Lie[L] = Base[L];
				}
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
					const V3 Laid = Base[Li] + Al[Li] * Dp.Dot(Tr.Along[Li]) + In[Li] * (Tr.BMax - Dp.Dot(Tr.Out[Li])) + Xa[Li] * Dp.Dot(Side);
					const V3 Facing = Al[Li] * N.Dot(Tr.Along[Li]) - In[Li] * N.Dot(Tr.Out[Li]) + Xa[Li] * N.Dot(Side);
					const double Soot = I < Me.Soot.size() ? Me.Soot[I] : 0.8;
					const Burnt Bu = Burn(W, &Me.Colours[I * 4], Soot, Bo.Pivot + P, N, Facing.Unit().Z, 0.1, false, Stuff::Link, 0);
					Index[I] = Point(Out, Laid, Facing, Bu.C);
					if (Note) Note->Coats.push_back(static_cast<uint8_t>(Bu.Is));
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
				const double Half = 0.5 * (Tr.DMax - Tr.DMin);
				const uint8_t Steel[4] = { 80, 70, 60, 255 };
				uint32_t Was[2] = { 0, 0 };
				for (size_t L = 0; L < Links; ++L)
				{
					// (the band's face that looks up: on the ground her inner face, turned over her outer)
					const V3 Face = L < Fold ? In[L] : -In[L];
					uint32_t Now[2];
					for (int E = 0; E < 2; ++E)
					{
						const V3 P = Lie[L] + Xa[L] * (E == 0 ? -Half : Half);
						// (each link's rust read where the near level reads it: her link's middle at rest, at its edge)
						const V3 Rest = Bo.Pivot + Tr.Mid[L] + V3(0.0, E == 0 ? -Half : Half, 0.0);
						const Burnt Bu = Burn(W, Steel, 0.9, Rest, V3(0.0, 0.0, 1.0), Face.Z, 0.1, false, Stuff::Link, 0);
						Now[E] = Point(Out, P, Face, Bu.C);
						if (Note) Note->Coats.push_back(static_cast<uint8_t>(Bu.Is));
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

	void Yard::Patch(size_t K, LandField::Mesh& Out, Dressing* Note) const
	{
		Out = LandField::Mesh();
		if (Note) Note->Coats.clear();
		if (K >= List.size() || !DrawnZ) return;
		const Hulk& H = List[K];
		double Fx, Fy, Rx, Ry;
		Ways(H.Heading, Fx, Fy, Rx, Ry);
		const double M = Note && Note->Months >= 0.0 ? Note->Months : std::max(0.0, H.Months);
		// How it goes with the months (round 2, the art director's notes of 24 November: not a soft round shadow): a week
		// on the grass burnt black under and round her to a ragged edge, ash black and grey at her engine's end, a rim of
		// brown burnt grass round it, and her fuel's dark stain soaked in beside her; the ash and the black washed into a
		// burnt brown within a few months (the fuel's stain the last to go), then the weeds come up lusher and darker than
		// the field round it, fading into it over the second year (round 1's green halo, kept). Under the snow fallen
		// since she burnt, none of it: the field's own snow (Colour) lies over her patch as over the field round it, and
		// only a fresh one shows black through it.
		const double Snowed = SnowSince(Snow, M);
		const double Fresh = (M < 1.0 ? 1.0 : std::exp(-(M - 1.0) / 2.5)) * (1.0 - Snowed);
		const double FuelLeft = (1.0 - Smooth(2.0, 10.0, M)) * (1.0 - Snowed);
		const double Brown = Smooth(0.5, 3.0, M) * (1.0 - Smooth(6.0, 14.0, M)) * (1.0 - Snowed);
		const double Lush = Smooth(2.0, 8.0, M) * (1.0 - 0.6 * Smooth(18.0, 36.0, M)) * (1.0 - Snowed);
		// Her engine's end (her tail's on a tank, her bow's on a lorry) and her fuel's stain, by her own dice.
		const double MidX = 0.5 * (H.FootLoX + H.FootHiX), MidY = 0.5 * (H.FootLoY + H.FootHiY);
		const double HalfLong = std::max(0.5, 0.5 * (H.FootHiX - H.FootLoX)), HalfWide = std::max(0.5, 0.5 * (H.FootHiY - H.FootLoY));
		double EngineEnd = -1.0;
		if (const LandKit::Model* Mo = ModelOf(K))
		{
			const int E = Mo->SocketIndex("engine");
			if (E >= 0) EngineEnd = Mo->Sockets[static_cast<size_t>(E)].At.X >= MidX ? 1.0 : -1.0;
		}
		Dice Fd(Mix(H.Seed, 0xF0E1ull));
		const double FuelSide = Fd.U() < 0.5 ? -1.0 : 1.0;
		const double FuelAlong = Fd.U(0.0, 0.6);
		const double FuelOut = Fd.U(0.7, 1.4);
		const double FuelLong = Fd.U(0.8, 1.3);
		const double FuelWide = Fd.U(0.45, 0.75);
		const double FuelTurn = Fd.U(-0.7, 0.7);
		const double FuelA = MidX + EngineEnd * FuelAlong * HalfLong, FuelB = MidY + FuelSide * (HalfWide + FuelOut);
		const double Ct = std::cos(FuelTurn), St = std::sin(FuelTurn);
		// The mesh: round 1's outline (the spokes' reach PatchClear read), three spokes to each of its and 15 rings, fine
		// enough to carry a ragged edge and the fuel's crisp one (round 2's four and 20 made 4,400 triangles a patch).
		constexpr int Sub = 3, Spokes = PatchSpokes * Sub, RingCount = 16;
		double Reach[PatchSpokes];
		for (int S = 0; S < PatchSpokes; ++S)
		{
			Reach[S] = SpokeReach(H, S);
		}
		const uint64_t Seed = H.Seed;
		std::vector<uint32_t> Index(static_cast<size_t>(Spokes * RingCount));
		for (int Ri = 0; Ri < RingCount; ++Ri)
		{
			const double Ring = static_cast<double>(Ri) / (RingCount - 1);
			for (int S = 0; S < Spokes; ++S)
			{
				const int J = S / Sub;
				const double T = static_cast<double>(S % Sub) / Sub;
				const double Rch = Reach[J] * (1.0 - T) + Reach[(J + 1) % PatchSpokes] * T;
				const double Ang = 2.0 * Pi * S / Spokes;
				const double A = std::cos(Ang) * Ring * Rch * H.PatchLong, B = std::sin(Ang) * Ring * Rch * H.PatchWide;
				const double X = H.X + Fx * A + Rx * B, Y = H.Y + Fy * A + Ry * B;
				double Nx = 0.0, Ny = 0.0, Nz = 1.0;
				if (DrawnUp) DrawnUp(X, Y, Nx, Ny, Nz);
				Tone Own(96, 108, 60);
				if (Colour)
				{
					const Rgb8 C = Colour(X, Y);
					Own = Tone(C.R, C.G, C.B);
				}
				// The burnt grass: out past her outline to a ragged edge (two noises, a stride and a hand across), farthest
				// at her engine's end; never to the mesh's own edge. Inside it black; round its edge a rim of brown.
				const double Past = std::hypot(std::max({ H.FootLoX - A, 0.0, A - H.FootHiX }), std::max({ H.FootLoY - B, 0.0, B - H.FootHiY }));
				const double Along = (A - MidX) / HalfLong * EngineEnd;
				const double Ng = Smooth(0.21, 0.79, Fbm(V3(X, Y, 0.0), 0.55, Seed ^ 0x5A1ull));
				const double Nf = Noise3(X * 2.4, Y * 2.4, 0.3, Seed ^ 0x5A2ull);
				const double Burn2 = 0.3 + 1.3 * Ng + 1.1 * Smooth(0.2, 1.1, Along) + 0.4 * (Nf - 0.5);
				const double Keep = 1.0 - Smooth(0.82, 0.96, Ring);
				const double U = Past - Burn2;
				const double RimW = 0.3 + 0.4 * Noise3(X * 1.1, Y * 1.1, 0.7, Seed ^ 0x5A3ull);
				const double Core = (1.0 - Smooth(-RimW - 0.07, -RimW + 0.07, U)) * Keep;
				const double Rim = Smooth(-RimW - 0.07, -RimW + 0.07, U) * (1.0 - Smooth(-0.04, 0.1, U)) * Keep;
				// Three zones told apart in value and hue (round 2's ash, burnt grass and fuel all came within 5 of one another
				// in the picture, about 41,35,26, and read as one drop shadow): ash at her engine's end grey-black with lighter
				// ash through it in soft drifts (some 55 to 65 grey in the picture, no hue; round 2's small pale flecks read as
				// puffs of smoke); the burnt grass inside the rim a dark brown (some 65,53,36), not black; her fuel's stain a
				// cool near-black, oily, a little green-blue (some 22,23,23).
				const double Ash = Core * Smooth(-0.3, 0.4, Along + 0.7 * (Noise3(X * 0.9, Y * 0.9, 1.3, Seed ^ 0x5A4ull) - 0.5));
				const Tone AshTone = Mixed(Tone(58, 58, 56), Tone(102, 100, 96), 0.65 * Smooth(0.45, 0.8, Noise3(X * 1.5, Y * 1.5, 2.1, Seed ^ 0x5A5ull)));
				// (her fuel's stain: soaked in beside her, a pool with a crisp lobed edge, not a soft blot: a soft dark blot
				// beside her reads as her shadow)
				const double Fa = (A - FuelA) * Ct + (B - FuelB) * St, Fb = -(A - FuelA) * St + (B - FuelB) * Ct;
				const double Q = std::hypot(Fa / FuelLong, Fb / FuelWide) + 0.55 * (Noise3(X * 2.2, Y * 2.2, 3.7, Seed ^ 0x5A6ull) - 0.5)
					+ 0.2 * (Noise3(X * 5.0, Y * 5.0, 4.1, Seed ^ 0x5A7ull) - 0.5);
				const double Fuel = (1.0 - Smooth(0.8, 0.88, Q)) * Keep;
				const Tone FuelTone = Mixed(Tone(30, 31, 29), Tone(21, 23, 23), 1.0 - Smooth(0.1, 0.8, Q));
				Tone C = Own;
				const double RimBy = 0.9 * Rim * Fresh, CoreBy = (Core - Ash) * Fresh, AshBy = Ash * Fresh, FuelBy = 0.96 * Fuel * FuelLeft;
				// (the rim's grass browned and yellowed outward, burnt darker inward)
				C = Mixed(C, Mixed(Tone(112, 92, 56), Tone(88, 68, 44), Smooth(-0.04, -RimW, U)), RimBy);
				C = Mixed(C, Tone(74, 60, 40), CoreBy);
				C = Mixed(C, AshTone, AshBy);
				C = Mixed(C, FuelTone, FuelBy);
				// (the months: washed into a burnt brown where it burnt, then the weeds lusher, round 1's halo)
				const double N = Noise3(X * 0.6, Y * 0.6, 1.5, H.Seed ^ 0xF6ull);
				const double In = 1.0 - Smooth(0.5, 1.0, Ring + 0.16 * (N - 0.5));
				const double BrownBy = 0.55 * Brown * std::max(In, Core + Rim);
				C = Mixed(C, Tone(84, 66, 46), BrownBy);
				const double Halo = Smooth(0.25, 0.6, Ring) * (1.0 - Smooth(0.85, 1.0, Ring));
				const double LushBy = Lush * (0.35 + 0.4 * Halo) * In;
				C = Mixed(C, Tone(Own.R * 0.72, Own.G * 0.86, Own.B * 0.6), LushBy);
				const double Z = DrawnZ(X, Y) + 0.02 - H.Z;
				Index[static_cast<size_t>(Ri * Spokes + S)] = Point(Out, V3(X - H.X, Y - H.Y, Z), V3(Nx, Ny, Nz), ToRgb(C));
				if (Note)
				{
					// (each zone by what it shows through the ones laid over it: the rim first, then the burnt grass, the
					// ash, the fuel, the brown and the weeds, so her fuel soaked into the burnt grass is her fuel's)
					const double After = (1.0 - BrownBy) * (1.0 - LushBy);
					const double By[Scorches] = { 0.25, AshBy * (1.0 - FuelBy) * After, CoreBy * (1.0 - AshBy) * (1.0 - FuelBy) * After,
						RimBy * (1.0 - CoreBy) * (1.0 - AshBy) * (1.0 - FuelBy) * After, FuelBy * After, BrownBy * (1.0 - LushBy), LushBy };
					int Chief = 0;
					for (int I = 1; I < Scorches; ++I) Chief = By[I] > By[Chief] ? I : Chief;
					Note->Coats.push_back(static_cast<uint8_t>(Chief));
				}
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

	const char* CoatName(Coat C)
	{
		switch (C)
		{
		case Coat::Soot: return "soot";
		case Coat::Scorched: return "scorched";
		case Coat::Bare: return "bare";
		case Coat::Flash: return "flash";
		case Coat::OldRust: return "old rust";
		case Coat::Paint: return "paint";
		case Coat::Charred: return "charred";
		case Coat::Links: return "links";
		case Coat::Snow: return "snow";
		case Coat::Hole: return "ring hole";
		default: return "runs";
		}
	}

	const char* ScorchName(Scorch S)
	{
		switch (S)
		{
		case Scorch::Field: return "field";
		case Scorch::Ash: return "ash";
		case Scorch::Burnt: return "burnt";
		case Scorch::Rim: return "rim";
		case Scorch::Fuel: return "fuel";
		case Scorch::Brown: return "brown";
		default: return "lush";
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
		if (NoRoom > 0) Out += "; " + std::to_string(NoRoom) + (bLive ? " found no room clear of the lanes, hedges and the ground the live battle is likely to cross" : " found no room clear of the lanes, hedges and the battle's own ways");
		if (bLive)
		{
			// (a live battle's: laid off the ground its men and machines are likely to cross, and cleared away by the scene's
			// guard should one come near her all the same)
			Out += "; laid for a live battle, clear of where its blocks draw up, the ground between its fronts and its lanes";
			if (OnLikely > 0)
			{
				Out += " (" + std::to_string(OnLikely) + (OnLikely == 1 ? " group" : " groups") + " along the old lines or at the village's edge or a junction on the rest of its start areas or the ground it may fall back over)";
			}
		}
		return Out;
	}
}
