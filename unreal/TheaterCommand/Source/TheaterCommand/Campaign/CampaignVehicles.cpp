#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignMiniatureParts.h"
#include "TheaterCommand.h"

/**
 * The real vehicles, each built in code from the measurements of the real thing.
 *
 * Every model is set out in metres, standing on the ground at the origin with its nose along the figure's
 * "forward", so a model's numbers can be checked against a drawing of the vehicle: 2.9 forward of the middle,
 * 1.2 to the right, 0.64 up is where the Panzer IV's driving wheel turns. Because all of them use the same
 * metre, a model set beside another, or beside a man, is in scale with it.
 *
 * A model is built once, the first time it is asked for, and after that only copied into place.
 */
namespace VehicleParts
{
	static FLinearColor Hex(const TCHAR* Code)
	{
		return FLinearColor(FColor::FromHex(Code));
	}

	static const FLinearColor TrackLight = Hex(TEXT("45403A"));
	static const FLinearColor TrackDark = Hex(TEXT("2E2B28"));
	static const FLinearColor TrackEdge = Hex(TEXT("242220"));
	static const FLinearColor Tyre = Hex(TEXT("1C1C1B"));
	static const FLinearColor GunSteel = Hex(TEXT("2E2F31"));
	static const FLinearColor Rust = Hex(TEXT("5E4534"));
	static const FLinearColor Timber = Hex(TEXT("7A5C3A"));
	static const FLinearColor Glass = Hex(TEXT("D8D4B8"));
	static const FLinearColor Pitch = Hex(TEXT("161616"));

	/** The track, wheels and rollers of one vehicle, the same on both sides. */
	struct FGear
	{
		/** How far the middle of each track is from the middle of the vehicle; how wide the track is; the length of one link. */
		double Right = 1.2, Width = 0.4, Link = 0.15;
		/** The wheel at the front end of the track and the one at the back: where each turns, and its radius. */
		double FrontX = 2.5, FrontUp = 0.6, FrontR = 0.3;
		double RearX = -2.5, RearUp = 0.6, RearR = 0.3;
		/** The wheels the vehicle stands on: how far forward each is, and their radius. */
		TArray<double> Wheels;
		double WheelR = 0.25;
		/** Whether the toothed wheel that drives the track is the front one (otherwise the back one), and how many teeth it has. */
		bool bDriveFront = true;
		int32 Teeth = 13;
		/** The small wheels that carry the track back along the top, if it has them. */
		TArray<double> Rollers;
		double RollerUp = 0.9, RollerR = 0.1;
	};

	/** A vehicle being built: the mesh it goes into and the paint it wears. */
	struct FBuild
	{
		FMiniatureMesh& M;
		FLinearColor Paint, Shade;

		FBuild(FMiniatureMesh& InMesh, const FLinearColor& InPaint)
			: M(InMesh), Paint(InPaint), Shade(InPaint.R * 0.74f, InPaint.G * 0.74f, InPaint.B * 0.74f, 1.0f)
		{
		}

		/** A point of the vehicle: so far forward of its middle, so far to its right, so far above the ground. */
		static FVector At(double F, double R, double U)
		{
			return FVector(-F, -R, U);
		}

		/** A plate or a box lying fore and aft. Tip leans it so that it rises toward the back, as a sloped front plate does. */
		void Plate(double F, double R, double U, double Long, double Wide, double High, const FLinearColor& Colour, double Tip = 0.0) const
		{
			M.Box(At(F, R, U), FVector(Long, Wide, High), Colour, 0.0, Tip);
		}

		void Tube(double F0, double R0, double U0, double F1, double R1, double U1, double Radius, const FLinearColor& Colour, int32 Sides = 8) const
		{
			M.Rod(At(F0, R0, U0), At(F1, R1, U1), Radius, Colour, Sides);
		}

		/** An upright drum standing at a height: a cupola, a hatch ring. */
		void Drum(double F, double R, double U, double Radius, double High, const FLinearColor& Colour, int32 Sides = 12, double TopRadius = -1.0) const
		{
			M.Pillar(At(F, R, U), Radius, TopRadius < 0.0 ? Radius : TopRadius, High, Colour, Sides);
		}

		/** A side view pushed across the vehicle from one distance right of the middle to another. */
		void Side(const TArray<FVector2D>& Outline, double R0, double R1, const FLinearColor& Colour) const
		{
			M.Profile(FVector::ZeroVector, 1.0, 0.0, Outline, R0, R1, Colour);
		}

		/** A shape built up from plans at rising heights; rounded into one skin for cast armour. */
		void Layers(const TArray<TArray<FVector2D>>& Plans, const TArray<double>& Heights, const FLinearColor& Colour, bool bSmooth) const
		{
			M.Stack(FVector::ZeroVector, 1.0, 0.0, Plans, Heights, Colour, bSmooth);
		}

		/** A headlamp facing forward. */
		void Lamp(double F, double R, double U, double Radius) const
		{
			Tube(F - 0.13, R, U, F, R, U, Radius, Shade, 8);
			M.Disc(At(F + 0.012, R, U), FVector(-1.0, 0.0, 0.0), Radius * 0.8, Glass, 8);
			Tube(F - 0.06, R, U - Radius, F - 0.06, R, U - Radius - 0.1, 0.02, Shade, 4);
		}

		/** A machine gun in a ball mount, pointing forward. */
		void BallGun(double F, double R, double U, double Long) const
		{
			M.Ball(At(F, R, U), FVector(0.26, 0.26, 0.26), Shade, 4, 8);
			Tube(F, R, U, F + Long, R, U, 0.022, GunSteel, 5);
		}

		/** A wireless aerial, whipping back a little. */
		void Aerial(double F, double R, double U, double High) const
		{
			Tube(F, R, U, F, R, U + 0.12, 0.035, Shade, 5);
			Tube(F, R, U + 0.1, F - High * 0.12, R, U + High, 0.012, GunSteel, 4);
		}

		/** A national marking on a surface that faces the way given (forward, right, up). */
		void Mark(double F, double R, double U, double Radius, const TCHAR* Tag, double FaceF, double FaceR, double FaceU) const
		{
			M.Marking(At(F, R, U), Radius, Tag, FVector(-FaceF, -FaceR, FaceU));
		}

		/** Both tracks, link by link, with their wheels. */
		void Gear(const FGear& G) const
		{
			const double Thick = 0.05;
			// The line the track follows, seen from the side: stretched tight round the wheels like a band.
			TArray<FVector2D> Points;
			auto Round = [&Points](double X, double U, double Radius)
			{
				for (int32 K = 0; K < 20; ++K)
				{
					Points.Add(FVector2D(X + FMath::Cos(K * UE_DOUBLE_TWO_PI / 20.0) * Radius, U + FMath::Sin(K * UE_DOUBLE_TWO_PI / 20.0) * Radius));
				}
			};
			Round(G.FrontX, G.FrontUp, G.FrontR + Thick);
			Round(G.RearX, G.RearUp, G.RearR + Thick);
			for (const double X : G.Wheels)
			{
				Round(X, G.WheelR + Thick, G.WheelR + Thick);
			}
			for (const double X : G.Rollers)
			{
				Round(X, G.RollerUp, G.RollerR + Thick);
			}
			Points.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X || (A.X == B.X && A.Y < B.Y); });
			auto Bend = [](const FVector2D& O, const FVector2D& A, const FVector2D& B) { return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X); };
			TArray<FVector2D> Band;
			for (int32 K = 0; K < Points.Num(); ++K)
			{
				while (Band.Num() >= 2 && Bend(Band[Band.Num() - 2], Band.Last(), Points[K]) <= 0.0)
				{
					Band.Pop();
				}
				Band.Add(Points[K]);
			}
			const int32 Under = Band.Num() + 1;
			for (int32 K = Points.Num() - 2; K >= 0; --K)
			{
				while (Band.Num() >= Under && Bend(Band[Band.Num() - 2], Band.Last(), Points[K]) <= 0.0)
				{
					Band.Pop();
				}
				Band.Add(Points[K]);
			}
			Band.Pop();

			// Cut the band into links of equal length.
			double Around = 0.0;
			for (int32 K = 0; K < Band.Num(); ++K)
			{
				Around += (Band[(K + 1) % Band.Num()] - Band[K]).Size();
			}
			const int32 Links = FMath::Max(8, FMath::RoundToInt32(Around / G.Link));
			const double Step = Around / Links;
			TArray<FVector2D> Loop;
			double Gone = 0.0, Next = 0.0;
			for (int32 K = 0; K < Band.Num() && Loop.Num() < Links; ++K)
			{
				const FVector2D A = Band[K], B = Band[(K + 1) % Band.Num()];
				const double Length = (B - A).Size();
				while (Next <= Gone + Length && Loop.Num() < Links)
				{
					Loop.Add(FMath::Lerp(A, B, Length > 0.0 ? (Next - Gone) / Length : 0.0));
					Next += Step;
				}
				Gone += Length;
			}

			const FColor Inner = TrackEdge.ToFColor(true), Edge = TrackEdge.ToFColor(true);
			const FVector Across(0.0, -1.0, 0.0);
			for (const double Hand : { -1.0, 1.0 })
			{
				const double Mid = Hand * G.Right, R0 = Mid - G.Width * 0.5, R1 = Mid + G.Width * 0.5;
				for (int32 K = 0; K < Loop.Num(); ++K)
				{
					const FVector2D A = Loop[K], B = Loop[(K + 1) % Loop.Num()];
					const FVector2D Along = (B - A).GetSafeNormal();
					const FVector2D Out2(Along.Y, -Along.X);
					const FVector Out(-Out2.X, 0.0, Out2.Y);
					const FVector2D A2 = A - Out2 * Thick, B2 = B - Out2 * Thick;
					const FColor Shoe = (K % 2 ? TrackLight : TrackDark).ToFColor(true);
					M.Quad(At(A.X, R0, A.Y), At(B.X, R0, B.Y), At(B.X, R1, B.Y), At(A.X, R1, A.Y), Out, Shoe);
					M.Quad(At(A2.X, R0, A2.Y), At(B2.X, R0, B2.Y), At(B2.X, R1, B2.Y), At(A2.X, R1, A2.Y), -Out, Inner);
					M.Quad(At(A.X, R0, A.Y), At(B.X, R0, B.Y), At(B2.X, R0, B2.Y), At(A2.X, R0, A2.Y), -Across, Edge);
					M.Quad(At(A.X, R1, A.Y), At(B.X, R1, B.Y), At(B2.X, R1, B2.Y), At(A2.X, R1, A2.Y), Across, Edge);
				}

				// The wheels: a rubber tyre on a painted steel wheel with a hub; the driving wheel and the idler bare steel.
				const double Face = Mid + Hand * G.Width * 0.4, Back = Mid - Hand * G.Width * 0.4;
				auto Wheel = [&](double X, double U, double Radius, bool bTyre, int32 Sides)
				{
					M.Rod(At(X, Back, U), At(X, Face, U), Radius, bTyre ? Tyre : Shade, Sides);
					M.Disc(At(X, Face + Hand * 0.02, U), FVector(0.0, -Hand, 0.0), Radius * (bTyre ? 0.74 : 0.82), Paint, Sides);
					M.Rod(At(X, Face, U), At(X, Face + Hand * 0.06, U), Radius * 0.26, Shade, 6);
				};
				for (const double X : G.Wheels)
				{
					Wheel(X, G.WheelR + Thick, G.WheelR, true, 16);
				}
				for (const double X : G.Rollers)
				{
					Wheel(X, G.RollerUp, G.RollerR, true, 8);
				}
				Wheel(G.FrontX, G.FrontUp, G.FrontR, false, 16);
				Wheel(G.RearX, G.RearUp, G.RearR, false, 16);
				// The ring of teeth on the driving wheel, standing through the outer edge of the track.
				{
					const double X = G.bDriveFront ? G.FrontX : G.RearX, U = G.bDriveFront ? G.FrontUp : G.RearUp, Radius = G.bDriveFront ? G.FrontR : G.RearR;
					const FColor Bare = FLinearColor(0.07f, 0.065f, 0.06f).ToFColor(true);
					for (const double Ring : { Mid + Hand * (G.Width * 0.5 + 0.012), Mid + Hand * G.Width * 0.18 })
					{
						const FVector Out(0.0, -Hand, 0.0);
						const int32 Hub = M.Add(At(X, Ring, U), Out, Bare);
						TArray<int32> Rim;
						for (int32 K = 0; K < G.Teeth * 4; ++K)
						{
							const double Angle = UE_DOUBLE_TWO_PI * K / (G.Teeth * 4);
							const double Reach = Radius + ((K % 4) < 2 ? 0.075 : -0.01);
							Rim.Add(M.Add(At(X + FMath::Cos(Angle) * Reach, Ring, U + FMath::Sin(Angle) * Reach), Out, Bare));
						}
						for (int32 K = 0; K < Rim.Num(); ++K)
						{
							M.Face(Hub, Rim[K], Rim[(K + 1) % Rim.Num()], Out);
							M.Face(Hub, Rim[K], Rim[(K + 1) % Rim.Num()], -Out);
						}
					}
				}
			}
		}
	};

	/** A rounded plan: an oval about a middle, longer behind than before if asked. */
	static TArray<FVector2D> Oval(double X, double Y, double Front, double Back, double Half, int32 Sides = 16)
	{
		TArray<FVector2D> Plan;
		for (int32 K = 0; K < Sides; ++K)
		{
			const double C = FMath::Cos(K * UE_DOUBLE_TWO_PI / Sides), S = FMath::Sin(K * UE_DOUBLE_TWO_PI / Sides);
			Plan.Add(FVector2D(X + C * (C >= 0.0 ? Front : Back), Y + S * Half));
		}
		return Plan;
	}

	/** A plan with rounded corners: from Rear to Front, Half wide each side of the middle. */
	static TArray<FVector2D> Lozenge(double Rear, double Front, double Half, double Corner, int32 PerCorner = 4)
	{
		TArray<FVector2D> Plan;
		const FVector2D Middles[4] = { { Front - Corner, Half - Corner }, { Rear + Corner, Half - Corner }, { Rear + Corner, -Half + Corner }, { Front - Corner, -Half + Corner } };
		for (int32 Quarter = 0; Quarter < 4; ++Quarter)
		{
			for (int32 K = 0; K <= PerCorner; ++K)
			{
				const double Angle = (Quarter + static_cast<double>(K) / PerCorner) * UE_DOUBLE_HALF_PI;
				Plan.Add(Middles[Quarter] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Corner);
			}
		}
		return Plan;
	}

	// ------------------------------------------------------------------------ Panzerkampfwagen IV

	/**
	 * Panzerkampfwagen IV as it went to war (Ausf. D, 1939-41): 5.92 m long, 2.84 wide, 2.68 high.
	 * Eight small road wheels a side under four rollers, the driving wheel in front; a box of a hull with a
	 * stepped front; a turret with sloped sides, a stowage bin behind and a tall cupola; the short 75 mm gun.
	 */
	static void PanzerIV(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("51575C")));
		const FLinearColor Grey = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 1.2;
		G.Width = 0.38;
		G.Link = 0.12;
		G.FrontX = 2.5; G.FrontUp = 0.64; G.FrontR = 0.33;
		G.RearX = -2.58; G.RearUp = 0.55; G.RearR = 0.29;
		G.WheelR = 0.235;
		for (int32 K = 0; K < 8; ++K)
		{
			G.Wheels.Add(-1.72 + K * 0.5);
		}
		G.Rollers = { -1.35, -0.4, 0.55, 1.5 };
		G.RollerUp = 0.87;
		G.RollerR = 0.11;
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The four leaf-sprung bogies, each carrying a pair of wheels.
			for (int32 K = 0; K < 4; ++K)
			{
				B.Plate(-1.47 + K * 1.0, Hand * 1.36, 0.44, 0.62, 0.05, 0.09, Dark);
			}
			// The mudguards over the tracks, turned down at each end.
			B.Plate(0.0, Hand * 1.19, 1.085, 5.7, 0.48, 0.03, Dark);
			B.Plate(2.94, Hand * 1.19, 1.02, 0.24, 0.48, 0.03, Dark, 35.0);
			B.Plate(-2.94, Hand * 1.19, 1.03, 0.22, 0.48, 0.03, Dark, -30.0);
			B.Lamp(2.6, Hand * 1.14, 1.24, 0.085);
		}

		// The tub between the tracks: a stepped nose, the near-flat plate over the gearbox, the tail.
		B.Side({ { 2.5, 0.42 }, { 2.93, 0.78 }, { 2.85, 1.0 }, { 1.6, 1.17 }, { -2.75, 1.1 }, { -2.95, 0.95 }, { -2.95, 0.7 }, { -2.6, 0.42 } }, -0.93, 0.93, Grey);
		// The fighting compartment and engine room, standing out over the tracks.
		B.Side({ { 1.6, 1.08 }, { 1.56, 1.7 }, { -2.7, 1.7 }, { -2.8, 1.48 }, { -2.8, 1.08 } }, -1.14, 1.14, Grey);
		// The driver's plate, stepped forward on his side, with his visor; the wireless operator's machine gun.
		B.Plate(1.62, -0.55, 1.42, 0.12, 0.98, 0.5, Grey);
		B.Plate(1.69, -0.55, 1.5, 0.04, 0.44, 0.16, Dark);
		B.Plate(1.715, -0.55, 1.5, 0.02, 0.3, 0.035, Pitch);
		B.Plate(1.6, 0.55, 1.44, 0.05, 0.36, 0.36, Dark);
		B.BallGun(1.62, 0.55, 1.44, 0.34);
		// Two hatches over the steering brakes, on the plate in front.
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Plate(2.25, Hand * 0.45, 1.1, 0.5, 0.48, 0.03, Dark, 7.7);
			// A vision port in each side, and the louvres that let air to the engine.
			B.Plate(1.05, Hand * 1.15, 1.5, 0.24, 0.03, 0.1, Dark);
			B.Plate(-2.0, Hand * 1.16, 1.4, 1.25, 0.05, 0.34, Dark);
			for (int32 K = 0; K < 6; ++K)
			{
				B.Plate(-2.5 + K * 0.2, Hand * 1.19, 1.4, 0.05, 0.02, 0.26, Pitch);
			}
			// The hatches over the engine.
			B.Plate(-1.85, Hand * 0.5, 1.715, 1.1, 0.8, 0.03, Dark);
			// The straight-armed cross on each side.
			B.Mark(0.15, Hand * 1.146, 1.4, 0.2, TEXT("GER"), 0.0, Hand, 0.0);
		}
		// On the left mudguard the jack and two spare wheels; on the right the aerial in its wooden trough and the tools.
		B.Plate(1.0, -1.31, 1.17, 0.5, 0.16, 0.14, Dark);
		for (const double X : { -0.9, -1.45 })
		{
			B.Tube(X, -1.2, 1.345, X, -1.36, 1.345, 0.235, Tyre, 16);
			M.Disc(FBuild::At(X, -1.375, 1.345), FVector(0.0, 1.0, 0.0), 0.175, Grey, 16);
		}
		B.Plate(-0.3, 1.19, 1.36, 2.3, 0.07, 0.07, Timber);
		B.Aerial(0.95, 1.2, 1.4, 2.0);
		B.Tube(-1.7, 1.31, 1.125, -0.7, 1.31, 1.125, 0.025, Timber, 5);
		B.Tube(0.3, 1.31, 1.15, 0.7, 1.31, 1.15, 0.06, Dark, 6);

		// The turret: its front plate, sides that widen and then close again, all leaning in; hatches in its sides.
		const double T = 0.3;
		B.Layers({ { { T + 1.0, 0.62 }, { T + 1.0, -0.62 }, { T + 0.35, -0.97 }, { T - 0.45, -0.97 }, { T - 1.0, -0.6 }, { T - 1.0, 0.6 }, { T - 0.45, 0.97 }, { T + 0.35, 0.97 } },
			{ { T + 0.9, 0.52 }, { T + 0.9, -0.52 }, { T + 0.3, -0.73 }, { T - 0.4, -0.73 }, { T - 0.85, -0.45 }, { T - 0.85, 0.45 }, { T - 0.4, 0.73 }, { T + 0.3, 0.73 } } },
			{ 1.7, 2.3 }, Grey, false);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Layers({ { { T + 0.28, Hand * 0.96 }, { T + 0.28, Hand * 0.9 }, { T - 0.32, Hand * 0.9 }, { T - 0.32, Hand * 0.96 } },
				{ { T + 0.26, Hand * 0.81 }, { T + 0.26, Hand * 0.74 }, { T - 0.3, Hand * 0.74 }, { T - 0.3, Hand * 0.81 } } }, { 1.8, 2.2 }, Dark, false);
			B.Drum(T + 0.25, Hand * 0.36, 2.3, 0.07, 0.03, Dark, 8);
		}
		B.Drum(T + 0.5, 0.0, 2.3, 0.13, 0.04, Dark, 10);
		// The stowage bin on the back of the turret.
		B.Plate(T - 1.27, 0.0, 1.99, 0.5, 1.16, 0.42, Dark);
		B.Plate(T - 1.27, 0.0, 2.21, 0.53, 1.19, 0.025, Grey);
		// The commander's cupola: a drum with a ring of vision slits and a two-piece hatch.
		B.Drum(T - 0.55, 0.0, 2.3, 0.34, 0.14, Grey, 14);
		B.Drum(T - 0.55, 0.0, 2.44, 0.355, 0.07, Pitch, 14);
		B.Drum(T - 0.55, 0.0, 2.51, 0.34, 0.11, Grey, 14);
		B.Drum(T - 0.55, 0.0, 2.62, 0.27, 0.05, Dark, 14);
		B.Plate(T - 0.55, 0.0, 2.673, 0.56, 0.02, 0.01, Pitch);
		// The gun: its shield, the sleeve over the recoil gear, and the short barrel; a machine gun beside it.
		B.Plate(T + 1.0, 0.0, 2.0, 0.16, 0.82, 0.46, Dark);
		B.Tube(T + 1.05, 0.0, 2.0, T + 1.5, 0.0, 2.0, 0.115, Grey, 10);
		B.Tube(T + 1.5, 0.0, 2.0, T + 2.2, 0.0, 2.0, 0.07, Grey, 10);
		B.Tube(T + 2.17, 0.0, 2.0, T + 2.205, 0.0, 2.0, 0.045, Pitch, 8);
		B.Tube(T + 1.05, 0.28, 1.98, T + 1.32, 0.28, 1.98, 0.025, GunSteel, 5);
		// The silencers across the tail.
		B.Tube(-3.03, -0.8, 0.92, -3.03, 0.8, 0.92, 0.17, Rust, 10);
		B.Tube(-3.0, -0.95, 0.62, -3.0, -0.4, 0.62, 0.09, Rust, 8);
	}

	// ------------------------------------------------------------------------ T-34

	/**
	 * T-34 with the 76 mm gun, as built in 1941-42: hull 5.92 m long (6.68 over the gun), 3.0 wide, 2.45 high.
	 * Armour sloped on every side, five big road wheels with the track lying on top of them, the driving
	 * wheel at the back, wide tracks, a small turret set well forward.
	 */
	static void T34(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("4F5E3E")));
		const FLinearColor Green = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 1.22;
		G.Width = 0.5;
		G.Link = 0.17;
		G.FrontX = 2.52; G.FrontUp = 0.6; G.FrontR = 0.25;
		G.RearX = -2.5; G.RearUp = 0.62; G.RearR = 0.32;
		G.WheelR = 0.415;
		G.bDriveFront = false;
		G.Teeth = 6; // (the T-34's driving wheel has rollers that catch the track's teeth, not a ring of its own)
		G.Wheels = { -1.72, -0.86, 0.0, 0.86, 1.72 };
		B.Gear(G);

		// The belly between the tracks, with its pointed nose and sloped tail.
		B.Side({ { 2.9, 0.96 }, { 2.96, 0.85 }, { 2.45, 0.4 }, { -2.75, 0.42 }, { -2.96, 0.85 }, { -2.9, 0.96 } }, -0.95, 0.95, Green);
		// The upper hull: front, sides and back all leaning inward to a narrow roof.
		B.Layers({ { { 2.9, -1.48 }, { 2.9, 1.48 }, { -2.92, 1.48 }, { -2.92, -1.48 } }, { { 1.92, -1.0 }, { 1.92, 1.0 }, { -2.45, 1.0 }, { -2.45, -1.0 } } }, { 0.95, 1.52 }, Green, false);
		// On the front plate: the driver's hatch, the machine gun in its ball, two towing hooks.
		B.Plate(2.38, -0.38, 1.275, 0.66, 0.56, 0.05, Dark, 30.0);
		B.Plate(2.2, -0.38, 1.4, 0.1, 0.3, 0.06, Pitch, 30.0);
		B.BallGun(2.42, 0.44, 1.26, 0.3);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Plate(2.78, Hand * 0.78, 1.04, 0.16, 0.05, 0.1, Dark, 30.0);
			// The mudguard over the front of each track.
			B.Plate(2.93, Hand * 1.22, 0.985, 0.3, 0.54, 0.03, Dark, 40.0);
			// Box fuel tanks on the sides at the back, and the grilles that let air to the engine.
			B.Plate(-1.75, Hand * 1.36, 1.2, 0.9, 0.2, 0.3, Green);
			B.Plate(-1.2, Hand * 0.8, 1.535, 1.4, 0.32, 0.03, Dark);
			for (int32 K = 0; K < 7; ++K)
			{
				B.Plate(-1.8 + K * 0.2, Hand * 0.8, 1.555, 0.07, 0.28, 0.012, Pitch);
			}
			// The exhausts, through the tail plate.
			B.Tube(-2.72, Hand * 0.55, 1.14, -3.02, Hand * 0.55, 0.98, 0.07, Rust, 8);
		}
		B.Lamp(2.4, -1.22, 1.36, 0.08);
		// The engine hatch with its dome, the mesh at the back of the deck, and the round hatch in the tail plate.
		B.Plate(-1.0, 0.0, 1.535, 0.9, 0.9, 0.03, Dark);
		B.Drum(-1.0, 0.0, 1.55, 0.2, 0.05, Green, 10);
		B.Plate(-2.12, 0.0, 1.535, 0.56, 1.8, 0.03, Dark);
		B.Tube(-2.685, 0.0, 1.235, -2.716, 0.0, 1.26, 0.28, Dark, 12);

		// The turret: welded plate, narrow in front, its sides leaning in, the rear overhanging a little.
		const double T = 1.0;
		B.Layers({ { { T + 0.95, 0.42 }, { T + 0.95, -0.42 }, { T + 0.3, -0.86 }, { T - 0.5, -0.86 }, { T - 0.9, -0.5 }, { T - 0.9, 0.5 }, { T - 0.5, 0.86 }, { T + 0.3, 0.86 } },
			{ { T + 0.8, 0.34 }, { T + 0.8, -0.34 }, { T + 0.28, -0.62 }, { T - 0.42, -0.62 }, { T - 0.74, -0.38 }, { T - 0.74, 0.38 }, { T - 0.42, 0.62 }, { T + 0.28, 0.62 } } },
			{ 1.52, 2.3 }, Green, false);
		// The one big hatch, the periscope, the ventilator.
		B.Plate(T - 0.25, 0.0, 2.32, 0.72, 0.82, 0.04, Dark);
		B.Plate(T - 0.05, -0.26, 2.38, 0.12, 0.12, 0.08, Dark);
		B.Drum(T + 0.48, 0.0, 2.3, 0.13, 0.05, Dark, 10);
		// The gun in its rounded shield, with the armoured cover of its recoil gear.
		B.Tube(T + 0.9, -0.32, 1.93, T + 0.9, 0.32, 1.93, 0.21, Dark, 10);
		B.Plate(T + 1.2, 0.0, 1.93, 0.5, 0.32, 0.32, Green);
		B.Tube(T + 1.4, 0.0, 1.93, 3.68, 0.0, 1.94, 0.052, Green, 10);
		B.Tube(3.65, 0.0, 1.94, 3.685, 0.0, 1.94, 0.034, Pitch, 8);
		B.Tube(T + 0.95, 0.24, 1.9, T + 1.25, 0.24, 1.9, 0.022, GunSteel, 5);
		// A red star on each side of the turret.
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Mark(T - 0.1, Hand * 0.752, 1.91, 0.2, TEXT("SOV-ARMY"), 0.0, Hand * 0.956, 0.294);
		}
	}

	// ------------------------------------------------------------------------ Sherman

	/**
	 * Medium Tank M4A1 Sherman, the cast-hull Sherman with the 75 mm gun: 5.84 m long, 2.62 wide, 2.74 high.
	 * A tall hull cast in one rounded piece, the rounded gearbox cover across the nose, three two-wheeled
	 * bogies a side with a roller on each, the toothed driving wheel in front, and a rounded cast turret.
	 */
	static void Sherman(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("5B6046")));
		const FLinearColor Olive = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 1.05;
		G.Width = 0.42;
		G.Link = 0.152;
		G.FrontX = 2.42; G.FrontUp = 0.66; G.FrontR = 0.33;
		G.RearX = -2.4; G.RearUp = 0.55; G.RearR = 0.28;
		G.WheelR = 0.254;
		G.Wheels = { -1.8, -1.08, -0.36, 0.36, 1.08, 1.8 };
		G.Rollers = { -1.62, -0.18, 1.26 };
		G.RollerUp = 0.9;
		G.RollerR = 0.1;
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The three bogies: a casting that holds a pair of wheels on arms, with an arm trailing back to the roller.
			const double Out0 = Hand * (G.Right + G.Width * 0.4 + 0.02), Out1 = Hand * (G.Right + G.Width * 0.4 + 0.09);
			for (const double X : { -1.44, 0.0, 1.44 })
			{
				B.Side({ { X - 0.2, 0.34 }, { X + 0.2, 0.34 }, { X + 0.13, 0.8 }, { X - 0.13, 0.8 } }, Out0, Out1, Dark);
				B.Plate(X, (Out0 + Out1) * 0.5, 0.305, 0.76, 0.06, 0.09, Dark);
				B.Plate(X - 0.12, (Out0 + Out1) * 0.5, 0.84, 0.36, 0.06, 0.07, Dark, -14.0);
			}
		}

		// The lower hull, with the one-piece cast cover of the gearbox across the nose.
		B.Side({ { 2.62, 1.22 }, { 2.97, 0.72 }, { 2.86, 0.52 }, { 2.35, 0.43 }, { -2.7, 0.45 }, { -2.92, 0.8 }, { -2.92, 1.12 } }, -0.84, 0.84, Olive);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The housings of the final drives, and the towing lugs.
			B.Tube(2.42, Hand * 0.84, 0.66, 2.42, Hand * 0.9, 0.66, 0.3, Olive, 12);
			B.Plate(2.99, Hand * 0.62, 0.66, 0.1, 0.06, 0.16, Dark);
		}
		B.Mark(2.805, 0.0, 0.977, 0.25, TEXT("USA-ARMY"), 0.82, 0.0, 0.573);

		// The upper hull: one rounded casting, out over the tracks, curving up from the nose to the roof.
		B.Layers({ Lozenge(-2.92, 2.5, 1.31, 0.15), Lozenge(-2.93, 2.6, 1.31, 0.2), Lozenge(-2.93, 2.28, 1.31, 0.3), Lozenge(-2.91, 1.9, 1.28, 0.36),
			Lozenge(-2.86, 1.62, 1.17, 0.42), Lozenge(-2.66, 1.36, 0.97, 0.42) }, { 1.1, 1.25, 1.55, 1.77, 1.87, 1.9 }, Olive, true);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The hoods of the driver and his mate, swelling out of the front, each with its hatch and periscope.
			M.Ball(FBuild::At(1.78, Hand * 0.6, 1.79), FVector(0.78, 0.62, 0.4), Olive, 5, 10);
			B.Drum(1.66, Hand * 0.6, 1.965, 0.23, 0.025, Dark, 12);
			B.Plate(1.76, Hand * 0.6, 2.01, 0.08, 0.15, 0.05, Dark);
			B.Lamp(2.4, Hand * 0.95, 1.56, 0.07);
			// The white star on each side, and the extra plate welded on over the ammunition racks.
			B.Mark(-0.45, Hand * 1.316, 1.42, 0.25, TEXT("USA-ARMY"), 0.0, Hand, 0.0);
			B.Plate(0.75, Hand * 1.32, 1.45, 0.7, 0.03, 0.46, Olive);
			// Air cleaners on the tail plate, and the fuel caps.
			B.Drum(-3.0, Hand * 0.8, 1.2, 0.13, 0.36, Dark, 8);
			B.Drum(-1.2, Hand * 0.8, 1.9, 0.07, 0.03, Dark, 8);
		}
		B.BallGun(2.3, 0.6, 1.5, 0.34);
		// The engine doors on the deck, and the exhaust deflector under the tail.
		B.Plate(-1.95, 0.0, 1.905, 1.0, 1.3, 0.02, Dark);
		B.Plate(-1.95, 0.0, 1.92, 0.02, 1.3, 0.012, Pitch);
		B.Plate(-2.96, 0.0, 0.98, 0.1, 1.3, 0.14, Rust);

		// The turret: one rounded casting, fuller behind where the wireless sits.
		const double T = 0.3;
		B.Layers({ Oval(T, 0.0, 0.93, 1.2, 0.93, 20), Oval(T, 0.0, 0.97, 1.27, 0.98, 20), Oval(T, 0.0, 0.94, 1.25, 0.95, 20), Oval(T, 0.0, 0.84, 1.14, 0.86, 20), Oval(T, 0.0, 0.66, 0.94, 0.68, 20) },
			{ 1.88, 1.99, 2.36, 2.53, 2.6 }, Olive, true);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Mark(T - 0.2, Hand * 0.975, 2.18, 0.2, TEXT("USA-ARMY"), 0.0, Hand, 0.06);
		}
		// The pistol port, a small hatch in the left of the turret.
		M.Ball(FBuild::At(T - 0.62, -0.93, 2.2), FVector(0.3, 0.12, 0.26), Olive, 4, 8);
		// The gun in its narrow rounded shield, and the machine gun beside it.
		B.Tube(T + 0.7, -0.3, 2.2, T + 0.7, 0.3, 2.2, 0.3, Olive, 14);
		B.Tube(T + 0.85, -0.16, 2.2, T + 0.85, 0.16, 2.2, 0.23, Dark, 12);
		B.Tube(T + 1.0, 0.0, 2.2, T + 1.5, 0.0, 2.215, 0.09, Olive, 10);
		B.Tube(T + 1.5, 0.0, 2.215, 2.9, 0.0, 2.26, 0.058, Olive, 10);
		B.Tube(2.87, 0.0, 2.259, 2.905, 0.0, 2.26, 0.038, Pitch, 8);
		B.Tube(T + 0.95, 0.42, 2.2, T + 1.2, 0.42, 2.2, 0.022, GunSteel, 5);
		// The commander's hatch ring with its heavy machine gun; the loader's periscope; the ventilator; the aerial.
		B.Drum(T - 0.25, 0.38, 2.6, 0.33, 0.1, Dark, 14);
		B.Drum(T - 0.25, 0.38, 2.7, 0.27, 0.025, Olive, 14);
		B.Plate(T - 0.25, 0.38, 2.73, 0.54, 0.02, 0.01, Pitch);
		B.Tube(T + 0.08, 0.38, 2.7, T + 0.08, 0.38, 2.9, 0.025, GunSteel, 5);
		B.Plate(T - 0.02, 0.38, 2.92, 0.5, 0.08, 0.1, GunSteel);
		B.Tube(T + 0.2, 0.38, 2.93, T + 1.25, 0.38, 2.95, 0.02, GunSteel, 5);
		B.Plate(T + 0.2, -0.42, 2.63, 0.12, 0.2, 0.06, Dark);
		B.Drum(T + 0.5, 0.0, 2.6, 0.14, 0.04, Dark, 10);
		B.Aerial(T - 0.95, -0.5, 2.56, 1.7);
	}

	// ------------------------------------------------------------------------ Matilda

	/**
	 * Infantry Tank Mark II, Matilda: 5.61 m long, 2.59 wide, 2.52 high. Low, heavy and slow; its
	 * suspension behind armoured skirts with a row of mud chutes, a small round cast turret, the slim
	 * two-pounder gun.
	 */
	static void Matilda(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("666B47")));
		const FLinearColor Khaki = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 1.08;
		G.Width = 0.36;
		G.Link = 0.13;
		G.FrontX = 2.42; G.FrontUp = 0.6; G.FrontR = 0.3;
		G.RearX = -2.42; G.RearUp = 0.58; G.RearR = 0.3;
		G.WheelR = 0.13;
		G.bDriveFront = false;
		G.Wheels = { -1.9, -1.14, -0.38, 0.38, 1.14, 1.9 };
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The skirt over the suspension, the mud chutes cut in it, and the plate over the top of the track.
			B.Side({ { 2.35, 0.32 }, { 2.72, 0.62 }, { 2.72, 0.95 }, { 2.3, 1.22 }, { -2.35, 1.22 }, { -2.75, 0.95 }, { -2.75, 0.6 }, { -2.4, 0.32 } }, Hand * 1.25, Hand * 1.295, Khaki);
			for (int32 K = 0; K < 5; ++K)
			{
				B.Side({ { -1.72 + K * 0.76, 0.48 }, { -1.32 + K * 0.76, 0.48 }, { -1.38 + K * 0.76, 0.8 }, { -1.66 + K * 0.76, 0.8 } }, Hand * 1.29, Hand * 1.31, Pitch);
			}
			B.Plate(0.0, Hand * 1.08, 1.215, 4.7, 0.44, 0.03, Khaki);
			// The red, white and red flash.
			B.Mark(1.95, Hand * 1.297, 0.98, 0.2, TEXT("UK-FLASH"), 0.0, Hand, 0.0);
			// The lockers that flank the driver, sloping down the nose.
			B.Side({ { 1.72, 1.12 }, { 2.78, 0.8 }, { 2.78, 0.98 }, { 1.6, 1.32 } }, Hand * 0.42, Hand * 0.88, Khaki);
			B.Lamp(2.6, Hand * 0.65, 1.12, 0.08);
			// The exhaust pipes down the deck.
			B.Tube(-0.95, Hand * 0.74, 1.5, -2.55, Hand * 0.74, 1.28, 0.055, Rust, 8);
		}
		// The hull: a nose that drops away, a flat roof, the engine deck sloping down behind.
		B.Side({ { 2.8, 0.8 }, { 2.55, 0.46 }, { -2.55, 0.46 }, { -2.8, 0.8 }, { -2.75, 1.18 }, { -0.85, 1.45 }, { 1.3, 1.45 }, { 1.75, 1.12 } }, -0.88, 0.88, Khaki);
		// The driver's hood in the middle of the nose, with his visor.
		B.Side({ { 1.25, 1.45 }, { 1.8, 1.45 }, { 2.1, 1.18 }, { 2.1, 1.0 }, { 1.5, 1.0 } }, -0.4, 0.4, Khaki);
		B.Plate(1.965, 0.0, 1.335, 0.12, 0.3, 0.03, Pitch, 42.0);
		// The louvres over the engine, and the silencer across the tail.
		for (int32 K = 0; K < 5; ++K)
		{
			const double X = -1.25 - K * 0.3;
			B.Plate(X, 0.0, 1.45 - (-0.85 - X) * 0.142 + 0.02, 0.2, 1.3, 0.04, Dark, -8.1);
		}
		B.Tube(-2.84, -0.62, 1.0, -2.84, 0.62, 1.0, 0.13, Rust, 10);

		// The turret: a short cast drum, a little wider at the waist.
		const double T = 0.35;
		B.Layers({ Oval(T, 0.0, 0.8, 0.8, 0.8), Oval(T, 0.0, 0.82, 0.82, 0.82), Oval(T, 0.0, 0.77, 0.77, 0.77), Oval(T, 0.0, 0.7, 0.7, 0.7), Oval(T, 0.0, 0.55, 0.55, 0.55) },
			{ 1.45, 1.62, 2.08, 2.2, 2.24 }, Khaki, true);
		// The commander's cupola, the loader's hatch, the aerial.
		B.Drum(T - 0.22, -0.28, 2.22, 0.3, 0.24, Khaki, 14);
		B.Drum(T - 0.22, -0.28, 2.46, 0.26, 0.03, Dark, 14);
		B.Plate(T + 0.05, 0.32, 2.25, 0.46, 0.4, 0.03, Dark);
		B.Aerial(T - 0.6, 0.3, 2.2, 1.5);
		// The two-pounder in its shield, with the water-cooled machine gun beside it.
		B.Plate(T + 0.76, 0.0, 1.85, 0.16, 0.62, 0.5, Dark);
		B.Tube(T + 0.8, 0.0, 1.86, T + 1.2, 0.0, 1.86, 0.07, Khaki, 8);
		B.Tube(T + 1.2, 0.0, 1.86, 2.62, 0.0, 1.87, 0.036, Khaki, 8);
		B.Tube(T + 0.8, 0.2, 1.84, T + 1.25, 0.2, 1.84, 0.047, Dark, 8);
		// The smoke dischargers on the right of the turret.
		B.Tube(T + 0.25, 0.84, 1.9, T + 0.6, 0.88, 1.98, 0.05, Dark, 8);
		B.Tube(T + 0.25, 0.84, 2.03, T + 0.6, 0.88, 2.11, 0.05, Dark, 8);
	}

	struct FModel
	{
		const TCHAR* Name;
		void (*Build)(FMiniatureMesh&);
		const TCHAR* Title;
		const TCHAR* Nation;
	};
	static const FModel Models[] = {
		{ TEXT("PanzerIV"), &PanzerIV, TEXT("Panzer IV  -  5.92 m long, 2.68 m high"), TEXT("GER") },
		{ TEXT("T34"), &T34, TEXT("T-34  -  5.92 m long, 2.45 m high"), TEXT("SOV") },
		{ TEXT("Sherman"), &Sherman, TEXT("M4 Sherman  -  5.84 m long, 2.74 m high"), TEXT("USA") },
		{ TEXT("Matilda"), &Matilda, TEXT("Matilda II  -  5.61 m long, 2.52 m high"), TEXT("UK") },
	};
}

bool FMiniatureMesh::Vehicle(const FString& Name, const FVector& Where, double Metre, double Turn)
{
	static TMap<FString, FMiniatureMesh> Built;
	FMiniatureMesh* Model = Built.Find(Name);
	if (!Model)
	{
		for (const VehicleParts::FModel& Known : VehicleParts::Models)
		{
			if (Name == Known.Name)
			{
				FMiniatureMesh Fresh;
				// The dust of the road on the tracks and the lower hull.
				Fresh.DustFoot = 0.0;
				Fresh.DustTop = 1.25;
				Fresh.DustAmount = 0.5f;
				Known.Build(Fresh);
				UE_LOG(LogTheater, Log, TEXT("Model %s built: %d points, %d triangles"), *Name, Fresh.Vertices.Num(), Fresh.Triangles.Num() / 3);
				Model = &Built.Add(Name, MoveTemp(Fresh));
				break;
			}
		}
	}
	if (!Model)
	{
		return false;
	}
	Stamp(*Model, Where, Metre, Turn);
	return true;
}

TArray<FString> FMiniatureMesh::VehicleNames()
{
	TArray<FString> Names;
	for (const VehicleParts::FModel& Known : VehicleParts::Models)
	{
		Names.Add(Known.Name);
	}
	return Names;
}

FString FMiniatureMesh::VehicleTitle(const FString& Name)
{
	for (const VehicleParts::FModel& Known : VehicleParts::Models)
	{
		if (Name == Known.Name)
		{
			return Known.Title;
		}
	}
	return Name;
}

FString FMiniatureMesh::VehicleNation(const FString& Name)
{
	for (const VehicleParts::FModel& Known : VehicleParts::Models)
	{
		if (Name == Known.Name)
		{
			return Known.Nation;
		}
	}
	return FString();
}

FString FMiniatureMesh::MainTankOf(const FString& Tag)
{
	if (Tag == TEXT("GER")) return TEXT("PanzerIV");
	if (Tag == TEXT("SOV")) return TEXT("T34");
	if (Tag == TEXT("USA")) return TEXT("Sherman");
	if (Tag == TEXT("UK") || Tag == TEXT("CAN") || Tag == TEXT("AUS") || Tag == TEXT("NZL") || Tag == TEXT("SAF") || Tag == TEXT("IND")) return TEXT("Matilda");
	return FString();
}

bool FMiniatureMesh::MainTank(const FVector& Where, double Metre, const FString& Tag, double Turn)
{
	const FString Name = MainTankOf(Tag);
	return !Name.IsEmpty() && Vehicle(Name, Where, Metre, Turn);
}
