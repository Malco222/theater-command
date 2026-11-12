#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignMiniatureParts.h"
#include "TheaterCommand.h"

/**
 * The aircraft, each built in code from the measurements of the real machine, in metres, flying level with
 * its nose forward and the middle of its fuselage at the origin.
 *
 * An aeroplane is a set of skins. A fuselage is a skin drawn over a row of cross-sections from nose to tail;
 * a wing is a skin drawn over a row of aerofoil sections from root to tip; nacelles, canopies, fins and
 * tailplanes are the same again. So a machine is described by its stations (how wide and deep the fuselage
 * is at each distance from the nose, where the wing's leading and trailing edges are at each distance out)
 * and those numbers come from drawings of the real thing.
 */
namespace AircraftParts
{
	static FLinearColor Hex(const TCHAR* Code)
	{
		return FLinearColor(FColor::FromHex(Code));
	}

	using MiniatureParts::At;
	using MiniatureParts::FPaint;
	using MiniatureParts::Skin;

	static const FLinearColor Glass = Hex(TEXT("2C3F4C"));
	static const FLinearColor Frame = Hex(TEXT("3A3F3A"));
	static const FLinearColor Black = Hex(TEXT("171717"));
	static const FLinearColor GunSteel = Hex(TEXT("2E2F31"));
	static const FLinearColor Tyre = Hex(TEXT("1C1C1B"));

	using MiniatureParts::FStation;
	using MiniatureParts::Flowing;
	using MiniatureParts::Body;

	/** One section of a wing or tail: how far out it is, where its leading and trailing edges are, how high, how thick. */
	struct FSection
	{
		double Out, Lead, Trail, Z, Thick;
	};

	/** More sections between the ones given, so the skin is fine enough to carry a camouflage pattern. */
	static TArray<FSection> Finer(const TArray<FSection>& Sections, double Step)
	{
		TArray<FSection> Fine;
		for (int32 K = 0; K + 1 < Sections.Num(); ++K)
		{
			const FSection& A = Sections[K];
			const FSection& B = Sections[K + 1];
			const int32 Parts = FMath::Max(1, FMath::CeilToInt32(FMath::Abs(B.Out - A.Out) / Step));
			for (int32 P = 0; P < Parts; ++P)
			{
				const double T = static_cast<double>(P) / Parts;
				Fine.Add({ FMath::Lerp(A.Out, B.Out, T), FMath::Lerp(A.Lead, B.Lead, T), FMath::Lerp(A.Trail, B.Trail, T), FMath::Lerp(A.Z, B.Z, T), FMath::Lerp(A.Thick, B.Thick, T) });
			}
		}
		Fine.Add(Sections.Last());
		return Fine;
	}

	/**
	 * The rings of a flying surface over its sections: an aerofoil at each. Hand is +1 for the right wing, -1
	 * for the left; bUpright makes a fin, whose sections are stacked upward and whose thickness is sideways
	 * (Side then moves it to one side, for twin fins).
	 */
	static TArray<TArray<FVector>> Surface(const TArray<FSection>& Sections, double Hand, bool bUpright = false, double Side = 0.0)
	{
		// Round the aerofoil: nose, over the top to the trailing edge, back underneath.
		static const double Chord[14] = { 0.0, 0.02, 0.08, 0.2, 0.4, 0.65, 0.85, 1.0, 0.85, 0.65, 0.4, 0.2, 0.08, 0.02 };
		static const double Camber[14] = { 0.0, 0.2, 0.36, 0.48, 0.5, 0.36, 0.17, 0.0, -0.1, -0.22, -0.34, -0.36, -0.28, -0.16 };
		TArray<TArray<FVector>> Rings;
		for (const FSection& S : Sections)
		{
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			for (int32 K = 0; K < 14; ++K)
			{
				const double X = FMath::Lerp(S.Lead, S.Trail, Chord[K]), Swell = Camber[K] * S.Thick;
				Ring.Add(bUpright ? At(X, Side + Swell * 0.8, S.Z) : At(X, Hand * S.Out, S.Z + Swell));
			}
		}
		return Rings;
	}

	/** A coat of paint: upper surfaces in one colour or two in patches, under surfaces in another. */
	struct FCoat
	{
		FLinearColor Upper, Upper2, Under;
		/** The size of the camouflage patches, in metres; 0 for one plain upper colour. */
		double Patch = 0.0;

		FLinearColor operator()(const FVector& Place, const FVector& Facing) const
		{
			if (Facing.Z < -0.3)
			{
				return Under;
			}
			if (Patch > 0.0 && FMath::PerlinNoise2D(FVector2D(Place.X + 31.7, Place.Y * 1.3 + 12.3) / Patch) > 0.02f)
			{
				return Upper2;
			}
			return Upper;
		}
	};

	/** An airscrew: its boss and blades, at the nose of an engine. */
	static void Airscrew(FMiniatureMesh& M, double F, double R, double U, double Radius, int32 Blades, double Turn, const FLinearColor& Colour)
	{
		const FVector Hub = At(F, R, U), Axis(-1.0, 0.0, 0.0);
		for (int32 K = 0; K < Blades; ++K)
		{
			const double Angle = FMath::DegreesToRadians(Turn) + UE_DOUBLE_TWO_PI * K / Blades;
			const FVector Along = FVector(0.0, FMath::Sin(Angle), FMath::Cos(Angle));
			const FVector Across = ((Axis ^ Along) * 0.75 + Axis * 0.66).GetSafeNormal();
			const FVector Normal = (Along ^ Across).GetSafeNormal();
			const FVector Root = Hub + Along * Radius * 0.12, Belly = Hub + Along * Radius * 0.55, Tip = Hub + Along * Radius;
			const double Wide = Radius * 0.085;
			const FColor Paint = Colour.ToFColor(true);
			for (const double Face : { -1.0, 1.0 })
			{
				M.Quad(Root - Across * Wide * 0.5, Root + Across * Wide * 0.5, Belly + Across * Wide, Belly - Across * Wide, Normal * Face, Paint);
				M.Quad(Belly - Across * Wide, Belly + Across * Wide, Tip + Across * Wide * 0.45, Tip - Across * Wide * 0.45, Normal * Face, Paint);
			}
		}
	}

	/** A gun barrel, from where it leaves the machine to its muzzle. */
	static void Gun(FMiniatureMesh& M, double F0, double R0, double U0, double F1, double R1, double U1, double Bore = 0.022)
	{
		M.Rod(At(F0, R0, U0), At(F1, R1, U1), Bore, GunSteel, 5);
	}

	/** A national marking on a surface facing the way given; Curve wraps it round a fuselage. */
	static void Mark(FMiniatureMesh& M, double F, double R, double U, double Radius, const TCHAR* Tag, double FaceF, double FaceR, double FaceU, double Curve = 0.0)
	{
		M.Marking(At(F, R, U), Radius, Tag, FVector(-FaceF, -FaceR, FaceU), Curve);
	}

	/**
	 * A national marking on each side of a fuselage, so far forward and so far up: it is laid against the skin
	 * there, leaning with the taper of the fuselage and bent to its roundness.
	 */
	static void SideMarks(FMiniatureMesh& M, const TArray<FStation>& Hull, double Square, double F, double Radius, const TCHAR* Tag)
	{
		for (int32 K = 0; K + 1 < Hull.Num(); ++K)
		{
			const FStation& A = Hull[K];
			const FStation& B = Hull[K + 1];
			if (F <= A.X && F >= B.X)
			{
				const double T = (A.X - F) / FMath::Max(A.X - B.X, 1.0e-6);
				const double W = FMath::Lerp(A.W, B.W, T), H = FMath::Lerp(A.H, B.H, T), Z = FMath::Lerp(A.Z, B.Z, T);
				const double Taper = (A.W - B.W) / FMath::Max(A.X - B.X, 1.0e-6); // how fast it narrows toward the tail
				const double Curve = FMath::Max(H * H / FMath::Max(W, 0.05) * FMath::Square(Square / 2.0), Radius * 0.8);
				for (const double Hand : { -1.0, 1.0 })
				{
					Mark(M, F, Hand * (W + 0.02), Z, Radius, Tag, -Taper, Hand, 0.0, Curve);
				}
				return;
			}
		}
	}

	/** Both wings, or both halves of a tailplane, from one set of sections. */
	static void BothSides(FMiniatureMesh& M, const TArray<FSection>& Sections, FPaint Paint, double Step = 0.6)
	{
		const TArray<FSection> Fine = Finer(Sections, Step);
		for (const double Hand : { -1.0, 1.0 })
		{
			Skin(M, Surface(Fine, Hand), Paint, false, true);
		}
	}

	/** The height of a wing's upper (or lower) skin above a point on it, between two of its sections: for laying markings on it. */
	static double WingSkin(const TArray<FSection>& Sections, double Out, bool bUpper)
	{
		for (int32 K = 0; K + 1 < Sections.Num(); ++K)
		{
			if (Out <= Sections[K + 1].Out)
			{
				const double T = (Out - Sections[K].Out) / FMath::Max(Sections[K + 1].Out - Sections[K].Out, 1.0e-6);
				const double Z = FMath::Lerp(Sections[K].Z, Sections[K + 1].Z, T), Thick = FMath::Lerp(Sections[K].Thick, Sections[K + 1].Thick, T);
				return bUpper ? Z + Thick * 0.5 + 0.012 : Z - Thick * 0.36 - 0.012;
			}
		}
		return Sections.Last().Z;
	}

	/** The national marking above one wing and below the other (or on both, both ways), at a distance out and a distance forward. */
	static void WingMarks(FMiniatureMesh& M, const TArray<FSection>& Sections, double Out, double F, double Radius, const TCHAR* Upper, const TCHAR* Lower, bool bBothUpper, bool bBothLower)
	{
		// The wing rises toward the tip: lay the marking along that rise.
		const double Rise = (WingSkin(Sections, Out + 0.5, true) - WingSkin(Sections, Out - 0.5, true));
		for (const double Hand : { -1.0, 1.0 })
		{
			if (bBothUpper || Hand < 0.0)
			{
				Mark(M, F, Hand * Out, WingSkin(Sections, Out, true), Radius, Upper, 0.0, -Hand * Rise, 1.0);
			}
			if (bBothLower || Hand > 0.0)
			{
				Mark(M, F, Hand * Out, WingSkin(Sections, Out, false), Radius, Lower, 0.0, Hand * Rise, -1.0);
			}
		}
	}

	// ------------------------------------------------------------------------ B-17

	/**
	 * Boeing B-17G Flying Fortress: span 31.62 m, length 22.66 m, four Wright Cyclone engines, ten men and
	 * thirteen half-inch machine guns: two in the chin turret, one in each cheek of the nose, two in the top
	 * turret, one over the radio room, two in the ball turret under the belly, one at each waist window, and
	 * two in the tail.
	 */
	static void B17(FMiniatureMesh& M)
	{
		const FCoat Coat{ Hex(TEXT("4F5236")), Hex(TEXT("4F5236")), Hex(TEXT("84878A")) };
		const FLinearColor Olive = Coat.Upper, Dark = Olive * 0.72f, Metal = Hex(TEXT("8E9092"));
		auto Plain = [&Coat](const FVector& Place, const FVector& Facing, int32) { return Coat(Place, Facing); };

		// The fuselage: round in section, the nose a clear cone for the bombardier, the tail gunner's glazing at the very end.
		const TArray<FStation> Hull = { { 8.3, 0.0, 0.12, 0.12 }, { 8.05, 0.0, 0.42, 0.42 }, { 7.6, 0.0, 0.74, 0.74 }, { 7.0, 0.0, 0.95, 0.96 }, { 6.0, 0.0, 1.08, 1.1 }, { 4.5, 0.0, 1.14, 1.15 }, { 2.0, 0.0, 1.15, 1.15 },
			{ -2.0, 0.0, 1.13, 1.14 }, { -5.0, 0.03, 1.0, 1.04 }, { -8.0, 0.1, 0.8, 0.86 }, { -11.0, 0.2, 0.55, 0.62 }, { -13.2, 0.26, 0.4, 0.46 }, { -13.9, 0.27, 0.34, 0.38 }, { -14.36, 0.27, 0.2, 0.22 } };
		Skin(M, Body(Hull, 2.0, 24), [&Coat](const FVector& Place, const FVector& Facing, int32)
		{
			if (Place.X > 7.55)
			{
				return Glass; // the bombardier's nose
			}
			if (Place.X < -13.3 && Place.X > -14.0 && Facing.Z > -0.2)
			{
				return Glass; // the tail gunner's windows
			}
			return Coat(Place, Facing);
		});
		// The frames of the nose glazing.
		for (int32 K = 0; K < 8; ++K)
		{
			const double Angle = K * UE_DOUBLE_TWO_PI / 8.0;
			M.Rod(At(8.28, FMath::Cos(Angle) * 0.13, FMath::Sin(Angle) * 0.13), At(7.6, FMath::Cos(Angle) * 0.75, FMath::Sin(Angle) * 0.75), 0.018, Frame, 4);
		}

		// The flight deck: the pilots' windows standing out of the top of the fuselage, the astrodome ahead of them.
		Skin(M, Body({ { 6.75, 0.9, 0.45, 0.08 }, { 6.2, 1.22, 0.66, 0.3 }, { 5.3, 1.32, 0.68, 0.34 }, { 4.6, 1.3, 0.66, 0.3 }, { 3.0, 1.2, 0.6, 0.2 }, { 1.2, 1.08, 0.45, 0.08 } }, 2.6, 16), [](const FVector& Place, const FVector& Facing, int32)
		{
			return Place.X > 5.25 && Facing.Z < 0.75 ? Glass : Hex(TEXT("4F5236"));
		});
		M.Ball(At(7.0, 0.0, 1.0), FVector(0.5, 0.5, 0.36), Glass, 4, 8);

		// The wings: the leading edge swept back a little, the trailing edge nearly straight, the tips rounded.
		const TArray<FSection> Wing = { { 0.0, 2.7, -3.1, -0.5, 0.95 }, { 4.6, 2.32, -2.8, -0.14, 0.8 }, { 9.0, 1.85, -2.0, 0.21, 0.55 }, { 14.4, 1.1, -0.95, 0.63, 0.28 }, { 15.4, 0.85, -0.7, 0.71, 0.18 }, { 15.81, 0.4, -0.35, 0.74, 0.04 } };
		BothSides(M, Wing, Plain, 1.2);
		// The tailplane, and the great fin with the long fillet running forward from it along the top of the fuselage.
		const TArray<FSection> Tail = { { 0.0, -10.3, -13.3, 0.42, 0.42 }, { 5.6, -11.5, -13.1, 0.42, 0.16 }, { 6.3, -11.9, -13.0, 0.42, 0.1 }, { 6.55, -12.2, -12.8, 0.42, 0.03 } };
		BothSides(M, Tail, Plain, 1.2);
		Skin(M, Surface(Finer({ { 0.0, -6.2, -13.9, 0.75, 0.2 }, { 0.0, -9.4, -14.0, 1.35, 0.34 }, { 0.0, -11.1, -14.05, 2.6, 0.3 }, { 0.0, -12.2, -14.1, 4.3, 0.2 }, { 0.0, -12.7, -13.95, 4.85, 0.1 }, { 0.0, -13.1, -13.7, 5.0, 0.02 } }, 0.7), 1.0, true), Plain, false, true);

		// Four engines in their nacelles, each with a three-bladed airscrew; the inner pair carry the wheels.
		const double Engines[4] = { -8.7, -4.6, 4.6, 8.7 };
		for (int32 K = 0; K < 4; ++K)
		{
			const double R = Engines[K];
			const bool bInner = FMath::Abs(R) < 6.0;
			const double Lead = bInner ? 2.32 : 1.88, Z = (bInner ? -0.14 : 0.19) - 0.12;
			Skin(M, Body({ { Lead + 1.95, Z, 0.5, 0.5 }, { Lead + 1.85, Z, 0.7, 0.7 }, { Lead + 0.9, Z, 0.72, 0.72 }, { Lead - 0.4, Z - 0.05, 0.66, 0.62 }, { Lead - (bInner ? 3.6 : 2.4), Z - 0.1, 0.4, 0.34 }, { Lead - (bInner ? 4.9 : 3.3), Z - 0.02, 0.1, 0.08 } }, 2.0, 18, R),
				[&Coat, Lead](const FVector& Place, const FVector& Facing, int32) { return Place.X > Lead + 1.9 ? Black : Coat(Place, Facing); });
			// The cowl flaps, the engine seen in the mouth of the cowling, and the boss.
			M.Rod(At(Lead + 0.95, R, Z), At(Lead + 0.8, R, Z), 0.76, Dark, 14);
			M.Rod(At(Lead + 1.96, R, Z), At(Lead + 2.25, R, Z), 0.16, Metal, 8);
			Airscrew(M, Lead + 2.18, R, Z, 1.76, 3, K * 27.0, Black);
			// The turbo-supercharger under each nacelle.
			M.Rod(At(Lead - 1.2, R, Z - 0.6), At(Lead - 1.2, R, Z - 0.7), 0.26, Hex(TEXT("5A4A3E")), 10);
		}

		// ---- The guns.
		// The chin turret under the nose, with its two guns.
		M.Ball(At(7.15, 0.0, -0.98), FVector(1.05, 0.95, 0.56), Olive, 5, 10);
		for (const double Hand : { -1.0, 1.0 })
		{
			Gun(M, 7.35, Hand * 0.16, -1.08, 8.45, Hand * 0.16, -1.1, 0.026);
			// The cheek guns, one each side of the nose.
			Gun(M, 7.25, Hand * 0.86, 0.12, 8.0, Hand * 1.22, 0.14);
			M.Box(At(7.2, Hand * 0.9, 0.14), FVector(0.4, 0.06, 0.3), Glass);
			// The waist guns, each at its open window.
			M.Box(At(-5.3 + Hand * 0.35, Hand * 1.0, 0.25), FVector(0.9, 0.06, 0.56), Black);
			Gun(M, -5.3 + Hand * 0.35, Hand * 0.95, 0.2, -5.9 + Hand * 0.35, Hand * 1.95, 0.1);
			// The tail guns.
			Gun(M, -14.3, Hand * 0.09, 0.22, -15.05, Hand * 0.09, 0.2, 0.026);
			// Windows along the nose and by the radio room.
			M.Box(At(6.5, Hand * 1.04, 0.3), FVector(0.5, 0.05, 0.3), Glass);
			M.Box(At(0.6, Hand * 1.15, 0.45), FVector(0.4, 0.05, 0.3), Glass);
		}
		// The top turret behind the pilots: a glazed dome with two guns.
		M.Ball(At(3.7, 0.0, 1.32), FVector(1.0, 1.0, 0.8), Glass, 5, 10);
		M.Rod(At(3.7, 0.0, 1.1), At(3.7, 0.0, 1.3), 0.52, Olive, 12);
		for (const double Hand : { -1.0, 1.0 })
		{
			Gun(M, 3.9, Hand * 0.16, 1.5, 4.95, Hand * 0.16, 1.78, 0.026);
		}
		// The radio room gun, pointing up and back through its hatch.
		M.Box(At(-0.3, 0.0, 1.14), FVector(1.0, 0.7, 0.06), Glass);
		Gun(M, -0.5, 0.0, 1.15, -1.3, 0.0, 1.7);
		// The ball turret under the belly, with its two guns.
		M.Ball(At(-3.9, 0.0, -1.22), FVector(1.12, 1.12, 1.12), Metal, 6, 12);
		M.Rod(At(-3.9, -0.5, -1.22), At(-3.9, 0.5, -1.22), 0.3, Glass, 10);
		for (const double Hand : { -1.0, 1.0 })
		{
			Gun(M, -3.6, Hand * 0.2, -1.45, -2.7, Hand * 0.2, -1.75, 0.026);
		}
		// The tail wheel, and the aerial mast.
		M.Rod(At(-10.4, -0.08, -0.75), At(-10.4, 0.08, -0.75), 0.24, Tyre, 10);
		M.Rod(At(2.2, 0.0, 1.15), At(2.0, 0.0, 1.75), 0.03, Dark, 4);

		// ---- Markings: the star and bar on the fuselage, above the left wing and below the right.
		SideMarks(M, Hull, 2.0, -6.9, 0.5, TEXT("USA-BAR"));
		WingMarks(M, Wing, 11.6, 0.2, 0.95, TEXT("USA-BAR"), TEXT("USA-BAR"), false, false);
	}

	// ------------------------------------------------------------------------ single-engined fighters

	/** What makes one single-engined fighter different from another. */
	struct FFighter
	{
		TArray<FStation> Hull;
		double Square = 2.0;
		/** How many of the first rings are the airscrew's spinner; how far back the nose colour runs. */
		int32 Spinner = 2;
		TArray<FStation> Hood;
		TArray<FSection> Wing, Tail, Fin;
		FCoat Coat;
		FLinearColor Nose, Boss;
		/** How far back the nose colour runs (forward of this is Nose); a very large number for none. */
		double NoseBack = 1000.0;
		double Screw = 1.5;
		int32 Blades = 3;
	};

	static void Fighter(FMiniatureMesh& M, const FFighter& F)
	{
		const FCoat& Coat = F.Coat;
		auto Plain = [&Coat](const FVector& Place, const FVector& Facing, int32) { return Coat(Place, Facing); };
		const double BossBack = F.Hull[F.Spinner - 1].X - 0.01;
		Skin(M, Body(F.Hull, F.Square, 20), [&F, BossBack](const FVector& Place, const FVector& Facing, int32)
		{
			if (Place.X > BossBack)
			{
				return F.Boss;
			}
			if (Place.X > F.NoseBack)
			{
				return F.Nose;
			}
			return F.Coat(Place, Facing);
		});
		Skin(M, Body(F.Hood, 2.3, 16), [](const FVector&, const FVector&, int32) { return Glass; });
		BothSides(M, F.Wing, Plain, 0.45);
		BothSides(M, F.Tail, Plain, 0.5);
		Skin(M, Surface(Finer(F.Fin, 0.35), 1.0, true), Plain, false, true);
		Airscrew(M, F.Hull[F.Spinner - 1].X + 0.06, 0.0, F.Hull[F.Spinner - 1].Z, F.Screw, F.Blades, 20.0, Black);
	}

	/**
	 * Messerschmitt Bf 109E, the German fighter of 1940: span 9.87 m, length 8.64 m. A narrow angular fuselage,
	 * a small framed hood, square-tipped wings, and the tailplane set halfway up the fin.
	 */
	static void Bf109(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.3, 0.0, 0.04, 0.04 }, { 2.9, 0.0, 0.29, 0.29 }, { 2.85, -0.03, 0.36, 0.43 }, { 1.6, -0.03, 0.41, 0.5 }, { 0.6, 0.02, 0.42, 0.55 }, { -0.6, 0.05, 0.4, 0.55 }, { -2.5, 0.12, 0.25, 0.38 }, { -4.6, 0.22, 0.08, 0.2 }, { -5.34, 0.25, 0.03, 0.1 } };
		F.Square = 2.5;
		F.Hood = { { 0.85, 0.5, 0.26, 0.1 }, { 0.35, 0.68, 0.3, 0.22 }, { -0.6, 0.67, 0.29, 0.2 }, { -1.15, 0.5, 0.22, 0.1 } };
		F.Wing = { { 0.0, 1.05, -1.15, -0.44, 0.32 }, { 0.42, 1.05, -1.15, -0.4, 0.31 }, { 4.2, 0.72, -0.45, 0.04, 0.15 }, { 4.8, 0.66, -0.4, 0.1, 0.11 }, { 4.935, 0.5, -0.3, 0.12, 0.03 } };
		F.Tail = { { 0.0, -4.25, -5.0, 0.62, 0.1 }, { 1.35, -4.5, -5.0, 0.62, 0.06 }, { 1.5, -4.65, -4.95, 0.62, 0.02 } };
		F.Fin = { { 0.0, -3.9, -5.36, 0.3, 0.12 }, { 0.0, -4.45, -5.42, 1.0, 0.09 }, { 0.0, -4.8, -5.38, 1.42, 0.05 }, { 0.0, -5.0, -5.3, 1.52, 0.01 } };
		F.Coat = { Hex(TEXT("4B5442")), Hex(TEXT("79806C")), Hex(TEXT("8FB0BF")), 1.9 };
		F.Nose = Hex(TEXT("D8B600"));
		F.Boss = Hex(TEXT("3A4034"));
		F.NoseBack = 1.75;
		F.Screw = 1.5;
		Fighter(M, F);
		const FLinearColor Blue = F.Coat.Under;
		// The radiators under the wings, the oil cooler under the nose, the struts under the tailplane.
		for (const double Hand : { -1.0, 1.0 })
		{
			M.Box(At(-0.35, Hand * 1.25, -0.5), FVector(0.95, 0.5, 0.17), Blue);
			M.Rod(At(-4.6, Hand * 0.75, 0.6), At(-4.7, Hand * 0.08, 0.2), 0.02, Blue, 4);
			// The cannon in each wing, and the main wheels folded outward into the wings.
			Gun(M, 0.85, Hand * 1.8, -0.24, 1.25, Hand * 1.8, -0.24, 0.03);
		}
		M.Box(At(1.95, 0.0, -0.5), FVector(0.8, 0.34, 0.16), Blue);
		M.Rod(At(-0.9, 0.0, 0.8), At(-1.1, 0.0, 1.25), 0.015, Frame, 4);
		// The frames of the hood.
		for (const double X : { 0.35, -0.12, -0.6 })
		{
			M.Box(At(X, 0.0, 0.79), FVector(0.03, 0.56, 0.22), Frame);
		}
		SideMarks(M, F.Hull, F.Square, -2.05, 0.33, TEXT("GER"));
		WingMarks(M, F.Wing, 3.05, 0.2, 0.42, TEXT("GER"), TEXT("GER"), true, true);
	}

	/**
	 * Supermarine Spitfire Mk I, the British fighter of 1940: span 11.23 m, length 9.12 m. Slim and
	 * oval-bodied, with the thin wing whose edges curve to a point: the ellipse nobody mistakes.
	 */
	static void Spitfire(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.55, 0.0, 0.04, 0.04 }, { 3.1, 0.0, 0.3, 0.3 }, { 3.05, -0.02, 0.36, 0.42 }, { 1.8, 0.0, 0.42, 0.52 }, { 0.5, 0.05, 0.43, 0.55 }, { -0.9, 0.08, 0.4, 0.52 }, { -3.0, 0.15, 0.22, 0.36 }, { -5.0, 0.25, 0.06, 0.2 }, { -5.57, 0.28, 0.02, 0.1 } };
		F.Hood = { { 0.8, 0.56, 0.2, 0.06 }, { 0.42, 0.76, 0.26, 0.2 }, { -0.05, 0.82, 0.27, 0.22 }, { -0.55, 0.78, 0.26, 0.2 }, { -1.35, 0.56, 0.16, 0.05 } };
		// The ellipse: the chord at each distance out, set about a line a third of the way back.
		const double Half = 5.615, Root = 2.55;
		for (const double Out : { 0.0, 0.45, 1.2, 2.2, 3.2, 4.1, 4.8, 5.25, 5.5, 5.6 })
		{
			const double Chord = Root * FMath::Sqrt(FMath::Max(0.0, 1.0 - FMath::Square(Out / Half)));
			F.Wing.Add({ Out, 0.2 + Chord * 0.34, 0.2 - Chord * 0.66, -0.46 + FMath::Max(0.0, Out - 0.45) * 0.105, 0.13 * Chord + 0.01 });
		}
		F.Tail = { { 0.0, -4.35, -5.25, 0.36, 0.1 }, { 0.9, -4.5, -5.25, 0.36, 0.08 }, { 1.35, -4.72, -5.2, 0.36, 0.05 }, { 1.6, -4.95, -5.1, 0.36, 0.01 } };
		F.Fin = { { 0.0, -4.0, -5.56, 0.35, 0.12 }, { 0.0, -4.5, -5.7, 1.0, 0.09 }, { 0.0, -4.85, -5.66, 1.35, 0.05 }, { 0.0, -5.15, -5.5, 1.5, 0.01 } };
		F.Coat = { Hex(TEXT("4A5237")), Hex(TEXT("7A6748")), Hex(TEXT("B5C4A8")), 2.1 };
		F.Nose = F.Coat.Upper;
		F.Boss = Black;
		F.Screw = 1.63;
		Fighter(M, F);
		// The radiator under the right wing and the oil cooler under the left; four guns in each wing.
		M.Box(At(-0.45, 1.25, -0.55), FVector(1.0, 0.6, 0.2), F.Coat.Under);
		M.Box(At(-0.2, -1.2, -0.5), FVector(0.6, 0.3, 0.14), F.Coat.Under);
		for (const double Hand : { -1.0, 1.0 })
		{
			for (int32 K = 0; K < 4; ++K)
			{
				const double Out = 2.3 + K * 0.28 + (K >= 2 ? 0.35 : 0.0);
				M.Box(At(0.2 + 0.34 * Root * FMath::Sqrt(1.0 - FMath::Square(Out / Half)) - 0.01, Hand * Out, -0.46 + (Out - 0.45) * 0.105), FVector(0.06, 0.1, 0.07), Hex(TEXT("A02020")));
			}
			Mark(M, -5.0, Hand * 0.05, 0.85, 0.2, TEXT("UK-FIN"), 0.0, Hand, 0.0);
		}
		M.Rod(At(-1.6, 0.0, 0.6), At(-1.75, 0.0, 1.1), 0.015, Frame, 4);
		SideMarks(M, F.Hull, F.Square, -2.45, 0.34, TEXT("UK-A1"));
		WingMarks(M, F.Wing, 3.7, 0.05, 0.62, TEXT("UK-B"), TEXT("UK"), true, true);
	}

	/**
	 * North American P-51D Mustang, the American long-range fighter: span 11.28 m, length 9.83 m. Bare metal,
	 * a clear bubble of a hood, square-tipped wings, and the deep radiator scoop under the belly.
	 */
	static void Mustang(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.9, 0.0, 0.04, 0.04 }, { 3.3, 0.0, 0.33, 0.33 }, { 3.25, -0.02, 0.4, 0.45 }, { 1.8, 0.0, 0.44, 0.56 }, { 0.6, 0.05, 0.44, 0.6 }, { -0.8, 0.06, 0.42, 0.58 }, { -3.2, 0.15, 0.22, 0.38 }, { -5.4, 0.27, 0.06, 0.2 }, { -5.93, 0.3, 0.02, 0.1 } };
		F.Square = 2.3;
		F.Hood = { { 0.95, 0.6, 0.26, 0.08 }, { 0.35, 0.84, 0.33, 0.26 }, { -0.55, 0.86, 0.34, 0.27 }, { -1.5, 0.62, 0.22, 0.06 } };
		F.Wing = { { 0.0, 1.45, -1.45, -0.52, 0.42 }, { 0.9, 1.12, -1.42, -0.46, 0.38 }, { 5.4, 0.55, -0.75, -0.06, 0.16 }, { 5.6, 0.45, -0.68, -0.04, 0.1 }, { 5.64, 0.35, -0.6, -0.04, 0.02 } };
		F.Tail = { { 0.0, -4.6, -5.6, 0.32, 0.12 }, { 1.85, -5.0, -5.62, 0.32, 0.06 }, { 2.0, -5.1, -5.58, 0.32, 0.01 } };
		F.Fin = { { 0.0, -3.9, -5.95, 0.34, 0.1 }, { 0.0, -4.7, -5.98, 0.75, 0.12 }, { 0.0, -5.0, -6.0, 1.3, 0.09 }, { 0.0, -5.25, -5.92, 1.72, 0.04 }, { 0.0, -5.4, -5.85, 1.8, 0.01 } };
		const FLinearColor Metal = Hex(TEXT("A6AAAD"));
		F.Coat = { Metal, Metal, Metal * 0.88f, 0.0 };
		F.Nose = Hex(TEXT("B01818"));
		F.Boss = Hex(TEXT("B01818"));
		F.NoseBack = 2.85;
		F.Screw = 1.7;
		F.Blades = 4;
		Fighter(M, F);
		// The olive panel ahead of the hood that keeps the glare out of the pilot's eyes.
		M.Box(At(2.0, 0.0, 0.55), FVector(2.0, 0.42, 0.04), Hex(TEXT("4F5236")));
		// The radiator scoop under the belly.
		Skin(M, Body({ { 0.25, -0.74, 0.27, 0.13 }, { 0.15, -0.76, 0.31, 0.2 }, { -0.7, -0.84, 0.35, 0.27 }, { -2.0, -0.62, 0.3, 0.2 }, { -2.9, -0.36, 0.18, 0.06 } }, 2.6, 14), [&Metal](const FVector& Place, const FVector&, int32)
		{
			return Place.X > 0.2 ? Black : Metal * 0.92f;
		});
		for (const double Hand : { -1.0, 1.0 })
		{
			// Three guns in each wing.
			for (int32 K = 0; K < 3; ++K)
			{
				Gun(M, 0.86 - K * 0.03, Hand * (2.0 + K * 0.22), -0.3, 1.2 - K * 0.03, Hand * (2.0 + K * 0.22), -0.3, 0.02);
			}
		}
		M.Rod(At(-1.9, 0.0, 0.5), At(-2.0, 0.0, 1.0), 0.015, Frame, 4);
		SideMarks(M, F.Hull, F.Square, -2.3, 0.3, TEXT("USA-BAR"));
		WingMarks(M, F.Wing, 3.7, -0.05, 0.4, TEXT("USA-BAR"), TEXT("USA-BAR"), false, false);
	}

	/**
	 * Mitsubishi A6M2 Zero, the Japanese naval fighter: span 12.0 m, length 9.06 m. A round black cowling over
	 * its radial engine, a long glasshouse of a hood, broad round-tipped wings, pale grey all over.
	 */
	static void Zero(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.76, 0.0, 0.04, 0.04 }, { 3.5, 0.0, 0.17, 0.17 }, { 3.46, 0.0, 0.48, 0.48 }, { 3.2, 0.0, 0.6, 0.6 }, { 2.5, 0.0, 0.61, 0.62 }, { 1.2, 0.0, 0.55, 0.65 }, { -0.6, 0.05, 0.45, 0.6 }, { -2.8, 0.12, 0.25, 0.4 }, { -4.9, 0.22, 0.06, 0.15 }, { -5.3, 0.25, 0.02, 0.08 } };
		F.Hood = { { 1.25, 0.6, 0.24, 0.06 }, { 0.75, 0.82, 0.3, 0.22 }, { -0.8, 0.82, 0.3, 0.22 }, { -1.95, 0.56, 0.18, 0.05 } };
		F.Wing = { { 0.0, 1.45, -1.05, -0.46, 0.38 }, { 0.5, 1.45, -1.05, -0.44, 0.37 }, { 4.6, 0.78, -0.5, -0.03, 0.17 }, { 5.5, 0.42, -0.34, 0.06, 0.1 }, { 5.9, 0.12, -0.2, 0.1, 0.05 }, { 6.0, -0.02, -0.1, 0.11, 0.01 } };
		F.Tail = { { 0.0, -4.1, -5.0, 0.3, 0.1 }, { 1.7, -4.45, -4.95, 0.3, 0.06 }, { 2.3, -4.7, -4.9, 0.3, 0.01 } };
		F.Fin = { { 0.0, -3.7, -5.3, 0.3, 0.12 }, { 0.0, -4.3, -5.4, 1.0, 0.09 }, { 0.0, -4.7, -5.3, 1.45, 0.04 }, { 0.0, -4.95, -5.2, 1.55, 0.01 } };
		const FLinearColor Grey = Hex(TEXT("A6A99C"));
		F.Coat = { Grey, Grey, Grey * 0.93f, 0.0 };
		F.Nose = Hex(TEXT("1E1F22"));
		F.Boss = Hex(TEXT("8E9092"));
		F.NoseBack = 2.45;
		F.Screw = 1.45;
		Fighter(M, F);
		for (const double Hand : { -1.0, 1.0 })
		{
			Gun(M, 1.0, Hand * 1.9, -0.26, 1.45, Hand * 1.9, -0.26, 0.028);
		}
		M.Rod(At(-0.4, 0.0, 1.0), At(-0.6, 0.0, 1.5), 0.015, Frame, 4);
		SideMarks(M, F.Hull, F.Square, -2.3, 0.3, TEXT("JAP-PLAIN"));
		WingMarks(M, F.Wing, 3.9, 0.15, 0.55, TEXT("JAP-PLAIN"), TEXT("JAP-PLAIN"), true, true);
	}

	/**
	 * Yakovlev Yak-1, the Soviet fighter of 1941-43: span 10.0 m, length 8.48 m. Small, pointed and clean, in
	 * green and black above and pale blue beneath, with red stars on its sides, its fin and under its wings.
	 */
	static void Yak(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.2, 0.0, 0.04, 0.04 }, { 2.8, 0.0, 0.27, 0.27 }, { 2.75, -0.03, 0.36, 0.42 }, { 1.5, -0.02, 0.42, 0.5 }, { 0.3, 0.03, 0.42, 0.56 }, { -1.0, 0.06, 0.38, 0.52 }, { -3.0, 0.14, 0.2, 0.34 }, { -4.9, 0.22, 0.05, 0.16 }, { -5.28, 0.25, 0.02, 0.08 } };
		F.Hood = { { 0.55, 0.52, 0.22, 0.06 }, { 0.1, 0.72, 0.28, 0.2 }, { -0.9, 0.7, 0.27, 0.18 }, { -1.7, 0.5, 0.18, 0.05 } };
		F.Wing = { { 0.0, 1.3, -1.1, -0.46, 0.36 }, { 0.45, 1.3, -1.1, -0.44, 0.35 }, { 4.5, 0.62, -0.38, -0.1, 0.15 }, { 4.9, 0.4, -0.28, -0.07, 0.08 }, { 5.0, 0.2, -0.18, -0.06, 0.01 } };
		F.Tail = { { 0.0, -4.2, -5.0, 0.32, 0.1 }, { 1.5, -4.5, -4.95, 0.32, 0.06 }, { 1.7, -4.7, -4.9, 0.32, 0.01 } };
		F.Fin = { { 0.0, -3.8, -5.28, 0.3, 0.12 }, { 0.0, -4.35, -5.38, 0.95, 0.09 }, { 0.0, -4.7, -5.3, 1.35, 0.04 }, { 0.0, -4.95, -5.2, 1.45, 0.01 } };
		F.Coat = { Hex(TEXT("4F5E3E")), Hex(TEXT("2C2F2A")), Hex(TEXT("8FB0BF")), 2.0 };
		F.Nose = F.Coat.Upper;
		F.Boss = Hex(TEXT("4F5E3E"));
		F.Screw = 1.5;
		Fighter(M, F);
		// The radiator under the belly, and the oil cooler under the nose.
		M.Box(At(-0.9, 0.0, -0.52), FVector(1.1, 0.5, 0.2), F.Coat.Under);
		M.Box(At(1.9, 0.0, -0.48), FVector(0.6, 0.3, 0.14), F.Coat.Under);
		SideMarks(M, F.Hull, F.Square, -2.4, 0.3, TEXT("SOV"));
		for (const double Hand : { -1.0, 1.0 })
		{
			Mark(M, -4.75, Hand * 0.05, 0.85, 0.22, TEXT("SOV"), 0.0, Hand, 0.0);
			Mark(M, 0.1, Hand * 3.3, WingSkin(F.Wing, 3.3, false), 0.42, TEXT("SOV"), 0.0, 0.0, -1.0);
		}
	}

	/**
	 * Focke-Wulf Fw 190A, the German fighter from 1942: span 10.5 m, length 8.95 m. A big round engine in a
	 * close cowling, a clear hood set well back, a wide undercarriage, square-cut wings; in greys.
	 */
	static void Fw190(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.45, 0.0, 0.04, 0.04 }, { 3.15, 0.0, 0.2, 0.2 }, { 3.1, 0.0, 0.5, 0.5 }, { 2.85, 0.0, 0.62, 0.62 }, { 1.9, 0.0, 0.62, 0.64 }, { 0.8, 0.02, 0.5, 0.62 }, { -0.6, 0.05, 0.42, 0.58 }, { -2.8, 0.12, 0.24, 0.4 }, { -5.0, 0.22, 0.06, 0.16 }, { -5.5, 0.25, 0.02, 0.08 } };
		F.Hood = { { 0.75, 0.6, 0.22, 0.05 }, { 0.25, 0.8, 0.27, 0.19 }, { -0.7, 0.78, 0.25, 0.16 }, { -1.75, 0.52, 0.14, 0.03 } };
		F.Wing = { { 0.0, 1.35, -0.95, -0.5, 0.36 }, { 0.5, 1.35, -0.95, -0.48, 0.35 }, { 4.6, 0.7, -0.45, -0.12, 0.15 }, { 5.15, 0.5, -0.35, -0.07, 0.08 }, { 5.25, 0.3, -0.25, -0.06, 0.01 } };
		F.Tail = { { 0.0, -4.4, -5.2, 0.35, 0.1 }, { 1.65, -4.65, -5.15, 0.35, 0.06 }, { 1.83, -4.85, -5.1, 0.35, 0.01 } };
		F.Fin = { { 0.0, -4.0, -5.5, 0.3, 0.12 }, { 0.0, -4.6, -5.6, 1.1, 0.09 }, { 0.0, -4.95, -5.5, 1.6, 0.05 }, { 0.0, -5.15, -5.4, 1.7, 0.01 } };
		F.Coat = { Hex(TEXT("5A6066")), Hex(TEXT("767C80")), Hex(TEXT("A9B8BE")), 1.8 };
		F.Nose = Hex(TEXT("2A2D30"));
		F.Boss = Hex(TEXT("2A2D30"));
		F.NoseBack = 2.95;
		F.Screw = 1.65;
		Fighter(M, F);
		for (const double Hand : { -1.0, 1.0 })
		{
			Gun(M, 1.0, Hand * 0.9, -0.36, 1.6, Hand * 0.9, -0.36, 0.028);
			Gun(M, 0.85, Hand * 2.3, -0.26, 1.3, Hand * 2.3, -0.26, 0.028);
		}
		M.Rod(At(-1.3, 0.0, 0.72), At(-1.5, 0.0, 1.15), 0.015, Frame, 4);
		SideMarks(M, F.Hull, F.Square, -2.2, 0.33, TEXT("GER"));
		WingMarks(M, F.Wing, 3.2, 0.15, 0.42, TEXT("GER"), TEXT("GER"), true, true);
	}

	/**
	 * Hawker Hurricane Mk I, the British fighter of 1939-40: span 12.2 m, length 9.84 m. A humped back behind
	 * a framed hood, thick round-tipped wings, a radiator under the belly.
	 */
	static void Hurricane(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.45, 0.0, 0.04, 0.04 }, { 3.05, 0.0, 0.3, 0.3 }, { 3.0, -0.02, 0.36, 0.44 }, { 1.8, 0.0, 0.44, 0.56 }, { 0.5, 0.08, 0.45, 0.62 }, { -0.8, 0.1, 0.42, 0.6 }, { -3.0, 0.16, 0.22, 0.38 }, { -5.4, 0.26, 0.06, 0.2 }, { -6.39, 0.3, 0.02, 0.1 } };
		F.Hood = { { 0.9, 0.66, 0.22, 0.05 }, { 0.4, 0.9, 0.28, 0.2 }, { -0.6, 0.9, 0.28, 0.2 }, { -1.5, 0.68, 0.2, 0.05 } };
		F.Wing = { { 0.0, 1.6, -1.1, -0.5, 0.48 }, { 1.4, 1.5, -1.2, -0.44, 0.46 }, { 5.2, 0.78, -0.58, -0.03, 0.2 }, { 5.8, 0.45, -0.34, 0.03, 0.11 }, { 6.05, 0.15, -0.16, 0.05, 0.05 }, { 6.095, 0.0, -0.08, 0.06, 0.01 } };
		F.Tail = { { 0.0, -5.0, -6.0, 0.36, 0.11 }, { 1.4, -5.25, -5.95, 0.36, 0.07 }, { 1.68, -5.55, -5.85, 0.36, 0.01 } };
		F.Fin = { { 0.0, -4.7, -6.35, 0.36, 0.12 }, { 0.0, -5.2, -6.5, 1.1, 0.09 }, { 0.0, -5.6, -6.45, 1.55, 0.05 }, { 0.0, -5.9, -6.3, 1.68, 0.01 } };
		F.Coat = { Hex(TEXT("4A5237")), Hex(TEXT("7A6748")), Hex(TEXT("B5C4A8")), 2.1 };
		F.Nose = F.Coat.Upper;
		F.Boss = Black;
		F.Screw = 1.7;
		Fighter(M, F);
		M.Box(At(-0.3, 0.0, -0.62), FVector(1.1, 0.6, 0.24), F.Coat.Under);
		for (const double Hand : { -1.0, 1.0 })
		{
			for (int32 K = 0; K < 4; ++K)
			{
				M.Box(At(1.14 - K * 0.02, Hand * (2.2 + K * 0.24), -0.26), FVector(0.06, 0.1, 0.07), Hex(TEXT("A02020")));
			}
			Mark(M, -5.7, Hand * 0.05, 0.95, 0.22, TEXT("UK-FIN"), 0.0, Hand, 0.0);
		}
		M.Rod(At(-1.8, 0.0, 0.72), At(-1.95, 0.0, 1.2), 0.015, Frame, 4);
		SideMarks(M, F.Hull, F.Square, -2.6, 0.36, TEXT("UK-A1"));
		WingMarks(M, F.Wing, 3.9, 0.05, 0.66, TEXT("UK-B"), TEXT("UK"), true, true);
	}

	/**
	 * Curtiss P-40 Warhawk, the American fighter of 1941-42: span 11.4 m, length 9.66 m. A deep radiator under
	 * the nose, a long pointed spinner, olive drab above and grey beneath, with the star of those years.
	 */
	static void Warhawk(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 3.6, 0.0, 0.04, 0.04 }, { 3.15, 0.0, 0.3, 0.3 }, { 3.1, -0.12, 0.38, 0.56 }, { 2.2, -0.1, 0.42, 0.62 }, { 1.0, 0.0, 0.44, 0.6 }, { -0.4, 0.06, 0.43, 0.6 }, { -2.8, 0.15, 0.22, 0.38 }, { -5.2, 0.27, 0.06, 0.2 }, { -6.06, 0.3, 0.02, 0.1 } };
		F.Square = 2.2;
		F.Hood = { { 0.5, 0.62, 0.24, 0.05 }, { 0.0, 0.84, 0.29, 0.21 }, { -0.9, 0.82, 0.28, 0.19 }, { -2.0, 0.6, 0.18, 0.04 } };
		F.Wing = { { 0.0, 1.35, -1.15, -0.5, 0.4 }, { 0.6, 1.35, -1.15, -0.47, 0.39 }, { 5.2, 0.62, -0.52, -0.06, 0.16 }, { 5.6, 0.4, -0.4, -0.03, 0.08 }, { 5.685, 0.2, -0.3, -0.02, 0.01 } };
		F.Tail = { { 0.0, -4.9, -5.8, 0.34, 0.11 }, { 1.7, -5.2, -5.75, 0.34, 0.06 }, { 1.95, -5.45, -5.7, 0.34, 0.01 } };
		F.Fin = { { 0.0, -4.5, -6.05, 0.34, 0.12 }, { 0.0, -5.05, -6.15, 1.1, 0.09 }, { 0.0, -5.4, -6.1, 1.55, 0.05 }, { 0.0, -5.65, -5.95, 1.65, 0.01 } };
		F.Coat = { Hex(TEXT("4F5236")), Hex(TEXT("4F5236")), Hex(TEXT("84878A")), 0.0 };
		F.Nose = F.Coat.Upper;
		F.Boss = Hex(TEXT("4F5236"));
		F.Screw = 1.68;
		Fighter(M, F);
		// The mouth of the radiator under the nose.
		M.Disc(At(3.12, 0.0, -0.42), FVector(-1.0, 0.0, 0.0), 0.24, Black, 10);
		for (const double Hand : { -1.0, 1.0 })
		{
			for (int32 K = 0; K < 3; ++K)
			{
				Gun(M, 0.85, Hand * (2.0 + K * 0.2), -0.3, 1.15, Hand * (2.0 + K * 0.2), -0.3, 0.02);
			}
		}
		M.Rod(At(-2.2, 0.0, 0.6), At(-2.35, 0.0, 1.1), 0.015, Frame, 4);
		SideMarks(M, F.Hull, F.Square, -2.5, 0.3, TEXT("USA"));
		WingMarks(M, F.Wing, 3.8, 0.0, 0.52, TEXT("USA"), TEXT("USA"), false, false);
	}

	/**
	 * Junkers Ju 87B, the Stuka dive bomber of 1939-41: span 13.8 m, length 11.1 m. Wings cranked like a
	 * gull's upside down, fixed wheels in big spats, a long glasshouse for pilot and gunner, a bomb under the belly.
	 */
	static void Stuka(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 4.6, 0.0, 0.04, 0.04 }, { 4.1, 0.0, 0.3, 0.3 }, { 4.05, -0.1, 0.42, 0.6 }, { 2.8, -0.05, 0.45, 0.62 }, { 1.5, 0.02, 0.45, 0.62 }, { -0.5, 0.08, 0.42, 0.6 }, { -3.5, 0.2, 0.22, 0.36 }, { -5.8, 0.3, 0.08, 0.2 }, { -6.5, 0.34, 0.03, 0.1 } };
		F.Square = 2.4;
		F.Hood = { { 1.6, 0.66, 0.24, 0.05 }, { 1.0, 0.92, 0.3, 0.22 }, { -1.2, 0.9, 0.3, 0.2 }, { -2.4, 0.66, 0.18, 0.04 } };
		F.Wing = { { 0.0, 1.9, -1.3, -0.55, 0.5 }, { 1.35, 1.75, -1.3, -0.98, 0.46 }, { 6.2, 0.92, -0.62, -0.2, 0.18 }, { 6.75, 0.55, -0.42, -0.14, 0.09 }, { 6.9, 0.2, -0.25, -0.13, 0.01 } };
		F.Tail = { { 0.0, -5.2, -6.3, 0.72, 0.12 }, { 2.3, -5.5, -6.2, 0.72, 0.07 }, { 2.5, -5.7, -6.1, 0.72, 0.01 } };
		F.Fin = { { 0.0, -4.9, -6.55, 0.4, 0.14 }, { 0.0, -5.4, -6.65, 1.4, 0.1 }, { 0.0, -5.8, -6.55, 2.0, 0.05 }, { 0.0, -6.0, -6.4, 2.1, 0.01 } };
		F.Coat = { Hex(TEXT("3E4638")), Hex(TEXT("56604A")), Hex(TEXT("8FB0BF")), 2.6 };
		F.Nose = F.Coat.Upper;
		F.Boss = Hex(TEXT("3A4034"));
		F.Screw = 1.7;
		Fighter(M, F);
		const FLinearColor Green = F.Coat.Upper, Blue = F.Coat.Under;
		for (const double Hand : { -1.0, 1.0 })
		{
			// The fixed undercarriage at the crank of each wing: a leg in a fairing, and the wheel in its spat.
			M.Limb(At(0.75, Hand * 1.35, -1.0), At(0.85, Hand * 1.35, -1.9), 0.2, 0.17, Green, 8);
			M.Ball(At(0.75, Hand * 1.35, -2.05), FVector(1.25, 0.36, 0.62), Green, 5, 10);
			M.Rod(At(0.8, Hand * 1.35 - 0.1, -2.2), At(0.8, Hand * 1.35 + 0.1, -2.2), 0.3, Tyre, 10);
			// The struts under the tailplane, and the dive brakes under the wings.
			M.Rod(At(-5.7, Hand * 1.2, 0.68), At(-5.75, Hand * 0.1, 0.25), 0.025, Green, 4);
			M.Box(At(0.4, Hand * 2.6, -0.82), FVector(0.12, 1.3, 0.05), Blue);
			Gun(M, 1.5, Hand * 2.0, -0.78, 1.95, Hand * 2.0, -0.78, 0.02);
		}
		// The bomb on its crutch under the belly; the gunner's machine gun; the aerial mast.
		Skin(M, Body({ { 1.3, -0.95, 0.03, 0.03 }, { 0.95, -0.95, 0.22, 0.22 }, { -0.3, -0.95, 0.22, 0.22 }, { -1.1, -0.95, 0.05, 0.05 } }, 2.0, 10), [](const FVector&, const FVector&, int32) { return Hex(TEXT("3A3D38")); });
		Gun(M, -2.3, 0.0, 0.98, -3.1, 0.0, 1.25);
		M.Rod(At(-0.4, 0.0, 1.1), At(-0.55, 0.0, 1.6), 0.015, Frame, 4);
		SideMarks(M, F.Hull, F.Square, -3.0, 0.36, TEXT("GER"));
		WingMarks(M, F.Wing, 4.6, 0.15, 0.55, TEXT("GER"), TEXT("GER"), true, true);
	}

	/**
	 * Ilyushin Il-2, the Soviet armoured ground-attack machine: span 14.6 m, length 11.6 m. A long nose, a
	 * hood for pilot and gunner, wings swept back a little outboard, pods under them for the wheels.
	 */
	static void Sturmovik(FMiniatureMesh& M)
	{
		FFighter F;
		F.Hull = { { 4.5, 0.0, 0.04, 0.04 }, { 4.0, 0.0, 0.3, 0.3 }, { 3.95, -0.05, 0.42, 0.52 }, { 2.6, -0.05, 0.5, 0.62 }, { 1.0, 0.0, 0.5, 0.66 }, { -0.8, 0.06, 0.46, 0.62 }, { -3.6, 0.18, 0.24, 0.38 }, { -6.2, 0.3, 0.07, 0.18 }, { -7.1, 0.33, 0.02, 0.08 } };
		F.Square = 2.3;
		F.Hood = { { 1.5, 0.68, 0.24, 0.05 }, { 0.9, 0.92, 0.28, 0.2 }, { -1.0, 0.9, 0.28, 0.18 }, { -2.0, 0.66, 0.18, 0.04 } };
		F.Wing = { { 0.0, 1.9, -1.4, -0.56, 0.5 }, { 1.6, 1.9, -1.4, -0.5, 0.48 }, { 6.5, 0.55, -0.78, -0.06, 0.18 }, { 7.15, 0.15, -0.52, -0.01, 0.08 }, { 7.3, -0.1, -0.4, 0.0, 0.01 } };
		F.Tail = { { 0.0, -5.9, -7.0, 0.42, 0.12 }, { 2.25, -6.2, -6.9, 0.42, 0.07 }, { 2.45, -6.4, -6.8, 0.42, 0.01 } };
		F.Fin = { { 0.0, -5.6, -7.15, 0.38, 0.12 }, { 0.0, -6.1, -7.25, 1.3, 0.09 }, { 0.0, -6.5, -7.15, 1.8, 0.05 }, { 0.0, -6.7, -7.0, 1.9, 0.01 } };
		F.Coat = { Hex(TEXT("4F5E3E")), Hex(TEXT("2C2F2A")), Hex(TEXT("8FB0BF")), 2.6 };
		F.Nose = F.Coat.Upper;
		F.Boss = Hex(TEXT("4F5E3E"));
		F.Screw = 1.7;
		Fighter(M, F);
		const FCoat Coat = F.Coat;
		for (const double Hand : { -1.0, 1.0 })
		{
			// The pod under each wing that the wheel folds back into, and the cannon outboard of it.
			Skin(M, Body({ { 2.3, -0.78, 0.03, 0.03 }, { 1.7, -0.82, 0.25, 0.22 }, { 0.2, -0.82, 0.25, 0.22 }, { -0.7, -0.72, 0.05, 0.05 } }, 2.0, 10, Hand * 1.6), [&Coat](const FVector& Place, const FVector& Facing, int32) { return Coat(Place, Facing); });
			Gun(M, 1.6, Hand * 2.3, -0.4, 2.5, Hand * 2.3, -0.4, 0.03);
			Mark(M, -6.6, Hand * 0.05, 1.05, 0.26, TEXT("SOV"), 0.0, Hand, 0.0);
			Mark(M, 0.2, Hand * 4.6, WingSkin(F.Wing, 4.6, false), 0.55, TEXT("SOV"), 0.0, 0.0, -1.0);
		}
		Gun(M, -1.95, 0.0, 0.95, -2.8, 0.0, 1.2);
		SideMarks(M, F.Hull, F.Square, -3.3, 0.36, TEXT("SOV"));
	}

	/** A fighter for a nation with none of its own drawn yet: a plain radial-engined monoplane in that nation's colours and markings. */
	static void PlainFighter(FMiniatureMesh& M)
	{
		const FString Tag = MiniatureParts::NationWanted();
		FFighter F;
		F.Hull = { { 3.4, 0.0, 0.04, 0.04 }, { 3.2, 0.0, 0.16, 0.16 }, { 3.15, 0.0, 0.46, 0.46 }, { 2.9, 0.0, 0.58, 0.58 }, { 2.2, 0.0, 0.6, 0.6 }, { 1.0, 0.0, 0.52, 0.62 }, { -0.6, 0.05, 0.44, 0.58 }, { -2.6, 0.12, 0.24, 0.38 }, { -4.5, 0.22, 0.06, 0.15 }, { -4.9, 0.25, 0.02, 0.08 } };
		F.Hood = { { 0.9, 0.56, 0.22, 0.06 }, { 0.4, 0.78, 0.28, 0.2 }, { -0.5, 0.78, 0.28, 0.2 }, { -1.4, 0.54, 0.18, 0.05 } };
		F.Wing = { { 0.0, 1.35, -1.05, -0.45, 0.36 }, { 0.5, 1.35, -1.05, -0.43, 0.35 }, { 4.4, 0.7, -0.45, -0.05, 0.16 }, { 5.1, 0.35, -0.3, 0.02, 0.08 }, { 5.3, 0.05, -0.15, 0.04, 0.01 } };
		F.Tail = { { 0.0, -3.8, -4.65, 0.3, 0.1 }, { 1.6, -4.1, -4.6, 0.3, 0.06 }, { 1.9, -4.35, -4.55, 0.3, 0.01 } };
		F.Fin = { { 0.0, -3.4, -4.9, 0.3, 0.12 }, { 0.0, -3.95, -5.0, 0.95, 0.09 }, { 0.0, -4.35, -4.95, 1.4, 0.04 }, { 0.0, -4.6, -4.85, 1.5, 0.01 } };
		const FLinearColor Paint = FMiniatureMesh::VehicleColour(Tag);
		F.Coat = { Paint, Paint * 0.72f, Hex(TEXT("9FB2B8")), 2.2 };
		F.Nose = Paint * 0.6f;
		F.Boss = Paint * 0.6f;
		F.NoseBack = 2.15;
		F.Screw = 1.45;
		Fighter(M, F);
		SideMarks(M, F.Hull, F.Square, -2.1, 0.3, *Tag);
		WingMarks(M, F.Wing, 3.5, 0.1, 0.5, *Tag, *Tag, true, true);
	}

	// ------------------------------------------------------------------------ bombers with two and four engines

	/** One engine of a bomber. */
	struct FEngine
	{
		/** How far out along the wing; where the wing's leading edge is there; how high. */
		double Out, Lead, Z;
		/** Round and stubby (an air-cooled radial) or long and narrow (a liquid-cooled engine in line). */
		bool bRadial;
		/** How far behind the leading edge the nacelle ends. */
		double Back;
	};

	static void Engine(FMiniatureMesh& M, const FEngine& E, const FCoat& Coat, double Screw, int32 Blades, double Turn)
	{
		const double R = E.Out, L = E.Lead, Z = E.Z;
		if (E.bRadial)
		{
			Skin(M, Body({ { L + 1.75, Z, 0.46, 0.46 }, { L + 1.65, Z, 0.66, 0.66 }, { L + 0.8, Z, 0.68, 0.68 }, { L - 0.4, Z - 0.05, 0.62, 0.58 }, { L - E.Back * 0.7, Z - 0.1, 0.38, 0.32 }, { L - E.Back, Z - 0.02, 0.08, 0.07 } }, 2.0, 18, R),
				[&Coat, L](const FVector& Place, const FVector& Facing, int32) { return Place.X > L + 1.7 ? Black : Coat(Place, Facing); });
			M.Rod(At(L + 1.76, R, Z), At(L + 2.02, R, Z), 0.15, Hex(TEXT("8E9092")), 8);
			Airscrew(M, L + 1.96, R, Z, Screw, Blades, Turn, Black);
		}
		else
		{
			Skin(M, Body({ { L + 2.55, Z, 0.04, 0.04 }, { L + 2.1, Z, 0.34, 0.34 }, { L + 2.05, Z - 0.04, 0.4, 0.48 }, { L + 0.6, Z - 0.1, 0.47, 0.6 }, { L - 1.2, Z - 0.14, 0.44, 0.5 }, { L - E.Back, Z - 0.04, 0.1, 0.1 } }, 2.3, 18, R),
				[&Coat, L](const FVector& Place, const FVector& Facing, int32) { return Place.X > L + 2.09 ? Black : Coat(Place, Facing); });
			Airscrew(M, L + 2.16, R, Z, Screw, Blades, Turn, Black);
			// The exhaust stubs along each side of the engine.
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Box(At(L + 1.2, R + Hand * 0.45, Z + 0.05), FVector(1.0, 0.05, 0.07), Hex(TEXT("4A3A30")));
			}
		}
	}

	/**
	 * Heinkel He 111H, the German bomber of 1940: span 22.6 m, length 16.4 m. Two engines, broad curved wings,
	 * and a nose that is all glass, with no step for a windscreen.
	 */
	static void He111(FMiniatureMesh& M)
	{
		const FCoat Coat{ Hex(TEXT("3E4638")), Hex(TEXT("56604A")), Hex(TEXT("8FB0BF")), 3.2 };
		auto Plain = [&Coat](const FVector& Place, const FVector& Facing, int32) { return Coat(Place, Facing); };
		const TArray<FStation> Hull = { { 7.0, -0.05, 0.2, 0.2 }, { 6.6, -0.03, 0.6, 0.62 }, { 5.6, 0.0, 0.84, 0.9 }, { 4.0, 0.0, 0.9, 1.0 }, { 1.0, 0.05, 0.9, 1.05 }, { -2.0, 0.1, 0.8, 0.95 }, { -5.5, 0.2, 0.5, 0.6 }, { -8.6, 0.3, 0.2, 0.3 }, { -9.4, 0.35, 0.05, 0.12 } };
		Skin(M, Body(Hull, 2.0, 22), [&Coat](const FVector& Place, const FVector& Facing, int32)
		{
			return Place.X > 4.7 && (Facing.Z > -0.55 || Place.X > 6.0) ? Glass : Coat(Place, Facing);
		});
		// The frames of the glass nose.
		for (int32 K = 0; K < 10; ++K)
		{
			const double Angle = K * UE_DOUBLE_TWO_PI / 10.0;
			M.Rod(At(6.98, FMath::Cos(Angle) * 0.2, -0.05 + FMath::Sin(Angle) * 0.2), At(5.6, FMath::Cos(Angle) * 0.85, FMath::Sin(Angle) * 0.91), 0.02, Frame, 4);
		}
		for (const double Ring : { 6.6, 5.6, 4.7 })
		{
			const double Wide = Ring > 6.0 ? 0.61 : (Ring > 5.0 ? 0.85 : 0.89), Deep = Ring > 6.0 ? 0.63 : (Ring > 5.0 ? 0.91 : 0.97);
			for (int32 K = 0; K < 16; ++K)
			{
				const double A0 = K * UE_DOUBLE_TWO_PI / 16.0, A1 = (K + 1) * UE_DOUBLE_TWO_PI / 16.0;
				M.Rod(At(Ring, FMath::Cos(A0) * Wide, FMath::Sin(A0) * Deep), At(Ring, FMath::Cos(A1) * Wide, FMath::Sin(A1) * Deep), 0.02, Frame, 4);
			}
		}
		// The gunner's place on the back, and the gondola under the belly with a gun at each end.
		Skin(M, Body({ { 0.9, 1.0, 0.3, 0.06 }, { 0.3, 1.22, 0.4, 0.22 }, { -0.6, 1.2, 0.38, 0.2 }, { -1.4, 1.0, 0.25, 0.05 } }, 2.3, 14), [](const FVector&, const FVector&, int32) { return Glass; });
		Skin(M, Body({ { 1.0, -0.98, 0.25, 0.08 }, { 0.3, -1.16, 0.4, 0.3 }, { -1.3, -1.16, 0.4, 0.3 }, { -2.1, -0.95, 0.25, 0.08 } }, 2.3, 14), [&Coat](const FVector& Place, const FVector& Facing, int32)
		{
			return Place.X > 0.5 || Place.X < -1.7 ? Glass : Coat.Under;
		});
		Gun(M, 7.0, 0.0, -0.05, 7.7, 0.0, -0.1);
		Gun(M, -0.9, 0.0, 1.3, -1.7, 0.0, 1.6);
		Gun(M, -1.9, 0.0, -1.1, -2.7, 0.0, -1.35);
		Gun(M, 0.8, 0.0, -1.1, 1.5, 0.0, -1.35);
		// The wings, broad at the root with curved edges; the tail.
		const TArray<FSection> Wing = { { 0.0, 2.3, -2.7, -0.6, 0.75 }, { 2.6, 2.3, -2.9, -0.5, 0.7 }, { 6.0, 1.9, -2.35, -0.25, 0.46 }, { 9.0, 1.25, -1.5, -0.04, 0.28 }, { 10.4, 0.75, -0.95, 0.06, 0.17 }, { 11.1, 0.25, -0.5, 0.11, 0.08 }, { 11.3, -0.05, -0.3, 0.12, 0.01 } };
		BothSides(M, Wing, Plain, 0.8);
		BothSides(M, { { 0.0, -7.2, -9.2, 0.45, 0.3 }, { 2.5, -7.6, -9.1, 0.45, 0.2 }, { 3.5, -8.15, -8.95, 0.45, 0.08 }, { 3.85, -8.5, -8.8, 0.45, 0.01 } }, Plain, 0.8);
		Skin(M, Surface(Finer({ { 0.0, -6.7, -9.4, 0.45, 0.2 }, { 0.0, -7.4, -9.5, 1.6, 0.26 }, { 0.0, -8.1, -9.45, 2.6, 0.16 }, { 0.0, -8.5, -9.3, 2.95, 0.06 }, { 0.0, -8.8, -9.1, 3.05, 0.01 } }, 0.5), 1.0, true), Plain, false, true);
		for (const double Hand : { -1.0, 1.0 })
		{
			Engine(M, { Hand * 2.6, 2.3, -0.62, false, 3.6 }, Coat, 1.7, 3, Hand * 20.0);
		}
		SideMarks(M, Hull, 2.0, -3.6, 0.5, TEXT("GER"));
		WingMarks(M, Wing, 7.6, -0.2, 0.75, TEXT("GER"), TEXT("GER"), true, true);
	}

	/**
	 * Avro Lancaster, the British heavy bomber: span 31.1 m, length 21.2 m. Four Merlin engines, a deep
	 * slab-sided fuselage, a long glasshouse over the cockpit, gun turrets in the nose, on the back and in the
	 * tail, and twin oval fins. Earth and green above, black beneath for the night.
	 */
	static void Lancaster(FMiniatureMesh& M)
	{
		const FCoat Coat{ Hex(TEXT("4A5237")), Hex(TEXT("7A6748")), Hex(TEXT("1B1C1E")), 3.4 };
		// (the black of the night bombers comes well up the sides)
		auto Night = [&Coat](const FVector& Place, const FVector& Facing, int32) { return Facing.Z < 0.45 ? Coat.Under : Coat(Place, Facing); };
		auto Plain = [&Coat](const FVector& Place, const FVector& Facing, int32) { return Coat(Place, Facing); };
		const TArray<FStation> Hull = { { 10.2, -0.1, 0.3, 0.34 }, { 9.7, -0.1, 0.6, 0.75 }, { 8.6, 0.0, 0.82, 1.1 }, { 6.0, 0.0, 0.86, 1.25 }, { 0.0, 0.0, 0.86, 1.25 }, { -5.0, 0.05, 0.8, 1.1 }, { -9.0, 0.15, 0.55, 0.75 }, { -10.5, 0.2, 0.42, 0.52 }, { -10.98, 0.2, 0.34, 0.42 } };
		Skin(M, Body(Hull, 2.8, 22), Night);
		// The long hood over the pilot and the navigator.
		Skin(M, Body({ { 8.3, 1.02, 0.4, 0.06 }, { 7.6, 1.42, 0.5, 0.3 }, { 6.2, 1.5, 0.5, 0.32 }, { 5.0, 1.38, 0.42, 0.2 }, { 4.4, 1.24, 0.3, 0.05 } }, 2.4, 14), [](const FVector&, const FVector&, int32) { return Glass; });
		// Turrets: two guns in the nose, two on the back, four in the tail; and the bomb aimer's blister under the nose.
		M.Ball(At(10.05, 0.0, 0.2), FVector(0.8, 0.8, 0.75), Glass, 5, 10);
		M.Ball(At(9.9, 0.0, -0.55), FVector(0.9, 0.8, 0.6), Glass, 5, 10);
		M.Ball(At(-3.6, 0.0, 1.3), FVector(0.95, 0.95, 0.75), Glass, 5, 10);
		M.Ball(At(-11.05, 0.0, 0.22), FVector(0.8, 0.8, 0.82), Glass, 5, 10);
		for (const double Hand : { -1.0, 1.0 })
		{
			Gun(M, 10.2, Hand * 0.1, 0.25, 10.95, Hand * 0.1, 0.28);
			Gun(M, -3.8, Hand * 0.12, 1.45, -4.6, Hand * 0.12, 1.75);
			Gun(M, -11.2, Hand * 0.08, 0.32, -11.95, Hand * 0.08, 0.34);
			Gun(M, -11.2, Hand * 0.08, 0.14, -11.95, Hand * 0.08, 0.12);
			// The row of small windows along each side.
			for (int32 K = 0; K < 6; ++K)
			{
				M.Box(At(3.5 - K * 0.9, Hand * 0.865, 0.55), FVector(0.3, 0.03, 0.16), Glass);
			}
		}
		// The wings: straight across the middle, then tapering and rising to the tips.
		const TArray<FSection> Wing = { { 0.0, 4.2, -0.7, 0.2, 1.0 }, { 3.4, 4.2, -0.7, 0.2, 0.95 }, { 14.6, 2.45, 0.85, 1.5, 0.28 }, { 15.3, 2.2, 1.0, 1.56, 0.16 }, { 15.545, 1.85, 1.2, 1.58, 0.02 } };
		BothSides(M, Wing, Night, 1.2);
		BothSides(M, { { 0.0, -8.6, -10.8, 0.45, 0.3 }, { 4.8, -9.0, -10.5, 0.45, 0.16 }, { 5.05, -9.2, -10.4, 0.45, 0.04 } }, Night, 1.2);
		// Twin oval fins, one at each end of the tailplane.
		for (const double Hand : { -1.0, 1.0 })
		{
			Skin(M, Surface(Finer({ { 0.0, -9.5, -10.1, -0.75, 0.02 }, { 0.0, -9.0, -10.55, -0.3, 0.14 }, { 0.0, -8.8, -10.75, 0.6, 0.18 }, { 0.0, -8.95, -10.7, 1.5, 0.14 }, { 0.0, -9.3, -10.45, 2.1, 0.08 }, { 0.0, -9.65, -10.15, 2.3, 0.01 } }, 0.35), 1.0, true, Hand * 5.0),
				[&Coat](const FVector&, const FVector&, int32) { return Coat.Under; }, true, true);
			Mark(M, -9.75, Hand * 5.0 + 0.085, 1.25, 0.34, TEXT("UK-FIN"), 0.0, 1.0, 0.0);
			Mark(M, -9.75, Hand * 5.0 - 0.085, 1.25, 0.34, TEXT("UK-FIN"), 0.0, -1.0, 0.0);
		}
		const double Outs[4] = { -7.2, -2.95, 2.95, 7.2 };
		for (int32 K = 0; K < 4; ++K)
		{
			const bool bInner = FMath::Abs(Outs[K]) < 5.0;
			Engine(M, { Outs[K], bInner ? 4.2 : 3.6, (bInner ? 0.2 : 0.66) - 0.3, false, bInner ? 4.4 : 3.0 }, FCoat{ Coat.Upper, Coat.Upper2, Coat.Under, Coat.Patch }, 1.98, 3, K * 31.0);
		}
		SideMarks(M, Hull, 2.8, -5.6, 0.55, TEXT("UK-A1"));
		WingMarks(M, Wing, 11.5, 2.0, 1.0, TEXT("UK-B"), TEXT("UK-B"), true, false);
	}

	/** A bomber for a nation with none of its own drawn yet: a plain twin-engined machine in that nation's colours and markings. */
	static void PlainBomber(FMiniatureMesh& M)
	{
		const FString Tag = MiniatureParts::NationWanted();
		const FLinearColor Paint = FMiniatureMesh::VehicleColour(Tag);
		const FCoat Coat{ Paint, Paint * 0.72f, Hex(TEXT("9FB2B8")), 3.0 };
		auto Plain = [&Coat](const FVector& Place, const FVector& Facing, int32) { return Coat(Place, Facing); };
		const TArray<FStation> Hull = { { 6.4, -0.05, 0.2, 0.22 }, { 6.0, -0.02, 0.55, 0.6 }, { 5.0, 0.0, 0.75, 0.85 }, { 3.5, 0.0, 0.8, 0.95 }, { 0.5, 0.05, 0.8, 0.95 }, { -2.5, 0.1, 0.68, 0.8 }, { -5.5, 0.2, 0.4, 0.5 }, { -7.4, 0.28, 0.16, 0.24 }, { -8.0, 0.3, 0.04, 0.1 } };
		Skin(M, Body(Hull, 2.0, 20), [&Coat](const FVector& Place, const FVector& Facing, int32)
		{
			return Place.X > 5.2 ? Glass : Coat(Place, Facing);
		});
		Skin(M, Body({ { 4.6, 0.86, 0.36, 0.06 }, { 4.0, 1.12, 0.46, 0.26 }, { 2.9, 1.14, 0.46, 0.26 }, { 2.1, 0.92, 0.32, 0.05 } }, 2.3, 14), [](const FVector&, const FVector&, int32) { return Glass; });
		M.Ball(At(-1.6, 0.0, 0.98), FVector(0.8, 0.8, 0.6), Glass, 5, 10);
		Gun(M, 6.4, 0.0, -0.05, 7.0, 0.0, -0.08);
		Gun(M, -1.8, 0.0, 1.1, -2.6, 0.0, 1.35);
		const TArray<FSection> Wing = { { 0.0, 2.0, -2.0, -0.5, 0.65 }, { 2.6, 1.95, -2.0, -0.38, 0.6 }, { 8.6, 0.9, -0.85, 0.05, 0.22 }, { 9.6, 0.5, -0.55, 0.12, 0.1 }, { 9.9, 0.1, -0.3, 0.14, 0.01 } };
		BothSides(M, Wing, Plain, 0.8);
		BothSides(M, { { 0.0, -6.2, -7.8, 0.4, 0.26 }, { 2.8, -6.7, -7.7, 0.4, 0.14 }, { 3.3, -7.1, -7.55, 0.4, 0.01 } }, Plain, 0.8);
		Skin(M, Surface(Finer({ { 0.0, -5.6, -7.95, 0.4, 0.2 }, { 0.0, -6.3, -8.05, 1.5, 0.22 }, { 0.0, -6.9, -8.0, 2.4, 0.12 }, { 0.0, -7.4, -7.8, 2.7, 0.01 } }, 0.5), 1.0, true), Plain, false, true);
		for (const double Hand : { -1.0, 1.0 })
		{
			Engine(M, { Hand * 2.6, 1.95, -0.48, true, 3.2 }, Coat, 1.6, 3, Hand * 20.0);
		}
		SideMarks(M, Hull, 2.0, -3.2, 0.42, *Tag);
		WingMarks(M, Wing, 6.6, 0.0, 0.62, *Tag, *Tag, true, true);
	}

	static const MiniatureParts::FModel Models[] = {
		{ TEXT("B17"), &B17, TEXT("B-17G Flying Fortress  -  span 31.6 m, length 22.7 m, 13 guns"), TEXT("USA"), false, 38.0, 4.5 },
		{ TEXT("Bf109"), &Bf109, TEXT("Messerschmitt Bf 109E  -  span 9.9 m, length 8.6 m"), TEXT("GER"), false, 14.0, 3.0 },
		{ TEXT("Spitfire"), &Spitfire, TEXT("Spitfire Mk I  -  span 11.2 m, length 9.1 m"), TEXT("UK"), false, 15.0, 3.0 },
		{ TEXT("Mustang"), &Mustang, TEXT("P-51D Mustang  -  span 11.3 m, length 9.8 m"), TEXT("USA"), false, 15.0, 3.0 },
		{ TEXT("Fw190"), &Fw190, TEXT("Focke-Wulf Fw 190A  -  span 10.5 m, length 9.0 m"), TEXT("GER"), false, 14.0, 3.0 },
		{ TEXT("Hurricane"), &Hurricane, TEXT("Hurricane Mk I  -  span 12.2 m, length 9.8 m"), TEXT("UK"), false, 16.0, 3.0 },
		{ TEXT("Warhawk"), &Warhawk, TEXT("P-40 Warhawk  -  span 11.4 m, length 9.7 m"), TEXT("USA"), false, 15.0, 3.0 },
		{ TEXT("Stuka"), &Stuka, TEXT("Ju 87 Stuka  -  span 13.8 m, length 11.1 m"), TEXT("GER"), false, 18.0, 4.0 },
		{ TEXT("Sturmovik"), &Sturmovik, TEXT("Il-2 Sturmovik  -  span 14.6 m, length 11.6 m"), TEXT("SOV"), false, 19.0, 4.0 },
		{ TEXT("Zero"), &Zero, TEXT("A6M2 Zero  -  span 12.0 m, length 9.1 m"), TEXT("JAP"), false, 15.0, 3.0 },
		{ TEXT("Yak"), &Yak, TEXT("Yak-1  -  span 10.0 m, length 8.5 m"), TEXT("SOV"), false, 14.0, 3.0 },
		{ TEXT("He111"), &He111, TEXT("Heinkel He 111  -  span 22.6 m, length 16.4 m"), TEXT("GER"), false, 28.0, 4.0 },
		{ TEXT("Lancaster"), &Lancaster, TEXT("Avro Lancaster  -  span 31.1 m, length 21.2 m"), TEXT("UK"), false, 38.0, 4.5 },
		{ TEXT("Fighter"), &PlainFighter, TEXT("Fighter  -  span 10.6 m"), TEXT("ITA"), true, 14.0, 3.0 },
		{ TEXT("Bomber"), &PlainBomber, TEXT("Twin-engined bomber  -  span 19.8 m"), TEXT("ITA"), true, 26.0, 4.0 },
	};
}

TConstArrayView<MiniatureParts::FModel> MiniatureParts::AircraftModels()
{
	return AircraftParts::Models;
}

FString FMiniatureMesh::MainFighterOf(const FString& Tag)
{
	// Each air force flies what its designers have brought into service: history's year, moved by research.
	const int32 Reached = YearOf(Tag, 1);
	if (Tag == TEXT("GER")) return Reached >= 1942 ? TEXT("Fw190") : TEXT("Bf109");
	if (Tag == TEXT("UK") || Tag == TEXT("CAN") || Tag == TEXT("AUS") || Tag == TEXT("NZL") || Tag == TEXT("SAF") || Tag == TEXT("IND")) return Reached >= 1941 ? TEXT("Spitfire") : TEXT("Hurricane");
	if (Tag == TEXT("USA")) return Reached >= 1943 ? TEXT("Mustang") : TEXT("Warhawk");
	if (Tag == TEXT("SOV")) return TEXT("Yak");
	if (Tag == TEXT("JAP") || Tag == TEXT("JPN")) return TEXT("Zero");
	return TEXT("Fighter:") + Tag;
}

FString FMiniatureMesh::MainBomberOf(const FString& Tag)
{
	const int32 Reached = YearOf(Tag, 2);
	if (Tag == TEXT("USA")) return TEXT("B17");
	if (Tag == TEXT("GER")) return Reached >= 1941 ? TEXT("He111") : TEXT("Stuka");
	if (Tag == TEXT("SOV")) return Reached >= 1942 ? TEXT("Sturmovik") : TEXT("Bomber:SOV");
	if (Tag == TEXT("UK") || Tag == TEXT("CAN") || Tag == TEXT("AUS") || Tag == TEXT("NZL") || Tag == TEXT("SAF") || Tag == TEXT("IND")) return Reached >= 1942 ? TEXT("Lancaster") : TEXT("Bomber:UK");
	return TEXT("Bomber:") + Tag;
}
