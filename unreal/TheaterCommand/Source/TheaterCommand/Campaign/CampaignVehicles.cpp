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
		/** Wheels that overlap one another, every other one set further in (the Tiger's). */
		bool bInterleaved = false;
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
				auto Wheel = [&](double X, double U, double Radius, bool bTyre, int32 Sides, double Inset = 0.0)
				{
					const double Front = Face - Hand * Inset;
					M.Rod(At(X, Back, U), At(X, Front, U), Radius, bTyre ? Tyre : Shade, Sides);
					M.Disc(At(X, Front + Hand * 0.02, U), FVector(0.0, -Hand, 0.0), Radius * (bTyre ? 0.74 : 0.82), Paint, Sides);
					M.Rod(At(X, Front, U), At(X, Front + Hand * 0.06, U), Radius * 0.26, Shade, 6);
				};
				for (int32 K = 0; K < G.Wheels.Num(); ++K)
				{
					Wheel(G.Wheels[K], G.WheelR + Thick, G.WheelR, true, 16, G.bInterleaved && K % 2 ? G.Width * 0.32 : 0.0);
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

	// ------------------------------------------------------------------------ Tiger

	/**
	 * Panzerkampfwagen VI Tiger: hull 6.32 m long (8.45 over the gun), 3.56 wide, 3.0 high; 57 tonnes.
	 * A great square box of a hull on very wide tracks, eight overlapping road wheels a side, a horseshoe
	 * turret with a broad flat gun shield, and the long 88 mm gun with its muzzle brake.
	 */
	static void Tiger(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("9C8A58")));
		const FLinearColor Sand = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 1.42;
		G.Width = 0.72;
		G.Link = 0.13;
		G.FrontX = 2.72; G.FrontUp = 0.72; G.FrontR = 0.42;
		G.RearX = -2.8; G.RearUp = 0.6; G.RearR = 0.3;
		G.Teeth = 20;
		G.WheelR = 0.4;
		G.bInterleaved = true;
		for (int32 K = 0; K < 8; ++K)
		{
			G.Wheels.Add(-2.02 + K * 0.555);
		}
		B.Gear(G);

		// The tub between the tracks: the lower nose, the near-flat plate over the gearbox, the tail.
		B.Side({ { 2.6, 0.48 }, { 3.14, 0.92 }, { 3.1, 1.16 }, { 2.45, 1.3 }, { -3.08, 1.25 }, { -3.16, 0.9 }, { -2.9, 0.5 } }, -1.02, 1.02, Sand);
		// The square upper hull, out over the tracks, with its upright front plate.
		B.Side({ { 2.45, 1.22 }, { 2.4, 1.78 }, { -3.0, 1.78 }, { -3.08, 1.22 } }, -1.57, 1.57, Sand);
		// The driver's visor, the machine gun in its ball, and the lamps on the corners of the front plate.
		B.Plate(2.44, -0.55, 1.56, 0.06, 0.5, 0.2, Dark);
		B.Plate(2.475, -0.55, 1.56, 0.02, 0.36, 0.045, Pitch);
		B.Plate(2.43, 0.55, 1.5, 0.05, 0.4, 0.4, Dark);
		B.BallGun(2.45, 0.55, 1.5, 0.36);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Lamp(2.42, Hand * 1.2, 1.9, 0.085);
			// Mudguards: turned down over the driving wheels in front, and a strip along each side.
			B.Plate(2.95, Hand * 1.42, 1.17, 0.9, 0.76, 0.03, Dark, 16.0);
			B.Plate(-0.3, Hand * 1.68, 1.215, 5.5, 0.22, 0.03, Dark);
			// The hatches of the driver and the wireless operator.
			B.Drum(1.75, Hand * 0.62, 1.78, 0.24, 0.035, Dark, 12);
			// The cross on each side.
			B.Mark(0.5, Hand * 1.576, 1.5, 0.21, TEXT("GER"), 0.0, Hand, 0.0);
			// The grilles over the radiators and fans, at the corners of the engine deck.
			B.Plate(-2.0, Hand * 1.05, 1.795, 0.75, 0.75, 0.03, Dark);
			B.Plate(-2.75, Hand * 1.05, 1.795, 0.5, 0.75, 0.03, Dark);
			for (int32 K = 0; K < 5; ++K)
			{
				B.Plate(-2.3 + K * 0.15, Hand * 1.05, 1.815, 0.05, 0.66, 0.012, Pitch);
			}
			// The two tall exhausts on the tail, in their shields.
			B.Drum(-3.24, Hand * 0.5, 1.2, 0.15, 0.62, Rust, 10);
			B.Plate(-3.36, Hand * 0.5, 1.5, 0.03, 0.42, 0.66, Dark);
			// Steel towing cables along the hull sides.
			B.Tube(1.9, Hand * 1.59, 1.72, -2.2, Hand * 1.59, 1.72, 0.022, GunSteel, 5);
		}
		B.Plate(-2.3, 0.0, 1.795, 1.3, 1.0, 0.03, Dark);

		// The turret: flat in front, straight at the sides, rounded behind like a horseshoe.
		const double T = 0.25;
		const TArray<FVector2D> Shoe = { { T + 1.05, 0.85 }, { T + 1.05, -0.85 }, { T + 0.5, -1.0 }, { T - 0.1, -1.07 }, { T - 0.6, -1.0 }, { T - 1.0, -0.75 }, { T - 1.25, -0.4 },
			{ T - 1.32, 0.0 }, { T - 1.25, 0.4 }, { T - 1.0, 0.75 }, { T - 0.6, 1.0 }, { T - 0.1, 1.07 }, { T + 0.5, 1.0 } };
		TArray<FVector2D> Crown = Shoe;
		for (FVector2D& Point : Crown)
		{
			Point = FVector2D(T + (Point.X - T) * 0.97, Point.Y * 0.97);
		}
		B.Layers({ Shoe, Shoe, Crown }, { 1.78, 2.5, 2.58 }, Sand, true);
		// The stowage bin on the back, the commander's cupola on the left, the loader's hatch on the right.
		B.Plate(T - 1.52, 0.0, 2.2, 0.42, 1.2, 0.48, Dark);
		B.Drum(T - 0.55, -0.48, 2.58, 0.37, 0.16, Sand, 14);
		B.Drum(T - 0.55, -0.48, 2.74, 0.385, 0.07, Pitch, 14);
		B.Drum(T - 0.55, -0.48, 2.81, 0.37, 0.1, Sand, 14);
		B.Drum(T - 0.55, -0.48, 2.91, 0.29, 0.05, Dark, 14);
		B.Plate(T - 0.3, 0.5, 2.6, 0.55, 0.5, 0.035, Dark);
		B.Drum(T + 0.35, 0.0, 2.58, 0.16, 0.05, Dark, 10);
		for (const double Hand : { -1.0, 1.0 })
		{
			// Three smoke dischargers on each side of the turret.
			for (int32 K = 0; K < 3; ++K)
			{
				B.Tube(T + 0.5, Hand * (1.1 + K * 0.02), 2.28 + K * 0.1, T + 0.78, Hand * (1.14 + K * 0.03), 2.36 + K * 0.12, 0.045, Dark, 6);
			}
		}
		// The broad gun shield, and the 88: a thick sleeve, the long barrel, the muzzle brake.
		B.Plate(T + 1.1, 0.0, 2.17, 0.16, 1.86, 0.72, Dark);
		B.Tube(T + 1.15, 0.0, 2.17, T + 1.75, 0.0, 2.17, 0.2, Sand, 12);
		B.Tube(T + 1.75, 0.0, 2.17, T + 2.5, 0.0, 2.17, 0.13, Sand, 10);
		B.Tube(T + 2.5, 0.0, 2.17, 5.0, 0.0, 2.18, 0.078, Sand, 10);
		B.Tube(4.95, 0.0, 2.18, 5.14, 0.0, 2.18, 0.13, Dark, 10);
		B.Tube(5.14, 0.0, 2.18, 5.2, 0.0, 2.18, 0.085, Sand, 8);
		B.Tube(5.2, 0.0, 2.18, 5.29, 0.0, 2.18, 0.115, Dark, 10);
		B.Tube(5.27, 0.0, 2.18, 5.295, 0.0, 2.18, 0.06, Pitch, 8);
		B.Tube(T + 1.12, 0.6, 2.17, T + 1.4, 0.6, 2.17, 0.025, GunSteel, 5);
		B.Aerial(-2.2, 1.45, 1.78, 2.0);
	}

	// ------------------------------------------------------------------------ Char B1 bis

	/**
	 * Char B1 bis, the French heavy tank of 1940: 6.37 m long, 2.5 wide, 2.79 high. Tall and slab-sided, its
	 * tracks running right round the hull behind armoured skirts; a 75 mm howitzer low in the hull front and a
	 * small cast turret with a 47 mm gun on top.
	 */
	static void CharB1(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("5E6A48")));
		const FLinearColor Green = B.Paint, Dark = B.Shade;
		const FLinearColor Ochre = Hex(TEXT("9A8656")), Brown = Hex(TEXT("5A4632"));

		FGear G;
		G.Right = 0.98;
		G.Width = 0.5;
		G.Link = 0.2;
		G.FrontX = 2.72; G.FrontUp = 1.02; G.FrontR = 0.42;
		G.RearX = -2.75; G.RearUp = 0.9; G.RearR = 0.4;
		G.bDriveFront = false;
		G.WheelR = 0.13;
		G.Wheels = { -2.0, -1.2, -0.4, 0.4, 1.2, 1.95 };
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The skirt over the suspension, and the plate over the top of the track.
			B.Side({ { 2.25, 0.3 }, { 2.5, 0.62 }, { 2.4, 1.36 }, { -2.4, 1.3 }, { -2.5, 0.6 }, { -2.2, 0.3 } }, Hand * 1.2, Hand * 1.245, Green);
			B.Plate(-0.1, Hand * 0.98, 1.53, 5.2, 0.52, 0.035, Green);
			// Patches of the camouflage of 1940, on the skirts.
			B.Side({ { 0.3, 0.5 }, { 1.5, 0.45 }, { 1.8, 1.0 }, { 0.9, 1.3 }, { 0.1, 1.05 } }, Hand * 1.243, Hand * 1.252, Ochre);
			B.Side({ { -2.0, 0.6 }, { -0.9, 0.5 }, { -0.6, 1.1 }, { -1.5, 1.28 }, { -2.2, 1.0 } }, Hand * 1.243, Hand * 1.252, Brown);
		}
		// The door in the right side, and the louvres of the radiator in the left.
		B.Plate(0.3, 1.26, 0.95, 0.7, 0.03, 0.7, Dark);
		B.Plate(-0.9, -1.26, 0.95, 1.3, 0.03, 0.6, Dark);
		for (int32 K = 0; K < 7; ++K)
		{
			B.Plate(-0.9, -1.285, 0.7 + K * 0.085, 1.2, 0.02, 0.03, Pitch);
		}
		// The hull between the tracks: a blunt nose, a flat roof, the engine deck sloping down behind.
		B.Side({ { 2.55, 0.5 }, { 3.12, 1.0 }, { 3.0, 1.5 }, { 2.4, 1.92 }, { -1.6, 1.92 }, { -3.1, 1.5 }, { -3.15, 0.9 }, { -2.7, 0.5 } }, -0.74, 0.74, Green);
		// The driver's armoured cab on the left, with his visor; the 75 mm howitzer low on the right.
		B.Plate(2.6, -0.34, 1.7, 0.7, 0.72, 0.5, Green);
		B.Plate(2.96, -0.34, 1.76, 0.02, 0.4, 0.07, Pitch);
		B.Plate(3.0, 0.32, 1.18, 0.2, 0.5, 0.46, Dark);
		B.Tube(3.05, 0.32, 1.18, 3.5, 0.32, 1.18, 0.09, Dark, 10);
		B.Tube(3.47, 0.32, 1.18, 3.505, 0.32, 1.18, 0.055, Pitch, 8);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Lamp(3.06, Hand * 0.6, 1.42, 0.07);
		}
		// Engine hatches and the exhausts along the deck.
		B.Plate(-1.3, 0.0, 1.935, 1.0, 1.1, 0.03, Dark);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Tube(-1.7, Hand * 0.5, 1.95, -2.9, Hand * 0.5, 1.64, 0.07, Rust, 8);
		}
		// The turret: a small one-man casting, with the commander's dome on top and the 47 mm gun.
		const double T = 1.0;
		B.Layers({ Oval(T, -0.05, 0.62, 0.66, 0.62), Oval(T, -0.05, 0.63, 0.68, 0.63), Oval(T, -0.05, 0.56, 0.62, 0.56), Oval(T, -0.05, 0.44, 0.5, 0.44) }, { 1.92, 2.1, 2.42, 2.5 }, Green, true);
		M.Ball(FBuild::At(T - 0.12, -0.05, 2.56), FVector(0.52, 0.52, 0.44), Green, 5, 12);
		B.Plate(T + 0.58, -0.05, 2.2, 0.12, 0.5, 0.36, Dark);
		B.Tube(T + 0.6, -0.05, 2.2, T + 1.75, -0.05, 2.21, 0.045, Dark, 8);
		B.Tube(T + 0.6, 0.18, 2.18, T + 0.95, 0.18, 2.18, 0.025, GunSteel, 5);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Mark(T - 0.25, -0.05 + Hand * 0.635, 2.22, 0.14, TEXT("FRA"), -0.25, Hand, 0.1);
		}
		B.Aerial(-0.6, 0.6, 1.92, 1.4);
	}

	// ------------------------------------------------------------------------ M13/40

	/**
	 * Carro Armato M13/40, the Italian medium tank of the desert war: 4.92 m long, 2.2 wide, 2.37 high.
	 * Small, riveted and boxy: eight little road wheels a side on leaf springs, a tall fighting compartment,
	 * a low engine deck behind it, and a turret with a 47 mm gun.
	 */
	static void M13(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("A39263")));
		const FLinearColor Sand = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 0.95;
		G.Width = 0.26;
		G.Link = 0.11;
		G.FrontX = 2.05; G.FrontUp = 0.56; G.FrontR = 0.3;
		G.RearX = -2.1; G.RearUp = 0.5; G.RearR = 0.27;
		G.WheelR = 0.16;
		G.Wheels = { -1.5, -1.12, -0.7, -0.32, 0.22, 0.6, 1.02, 1.4 };
		G.Rollers = { -1.1, 0.0, 1.1 };
		G.RollerUp = 0.78;
		G.RollerR = 0.085;
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The two bogies with their leaf springs, and the mudguards.
			for (const double X : { -0.9, 0.82 })
			{
				B.Plate(X, Hand * 1.07, 0.42, 1.3, 0.05, 0.08, Dark);
				B.Plate(X, Hand * 1.07, 0.52, 0.5, 0.05, 0.14, Dark);
			}
			B.Plate(0.0, Hand * 0.97, 0.95, 4.5, 0.34, 0.025, Dark);
			B.Lamp(1.9, Hand * 0.7, 1.12, 0.07);
		}
		// The tub, with its stepped nose.
		B.Side({ { 2.1, 0.42 }, { 2.45, 0.7 }, { 2.4, 0.92 }, { 1.5, 1.1 }, { -2.3, 1.05 }, { -2.46, 0.8 }, { -2.2, 0.42 } }, -0.8, 0.8, Sand);
		// The fighting compartment: a tall riveted box; the engine deck lower behind it.
		B.Layers({ { { 1.52, -1.05 }, { 1.52, 1.05 }, { -0.5, 1.05 }, { -0.5, -1.05 } }, { { 1.3, -0.95 }, { 1.3, 0.95 }, { -0.45, 0.95 }, { -0.45, -0.95 } } }, { 1.05, 1.62 }, Sand, false);
		B.Side({ { -0.5, 1.03 }, { -0.5, 1.34 }, { -2.2, 1.26 }, { -2.36, 1.03 } }, -1.0, 1.0, Sand);
		B.Plate(-1.3, 0.0, 1.32, 1.3, 1.5, 0.03, Dark, 2.7);
		// The twin machine guns in the hull front on the right; the driver's visor on the left.
		B.Plate(1.45, 0.45, 1.36, 0.08, 0.46, 0.36, Dark);
		B.Tube(1.45, 0.38, 1.36, 1.82, 0.38, 1.36, 0.022, GunSteel, 5);
		B.Tube(1.45, 0.52, 1.36, 1.82, 0.52, 1.36, 0.022, GunSteel, 5);
		B.Plate(1.45, -0.45, 1.42, 0.05, 0.4, 0.14, Dark);
		B.Plate(1.48, -0.45, 1.42, 0.02, 0.28, 0.035, Pitch);
		// Rows of rivets along the edges of the box.
		for (int32 K = 0; K < 9; ++K)
		{
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Ball(FBuild::At(1.3 - K * 0.21, Hand * 1.035, 1.2), FVector(0.035, 0.035, 0.035), Dark, 2, 5);
			}
		}
		// The silencers on the mudguards at the back.
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Tube(-1.3, Hand * 1.0, 1.06, -2.1, Hand * 1.0, 1.06, 0.09, Rust, 8);
		}
		// The turret, and the 47 mm gun.
		const double T = 0.45;
		B.Layers({ Oval(T, 0.0, 0.62, 0.72, 0.64, 10), Oval(T, 0.0, 0.52, 0.62, 0.54, 10) }, { 1.62, 2.24 }, Sand, false);
		B.Plate(T - 0.1, 0.0, 2.26, 0.62, 0.7, 0.035, Dark);
		B.Plate(T + 0.56, 0.0, 1.95, 0.14, 0.5, 0.4, Dark);
		B.Tube(T + 0.6, 0.0, 1.95, T + 1.0, 0.0, 1.95, 0.07, Sand, 8);
		B.Tube(T + 1.0, 0.0, 1.95, T + 1.7, 0.0, 1.96, 0.04, Sand, 8);
		B.Tube(T + 0.6, 0.2, 1.93, T + 0.9, 0.2, 1.93, 0.022, GunSteel, 5);
		// The white disc on the engine deck, for friendly aircraft.
		B.Mark(-1.3, 0.0, 1.345, 0.36, TEXT("WHITE-DISC"), 0.047, 0.0, 1.0);
		B.Aerial(-0.3, -0.9, 1.62, 1.6);
	}

	// ------------------------------------------------------------------------ Type 97

	/**
	 * Type 97 medium tank Chi-Ha, Japan's main tank: 5.5 m long, 2.33 wide, 2.23 high. Low, with six road
	 * wheels a side, sloped hull sides, a turret set to the right with a short 57 mm gun and a wireless rail
	 * round its top like a handrail.
	 */
	static void Type97(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("7A6A42")));
		const FLinearColor Khaki = B.Paint, Dark = B.Shade;
		const FLinearColor Stripe = Hex(TEXT("C2A53A")), Leaf = Hex(TEXT("4E5A38"));

		FGear G;
		G.Right = 0.98;
		G.Width = 0.33;
		G.Link = 0.12;
		G.FrontX = 2.3; G.FrontUp = 0.56; G.FrontR = 0.3;
		G.RearX = -2.36; G.RearUp = 0.5; G.RearR = 0.28;
		G.WheelR = 0.265;
		G.Wheels = { -1.72, -1.1, -0.48, 0.2, 0.82, 1.5 };
		G.Rollers = { -1.2, 0.0, 1.2 };
		G.RollerUp = 0.82;
		G.RollerR = 0.09;
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Plate(0.0, Hand * 0.99, 1.0, 5.0, 0.38, 0.025, Dark);
			B.Plate(-0.45, Hand * 1.15, 0.5, 1.5, 0.05, 0.08, Dark);
			B.Lamp(2.45, Hand * 0.55, 1.12, 0.07);
		}
		// The tub and its nose.
		B.Side({ { 2.35, 0.4 }, { 2.72, 0.7 }, { 2.65, 0.95 }, { 1.7, 1.15 }, { -2.6, 1.1 }, { -2.75, 0.8 }, { -2.45, 0.4 } }, -0.8, 0.8, Khaki);
		// The upper hull, its sides leaning in.
		B.Layers({ { { 1.78, -1.15 }, { 1.78, 1.15 }, { -2.5, 1.15 }, { -2.5, -1.15 } }, { { 1.42, -0.9 }, { 1.42, 0.9 }, { -2.3, 0.9 }, { -2.3, -0.9 } } }, { 1.05, 1.5 }, Khaki, false);
		// Camouflage: a yellow stripe over the hull and patches of green.
		B.Plate(-0.6, 0.0, 1.505, 0.16, 1.8, 0.012, Stripe);
		B.Plate(-1.6, 0.4, 1.505, 0.9, 0.8, 0.01, Leaf);
		B.Plate(0.9, -0.5, 1.505, 0.6, 0.7, 0.01, Leaf);
		// The driver's visor and the hull machine gun.
		B.Plate(1.62, 0.5, 1.3, 0.05, 0.36, 0.12, Dark);
		B.BallGun(1.62, -0.5, 1.28, 0.3);
		// The engine louvres along the sides at the back, and the silencers.
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Plate(-1.5, Hand * 1.04, 1.3, 1.4, 0.04, 0.2, Dark);
			B.Tube(-1.6, Hand * 1.02, 1.1, -2.4, Hand * 1.02, 1.1, 0.085, Rust, 8);
		}
		// The turret, set to the right, with its cupola and the short gun; a machine gun in its back.
		const double T = 0.35, Off = 0.16;
		B.Layers({ Oval(T, Off, 0.74, 0.8, 0.74, 10), Oval(T, Off, 0.6, 0.68, 0.6, 10) }, { 1.5, 1.98 }, Khaki, false);
		B.Drum(T - 0.2, Off, 1.98, 0.3, 0.2, Khaki, 12);
		B.Drum(T - 0.2, Off, 2.18, 0.26, 0.04, Dark, 12);
		B.Plate(T + 0.66, Off, 1.76, 0.12, 0.44, 0.34, Dark);
		B.Tube(T + 0.68, Off, 1.76, T + 1.0, Off, 1.76, 0.085, Khaki, 8);
		B.Tube(T + 1.0, Off, 1.76, T + 1.45, Off, 1.76, 0.055, Khaki, 8);
		B.Tube(T - 0.72, Off - 0.25, 1.74, T - 1.05, Off - 0.32, 1.74, 0.022, GunSteel, 5);
		// The wireless rail round the top of the turret, open at the back.
		FVector Previous = FVector::ZeroVector;
		for (int32 K = 0; K <= 10; ++K)
		{
			const double Angle = FMath::DegreesToRadians(-130.0 + K * 26.0);
			const FVector Here = FBuild::At(T + FMath::Cos(Angle) * 0.78, Off + FMath::Sin(Angle) * 0.78, 2.12);
			if (K > 0)
			{
				M.Rod(Previous, Here, 0.018, Dark, 4);
			}
			if (K % 2 == 0)
			{
				M.Rod(Here, FBuild::At(T + FMath::Cos(Angle) * 0.62, Off + FMath::Sin(Angle) * 0.62, 1.96), 0.014, Dark, 4);
			}
			Previous = Here;
		}
		// The sun disc on each side of the hull.
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Mark(0.9, Hand * 1.04, 1.27, 0.15, TEXT("JAP"), 0.0, Hand * 0.874, 0.486);
		}
	}

	// ------------------------------------------------------------------------ 7TP

	/**
	 * 7TP, the Polish light tank of 1939: 4.6 m long, 2.43 wide, 2.3 high. Eight small road wheels a side in
	 * two bogies, a boxy fighting compartment forward, and a turret with a long thin 37 mm gun and a box on
	 * its back for the wireless.
	 */
	static void SevenTP(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("6A6C4C")));
		const FLinearColor Olive = B.Paint, Dark = B.Shade;
		const FLinearColor Sandy = Hex(TEXT("A89868")), Umber = Hex(TEXT("4E3E2E"));

		FGear G;
		G.Right = 1.07;
		G.Width = 0.27;
		G.Link = 0.1;
		G.FrontX = 1.95; G.FrontUp = 0.56; G.FrontR = 0.3;
		G.RearX = -1.95; G.RearUp = 0.5; G.RearR = 0.3;
		G.WheelR = 0.15;
		G.Wheels = { -1.62, -1.26, -0.84, -0.48, 0.3, 0.66, 1.08, 1.44 };
		G.Rollers = { -1.35, -0.45, 0.45, 1.35 };
		G.RollerUp = 0.8;
		G.RollerR = 0.085;
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			// The two bogies on their leaf springs, and the mudguards.
			for (const double X : { -1.05, 0.87 })
			{
				B.Plate(X, Hand * 1.19, 0.4, 1.3, 0.05, 0.07, Dark);
				B.Side({ { X - 0.3, 0.46 }, { X + 0.3, 0.46 }, { X, 0.72 } }, Hand * 1.17, Hand * 1.22, Dark);
			}
			B.Plate(0.0, Hand * 1.08, 0.96, 4.4, 0.36, 0.025, Dark);
			B.Lamp(1.85, Hand * 0.7, 1.08, 0.07);
		}
		// The tub and its nose.
		B.Side({ { 2.0, 0.4 }, { 2.3, 0.7 }, { 2.2, 0.92 }, { 1.3, 1.1 }, { -2.2, 1.1 }, { -2.3, 0.75 }, { -2.0, 0.4 } }, -0.86, 0.86, Olive);
		// The fighting compartment, and the engine deck sloping away behind it.
		B.Side({ { 1.35, 1.05 }, { 1.2, 1.52 }, { -0.35, 1.52 }, { -0.4, 1.05 } }, -1.02, 1.02, Olive);
		B.Side({ { -0.4, 1.05 }, { -0.4, 1.38 }, { -2.1, 1.22 }, { -2.25, 1.05 } }, -0.98, 0.98, Olive);
		// The three colours of the Polish camouflage, in broad patches.
		B.Plate(-1.2, 0.3, 1.33, 0.9, 1.1, 0.012, Sandy, 5.4);
		B.Plate(0.5, 1.03, 1.3, 0.8, 0.012, 0.4, Umber);
		B.Plate(0.4, -1.03, 1.3, 0.9, 0.012, 0.4, Sandy);
		// The driver's hatch in the front plate; the louvres over the engine; the silencer across the tail.
		B.Plate(1.3, 0.4, 1.3, 0.05, 0.5, 0.36, Dark);
		B.Plate(1.33, 0.4, 1.36, 0.02, 0.3, 0.035, Pitch);
		for (int32 K = 0; K < 5; ++K)
		{
			B.Plate(-0.9 - K * 0.24, -0.45, 1.36 - K * 0.023, 0.14, 0.7, 0.025, Pitch, 5.4);
		}
		B.Tube(-2.32, -0.6, 1.0, -2.32, 0.6, 1.0, 0.1, Rust, 8);
		// The turret, set a little to the left, with the wireless box on its back and the long 37 mm gun.
		const double T = 0.45, Off = -0.1;
		B.Layers({ Oval(T, Off, 0.62, 0.62, 0.62, 10), Oval(T, Off, 0.54, 0.54, 0.54, 10) }, { 1.52, 2.14 }, Olive, false);
		B.Plate(T - 0.78, Off, 1.9, 0.5, 0.7, 0.4, Olive);
		B.Plate(T - 0.1, Off, 2.16, 0.5, 0.6, 0.035, Dark);
		B.Plate(T + 0.05, Off + 0.3, 2.22, 0.1, 0.1, 0.1, Dark);
		B.Plate(T + 0.56, Off, 1.84, 0.12, 0.46, 0.36, Dark);
		B.Tube(T + 0.58, Off, 1.84, T + 0.95, Off, 1.84, 0.06, Olive, 8);
		B.Tube(T + 0.95, Off, 1.84, T + 2.05, Off, 1.85, 0.032, Olive, 8);
		B.Tube(T + 0.58, Off + 0.2, 1.82, T + 0.95, Off + 0.2, 1.82, 0.04, Dark, 6);
	}

	// ------------------------------------------------------------------------ guns, half-tracks, lorries

	/** The paint for a model that is the same for several nations (a lorry, an old field gun): set before it is built. */
	static FLinearColor NationPaint = Hex(TEXT("60664A"));
	static FString NationTag;

	/** A wheel on an axle across the vehicle: a tyre, a painted wheel inside it, a hub. For lorries and guns. */
	static void RoadWheel(const FBuild& B, double F, double R, double Radius, double Wide, bool bSpoked)
	{
		const double Hand = R >= 0.0 ? 1.0 : -1.0;
		const double In = R - Hand * Wide * 0.5, Out = R + Hand * Wide * 0.5;
		B.Tube(F, In, Radius, F, Out, Radius, Radius, Tyre, 16);
		B.M.Disc(FBuild::At(F, Out + Hand * 0.015, Radius), FVector(0.0, -Hand, 0.0), Radius * (bSpoked ? 0.9 : 0.62), bSpoked ? B.Shade : B.Paint, 16);
		if (bSpoked)
		{
			// Spokes: a dozen, from the hub to the rim.
			for (int32 K = 0; K < 12; ++K)
			{
				const double Angle = K * UE_DOUBLE_TWO_PI / 12.0;
				B.Tube(F, Out + Hand * 0.03, Radius, F + FMath::Cos(Angle) * Radius * 0.88, Out + Hand * 0.03, Radius + FMath::Sin(Angle) * Radius * 0.88, 0.022, B.Paint, 4);
			}
		}
		B.Tube(F, Out, Radius, F, Out + Hand * 0.07, Radius, Radius * 0.2, B.Shade, 8);
	}

	/** What makes one field gun different from another. */
	struct FGunSpec
	{
		FLinearColor Paint;
		/** The wheels: radius, how far each is from the middle, and whether they are spoked (otherwise on tyres). */
		double WheelR = 0.5, Track = 0.8;
		bool bSpoked = false;
		/** The barrel: how far the muzzle is ahead of the trunnions, the breech behind them, how thick, how high, how far raised; a muzzle brake. */
		double Ahead = 2.0, Behind = 0.6, Bore = 0.06, High = 1.05, Raised = 8.0;
		bool bBrake = false;
		/** The cylinder of the recoil gear: above the barrel, or below it; and how long. */
		bool bAbove = false;
		double Recoil = 1.2;
		/** The shield: half its width, its foot and its top. */
		double Shield = 0.9, ShieldFoot = 0.45, ShieldTop = 1.55;
		/** The trail: two legs spread behind, or one; its length. A round platform under the wheels (the 25-pounder's). */
		bool bSplit = true;
		double Trail = 3.0;
		bool bPlatform = false;
	};

	/** A field gun on its carriage, its barrel pointing forward, set so that the middle of the whole is at the origin. */
	static void FieldPiece(FMiniatureMesh& M, const FGunSpec& G)
	{
		const FBuild B(M, G.Paint);
		const FLinearColor Dark = B.Shade;
		const double X = (G.Trail - G.Ahead) * 0.5; // where the axle stands
		for (const double Hand : { -1.0, 1.0 })
		{
			RoadWheel(B, X, Hand * G.Track, G.WheelR, G.bSpoked ? 0.1 : 0.2, G.bSpoked);
		}
		B.Tube(X, -G.Track, G.WheelR, X, G.Track, G.WheelR, 0.06, Dark, 8);
		if (G.bPlatform)
		{
			B.Drum(X, 0.0, 0.0, 0.8, 0.05, Dark, 18);
		}
		// The saddle on the axle that carries the cradle.
		B.Side({ { X - 0.3, G.WheelR - 0.1 }, { X + 0.3, G.WheelR - 0.1 }, { X + 0.18, G.High }, { X - 0.18, G.High } }, -0.2, 0.2, G.Paint);
		// The barrel, raised a little: breech ring and block, the tube tapering to the muzzle, the brake.
		const double Slope = FMath::Tan(FMath::DegreesToRadians(G.Raised));
		auto Along = [&](double D) { return FVector2D(X + D, G.High + D * Slope); };
		const FVector2D Breech = Along(-G.Behind), Mid = Along(G.Ahead * 0.35), Muzzle = Along(G.Ahead);
		B.Tube(Breech.X, 0.0, Breech.Y, Mid.X, 0.0, Mid.Y, G.Bore * 1.5, G.Paint, 10);
		B.Tube(Mid.X, 0.0, Mid.Y, Muzzle.X, 0.0, Muzzle.Y, G.Bore * 1.15, G.Paint, 10);
		B.Plate(Breech.X - 0.05, 0.0, Breech.Y, 0.3, G.Bore * 5.0, G.Bore * 5.0, Dark, -G.Raised);
		if (G.bBrake)
		{
			const FVector2D A = Along(G.Ahead - 0.02), C = Along(G.Ahead + 0.3);
			B.Tube(A.X, 0.0, A.Y, C.X, 0.0, C.Y, G.Bore * 2.1, Dark, 10);
		}
		const FVector2D Tip = Along(G.Ahead + (G.bBrake ? 0.3 : 0.0)), Lip = Along(G.Ahead + (G.bBrake ? 0.32 : 0.02));
		B.Tube(Tip.X, 0.0, Tip.Y, Lip.X, 0.0, Lip.Y, G.Bore * 0.8, Pitch, 8);
		// The cradle under the barrel and the cylinder of the recoil gear.
		const double Lift = G.bAbove ? G.Bore * 2.6 : -G.Bore * 2.6;
		const FVector2D R0 = Along(-0.3), R1 = Along(G.Recoil);
		B.Tube(R0.X, 0.0, R0.Y + Lift, R1.X, 0.0, R1.Y + Lift, G.Bore * 1.3, Dark, 8);
		// The shield: a lower plate leaning back, an upper one leaning back further; a slot for the barrel.
		const double Fore = X + 0.32, Waist = (G.ShieldFoot + G.ShieldTop) * 0.55;
		B.M.Stack(FVector::ZeroVector, 1.0, 0.0,
			{ { { Fore + 0.03, -G.Shield }, { Fore + 0.03, G.Shield }, { Fore, G.Shield }, { Fore, -G.Shield } }, { { Fore - 0.05, -G.Shield }, { Fore - 0.05, G.Shield }, { Fore - 0.08, G.Shield }, { Fore - 0.08, -G.Shield } },
				{ { Fore - 0.22, -G.Shield * 0.94 }, { Fore - 0.22, G.Shield * 0.94 }, { Fore - 0.25, G.Shield * 0.94 }, { Fore - 0.25, -G.Shield * 0.94 } } },
			{ G.ShieldFoot, Waist, G.ShieldTop }, G.Paint, false);
		B.Plate(Fore + 0.02, 0.0, G.High + 0.32 * Slope, 0.06, G.Bore * 5.0, G.Bore * 6.0, Pitch);
		// The sight on the left, and the handwheels.
		B.Plate(X - 0.15, -0.3, G.High + 0.22, 0.08, 0.06, 0.2, Dark);
		B.Tube(X - 0.3, -0.36, G.High - 0.1, X - 0.3, -0.42, G.High - 0.1, 0.11, Dark, 8);
		// The trail, with a spade at the end of each leg.
		if (G.bSplit)
		{
			for (const double Hand : { -1.0, 1.0 })
			{
				B.Tube(X - 0.1, Hand * 0.24, G.WheelR - 0.05, X - G.Trail, Hand * 0.95, 0.12, 0.075, G.Paint, 8);
				B.Plate(X - G.Trail - 0.02, Hand * 0.95, 0.14, 0.06, 0.36, 0.3, Dark);
			}
		}
		else
		{
			B.M.Stack(FVector::ZeroVector, 1.0, 0.0,
				{ { { X - 0.1, -0.3 }, { X - 0.1, 0.3 }, { X - G.Trail, 0.14 }, { X - G.Trail, -0.14 } }, { { X - 0.1, -0.3 }, { X - 0.1, 0.3 }, { X - G.Trail * 0.6, 0.2 }, { X - G.Trail * 0.6, -0.2 } } },
				{ 0.08, G.WheelR + 0.05 }, G.Paint, false);
			B.Plate(X - G.Trail - 0.02, 0.0, 0.16, 0.06, 0.5, 0.34, Dark);
		}
	}

	/** 10.5 cm leichte Feldhaubitze 18, the German field howitzer: big spoked wheels, split trail, the recoil cylinder over the barrel. */
	static void LeFH18(FMiniatureMesh& M)
	{
		FGunSpec G;
		G.Paint = Hex(TEXT("51575C"));
		G.WheelR = 0.65; G.Track = 0.8; G.bSpoked = true;
		G.Ahead = 2.3; G.Behind = 0.65; G.Bore = 0.07; G.High = 1.18; G.Raised = 10.0;
		G.bAbove = true; G.Recoil = 1.3;
		G.Shield = 0.95; G.ShieldFoot = 0.5; G.ShieldTop = 1.75;
		G.Trail = 3.2;
		FieldPiece(M, G);
	}

	/** Ordnance QF 25-pounder, the British field gun: low on tyres, a box trail, a muzzle brake, and the round platform it turns on. */
	static void TwentyFivePounder(FMiniatureMesh& M)
	{
		FGunSpec G;
		G.Paint = Hex(TEXT("666B47"));
		G.WheelR = 0.45; G.Track = 0.82;
		G.Ahead = 1.9; G.Behind = 0.6; G.Bore = 0.058; G.High = 1.02; G.Raised = 8.0;
		G.bBrake = true; G.Recoil = 1.1;
		G.Shield = 0.9; G.ShieldFoot = 0.4; G.ShieldTop = 1.55;
		G.bSplit = false; G.Trail = 2.6; G.bPlatform = true;
		FieldPiece(M, G);
	}

	/** 105 mm Howitzer M2A1, the American field howitzer: a short thick barrel, tyres, a split trail. */
	static void Howitzer105(FMiniatureMesh& M)
	{
		FGunSpec G;
		G.Paint = Hex(TEXT("5B6046"));
		G.WheelR = 0.5; G.Track = 0.92;
		G.Ahead = 1.7; G.Behind = 0.65; G.Bore = 0.072; G.High = 1.12; G.Raised = 10.0;
		G.bAbove = true; G.Recoil = 1.0;
		G.Shield = 1.0; G.ShieldFoot = 0.45; G.ShieldTop = 1.6;
		G.Trail = 3.0;
		FieldPiece(M, G);
	}

	/** 76 mm divisional gun ZiS-3, the Soviet field gun: a long thin barrel with a muzzle brake, low, on tubular legs. */
	static void ZiS3(FMiniatureMesh& M)
	{
		FGunSpec G;
		G.Paint = Hex(TEXT("4F5E3E"));
		G.WheelR = 0.47; G.Track = 0.72;
		G.Ahead = 2.75; G.Behind = 0.7; G.Bore = 0.046; G.High = 0.98; G.Raised = 6.0;
		G.bBrake = true; G.Recoil = 1.3;
		G.Shield = 0.82; G.ShieldFoot = 0.4; G.ShieldTop = 1.38;
		G.Trail = 2.9;
		FieldPiece(M, G);
	}

	/** Canon de 75 modele 1897, the French seventy-five, used by many armies: wooden wheels, one trail, the long recoil cylinder under the barrel. */
	static void SeventyFive(FMiniatureMesh& M)
	{
		FGunSpec G;
		G.Paint = NationPaint;
		G.WheelR = 0.67; G.Track = 0.76; G.bSpoked = true;
		G.Ahead = 1.95; G.Behind = 0.75; G.Bore = 0.048; G.High = 1.06; G.Raised = 8.0;
		G.Recoil = 1.7;
		G.Shield = 0.75; G.ShieldFoot = 0.45; G.ShieldTop = 1.55;
		G.bSplit = false; G.Trail = 2.4;
		FieldPiece(M, G);
	}

	/** The tops of helmets showing over the side of an open vehicle: the men riding in it. */
	static void Riders(const FBuild& B, const FString& Tag, double From, double To, double Half, double Up, int32 Rows)
	{
		const FLinearColor Steel = FMiniatureMesh::UniformColour(Tag) * 0.7f;
		for (int32 K = 0; K < Rows; ++K)
		{
			const double F = Rows > 1 ? FMath::Lerp(From, To, static_cast<double>(K) / (Rows - 1)) : From;
			for (const double Hand : { -1.0, 1.0 })
			{
				B.M.Ball(FBuild::At(F, Hand * Half, Up), FVector(0.27, 0.25, 0.2), Steel, 3, 8);
				B.M.Ball(FBuild::At(F, Hand * Half, Up - 0.16), FVector(0.4, 0.44, 0.3), FMiniatureMesh::UniformColour(Tag), 3, 8);
			}
		}
	}

	/**
	 * Sd.Kfz. 251, the German armoured half-track: 5.8 m long, 2.1 wide, 1.75 high. Wheels in front, tracks on
	 * overlapping wheels behind, and an open-topped body of flat plates that lean out to the waist and in above it.
	 */
	static void Hanomag(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("51575C")));
		const FLinearColor Grey = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 0.82;
		G.Width = 0.28;
		G.Link = 0.14;
		G.FrontX = 0.75; G.FrontUp = 0.5; G.FrontR = 0.3;
		G.RearX = -2.3; G.RearUp = 0.42; G.RearR = 0.27;
		G.Teeth = 12;
		G.WheelR = 0.28;
		G.bInterleaved = true;
		G.Wheels = { -1.8, -1.42, -1.04, -0.66, -0.28, 0.1 };
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			RoadWheel(B, 2.05, Hand * 0.88, 0.45, 0.2, false);
			// Mudguards: over the front wheels, then running back over the tracks.
			B.Plate(2.05, Hand * 0.88, 0.97, 0.75, 0.3, 0.03, Dark);
			B.Plate(2.55, Hand * 0.88, 0.86, 0.36, 0.3, 0.03, Dark, 38.0);
			B.Plate(1.55, Hand * 0.88, 0.86, 0.36, 0.3, 0.03, Dark, -38.0);
			B.Plate(-0.8, Hand * 0.92, 0.82, 3.6, 0.34, 0.03, Dark);
			B.Lamp(2.5, Hand * 0.62, 1.12, 0.07);
			// The cross on each side.
			B.Mark(-0.9, Hand * 1.0, 1.02, 0.17, TEXT("GER"), 0.0, Hand * 0.9, -0.42);
		}
		B.Tube(2.05, -0.8, 0.45, 2.05, 0.8, 0.45, 0.05, Dark, 6);
		// The bonnet: narrow at the nose, its top sloping up to the driver's plate.
		B.Layers({ { { 2.8, -0.42 }, { 2.8, 0.42 }, { 0.95, 0.8 }, { 0.95, -0.8 } }, { { 2.9, -0.5 }, { 2.9, 0.5 }, { 0.95, 1.0 }, { 0.95, -1.0 } }, { { 2.6, -0.36 }, { 2.6, 0.36 }, { 0.95, 0.8 }, { 0.95, -0.8 } } },
			{ 0.62, 1.05, 1.38 }, Grey, false);
		B.Plate(2.92, 0.0, 0.9, 0.03, 0.6, 0.3, Dark);
		// The body: floor, waist and top, the plates leaning out and then in; open above.
		B.Layers({ { { 0.95, -0.8 }, { 0.95, 0.8 }, { -2.85, 0.8 }, { -2.85, -0.8 } }, { { 1.0, -1.02 }, { 1.0, 1.02 }, { -2.95, 1.02 }, { -2.95, -1.02 } }, { { 0.55, -0.84 }, { 0.55, 0.84 }, { -2.75, 0.84 }, { -2.75, -0.84 } } },
			{ 0.75, 1.22, 1.75 }, Grey, false);
		B.Plate(-1.25, 0.0, 1.752, 2.8, 1.5, 0.012, Pitch);
		// The driver's visors in the sloped front plate; the machine gun with its shield over the cab.
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Plate(0.79, Hand * 0.42, 1.5, 0.03, 0.4, 0.1, Pitch, 40.0);
		}
		B.Plate(0.3, 0.0, 1.95, 0.04, 0.6, 0.4, Dark, 12.0);
		B.Tube(0.1, 0.0, 1.93, 1.15, 0.0, 1.98, 0.022, GunSteel, 5);
		B.Plate(0.05, 0.0, 1.9, 0.3, 0.06, 0.1, GunSteel);
		// The doors in the back, and stowage boxes on the mudguards.
		B.Plate(-2.9, 0.0, 1.2, 0.03, 0.9, 0.8, Dark);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Plate(-0.4, Hand * 1.0, 0.92, 0.8, 0.2, 0.18, Dark);
		}
		Riders(B, TEXT("GER"), -0.4, -2.3, 0.45, 1.82, 4);
		B.Aerial(0.4, 0.8, 1.7, 1.8);
	}

	/**
	 * Half-track M3, the American armoured carrier: 6.18 m long, 2.22 wide, 2.26 high. A lorry's bonnet and
	 * front wheels, a short track behind on small bogie wheels, and a square open-topped armoured box.
	 */
	static void M3Halftrack(FMiniatureMesh& M)
	{
		const FBuild B(M, Hex(TEXT("5B6046")));
		const FLinearColor Olive = B.Paint, Dark = B.Shade;

		FGear G;
		G.Right = 0.93;
		G.Width = 0.3;
		G.Link = 0.12;
		G.FrontX = 0.05; G.FrontUp = 0.56; G.FrontR = 0.33;
		G.RearX = -2.3; G.RearUp = 0.5; G.RearR = 0.3;
		G.Teeth = 14;
		G.WheelR = 0.16;
		G.Wheels = { -1.7, -1.34, -0.96, -0.6 };
		G.Rollers = { -1.15 };
		G.RollerUp = 0.83;
		G.RollerR = 0.08;
		B.Gear(G);
		for (const double Hand : { -1.0, 1.0 })
		{
			RoadWheel(B, 2.1, Hand * 0.9, 0.48, 0.22, false);
			B.Plate(2.1, Hand * 0.9, 1.03, 0.8, 0.34, 0.03, Dark);
			B.Plate(2.62, Hand * 0.9, 0.92, 0.36, 0.34, 0.03, Dark, 38.0);
			B.Plate(1.58, Hand * 0.9, 0.92, 0.36, 0.34, 0.03, Dark, -38.0);
			B.Lamp(2.55, Hand * 0.9, 1.2, 0.08);
			// The bogie frame, and the white star on each door.
			B.Side({ { -1.5, 0.3 }, { -0.8, 0.3 }, { -1.05, 0.7 }, { -1.25, 0.7 } }, Hand * 1.07, Hand * 1.11, Dark);
			B.Mark(0.75, Hand * 1.116, 1.38, 0.2, TEXT("USA-ARMY"), 0.0, Hand, 0.0);
		}
		B.Tube(2.1, -0.85, 0.48, 2.1, 0.85, 0.48, 0.05, Dark, 6);
		// The roller on the front bumper, for climbing out of ditches.
		B.Tube(3.02, -0.6, 0.72, 3.02, 0.6, 0.72, 0.15, Dark, 10);
		B.Plate(2.85, 0.0, 0.62, 0.1, 1.9, 0.12, Dark);
		// The bonnet, with the armoured shutters over the radiator and louvres in its sides.
		B.Side({ { 1.3, 0.9 }, { 2.85, 0.9 }, { 2.9, 1.45 }, { 2.75, 1.56 }, { 1.3, 1.62 } }, -0.6, 0.6, Olive);
		for (int32 K = 0; K < 5; ++K)
		{
			B.Plate(2.915, 0.0, 1.0 + K * 0.1, 0.02, 1.0, 0.06, Dark);
			for (const double Hand : { -1.0, 1.0 })
			{
				B.Plate(1.75 + K * 0.2, Hand * 0.61, 1.3, 0.06, 0.02, 0.36, Pitch);
			}
		}
		B.Mark(2.05, 0.0, 1.6, 0.3, TEXT("USA-ARMY"), 0.04, 0.0, 1.0);
		// The armoured box: cab and body in one, open above; the windscreen plate propped over the cab.
		B.Side({ { 1.3, 0.9 }, { 1.3, 1.75 }, { -2.9, 1.75 }, { -2.9, 0.9 } }, -1.11, 1.11, Olive);
		B.Plate(-0.95, 0.0, 1.752, 3.6, 1.9, 0.012, Pitch);
		B.Plate(1.32, 0.0, 1.95, 0.04, 2.0, 0.42, Olive, 20.0);
		for (const double Hand : { -1.0, 1.0 })
		{
			B.Plate(1.345, Hand * 0.5, 1.96, 0.02, 0.6, 0.12, Pitch, 20.0);
			// Racks of mines along the sides, and the fuel cans.
			B.Plate(-1.2, Hand * 1.16, 1.2, 2.2, 0.1, 0.3, Dark);
		}
		// The heavy machine gun on its ring over the cab.
		B.Drum(0.75, 0.45, 1.75, 0.42, 0.2, Dark, 12);
		B.Plate(0.75, 0.45, 2.08, 0.5, 0.08, 0.1, GunSteel);
		B.Tube(0.95, 0.45, 2.09, 2.0, 0.45, 2.12, 0.02, GunSteel, 5);
		B.Tube(0.75, 0.45, 1.95, 0.75, 0.45, 2.06, 0.025, GunSteel, 5);
		Riders(B, TEXT("USA"), -0.4, -2.4, 0.6, 1.83, 4);
		B.Aerial(1.1, -1.0, 1.75, 1.8);
	}

	/**
	 * A three-ton army lorry of the war, of the kind every nation built (Opel Blitz, Bedford, ZIS-5, GMC):
	 * about 6 m long, 2.2 wide, 2.6 high over the canvas. Painted for whichever nation it is asked for.
	 */
	static void Truck(FMiniatureMesh& M)
	{
		const FBuild B(M, NationPaint);
		const FLinearColor Body = B.Paint, Dark = B.Shade;
		const FLinearColor Tilt = NationTag == TEXT("GER") ? Hex(TEXT("6E705E")) : Hex(TEXT("8A8261"));

		for (const double Hand : { -1.0, 1.0 })
		{
			RoadWheel(B, 1.95, Hand * 0.86, 0.43, 0.2, false);
			RoadWheel(B, -1.55, Hand * 0.8, 0.43, 0.42, false);
			// Mudguards over the front wheels, each with a lamp.
			B.Plate(1.95, Hand * 0.86, 0.94, 0.7, 0.3, 0.03, Dark);
			B.Plate(2.45, Hand * 0.86, 0.84, 0.34, 0.3, 0.03, Dark, 38.0);
			B.Plate(1.47, Hand * 0.86, 0.84, 0.34, 0.3, 0.03, Dark, -38.0);
			B.Lamp(2.5, Hand * 0.74, 1.12, 0.09);
			// The running board under each door, and the door's window.
			B.Plate(0.8, Hand * 1.0, 0.62, 1.0, 0.24, 0.03, Dark);
			B.Plate(0.72, Hand * 0.96, 1.85, 0.6, 0.02, 0.36, Pitch);
		}
		// The frame, the axles, and the bumper.
		B.Plate(-0.2, 0.0, 0.62, 5.8, 0.8, 0.14, Dark);
		B.Tube(1.95, -0.8, 0.43, 1.95, 0.8, 0.43, 0.05, Dark, 6);
		B.Tube(-1.55, -0.7, 0.43, -1.55, 0.7, 0.43, 0.07, Dark, 6);
		B.Plate(2.95, 0.0, 0.66, 0.08, 1.9, 0.14, Dark);
		// The bonnet with its radiator grille; the cab with its windscreen.
		B.Side({ { 1.3, 0.75 }, { 2.82, 0.75 }, { 2.86, 1.38 }, { 2.7, 1.5 }, { 1.3, 1.62 } }, -0.5, 0.5, Body);
		B.Plate(2.87, 0.0, 1.1, 0.02, 0.7, 0.6, Pitch);
		for (int32 K = 0; K < 6; ++K)
		{
			B.Plate(2.885, -0.3 + K * 0.12, 1.1, 0.02, 0.03, 0.58, Dark);
		}
		B.Side({ { 0.2, 0.75 }, { 1.3, 0.75 }, { 1.3, 1.62 }, { 1.12, 2.16 }, { 0.2, 2.2 } }, -0.95, 0.95, Body);
		B.Plate(1.225, 0.0, 1.9, 0.02, 1.6, 0.42, Pitch, 18.0);
		// The load bed with its planked sides, and the canvas tilt over its hoops.
		B.Side({ { 0.1, 0.9 }, { 0.1, 1.5 }, { -2.95, 1.5 }, { -2.95, 0.9 } }, -1.08, 1.08, Body);
		for (int32 K = 0; K < 5; ++K)
		{
			for (const double Hand : { -1.0, 1.0 })
			{
				B.Plate(-0.25 - K * 0.6, Hand * 1.09, 1.2, 0.07, 0.03, 0.56, Dark);
			}
		}
		B.Layers({ Lozenge(-2.95, 0.1, 1.08, 0.06, 2), Lozenge(-2.95, 0.1, 1.08, 0.06, 2), Lozenge(-2.93, 0.08, 0.9, 0.06, 2), Lozenge(-2.9, 0.05, 0.55, 0.06, 2) }, { 1.5, 2.15, 2.45, 2.58 }, Tilt, true);
		// The spare wheel behind the cab on the left, and the fuel tank under the bed on the right.
		B.Tube(0.0, -1.0, 1.05, 0.0, -1.16, 1.05, 0.42, Tyre, 14);
		B.Tube(-0.2, 0.9, 0.72, -1.0, 0.9, 0.72, 0.17, Dark, 10);
		if (NationTag == TEXT("USA"))
		{
			for (const double Hand : { -1.0, 1.0 })
			{
				B.Mark(0.75, Hand * 0.957, 1.3, 0.2, TEXT("USA-ARMY"), 0.0, Hand, 0.0);
			}
		}
	}

	using FModel = MiniatureParts::FModel;
	static const FModel Models[] = {
		{ TEXT("PanzerIV"), &PanzerIV, TEXT("Panzer IV  -  5.92 m long, 2.68 m high"), TEXT("GER") },
		{ TEXT("T34"), &T34, TEXT("T-34  -  5.92 m long, 2.45 m high"), TEXT("SOV") },
		{ TEXT("Sherman"), &Sherman, TEXT("M4 Sherman  -  5.84 m long, 2.74 m high"), TEXT("USA") },
		{ TEXT("Matilda"), &Matilda, TEXT("Matilda II  -  5.61 m long, 2.52 m high"), TEXT("UK") },
		{ TEXT("Tiger"), &Tiger, TEXT("Tiger I  -  6.32 m long, 3.00 m high"), TEXT("GER") },
		{ TEXT("CharB1"), &CharB1, TEXT("Char B1 bis  -  6.37 m long, 2.79 m high"), TEXT("FRA") },
		{ TEXT("M13"), &M13, TEXT("M13/40  -  4.92 m long, 2.37 m high"), TEXT("ITA") },
		{ TEXT("Type97"), &Type97, TEXT("Type 97 Chi-Ha  -  5.50 m long, 2.23 m high"), TEXT("JAP") },
		{ TEXT("7TP"), &SevenTP, TEXT("7TP  -  4.60 m long, 2.30 m high"), TEXT("POL") },
		{ TEXT("Hanomag"), &Hanomag, TEXT("Sd.Kfz. 251 half-track  -  5.80 m long, 1.75 m high"), TEXT("GER") },
		{ TEXT("M3Halftrack"), &M3Halftrack, TEXT("M3 half-track  -  6.18 m long, 2.26 m high"), TEXT("USA") },
		{ TEXT("Truck"), &Truck, TEXT("Three-ton lorry  -  6.0 m long, 2.6 m high"), TEXT("GER"), true },
		{ TEXT("leFH18"), &LeFH18, TEXT("10.5 cm leFH 18 howitzer  -  barrel 2.94 m"), TEXT("GER") },
		{ TEXT("25pdr"), &TwentyFivePounder, TEXT("25-pounder field gun  -  barrel 2.47 m"), TEXT("UK") },
		{ TEXT("Howitzer105"), &Howitzer105, TEXT("105 mm M2A1 howitzer  -  barrel 2.36 m"), TEXT("USA") },
		{ TEXT("ZiS3"), &ZiS3, TEXT("76 mm ZiS-3 field gun  -  barrel 3.46 m"), TEXT("SOV") },
		{ TEXT("Gun75"), &SeventyFive, TEXT("75 mm mle 1897 field gun  -  barrel 2.70 m"), TEXT("FRA"), true },
	};

	/** A model of this name, among the vehicles and guns here and the aircraft. */
	static const FModel* Find(const FString& Kind)
	{
		for (const FModel& Known : Models)
		{
			if (Kind == Known.Name)
			{
				return &Known;
			}
		}
		for (const FModel& Known : MiniatureParts::AircraftModels())
		{
			if (Kind == Known.Name)
			{
				return &Known;
			}
		}
		return nullptr;
	}
}

FString& MiniatureParts::NationWanted()
{
	static FString Tag;
	return Tag;
}

bool FMiniatureMesh::Vehicle(const FString& Name, const FVector& Where, double Metre, double Turn)
{
	static TMap<FString, FMiniatureMesh> Built;
	FMiniatureMesh* Model = Built.Find(Name);
	if (!Model)
	{
		// "Truck:SOV" is the lorry in Soviet paint: the part before the colon is the model.
		FString Kind = Name, Whose;
		Name.Split(TEXT(":"), &Kind, &Whose);
		const VehicleParts::FModel* Known = VehicleParts::Find(Kind);
		if (!Known)
		{
			return false;
		}
		FMiniatureMesh Fresh;
		if (Known->Aloft <= 0.0)
		{
			// The dust of the road on the tracks and the lower hull.
			Fresh.DustFoot = 0.0;
			Fresh.DustTop = 1.25;
			Fresh.DustAmount = 0.5f;
		}
		VehicleParts::NationTag = Whose.IsEmpty() ? FString(Known->Nation) : Whose;
		MiniatureParts::NationWanted() = VehicleParts::NationTag;
		VehicleParts::NationPaint = VehicleColour(VehicleParts::NationTag);
		Known->Build(Fresh);
		UE_LOG(LogTheater, Log, TEXT("Model %s built: %d points, %d triangles"), *Name, Fresh.Vertices.Num(), Fresh.Triangles.Num() / 3);
		Model = &Built.Add(Name, MoveTemp(Fresh));
	}
	Stamp(*Model, Where, Metre, Turn);
	return true;
}

TArray<FString> FMiniatureMesh::VehicleNames()
{
	TArray<FString> Names;
	for (const VehicleParts::FModel& Known : VehicleParts::Models)
	{
		Names.Add(Known.bAnyNation ? FString(Known.Name) + TEXT(":") + Known.Nation : FString(Known.Name));
	}
	return Names;
}

TArray<FString> FMiniatureMesh::AircraftNames()
{
	TArray<FString> Names;
	for (const VehicleParts::FModel& Known : MiniatureParts::AircraftModels())
	{
		Names.Add(Known.bAnyNation ? FString(Known.Name) + TEXT(":") + Known.Nation : FString(Known.Name));
	}
	return Names;
}

static const VehicleParts::FModel* ModelCalled(const FString& Name)
{
	FString Kind = Name;
	Name.Split(TEXT(":"), &Kind, nullptr);
	return VehicleParts::Find(Kind);
}

FString FMiniatureMesh::VehicleTitle(const FString& Name)
{
	const VehicleParts::FModel* Known = ModelCalled(Name);
	return Known ? FString(Known->Title) : Name;
}

FString FMiniatureMesh::VehicleNation(const FString& Name)
{
	FString Whose;
	if (Name.Split(TEXT(":"), nullptr, &Whose))
	{
		return Whose;
	}
	const VehicleParts::FModel* Known = ModelCalled(Name);
	return Known ? FString(Known->Nation) : FString();
}

double FMiniatureMesh::VehicleRoom(const FString& Name)
{
	const VehicleParts::FModel* Known = ModelCalled(Name);
	return Known ? Known->Room : 12.0;
}

double FMiniatureMesh::VehicleAloft(const FString& Name)
{
	const VehicleParts::FModel* Known = ModelCalled(Name);
	return Known ? Known->Aloft : 0.0;
}

int32 FMiniatureMesh::Year = 1939;

FString FMiniatureMesh::MainTankOf(const FString& Tag)
{
	if (Tag == TEXT("GER")) return Year >= 1943 ? TEXT("Tiger") : TEXT("PanzerIV");
	if (Tag == TEXT("SOV")) return TEXT("T34");
	if (Tag == TEXT("USA")) return TEXT("Sherman");
	if (Tag == TEXT("FRA") || Tag == TEXT("VIC") || Tag == TEXT("FFR")) return TEXT("CharB1");
	if (Tag == TEXT("ITA")) return TEXT("M13");
	if (Tag == TEXT("JAP") || Tag == TEXT("JPN")) return TEXT("Type97");
	if (Tag == TEXT("POL")) return TEXT("7TP");
	if (Tag == TEXT("UK") || Tag == TEXT("CAN") || Tag == TEXT("AUS") || Tag == TEXT("NZL") || Tag == TEXT("SAF") || Tag == TEXT("IND")) return TEXT("Matilda");
	return FString();
}

FString FMiniatureMesh::MainGunOf(const FString& Tag)
{
	if (Tag == TEXT("GER")) return TEXT("leFH18");
	if (Tag == TEXT("SOV")) return TEXT("ZiS3");
	if (Tag == TEXT("USA")) return TEXT("Howitzer105");
	if (Tag == TEXT("UK") || Tag == TEXT("CAN") || Tag == TEXT("AUS") || Tag == TEXT("NZL") || Tag == TEXT("SAF") || Tag == TEXT("IND")) return TEXT("25pdr");
	return TEXT("Gun75:") + Tag;
}

FString FMiniatureMesh::MainCarrierOf(const FString& Tag)
{
	if (Tag == TEXT("GER")) return TEXT("Hanomag");
	if (Tag == TEXT("USA")) return TEXT("M3Halftrack");
	return TEXT("Truck:") + Tag;
}

bool FMiniatureMesh::MainTank(const FVector& Where, double Metre, const FString& Tag, double Turn)
{
	const FString Name = MainTankOf(Tag);
	return !Name.IsEmpty() && Vehicle(Name, Where, Metre, Turn);
}
