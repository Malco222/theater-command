#include "Campaign/CampaignMiniatures.h"

namespace
{
	const FLinearColor Gunmetal = FLinearColor(FColor::FromHex(TEXT("2A2B2E")));
	const FLinearColor Rubber = FLinearColor(FColor::FromHex(TEXT("1E1D1C")));
	const FLinearColor Skin = FLinearColor(FColor::FromHex(TEXT("C9A27E")));
	const FLinearColor Canvas = FLinearColor(FColor::FromHex(TEXT("8A8261")));
	const FLinearColor Concrete = FLinearColor(FColor::FromHex(TEXT("9A968C")));
	const FLinearColor Stone = FLinearColor(FColor::FromHex(TEXT("7C766B")));
	const FLinearColor SnowCap = FLinearColor(FColor::FromHex(TEXT("EEECE6")));
	const FLinearColor Horsehide = FLinearColor(FColor::FromHex(TEXT("5B4030")));

	/** The colour a figure is moulded in: the nation's, a little deeper so it stands out from its base. */
	FLinearColor Moulded(const FLinearColor& Nation, float Depth = 0.72f)
	{
		return FLinearColor(Nation.R * Depth, Nation.G * Depth, Nation.B * Depth, 1.0f);
	}

	/** A point of a figure, set out from where the figure stands and turned with it. */
	struct FPlacing
	{
		FVector Where;
		double Scale;
		double Cos, Sin;

		FPlacing(const FVector& InWhere, double InScale, double TurnDegrees)
			: Where(InWhere), Scale(InScale), Cos(FMath::Cos(FMath::DegreesToRadians(TurnDegrees))), Sin(FMath::Sin(FMath::DegreesToRadians(TurnDegrees)))
		{
		}

		/** Forward is toward the figure's front, Right to its right, Up above the ground. */
		FVector At(double Forward, double Right, double Up) const
		{
			// A figure that is not turned faces along -X.
			const double X = -Forward, Y = Right;
			return Where + FVector(X * Cos - Y * Sin, X * Sin + Y * Cos, Up) * Scale;
		}
	};
}

int32 FMiniatureMesh::Add(const FVector& Position, const FVector& Normal, const FColor& Colour)
{
	Vertices.Add(Position);
	Normals.Add(Normal);
	Colors.Add(Colour);
	return Vertices.Num() - 1;
}

void FMiniatureMesh::Face(int32 A, int32 B, int32 C, const FVector& Outward)
{
	// The engine draws the side of a triangle whose corners run one way round; put them that way round for the side that faces out.
	const FVector Facing = (Vertices[B] - Vertices[C]) ^ (Vertices[A] - Vertices[C]);
	if ((Facing | Outward) >= 0.0)
	{
		Triangles.Append({ A, B, C });
	}
	else
	{
		Triangles.Append({ A, C, B });
	}
}

void FMiniatureMesh::Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Outward, const FColor& Colour)
{
	const FVector Normal = Outward.GetSafeNormal();
	const int32 IA = Add(A, Normal, Colour), IB = Add(B, Normal, Colour), IC = Add(C, Normal, Colour), ID = Add(D, Normal, Colour);
	Face(IA, IB, IC, Normal);
	Face(IA, IC, ID, Normal);
}

void FMiniatureMesh::Box(const FVector& Center, const FVector& Size, const FLinearColor& Colour, double Turn, double Tip)
{
	const FQuat Rotation = FQuat(FRotator(Tip, Turn, 0.0));
	const FVector X = Rotation.RotateVector(FVector(Size.X * 0.5, 0.0, 0.0));
	const FVector Y = Rotation.RotateVector(FVector(0.0, Size.Y * 0.5, 0.0));
	const FVector Z = Rotation.RotateVector(FVector(0.0, 0.0, Size.Z * 0.5));
	const FColor Paint = Colour.ToFColor(true);
	Quad(Center + X - Y - Z, Center + X + Y - Z, Center + X + Y + Z, Center + X - Y + Z, X, Paint);
	Quad(Center - X - Y - Z, Center - X + Y - Z, Center - X + Y + Z, Center - X - Y + Z, -X, Paint);
	Quad(Center - X + Y - Z, Center + X + Y - Z, Center + X + Y + Z, Center - X + Y + Z, Y, Paint);
	Quad(Center - X - Y - Z, Center + X - Y - Z, Center + X - Y + Z, Center - X - Y + Z, -Y, Paint);
	Quad(Center - X - Y + Z, Center + X - Y + Z, Center + X + Y + Z, Center - X + Y + Z, Z, Paint);
	Quad(Center - X - Y - Z, Center + X - Y - Z, Center + X + Y - Z, Center - X + Y - Z, -Z, Paint);
}

void FMiniatureMesh::Rod(const FVector& From, const FVector& To, double Radius, const FLinearColor& Colour, int32 Sides)
{
	const FVector Axis = (To - From).GetSafeNormal();
	FVector Side, Other;
	Axis.FindBestAxisVectors(Side, Other);
	const FColor Paint = Colour.ToFColor(true);
	TArray<int32> Foot, Top;
	for (int32 K = 0; K <= Sides; ++K)
	{
		const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
		const FVector Out = Side * FMath::Cos(Angle) + Other * FMath::Sin(Angle);
		Foot.Add(Add(From + Out * Radius, Out, Paint));
		Top.Add(Add(To + Out * Radius, Out, Paint));
	}
	for (int32 K = 0; K < Sides; ++K)
	{
		Face(Foot[K], Foot[K + 1], Top[K], Normals[Foot[K]]);
		Face(Foot[K + 1], Top[K + 1], Top[K], Normals[Foot[K]]);
	}
	// The two ends.
	for (int32 End = 0; End < 2; ++End)
	{
		const FVector At = End ? To : From, Out = End ? Axis : -Axis;
		const int32 Middle = Add(At, Out, Paint);
		int32 Previous = INDEX_NONE;
		for (int32 K = 0; K <= Sides; ++K)
		{
			const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
			const int32 Rim = Add(At + (Side * FMath::Cos(Angle) + Other * FMath::Sin(Angle)) * Radius, Out, Paint);
			if (Previous != INDEX_NONE)
			{
				Face(Middle, Previous, Rim, Out);
			}
			Previous = Rim;
		}
	}
}

void FMiniatureMesh::Pillar(const FVector& Base, double FootRadius, double TopRadius, double Height, const FLinearColor& Colour, int32 Sides)
{
	const FColor Paint = Colour.ToFColor(true);
	const double Lean = (FootRadius - TopRadius) / FMath::Max(Height, 1.0e-3);
	TArray<int32> Foot, Top;
	for (int32 K = 0; K <= Sides; ++K)
	{
		const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		const FVector Normal = (Out + FVector(0.0, 0.0, Lean)).GetSafeNormal();
		Foot.Add(Add(Base + Out * FootRadius, Normal, Paint));
		Top.Add(Add(Base + Out * TopRadius + FVector(0.0, 0.0, Height), Normal, Paint));
	}
	for (int32 K = 0; K < Sides; ++K)
	{
		Face(Foot[K], Foot[K + 1], Top[K], Normals[Foot[K]]);
		Face(Foot[K + 1], Top[K + 1], Top[K], Normals[Foot[K]]);
	}
	if (TopRadius > 1.0e-3)
	{
		const int32 Middle = Add(Base + FVector(0.0, 0.0, Height), FVector::UpVector, Paint);
		int32 Previous = INDEX_NONE;
		for (int32 K = 0; K <= Sides; ++K)
		{
			const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
			const int32 Rim = Add(Base + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * TopRadius + FVector(0.0, 0.0, Height), FVector::UpVector, Paint);
			if (Previous != INDEX_NONE)
			{
				Face(Middle, Previous, Rim, FVector::UpVector);
			}
			Previous = Rim;
		}
	}
}

void FMiniatureMesh::Ball(const FVector& Center, const FVector& Size, const FLinearColor& Colour, int32 Rings, int32 Sides)
{
	const FColor Paint = Colour.ToFColor(true);
	const FVector Half = Size * 0.5;
	const int32 First = Vertices.Num();
	for (int32 R = 0; R <= Rings; ++R)
	{
		const double Polar = UE_DOUBLE_PI * R / Rings;
		for (int32 K = 0; K <= Sides; ++K)
		{
			const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
			const FVector Out(FMath::Sin(Polar) * FMath::Cos(Angle), FMath::Sin(Polar) * FMath::Sin(Angle), FMath::Cos(Polar));
			// On an egg the surface leans more where it is squashed.
			const FVector Normal = FVector(Out.X / Half.X, Out.Y / Half.Y, Out.Z / Half.Z).GetSafeNormal();
			Add(Center + Out * Half, Normal, Paint);
		}
	}
	for (int32 R = 0; R < Rings; ++R)
	{
		for (int32 K = 0; K < Sides; ++K)
		{
			const int32 A = First + R * (Sides + 1) + K, B = A + 1, C = A + Sides + 1, D = C + 1;
			const FVector Out = Normals[R == 0 ? C : A];
			if (R > 0)
			{
				Face(A, B, C, Out);
			}
			if (R < Rings - 1)
			{
				Face(B, D, C, Out);
			}
		}
	}
}

// ---------------------------------------------------------------------------- the figures

void FMiniatureMesh::Rifleman(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Scale, Turn);
	const FLinearColor Cloth = Moulded(Nation, 0.8f);
	const FLinearColor Dark = Moulded(Nation, 0.5f);
	// Legs and boots, coat, head and helmet. A man is 1.0 tall to the top of his helmet.
	Pillar(P.At(0.0, -0.08, 0.0), 0.075 * Scale, 0.07 * Scale, 0.42 * Scale, Dark, 7);
	Pillar(P.At(0.0, 0.08, 0.0), 0.075 * Scale, 0.07 * Scale, 0.42 * Scale, Dark, 7);
	Pillar(P.At(0.0, 0.0, 0.38), 0.17 * Scale, 0.14 * Scale, 0.42 * Scale, Cloth, 9);
	Ball(P.At(0.0, 0.0, 0.88), FVector(0.2, 0.2, 0.22) * Scale, Skin, 5, 8);
	Ball(P.At(0.0, 0.0, 0.94), FVector(0.29, 0.29, 0.17) * Scale, Dark, 4, 9);
	// The rifle, carried at the slope across the body.
	Rod(P.At(0.14, 0.17, 0.3), P.At(-0.06, 0.1, 1.12), 0.022 * Scale, Gunmetal, 5);
	// A pack on his back.
	Box(P.At(-0.16, 0.0, 0.62), FVector(0.13, 0.24, 0.24) * Scale, Canvas, Turn);
}

void FMiniatureMesh::Tank(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Scale, Turn);
	const FLinearColor Hull = Moulded(Nation, 0.74f);
	const FLinearColor Deck = Moulded(Nation, 0.6f);
	// Tracks, with a row of wheels showing; the hull between them; the engine deck; turret, gun and cupola.
	for (const double Side : { -1.0, 1.0 })
	{
		Box(P.At(0.0, Side * 0.62, 0.2), FVector(2.5, 0.3, 0.4) * Scale, Rubber, Turn);
		for (int32 Wheel = 0; Wheel < 5; ++Wheel)
		{
			const double Along = -0.92 + Wheel * 0.46;
			Rod(P.At(Along, Side * 0.74, 0.19), P.At(Along, Side * 0.8, 0.19), 0.15 * Scale, Gunmetal, 8);
		}
	}
	Box(P.At(0.0, 0.0, 0.4), FVector(2.3, 1.0, 0.42) * Scale, Hull, Turn);
	Box(P.At(1.02, 0.0, 0.36), FVector(0.5, 1.0, 0.3) * Scale, Hull, Turn, 24.0); // the sloped front plate
	Box(P.At(-0.72, 0.0, 0.66), FVector(0.8, 0.86, 0.12) * Scale, Deck, Turn);
	Pillar(P.At(0.18, 0.0, 0.61), 0.44 * Scale, 0.38 * Scale, 0.3 * Scale, Hull, 12);
	Rod(P.At(0.5, 0.0, 0.76), P.At(1.75, 0.0, 0.8), 0.05 * Scale, Gunmetal, 7);
	Rod(P.At(0.5, 0.0, 0.76), P.At(0.72, 0.0, 0.77), 0.1 * Scale, Deck, 7); // the mantlet
	Pillar(P.At(0.0, 0.14, 0.9), 0.13 * Scale, 0.12 * Scale, 0.08 * Scale, Deck, 8);
}

void FMiniatureMesh::Lorry(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Scale, Turn);
	const FLinearColor Body = Moulded(Nation, 0.74f);
	// Chassis, bonnet and cab, the load under canvas, and four wheels.
	Box(P.At(0.0, 0.0, 0.3), FVector(2.2, 0.7, 0.12) * Scale, Gunmetal, Turn);
	Box(P.At(0.9, 0.0, 0.52), FVector(0.5, 0.62, 0.34) * Scale, Body, Turn);
	Box(P.At(0.42, 0.0, 0.68), FVector(0.5, 0.78, 0.66) * Scale, Body, Turn);
	Box(P.At(0.42, 0.0, 0.86), FVector(0.52, 0.7, 0.2) * Scale, Gunmetal, Turn); // the windows
	Box(P.At(-0.5, 0.0, 0.62), FVector(1.25, 0.86, 0.5) * Scale, Body, Turn);
	Rod(P.At(-1.1, 0.0, 0.92), P.At(0.1, 0.0, 0.92), 0.4 * Scale, Canvas, 9); // the canvas tilt, as a half barrel
	for (const double Side : { -1.0, 1.0 })
	{
		for (const double Along : { 0.82, -0.7 })
		{
			Rod(P.At(Along, Side * 0.36, 0.22), P.At(Along, Side * 0.5, 0.22), 0.22 * Scale, Rubber, 9);
		}
	}
}

void FMiniatureMesh::Horseman(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Scale, Turn);
	const FLinearColor Cloth = Moulded(Nation, 0.8f);
	const FLinearColor Dark = Moulded(Nation, 0.5f);
	// The horse: barrel, neck, head, four legs, tail.
	Ball(P.At(0.0, 0.0, 0.78), FVector(1.15, 0.42, 0.5) * Scale, Horsehide, 6, 10);
	Rod(P.At(0.45, 0.0, 0.88), P.At(0.72, 0.0, 1.28), 0.13 * Scale, Horsehide, 7);
	Box(P.At(0.86, 0.0, 1.3), FVector(0.36, 0.16, 0.18) * Scale, Horsehide, Turn, -28.0);
	for (const double Side : { -1.0, 1.0 })
	{
		Rod(P.At(0.4, Side * 0.12, 0.62), P.At(0.44, Side * 0.12, 0.0), 0.05 * Scale, Horsehide, 5);
		Rod(P.At(-0.4, Side * 0.12, 0.62), P.At(-0.46, Side * 0.12, 0.0), 0.05 * Scale, Horsehide, 5);
	}
	Rod(P.At(-0.55, 0.0, 0.85), P.At(-0.78, 0.0, 0.5), 0.045 * Scale, Rubber, 5);
	// The rider.
	Pillar(P.At(-0.02, 0.0, 0.98), 0.16 * Scale, 0.13 * Scale, 0.42 * Scale, Cloth, 8);
	Ball(P.At(-0.02, 0.0, 1.48), FVector(0.19, 0.19, 0.21) * Scale, Skin, 5, 8);
	Ball(P.At(-0.02, 0.0, 1.54), FVector(0.27, 0.27, 0.15) * Scale, Dark, 4, 8);
	for (const double Side : { -1.0, 1.0 })
	{
		Rod(P.At(0.02, Side * 0.2, 1.02), P.At(0.08, Side * 0.24, 0.6), 0.05 * Scale, Dark, 5);
	}
	Rod(P.At(0.2, 0.18, 1.0), P.At(0.05, 0.18, 1.85), 0.018 * Scale, Gunmetal, 5); // a lance
}

void FMiniatureMesh::Bunker(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Scale, Turn);
	// A low concrete casemate with a firing slit, earth banked against it, and a gun looking out.
	Pillar(P.At(0.0, 0.0, 0.0), 1.05 * Scale, 0.8 * Scale, 0.2 * Scale, FLinearColor(FColor::FromHex(TEXT("6F7450"))), 12);
	Pillar(P.At(0.0, 0.0, 0.16), 0.74 * Scale, 0.62 * Scale, 0.46 * Scale, Concrete, 12);
	Pillar(P.At(0.0, 0.0, 0.62), 0.62 * Scale, 0.4 * Scale, 0.12 * Scale, Concrete, 12);
	Box(P.At(0.68, 0.0, 0.44), FVector(0.1, 0.56, 0.11) * Scale, Rubber, Turn);
	Rod(P.At(0.6, 0.0, 0.44), P.At(1.25, 0.0, 0.47), 0.04 * Scale, Gunmetal, 6);
	Box(P.At(-0.2, 0.0, 0.78), FVector(0.2, 0.2, 0.1) * Scale, Moulded(Nation, 0.8f), Turn);
}

void FMiniatureMesh::FieldGun(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Scale, Turn);
	const FLinearColor Paint = Moulded(Nation, 0.74f);
	// Two wheels on an axle, a split trail behind, a shield, and the barrel looking over it.
	for (const double Side : { -1.0, 1.0 })
	{
		Rod(P.At(0.0, Side * 0.42, 0.3), P.At(0.0, Side * 0.52, 0.3), 0.3 * Scale, Rubber, 10);
		Rod(P.At(-0.1, Side * 0.12, 0.3), P.At(-1.25, Side * 0.38, 0.06), 0.05 * Scale, Paint, 6);
	}
	Rod(P.At(0.0, -0.42, 0.3), P.At(0.0, 0.42, 0.3), 0.06 * Scale, Gunmetal, 6);
	Box(P.At(0.12, 0.0, 0.52), FVector(0.06, 0.8, 0.5) * Scale, Paint, Turn, 8.0);
	Rod(P.At(-0.35, 0.0, 0.5), P.At(1.35, 0.0, 0.66), 0.055 * Scale, Gunmetal, 7);
	Rod(P.At(-0.3, 0.0, 0.5), P.At(0.2, 0.0, 0.55), 0.1 * Scale, Paint, 7);
}

void FMiniatureMesh::Crag(const FVector& Where, double Scale)
{
	// A rock with snow on it: what mountain troops stand beside.
	Pillar(Where, 0.55 * Scale, 0.16 * Scale, 0.95 * Scale, Stone, 7);
	Pillar(Where + FVector(0.0, 0.0, 0.62 * Scale), 0.3 * Scale, 0.0, 0.5 * Scale, SnowCap, 7);
	Pillar(Where + FVector(0.32, -0.25, 0.0) * Scale, 0.36 * Scale, 0.1 * Scale, 0.55 * Scale, Stone, 6);
}

void FMiniatureMesh::Force(const TArray<FString>& Kinds, double BaseRadius, double Top, double Unit, const FLinearColor& Nation)
{
	int32 Armour = 0, Guns = 0, Motor = 0, Horse = 0, Mountain = 0, Fort = 0, Foot = 0;
	for (const FString& Kind : Kinds)
	{
		if (Kind == TEXT("arm")) ++Armour;
		else if (Kind == TEXT("art")) ++Guns;
		else if (Kind == TEXT("mot")) ++Motor;
		else if (Kind == TEXT("cav")) ++Horse;
		else if (Kind == TEXT("mtn")) ++Mountain;
		else if (Kind == TEXT("fort")) ++Fort;
		else ++Foot;
	}
	const double R = BaseRadius;
	auto On = [Top, R](double X, double Y) { return FVector(X * R, Y * R, Top); };

	// The big things, at most two or the base is crowded: tanks before guns before lorries before bunkers.
	// The figures are large for their base, as a board game's are, so that they read from across the table.
	TArray<int32> Heavy; // 0 tank, 1 gun, 2 lorry, 3 bunker
	for (int32 K = 0; K < FMath::Min(Armour, 2); ++K) Heavy.Add(0);
	for (int32 K = 0; K < FMath::Min(Guns, 1); ++K) Heavy.Add(1);
	for (int32 K = 0; K < FMath::Min(Motor, 1); ++K) Heavy.Add(2);
	for (int32 K = 0; K < FMath::Min(Fort, 1); ++K) Heavy.Add(3);
	Heavy.SetNum(FMath::Min(Heavy.Num(), 2));
	const bool bPair = Heavy.Num() == 2;
	for (int32 K = 0; K < Heavy.Num(); ++K)
	{
		// One stands to the fore and a little to the left; two stand side by side. Each is turned to be seen three-quarters on.
		const FVector Where = bPair ? On(-0.18, K ? 0.46 : -0.44) : On(-0.2, -0.22);
		const double Size = (bPair ? 1.75 : 2.3) * Unit;
		const double Turn = bPair ? (K ? 20.0 : -20.0) : -24.0;
		switch (Heavy[K])
		{
		case 0: Tank(Where, Size, Nation, Turn); break;
		case 1: FieldGun(Where, Size * 1.05, Nation, Turn); break;
		case 2: Lorry(Where, Size, Nation, Turn); break;
		default: Bunker(Where, Size * 1.15, Nation, Turn); break;
		}
	}

	// The men fill what room is left: riflemen for the infantry, a horseman for cavalry, riflemen by a rock for mountain troops.
	TArray<FVector2D> Places;
	if (Heavy.Num() == 0)
	{
		Places = { { -0.36, -0.36 }, { -0.4, 0.3 }, { 0.02, -0.02 }, { 0.0, 0.62 } };
	}
	else if (Heavy.Num() == 1)
	{
		Places = { { -0.42, 0.52 }, { 0.02, 0.52 } };
	}
	else
	{
		Places = { { 0.3, 0.1 } };
	}
	const double Man = 4.4 * Unit;
	int32 Slot = 0;
	if (Mountain > 0 && Places.IsValidIndex(Slot))
	{
		Crag(On(Places[Slot].X, Places[Slot].Y), Man * 0.8);
		++Slot;
	}
	for (int32 K = 0; K < FMath::Min(Horse, 1) && Places.IsValidIndex(Slot); ++K, ++Slot)
	{
		Horseman(On(Places[Slot].X, Places[Slot].Y), Man * 0.7, Nation, -16.0);
	}
	// One rifleman for each corps on foot, up to the room there is; at least one if there is nothing else on the base.
	int32 Men = FMath::Min(Foot + Mountain, Places.Num() - Slot);
	if (Men == 0 && Heavy.Num() == 0 && Horse == 0)
	{
		Men = 1;
	}
	for (int32 K = 0; K < Men && Places.IsValidIndex(Slot); ++K, ++Slot)
	{
		Rifleman(On(Places[Slot].X, Places[Slot].Y), Man, Nation, (K % 3 - 1) * 16.0);
	}
}
