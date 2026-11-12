#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignMiniatureParts.h"

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

	using FPlacing = MiniatureParts::FPlacing;
}

int32 FMiniatureMesh::Add(const FVector& Position, const FVector& Normal, const FColor& Colour)
{
	Vertices.Add(Position);
	Normals.Add(Normal);
	if (DustTop > DustFoot && Position.Z < DustTop)
	{
		// The dust of the road: thickest at the ground, gone by the height it is told to stop at.
		const float Much = FMath::Clamp(static_cast<float>((DustTop - Position.Z) / (DustTop - DustFoot)), 0.0f, 1.0f) * DustAmount;
		Colors.Add(FColor(
			static_cast<uint8>(FMath::Lerp(static_cast<float>(Colour.R), static_cast<float>(Dust.R), Much)),
			static_cast<uint8>(FMath::Lerp(static_cast<float>(Colour.G), static_cast<float>(Dust.G), Much)),
			static_cast<uint8>(FMath::Lerp(static_cast<float>(Colour.B), static_cast<float>(Dust.B), Much)), Colour.A));
	}
	else
	{
		Colors.Add(Colour);
	}
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

void FMiniatureMesh::Limb(const FVector& From, const FVector& To, double FromRadius, double ToRadius, const FLinearColor& Colour, int32 Sides, bool bEnds)
{
	const FVector Axis = (To - From).GetSafeNormal();
	FVector Side, Other;
	Axis.FindBestAxisVectors(Side, Other);
	const FColor Paint = Colour.ToFColor(true);
	const double Lean = (FromRadius - ToRadius) / FMath::Max((To - From).Size(), 1.0e-6);
	TArray<int32> Foot, Top;
	for (int32 K = 0; K <= Sides; ++K)
	{
		const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
		const FVector Out = Side * FMath::Cos(Angle) + Other * FMath::Sin(Angle);
		const FVector Normal = (Out + Axis * Lean).GetSafeNormal();
		Foot.Add(Add(From + Out * FromRadius, Normal, Paint));
		Top.Add(Add(To + Out * ToRadius, Normal, Paint));
	}
	for (int32 K = 0; K < Sides; ++K)
	{
		Face(Foot[K], Foot[K + 1], Top[K], Normals[Foot[K]]);
		Face(Foot[K + 1], Top[K + 1], Top[K], Normals[Foot[K]]);
	}
	if (bEnds)
	{
		Disc(From, -Axis, FromRadius, Colour, Sides);
		Disc(To, Axis, ToRadius, Colour, Sides);
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

// ---------------------------------------------------------------------------- shapes for real vehicles

void FMiniatureMesh::Profile(const FVector& Where, double Scale, double Turn, const TArray<FVector2D>& SideOutline, double Right0, double Right1, const FLinearColor& Colour)
{
	const FPlacing P(Where, Scale, Turn);
	const FColor Paint = Colour.ToFColor(true);
	const int32 Count = SideOutline.Num();
	FVector2D Middle = FVector2D::ZeroVector;
	for (const FVector2D& Point : SideOutline)
	{
		Middle += Point / Count;
	}
	const FVector Across = (P.At(0.0, 1.0, 0.0) - P.At(0.0, 0.0, 0.0)).GetSafeNormal();
	// The two flat sides, each a fan round its middle; then the skin between them.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const double Right = Side ? Right1 : Right0;
		const FVector Out = Side ? Across : -Across;
		const int32 Centre = Add(P.At(Middle.X, Right, Middle.Y), Out, Paint);
		TArray<int32> Rim;
		for (const FVector2D& Point : SideOutline)
		{
			Rim.Add(Add(P.At(Point.X, Right, Point.Y), Out, Paint));
		}
		for (int32 K = 0; K < Count; ++K)
		{
			Face(Centre, Rim[K], Rim[(K + 1) % Count], Out);
		}
	}
	const FVector Heart = P.At(Middle.X, (Right0 + Right1) * 0.5, Middle.Y);
	for (int32 K = 0; K < Count; ++K)
	{
		const FVector2D A = SideOutline[K], B = SideOutline[(K + 1) % Count];
		const FVector A0 = P.At(A.X, Right0, A.Y), A1 = P.At(A.X, Right1, A.Y), B0 = P.At(B.X, Right0, B.Y), B1 = P.At(B.X, Right1, B.Y);
		FVector Out = ((B0 - A0) ^ (A1 - A0)).GetSafeNormal();
		if ((Out | ((A0 + B1) * 0.5 - Heart)) < 0.0)
		{
			Out = -Out;
		}
		Quad(A0, B0, B1, A1, Out, Paint);
	}
}

void FMiniatureMesh::Loft(const FVector& Where, double Scale, double Turn, const TArray<FVector2D>& Foot, double FootHeight, const TArray<FVector2D>& Head, double HeadHeight, const FLinearColor& Colour)
{
	const FPlacing P(Where, Scale, Turn);
	const FColor Paint = Colour.ToFColor(true);
	const int32 Count = FMath::Min(Foot.Num(), Head.Num());
	FVector2D FootMiddle = FVector2D::ZeroVector, HeadMiddle = FVector2D::ZeroVector;
	for (int32 K = 0; K < Count; ++K)
	{
		FootMiddle += Foot[K] / Count;
		HeadMiddle += Head[K] / Count;
	}
	const FVector Heart = P.At((FootMiddle.X + HeadMiddle.X) * 0.5, (FootMiddle.Y + HeadMiddle.Y) * 0.5, (FootHeight + HeadHeight) * 0.5);
	// The roof (and the floor, which is seldom seen), then the sides.
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		const TArray<FVector2D>& Plan = Layer ? Head : Foot;
		const FVector2D Middle = Layer ? HeadMiddle : FootMiddle;
		const double Height = Layer ? HeadHeight : FootHeight;
		const FVector Out = Layer ? FVector::UpVector : -FVector::UpVector;
		const int32 Centre = Add(P.At(Middle.X, Middle.Y, Height), Out, Paint);
		TArray<int32> Rim;
		for (int32 K = 0; K < Count; ++K)
		{
			Rim.Add(Add(P.At(Plan[K].X, Plan[K].Y, Height), Out, Paint));
		}
		for (int32 K = 0; K < Count; ++K)
		{
			Face(Centre, Rim[K], Rim[(K + 1) % Count], Out);
		}
	}
	for (int32 K = 0; K < Count; ++K)
	{
		const int32 N = (K + 1) % Count;
		const FVector A0 = P.At(Foot[K].X, Foot[K].Y, FootHeight), B0 = P.At(Foot[N].X, Foot[N].Y, FootHeight);
		const FVector A1 = P.At(Head[K].X, Head[K].Y, HeadHeight), B1 = P.At(Head[N].X, Head[N].Y, HeadHeight);
		FVector Out = ((B0 - A0) ^ (A1 - A0)).GetSafeNormal();
		if ((Out | ((A0 + B1) * 0.5 - Heart)) < 0.0)
		{
			Out = -Out;
		}
		Quad(A0, B0, B1, A1, Out, Paint);
	}
}

void FMiniatureMesh::Stack(const FVector& Where, double Scale, double Turn, const TArray<TArray<FVector2D>>& Plans, const TArray<double>& Heights, const FLinearColor& Colour, bool bSmooth)
{
	const FPlacing P(Where, Scale, Turn);
	const FColor Paint = Colour.ToFColor(true);
	const int32 Layers = FMath::Min(Plans.Num(), Heights.Num());
	if (Layers < 2)
	{
		return;
	}
	const int32 Count = Plans[0].Num();
	TArray<FVector2D> Middles;
	for (int32 L = 0; L < Layers; ++L)
	{
		FVector2D Middle = FVector2D::ZeroVector;
		for (const FVector2D& Point : Plans[L])
		{
			Middle += Point / Count;
		}
		Middles.Add(Middle);
	}
	// The floor and the roof.
	for (int32 End = 0; End < 2; ++End)
	{
		const int32 L = End ? Layers - 1 : 0;
		const FVector Out = End ? FVector::UpVector : -FVector::UpVector;
		const int32 Centre = Add(P.At(Middles[L].X, Middles[L].Y, Heights[L]), Out, Paint);
		TArray<int32> Rim;
		for (int32 K = 0; K < Count; ++K)
		{
			Rim.Add(Add(P.At(Plans[L][K].X, Plans[L][K].Y, Heights[L]), Out, Paint));
		}
		for (int32 K = 0; K < Count; ++K)
		{
			Face(Centre, Rim[K], Rim[(K + 1) % Count], Out);
		}
	}
	if (!bSmooth)
	{
		// Flat plates, each with its own edge: welded armour.
		for (int32 L = 0; L + 1 < Layers; ++L)
		{
			const FVector Heart = P.At((Middles[L].X + Middles[L + 1].X) * 0.5, (Middles[L].Y + Middles[L + 1].Y) * 0.5, (Heights[L] + Heights[L + 1]) * 0.5);
			for (int32 K = 0; K < Count; ++K)
			{
				const int32 N = (K + 1) % Count;
				const FVector A0 = P.At(Plans[L][K].X, Plans[L][K].Y, Heights[L]), B0 = P.At(Plans[L][N].X, Plans[L][N].Y, Heights[L]);
				const FVector A1 = P.At(Plans[L + 1][K].X, Plans[L + 1][K].Y, Heights[L + 1]), B1 = P.At(Plans[L + 1][N].X, Plans[L + 1][N].Y, Heights[L + 1]);
				FVector Out = ((B0 - A0) ^ (A1 - A0)).GetSafeNormal();
				if ((Out | ((A0 + B1) * 0.5 - Heart)) < 0.0)
				{
					Out = -Out;
				}
				Quad(A0, B0, B1, A1, Out, Paint);
			}
		}
		return;
	}
	// One rounded skin, lit as a curve: cast armour.
	TArray<int32> Shell;
	for (int32 L = 0; L < Layers; ++L)
	{
		const int32 Lower = FMath::Max(L - 1, 0), Upper = FMath::Min(L + 1, Layers - 1);
		const double Rise = Heights[Upper] - Heights[Lower];
		for (int32 K = 0; K < Count; ++K)
		{
			const FVector2D Here = Plans[L][K];
			const FVector2D Along = (Plans[L][(K + 1) % Count] - Plans[L][(K + Count - 1) % Count]).GetSafeNormal();
			FVector2D Out(Along.Y, -Along.X);
			if ((Out | (Here - Middles[L])) < 0.0)
			{
				Out = -Out;
			}
			const double Spread = (Plans[Upper][K] - Middles[Upper]).Size() - (Plans[Lower][K] - Middles[Lower]).Size();
			Shell.Add(Add(P.At(Here.X, Here.Y, Heights[L]), P.Toward(Out.X * Rise, Out.Y * Rise, -Spread), Paint));
		}
	}
	for (int32 L = 0; L + 1 < Layers; ++L)
	{
		for (int32 K = 0; K < Count; ++K)
		{
			const int32 A = Shell[L * Count + K], B = Shell[L * Count + (K + 1) % Count], C = Shell[(L + 1) * Count + K], D = Shell[(L + 1) * Count + (K + 1) % Count];
			const FVector Out = Normals[A] + Normals[D];
			Face(A, B, C, Out);
			Face(B, D, C, Out);
		}
	}
}

void FMiniatureMesh::Disc(const FVector& Center, const FVector& Facing, double Radius, const FLinearColor& Colour, int32 Sides)
{
	const FVector Out = Facing.GetSafeNormal();
	FVector Side, Other;
	Out.FindBestAxisVectors(Side, Other);
	const FColor Paint = Colour.ToFColor(true);
	const int32 Middle = Add(Center, Out, Paint);
	int32 Previous = INDEX_NONE;
	for (int32 K = 0; K <= Sides; ++K)
	{
		const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
		const int32 Rim = Add(Center + (Side * FMath::Cos(Angle) + Other * FMath::Sin(Angle)) * Radius, Out, Paint);
		if (Previous != INDEX_NONE)
		{
			Face(Middle, Previous, Rim, Out);
		}
		Previous = Rim;
	}
}

void FMiniatureMesh::Marking(const FVector& Where, double Radius, const FString& Tag, const FVector& Facing, double Curve)
{
	// Drawn lying flat, then stood up against the surface: its top toward the sky (or toward the front, on a roof).
	FMiniatureMesh Flat;
	Flat.WingMarking(FVector::ZeroVector, Radius, Tag, 0.0);
	const FVector Out = Facing.GetSafeNormal();
	// On a wall its top is toward the sky; on a roof or a wing (or under one) its top is toward the front.
	const FVector Skyward = FMath::Abs(Out.Z) > 0.8 ? FVector(-1.0, 0.0, 0.0) : FVector::UpVector;
	const FVector Top = (Skyward - Out * (Skyward | Out)).GetSafeNormal();
	const FVector Side = Top ^ Out;
	const int32 First = Vertices.Num();
	for (int32 K = 0; K < Flat.Vertices.Num(); ++K)
	{
		const FVector V = Flat.Vertices[K], N = Flat.Normals[K];
		if (Curve > 0.0)
		{
			// Bent round a barrel whose axis runs along the marking's width: up the marking is round the barrel.
			const double Angle = -V.X / Curve;
			const FVector Outward = Out * FMath::Cos(Angle) + Top * FMath::Sin(Angle);
			Vertices.Add(Where - Out * Curve + Outward * (Curve + V.Z) + Side * V.Y);
			Normals.Add(Outward);
		}
		else
		{
			Vertices.Add(Where + Top * -V.X + Side * V.Y + Out * V.Z);
			Normals.Add((Top * -N.X + Side * N.Y + Out * N.Z).GetSafeNormal());
		}
		Colors.Add(Flat.Colors[K]);
	}
	for (const int32 Index : Flat.Triangles)
	{
		Triangles.Add(First + Index);
	}
}

void FMiniatureMesh::Stamp(const FMiniatureMesh& Model, const FVector& Where, double Scale, double Turn)
{
	const double Cos = FMath::Cos(FMath::DegreesToRadians(Turn)), Sin = FMath::Sin(FMath::DegreesToRadians(Turn));
	const int32 First = Vertices.Num();
	Vertices.Reserve(First + Model.Vertices.Num());
	Normals.Reserve(First + Model.Vertices.Num());
	Colors.Reserve(First + Model.Vertices.Num());
	for (int32 K = 0; K < Model.Vertices.Num(); ++K)
	{
		const FVector V = Model.Vertices[K], N = Model.Normals[K];
		Vertices.Add(Where + FVector(V.X * Cos - V.Y * Sin, V.X * Sin + V.Y * Cos, V.Z) * Scale);
		Normals.Add(FVector(N.X * Cos - N.Y * Sin, N.X * Sin + N.Y * Cos, N.Z));
		Colors.Add(Model.Colors[K]);
	}
	Triangles.Reserve(Triangles.Num() + Model.Triangles.Num());
	for (const int32 Index : Model.Triangles)
	{
		Triangles.Add(First + Index);
	}
}

// ---------------------------------------------------------------------------- ships and aircraft

void FMiniatureMesh::Slab(const FVector& Where, double Scale, double Turn, const TArray<FVector2D>& Outline, double Foot, double Top, const FLinearColor& Colour)
{
	const FPlacing P(Where, Scale, Turn);
	const FColor Paint = Colour.ToFColor(true);
	const int32 Count = Outline.Num();
	FVector2D Middle = FVector2D::ZeroVector;
	for (const FVector2D& Point : Outline)
	{
		Middle += Point / Count;
	}
	// The top and the underside, each a fan round the middle; then the sides.
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		const double Height = Layer ? Top : Foot;
		const FVector Out = Layer ? FVector::UpVector : -FVector::UpVector;
		const int32 Centre = Add(P.At(Middle.X, Middle.Y, Height), Out, Paint);
		TArray<int32> Rim;
		for (const FVector2D& Point : Outline)
		{
			Rim.Add(Add(P.At(Point.X, Point.Y, Height), Out, Paint));
		}
		for (int32 K = 0; K < Count; ++K)
		{
			Face(Centre, Rim[K], Rim[(K + 1) % Count], Out);
		}
	}
	for (int32 K = 0; K < Count; ++K)
	{
		const FVector2D A = Outline[K], B = Outline[(K + 1) % Count];
		const FVector LowA = P.At(A.X, A.Y, Foot), LowB = P.At(B.X, B.Y, Foot), HighA = P.At(A.X, A.Y, Top), HighB = P.At(B.X, B.Y, Top);
		const FVector Out = ((LowA + LowB) * 0.5 - P.At(Middle.X, Middle.Y, Foot)).GetSafeNormal2D();
		Quad(LowA, LowB, HighB, HighA, Out, Paint);
	}
}

namespace
{
	/** The plan of a ship's hull, a unit long: a pointed bow, full amidships, a squared stern. */
	TArray<FVector2D> HullPlan(double Beam)
	{
		return { { 0.5, 0.0 }, { 0.3, Beam * 0.42 }, { 0.05, Beam * 0.5 }, { -0.38, Beam * 0.46 }, { -0.5, Beam * 0.3 },
			{ -0.5, -Beam * 0.3 }, { -0.38, -Beam * 0.46 }, { 0.05, -Beam * 0.5 }, { 0.3, -Beam * 0.42 } };
	}
	const FLinearColor DeckPlanks = FLinearColor(FColor::FromHex(TEXT("8E7B5A")));
}

void FMiniatureMesh::Battleship(const FVector& Where, double Length, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Length, Turn);
	const FLinearColor Hull = Moulded(Nation, 0.78f), Upper = Moulded(Nation, 0.92f);
	Slab(Where, Length, Turn, HullPlan(0.17), 0.0, 0.07, Hull);
	Slab(Where, Length, Turn, HullPlan(0.13), 0.07, 0.078, DeckPlanks);
	// The citadel amidships: bridge, funnels, a mast; and the big guns, two turrets forward and one aft.
	Box(P.At(0.0, 0.0, 0.11), FVector(0.26, 0.1, 0.07) * Length, Upper, Turn);
	Box(P.At(0.07, 0.0, 0.18), FVector(0.08, 0.07, 0.08) * Length, Upper, Turn);
	Pillar(P.At(-0.02, 0.0, 0.14), 0.03 * Length, 0.026 * Length, 0.1 * Length, Gunmetal, 8);
	Pillar(P.At(-0.09, 0.0, 0.14), 0.026 * Length, 0.022 * Length, 0.08 * Length, Gunmetal, 8);
	Rod(P.At(0.07, 0.0, 0.2), P.At(0.07, 0.0, 0.34), 0.006 * Length, Gunmetal, 5);
	for (const double Along : { 0.3, 0.2, -0.26 })
	{
		const double Up = Along == 0.2 ? 0.105 : 0.08;
		const double Fore = Along > 0.0 ? 1.0 : -1.0;
		Pillar(P.At(Along, 0.0, Up - 0.002), 0.045 * Length, 0.04 * Length, 0.03 * Length, Upper, 8);
		for (const double Side : { -0.014, 0.014 })
		{
			Rod(P.At(Along + Fore * 0.03, Side, Up + 0.016), P.At(Along + Fore * 0.13, Side, Up + 0.022), 0.006 * Length, Gunmetal, 5);
		}
	}
}

void FMiniatureMesh::Cruiser(const FVector& Where, double Length, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Length, Turn);
	const FLinearColor Hull = Moulded(Nation, 0.78f), Upper = Moulded(Nation, 0.92f);
	Slab(Where, Length, Turn, HullPlan(0.13), 0.0, 0.06, Hull);
	Box(P.At(0.02, 0.0, 0.095), FVector(0.22, 0.075, 0.07) * Length, Upper, Turn);
	Box(P.At(0.09, 0.0, 0.16), FVector(0.06, 0.055, 0.06) * Length, Upper, Turn);
	for (const double Along : { 0.0, -0.08 })
	{
		Pillar(P.At(Along, 0.0, 0.13), 0.024 * Length, 0.02 * Length, 0.09 * Length, Gunmetal, 8);
	}
	for (const double Along : { 0.27, -0.27 })
	{
		const double Fore = Along > 0.0 ? 1.0 : -1.0;
		Pillar(P.At(Along, 0.0, 0.06), 0.034 * Length, 0.03 * Length, 0.026 * Length, Upper, 8);
		Rod(P.At(Along + Fore * 0.02, 0.0, 0.076), P.At(Along + Fore * 0.11, 0.0, 0.082), 0.006 * Length, Gunmetal, 5);
	}
}

void FMiniatureMesh::Destroyer(const FVector& Where, double Length, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Length, Turn);
	const FLinearColor Hull = Moulded(Nation, 0.78f), Upper = Moulded(Nation, 0.92f);
	Slab(Where, Length, Turn, HullPlan(0.12), 0.0, 0.055, Hull);
	Box(P.At(0.12, 0.0, 0.09), FVector(0.14, 0.07, 0.07) * Length, Upper, Turn);
	Pillar(P.At(-0.03, 0.0, 0.055), 0.03 * Length, 0.024 * Length, 0.1 * Length, Gunmetal, 8);
	Pillar(P.At(-0.15, 0.0, 0.055), 0.026 * Length, 0.02 * Length, 0.08 * Length, Gunmetal, 8);
	Rod(P.At(0.3, 0.0, 0.07), P.At(0.4, 0.0, 0.08), 0.008 * Length, Gunmetal, 5);
	Rod(P.At(-0.32, 0.0, 0.07), P.At(-0.42, 0.0, 0.08), 0.008 * Length, Gunmetal, 5);
}

void FMiniatureMesh::Carrier(const FVector& Where, double Length, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Length, Turn);
	const FLinearColor Hull = Moulded(Nation, 0.78f), Upper = Moulded(Nation, 0.92f);
	Slab(Where, Length, Turn, HullPlan(0.16), 0.0, 0.085, Hull);
	// The flight deck, wider and longer than the hull under it, and the island to one side.
	Slab(Where, Length, Turn, { { 0.5, 0.07 }, { 0.5, -0.07 }, { -0.5, -0.085 }, { -0.5, 0.085 } }, 0.085, 0.1, DeckPlanks);
	Box(P.At(0.06, 0.075, 0.14), FVector(0.14, 0.035, 0.08) * Length, Upper, Turn);
	Pillar(P.At(0.01, 0.075, 0.18), 0.02 * Length, 0.018 * Length, 0.06 * Length, Gunmetal, 8);
	Box(P.At(0.0, 0.0, 0.101), FVector(0.86, 0.008, 0.002) * Length, FLinearColor(FColor::FromHex(TEXT("E8E2CE"))), Turn); // the centre line
}

void FMiniatureMesh::Submarine(const FVector& Where, double Length, const FLinearColor& Nation, double Turn)
{
	const FPlacing P(Where, Length, Turn);
	const FLinearColor Hull = Moulded(Nation, 0.6f);
	// Awash: a long low hull, the conning tower, and a gun on the casing.
	Slab(Where, Length, Turn, HullPlan(0.11), 0.0, 0.035, Hull);
	Box(P.At(0.03, 0.0, 0.075), FVector(0.14, 0.05, 0.08) * Length, Hull, Turn);
	Rod(P.At(0.05, 0.0, 0.11), P.At(0.05, 0.0, 0.2), 0.006 * Length, Gunmetal, 5);
	Rod(P.At(0.2, 0.0, 0.05), P.At(0.28, 0.0, 0.06), 0.008 * Length, Gunmetal, 5);
}

void FMiniatureMesh::WingMarking(const FVector& Where, double Radius, const FString& Tag, double Turn)
{
	auto Paint = [](const TCHAR* Hex) { return FLinearColor(FColor::FromHex(Hex)); };
	const FLinearColor White = Paint(TEXT("F2F0E8")), Red = Paint(TEXT("C8102E")), Blue = Paint(TEXT("1F3A93")), Black = Paint(TEXT("141414")),
		Yellow = Paint(TEXT("F2C200")), Green = Paint(TEXT("1E8A44")), Orange = Paint(TEXT("F47B20"));
	// Each layer lies a hair above the one before, so they do not fight for the same place.
	int32 Layer = 0;
	const double Hair = Radius * 0.012;
	auto Disc = [&](double Size, const FLinearColor& Colour)
	{
		TArray<FVector2D> Outline;
		for (int32 K = 0; K < 18; ++K)
		{
			Outline.Add(FVector2D(FMath::Cos(K * UE_DOUBLE_TWO_PI / 18.0), FMath::Sin(K * UE_DOUBLE_TWO_PI / 18.0)) * Size);
		}
		Slab(Where, Radius, Turn, Outline, Layer * Hair / Radius, (Layer + 1) * Hair / Radius, Colour);
		++Layer;
	};
	auto Star = [&](double Size, const FLinearColor& Colour, int32 Points = 5, double Inner = 0.4)
	{
		TArray<FVector2D> Outline;
		for (int32 K = 0; K < Points * 2; ++K)
		{
			const double Angle = K * UE_DOUBLE_PI / Points, R = (K % 2 ? Size * Inner : Size);
			Outline.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
		}
		Slab(Where, Radius, Turn, Outline, Layer * Hair / Radius, (Layer + 1) * Hair / Radius, Colour);
		++Layer;
	};
	auto Bar = [&](double X0, double Y0, double X1, double Y1, const FLinearColor& Colour)
	{
		Slab(Where, Radius, Turn, { { X0, Y0 }, { X1, Y0 }, { X1, Y1 }, { X0, Y1 } }, Layer * Hair / Radius, (Layer + 1) * Hair / Radius, Colour);
	};
	auto Cross = [&](double Arm, double HalfWidth, const FLinearColor& Colour)
	{
		Bar(-Arm, -HalfWidth, Arm, HalfWidth, Colour);
		Bar(-HalfWidth, -Arm, HalfWidth, Arm, Colour);
		++Layer;
	};
	auto Rings = [&](const FLinearColor& Outer, const FLinearColor& Middle, const FLinearColor& Centre)
	{
		Disc(1.0, Outer);
		Disc(0.66, Middle);
		Disc(0.33, Centre);
	};

	if (Tag == TEXT("UK") || Tag == TEXT("CAN") || Tag == TEXT("AUS") || Tag == TEXT("NZL") || Tag == TEXT("SAF") || Tag == TEXT("IND"))
	{
		Rings(Blue, White, Red);
	}
	else if (Tag == TEXT("FRA") || Tag == TEXT("VIC") || Tag == TEXT("FFR"))
	{
		Rings(Red, White, Blue);
	}
	else if (Tag == TEXT("USA"))
	{
		Disc(1.0, Paint(TEXT("1B2A5C")));
		Star(0.95, White);
		Disc(0.2, Red); // the red centre, carried until the spring of 1942
	}
	else if (Tag == TEXT("USA-ARMY"))
	{
		Star(1.0, White); // the plain white star of the army's vehicles
	}
	else if (Tag == TEXT("UK-FLASH"))
	{
		// The red, white and red recognition flash of British tanks.
		Bar(-0.62, -1.0, 0.62, -0.34, Red);
		Bar(-0.62, -0.34, 0.62, 0.34, White);
		Bar(-0.62, 0.34, 0.62, 1.0, Red);
		++Layer;
	}
	else if (Tag == TEXT("USA-BAR"))
	{
		// The star and bar carried from 1943: a white star on a blue disc, a white bar each side, edged in blue.
		const FLinearColor Navy = Paint(TEXT("1B2A5C"));
		Bar(-0.3, -2.05, 0.3, 2.05, Navy);
		++Layer;
		Bar(-0.24, -1.98, 0.24, 1.98, White);
		++Layer;
		Disc(1.0, Navy);
		Star(0.95, White);
	}
	else if (Tag == TEXT("UK-A1"))
	{
		// The fuselage roundel of 1940, with its yellow outer ring.
		Disc(1.0, Yellow);
		Disc(0.8, Blue);
		Disc(0.52, White);
		Disc(0.26, Red);
	}
	else if (Tag == TEXT("UK-B"))
	{
		// The roundel of the upper wings: red on blue, with no white to catch the eye from above.
		Disc(1.0, Blue);
		Disc(0.42, Red);
	}
	else if (Tag == TEXT("UK-FIN"))
	{
		// The fin flash: red toward the nose, white, blue.
		Bar(-0.75, -1.0, 0.75, -0.34, Blue);
		Bar(-0.75, -0.34, 0.75, 0.34, White);
		Bar(-0.75, 0.34, 0.75, 1.0, Red);
		++Layer;
	}
	else if (Tag == TEXT("WHITE-DISC"))
	{
		Disc(1.0, White); // the plain white disc painted on tanks for friendly aircraft to see
	}
	else if (Tag == TEXT("SOV-ARMY"))
	{
		Star(1.0, Paint(TEXT("B01818")));
	}
	else if (Tag == TEXT("SOV"))
	{
		Star(1.0, White);
		Star(0.8, Paint(TEXT("CC0000")));
	}
	else if (Tag == TEXT("JAP-PLAIN"))
	{
		Disc(1.0, Paint(TEXT("BC002D"))); // the sun disc with no white edge, as on the pale grey fighters
	}
	else if (Tag == TEXT("JAP") || Tag == TEXT("JPN"))
	{
		Disc(1.0, White);
		Disc(0.84, Paint(TEXT("BC002D")));
	}
	else if (Tag == TEXT("GER"))
	{
		// The straight-armed cross: black, edged in white.
		Cross(1.0, 0.36, White);
		Cross(0.94, 0.19, Black);
	}
	else if (Tag == TEXT("ITA"))
	{
		Rings(Red, White, Green);
	}
	else if (Tag == TEXT("POL"))
	{
		// The chequer: red and white squares, edged the other way about.
		Bar(-1.0, -1.0, 1.0, 1.0, White);
		++Layer;
		Bar(-1.0, -1.0, 0.0, 0.0, Red);
		Bar(0.0, 0.0, 1.0, 1.0, Red);
		++Layer;
	}
	else if (Tag == TEXT("BEL")) Rings(Red, Yellow, Black);
	else if (Tag == TEXT("NED"))
	{
		Rings(Red, White, Blue);
		Disc(0.16, Orange);
	}
	else if (Tag == TEXT("ROM")) Rings(Red, Yellow, Blue);
	else if (Tag == TEXT("YUG")) Rings(Blue, White, Red);
	else if (Tag == TEXT("GRE")) Rings(Paint(TEXT("0D5EAF")), White, Paint(TEXT("0D5EAF")));
	else if (Tag == TEXT("ESP")) Rings(Red, Yellow, Red);
	else if (Tag == TEXT("HUN")) Rings(Red, White, Green);
	else if (Tag == TEXT("BUL")) Rings(Red, Green, White);
	else if (Tag == TEXT("NOR")) Rings(Red, White, Blue);
	else if (Tag == TEXT("DEN")) Rings(Red, White, Red);
	else if (Tag == TEXT("SWE"))
	{
		Disc(1.0, Yellow);
		Disc(0.82, Paint(TEXT("006AA7")));
	}
	else if (Tag == TEXT("FIN"))
	{
		Disc(1.0, White);
		Disc(0.7, Paint(TEXT("003580")));
		Disc(0.4, White);
	}
	else if (Tag == TEXT("SUI"))
	{
		Disc(1.0, Paint(TEXT("DA291C")));
		Cross(0.62, 0.19, White);
	}
	else if (Tag == TEXT("POR"))
	{
		Disc(1.0, White);
		Cross(0.8, 0.2, Red);
	}
	else if (Tag == TEXT("TUR"))
	{
		Disc(1.0, White);
		Disc(0.8, Paint(TEXT("E30A17")));
	}
	else if (Tag == TEXT("CHN") || Tag == TEXT("CHI"))
	{
		Disc(1.0, Paint(TEXT("0A1C8C")));
		Star(0.82, White, 12, 0.6);
	}
	else if (Tag == TEXT("IRL")) Rings(Green, White, Orange);
	else
	{
		Disc(1.0, White);
		Disc(0.6, Paint(TEXT("8A8A8A")));
	}
}

void FMiniatureMesh::Aeroplane(const FVector& Where, double Span, bool bBomber, const FLinearColor& Nation, const FString& Tag, double Turn)
{
	const FPlacing P(Where, Span, Turn);
	const FLinearColor Paintwork = Moulded(Nation, 0.86f), Under = Moulded(Nation, 0.6f);
	const double Body = bBomber ? 0.085 : 0.07, Long = bBomber ? 0.78 : 0.68;
	// The fuselage, tapering to the tail; the wing; tailplane and fin; a canopy; and the engines.
	Rod(P.At(Long * 0.42, 0.0, 0.0), P.At(-Long * 0.1, 0.0, 0.0), Body * Span, Paintwork, 8);
	Rod(P.At(-Long * 0.1, 0.0, 0.0), P.At(-Long * 0.58, 0.0, 0.02), Body * 0.55 * Span, Paintwork, 8);
	Slab(Where, Span, Turn, { { 0.16, 0.0 }, { 0.1, 0.5 }, { -0.02, 0.5 }, { -0.1, 0.0 }, { -0.02, -0.5 }, { 0.1, -0.5 } }, -0.012, 0.012, Under);
	Slab(Where, Span, Turn, { { -Long * 0.44, 0.0 }, { -Long * 0.48, 0.19 }, { -Long * 0.58, 0.19 }, { -Long * 0.6, 0.0 }, { -Long * 0.58, -0.19 }, { -Long * 0.48, -0.19 } }, 0.012, 0.03, Under);
	Box(P.At(-Long * 0.52, 0.0, 0.1), FVector(0.14, 0.012, 0.15) * Span, Paintwork, Turn);
	Ball(P.At(Long * 0.12, 0.0, Body * 0.9), FVector(0.2, 0.09, 0.08) * Span, FLinearColor(FColor::FromHex(TEXT("9FB6C4"))), 4, 8);
	// The nation's marking on each wing, and smaller on the fin.
	for (const double Side : { -0.34, 0.34 })
	{
		WingMarking(P.At(0.035, Side, 0.0125), 0.1 * Span, Tag, Turn);
	}
	if (bBomber)
	{
		for (const double Side : { -0.2, 0.2 })
		{
			Rod(P.At(0.24, Side, 0.0), P.At(0.02, Side, 0.0), 0.05 * Span, Paintwork, 7);
			Rod(P.At(0.245, Side, 0.0), P.At(0.25, Side, 0.0), 0.12 * Span, Gunmetal, 10); // the airscrew, as a disc
		}
	}
	else
	{
		Rod(P.At(Long * 0.425, 0.0, 0.0), P.At(Long * 0.43, 0.0, 0.0), 0.15 * Span, Gunmetal, 10);
	}
}

void FMiniatureMesh::Fleet(int32 Battleships, int32 Carriers, int32 Cruisers, int32 Destroyers, int32 Submarines, double Unit, const FLinearColor& /*Nation*/, const FString& Tag)
{
	// The three kinds of ship most worth showing: the big ships first.
	TArray<const TCHAR*> Show;
	if (Battleships > 0) Show.Add(TEXT("Battleship:"));
	if (Carriers > 0) Show.Add(TEXT("Carrier:"));
	if (Cruisers > 0) Show.Add(TEXT("Cruiser:"));
	if (Destroyers > 0) Show.Add(TEXT("Destroyer:"));
	if (Submarines > 0) Show.Add(TEXT("Submarine:"));
	Show.SetNum(FMath::Min(Show.Num(), 3));
	if (Show.Num() == 1 && Submarines > 0)
	{
		Show.Add(TEXT("Submarine:")); // a submarine arm is a pack of boats
	}
	// The ships are in scale with one another: a real metre of ship is this much of the map, which makes a
	// battleship about as long as two army pieces side by side. They lie in echelon, bows to the right as the
	// eye sees them, the biggest nearest.
	const double Metre = 0.066 * Unit;
	for (int32 K = 0; K < Show.Num(); ++K)
	{
		const FVector Where((K - (Show.Num() - 1) * 0.5) * 4.2 * Unit, (K % 2 ? 2.4 : -1.8) * Unit, 0.02 * Unit);
		Vehicle(FString(Show[K]) + Tag, Where, Metre, -90.0 + (K - 1) * 4.0);
	}
}

void FMiniatureMesh::AirFleet(int32 Fighters, int32 Bombers, double Unit, const FLinearColor& Nation, const FString& Tag)
{
	// A patch of airfield: mown grass with a runway across it.
	Pillar(FVector(0.0, 0.0, 0.05 * Unit), 5.0 * Unit, 4.9 * Unit, 0.14 * Unit, FLinearColor(FColor::FromHex(TEXT("70804F"))), 28);
	Box(FVector(0.0, 0.0, 0.2 * Unit), FVector(1.2, 9.0, 0.03) * Unit, FLinearColor(FColor::FromHex(TEXT("8E8A80"))), 28.0);
	// The aircraft fly over it on thin stands, to the same scale as the soldiers and tanks on the army pieces
	// (a real metre is the same size on both): a bomber higher and behind, a fighter lower and to the fore.
	// A nation with no model of its own yet flies a plain machine of a likely size.
	const double Metre = 4.3 * Unit / 3.7;
	if (Bombers > 0)
	{
		const FVector At(4.0 * Unit, (Fighters > 0 ? 3.0 : 0.0) * Unit, 9.5 * Unit);
		Rod(FVector(At.X, At.Y, 0.2 * Unit), At, 0.06 * Unit, Gunmetal, 5);
		if (!Vehicle(MainBomberOf(Tag), At, Metre, 68.0))
		{
			Aeroplane(At, 22.0 * Metre, true, Nation, Tag, 68.0);
		}
	}
	if (Fighters > 0)
	{
		const FVector At(-2.0 * Unit, (Bombers > 0 ? -3.5 : 0.0) * Unit, 4.2 * Unit);
		Rod(FVector(At.X, At.Y, 0.2 * Unit), At, 0.05 * Unit, Gunmetal, 5);
		if (!Vehicle(MainFighterOf(Tag), At, Metre, 62.0))
		{
			Aeroplane(At, 10.5 * Metre, false, Nation, Tag, 62.0);
		}
	}
}

// ---------------------------------------------------------------------------- scenery

void FMiniatureMesh::House(const FVector& Where, double Length, double Width, double WallHeight, double RoofHeight, const FLinearColor& Wall, const FLinearColor& Roof, double Turn)
{
	const double Cos = FMath::Cos(FMath::DegreesToRadians(Turn)), Sin = FMath::Sin(FMath::DegreesToRadians(Turn));
	auto At = [&](double Along, double Across, double Up)
	{
		return Where + FVector(Along * Cos - Across * Sin, Along * Sin + Across * Cos, Up);
	};
	Box(Where + FVector(0.0, 0.0, WallHeight * 0.5), FVector(Length, Width, WallHeight), Wall, Turn);
	// The roof overhangs the walls a little.
	const double L = Length * 0.54, W = Width * 0.58;
	const FColor Tiles = Roof.ToFColor(true);
	const FVector RidgeA = At(-L, 0.0, WallHeight + RoofHeight), RidgeB = At(L, 0.0, WallHeight + RoofHeight);
	const FVector Along = (RidgeB - RidgeA).GetSafeNormal();
	const FVector Across = FVector(-Along.Y, Along.X, 0.0);
	Quad(At(-L, W, WallHeight), At(L, W, WallHeight), RidgeB, RidgeA, Across * RoofHeight + FVector(0.0, 0.0, W), Tiles);
	Quad(At(-L, -W, WallHeight), At(L, -W, WallHeight), RidgeB, RidgeA, -Across * RoofHeight + FVector(0.0, 0.0, W), Tiles);
	const FColor Gable = Wall.ToFColor(true);
	for (const double End : { -1.0, 1.0 })
	{
		const FVector Out = Along * End;
		const int32 A = Add(At(End * L, W, WallHeight), Out, Gable), B = Add(At(End * L, -W, WallHeight), Out, Gable), C = Add(End < 0.0 ? RidgeA : RidgeB, Out, Gable);
		Face(A, B, C, Out);
	}
}

void FMiniatureMesh::Fir(const FVector& Where, double Height, double Radius, const FLinearColor& Leaf)
{
	Pillar(Where + FVector(0.0, 0.0, Height * 0.12), Radius, 0.0, Height * 0.88, Leaf, 6);
}

void FMiniatureMesh::Broadleaf(const FVector& Where, double Height, double Radius, const FLinearColor& Leaf)
{
	Ball(Where + FVector(0.0, 0.0, Height * 0.58), FVector(Radius * 2.0, Radius * 2.0, Height * 0.84), Leaf, 4, 6);
}

void FMiniatureMesh::Town(const FVector& Where, double Unit, int32 Houses, bool bCapital, bool bCity, int32 Works, int32 Seed)
{
	FRandomStream Random(Seed * 7919 + 13);
	static const TCHAR* WallPaints[] = { TEXT("E6DCC4"), TEXT("D8C8A6"), TEXT("CEC6B6"), TEXT("E2D2AE"), TEXT("C9B79A") };
	static const TCHAR* RoofPaints[] = { TEXT("9A4A32"), TEXT("7C3F2E"), TEXT("5E5A58"), TEXT("8B5A3C"), TEXT("A65B3A") };
	const FLinearColor StoneWork(FColor::FromHex(TEXT("D9D2C0")));
	const FLinearColor Copper(FColor::FromHex(TEXT("6FA08C")));
	const FLinearColor Brick(FColor::FromHex(TEXT("7A5547")));
	const FLinearColor Soot(FColor::FromHex(TEXT("3B3532")));

	// The great building in the middle: a domed palace for a capital, a church with a tower for a city.
	double Clear = 0.0;
	if (bCapital)
	{
		Box(Where + FVector(0.0, 0.0, 0.45 * Unit), FVector(1.5, 2.3, 0.9) * Unit, StoneWork);
		Pillar(Where + FVector(0.0, 0.0, 0.9 * Unit), 0.5 * Unit, 0.5 * Unit, 0.35 * Unit, StoneWork, 12);
		Ball(Where + FVector(0.0, 0.0, 1.25 * Unit), FVector(1.0, 1.0, 0.9) * Unit, Copper, 5, 10);
		Pillar(Where + FVector(0.0, 0.0, 1.65 * Unit), 0.06 * Unit, 0.0, 0.5 * Unit, FLinearColor(FColor::FromHex(TEXT("C9A227"))), 6);
		Clear = 1.5 * Unit;
	}
	else if (bCity)
	{
		House(Where, 1.5 * Unit, 0.7 * Unit, 0.6 * Unit, 0.45 * Unit, StoneWork, FLinearColor(FColor::FromHex(TEXT("5E5A58"))), 0.0);
		Box(Where + FVector(0.0, 0.95 * Unit, 0.7 * Unit), FVector(0.5, 0.5, 1.4) * Unit, StoneWork);
		Pillar(Where + FVector(0.0, 0.95 * Unit, 1.4 * Unit), 0.36 * Unit, 0.0, 0.8 * Unit, FLinearColor(FColor::FromHex(TEXT("5E5A58"))), 4);
		Clear = 1.2 * Unit;
	}

	// Houses round it, closer packed toward the middle, each turned its own way.
	const double Spread = (0.9 + 0.36 * FMath::Sqrt(static_cast<double>(Houses))) * Unit;
	for (int32 K = 0; K < Houses; ++K)
	{
		const double Angle = K * 2.39996 + Random.FRandRange(-0.3f, 0.3f); // the golden angle scatters them evenly
		const double Distance = Clear + Spread * FMath::Sqrt((K + 0.6) / Houses);
		const FVector At = Where + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Distance;
		const double Length = Random.FRandRange(0.62f, 0.95f) * Unit, Width = Random.FRandRange(0.42f, 0.58f) * Unit;
		House(At, Length, Width, Random.FRandRange(0.3f, 0.5f) * Unit, Random.FRandRange(0.22f, 0.34f) * Unit,
			FLinearColor(FColor::FromHex(WallPaints[Random.RandRange(0, 4)])), FLinearColor(FColor::FromHex(RoofPaints[Random.RandRange(0, 4)])), Random.FRandRange(0.0f, 180.0f));
	}

	// The works, out at the edge: a long shed with a chimney.
	for (int32 K = 0; K < Works; ++K)
	{
		const double Angle = 0.9 + K * 2.1;
		const FVector At = Where + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * (Clear + Spread + 0.9 * Unit);
		const double Turn = FMath::RadiansToDegrees(Angle) + 90.0;
		House(At, 1.5 * Unit, 0.8 * Unit, 0.5 * Unit, 0.22 * Unit, Brick, Soot, Turn);
		Pillar(At + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * 0.6 * Unit, 0.13 * Unit, 0.09 * Unit, 1.5 * Unit, Brick, 7);
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

FLinearColor FMiniatureMesh::UniformColour(const FString& Tag)
{
	static const TMap<FString, const TCHAR*> Cloth = {
		{ TEXT("GER"), TEXT("5F665C") }, // field grey
		{ TEXT("USA"), TEXT("6B6A45") }, // olive drab
		{ TEXT("UK"), TEXT("8B7B52") }, // khaki serge
		{ TEXT("CAN"), TEXT("8B7B52") }, { TEXT("AUS"), TEXT("8B7B52") }, { TEXT("NZL"), TEXT("8B7B52") }, { TEXT("SAF"), TEXT("8B7B52") }, { TEXT("IND"), TEXT("8B7B52") },
		{ TEXT("SOV"), TEXT("7B7748") }, // khaki-green
		{ TEXT("JAP"), TEXT("9A8450") }, { TEXT("JPN"), TEXT("9A8450") }, // mustard khaki
		{ TEXT("FRA"), TEXT("7A7250") }, { TEXT("VIC"), TEXT("7A7250") }, { TEXT("FFR"), TEXT("7A7250") }, // the khaki of 1940
		{ TEXT("ITA"), TEXT("73806C") }, // grey-green
		{ TEXT("POL"), TEXT("7C7450") }, { TEXT("BEL"), TEXT("857A56") }, { TEXT("NED"), TEXT("6E7B6A") }, // Dutch field grey-green
		{ TEXT("FIN"), TEXT("767B76") }, { TEXT("SWE"), TEXT("767C78") }, { TEXT("NOR"), TEXT("70786E") }, { TEXT("DEN"), TEXT("7E7A5C") },
		{ TEXT("HUN"), TEXT("7C7650") }, { TEXT("ROM"), TEXT("7E7852") }, { TEXT("BUL"), TEXT("7A6E52") }, { TEXT("YUG"), TEXT("77775A") },
		{ TEXT("GRE"), TEXT("7E7A52") }, { TEXT("TUR"), TEXT("7E7654") }, { TEXT("ESP"), TEXT("7E7850") }, { TEXT("POR"), TEXT("76786A") },
		{ TEXT("SUI"), TEXT("6C746C") }, { TEXT("SVK"), TEXT("7A7452") }, { TEXT("CHN"), TEXT("7F8A96") }, // the blue-grey of the Chinese armies
	};
	const TCHAR* const* Found = Cloth.Find(Tag);
	return FLinearColor(FColor::FromHex(Found ? *Found : TEXT("807A56")));
}

FLinearColor FMiniatureMesh::VehicleColour(const FString& Tag)
{
	static const TMap<FString, const TCHAR*> Paint = {
		{ TEXT("GER"), TEXT("4E5459") }, // tank grey
		{ TEXT("USA"), TEXT("5A5B3C") }, // olive drab
		{ TEXT("UK"), TEXT("65704E") }, { TEXT("CAN"), TEXT("65704E") }, { TEXT("AUS"), TEXT("65704E") }, { TEXT("IND"), TEXT("65704E") },
		{ TEXT("SOV"), TEXT("4F5E3E") }, // protective green
		{ TEXT("JAP"), TEXT("7A6A42") }, { TEXT("JPN"), TEXT("7A6A42") },
		{ TEXT("FRA"), TEXT("5E6A48") }, { TEXT("VIC"), TEXT("5E6A48") },
		{ TEXT("ITA"), TEXT("8A7F5E") }, // sand
		{ TEXT("POL"), TEXT("66684A") },
	};
	const TCHAR* const* Found = Paint.Find(Tag);
	return FLinearColor(FColor::FromHex(Found ? *Found : TEXT("60664A")));
}

void FMiniatureMesh::Force(const TArray<FString>& Kinds, double BaseRadius, double Top, double /*Unit*/, const FLinearColor& /*NationColour*/, const FString& Tag)
{
	// Everything on the base is to one scale, the scale of the real things: a tank is as long beside a man as a
	// real tank is. The base is taken to be a little over seven metres across: room for one tank, or a dozen men in ranks. The figures wear what the
	// nation's soldiers wore and its vehicles their wartime paint; the base under them keeps the nation's colour
	// on the map, so the two together say whose they are.
	const double Metre = BaseRadius / 3.7;
	const FLinearColor Nation = UniformColour(Tag) * 1.25f;
	const FLinearColor Vehicles = VehicleColour(Tag) * 1.3f;
	int32 Armour = 0, Guns = 0, Motor = 0, Horse = 0, Mountain = 0, Fort = 0, Foot = 0, Sappers = 0;
	for (const FString& Kind : Kinds)
	{
		if (Kind == TEXT("arm")) ++Armour;
		else if (Kind == TEXT("art")) ++Guns;
		else if (Kind == TEXT("mot")) ++Motor;
		else if (Kind == TEXT("cav")) ++Horse;
		else if (Kind == TEXT("mtn")) ++Mountain;
		else if (Kind == TEXT("fort")) ++Fort;
		else if (Kind == TEXT("eng")) ++Sappers;
		else ++Foot;
	}
	// A place on the base, in metres from its middle: so far toward the back, so far to the right as it is looked at.
	auto On = [Top, Metre](double Back, double Right) { return FVector(Back * Metre, Right * Metre, Top); };

	// One big thing takes the middle of the base: a tank before a gun before a lorry before a bunker.
	int32 Heavy = INDEX_NONE; // 0 tank, 1 gun, 2 lorry, 3 bunker
	if (Armour > 0) Heavy = 0;
	else if (Guns > 0) Heavy = 1;
	else if (Motor > 0) Heavy = 2;
	else if (Fort > 0) Heavy = 3;
	else if (Sappers > 0 && Foot + Mountain + Horse == 0) Heavy = 4; // engineers alone stand by their bridging gear
	const bool bHeavy = Heavy != INDEX_NONE;
	if (bHeavy)
	{
		// Turned to be seen three-quarters on.
		const FVector Where = On(-0.2, 0.45);
		const double Turn = -24.0;
		switch (Heavy)
		{
		case 0:
			// The nation's own tank if there is a model of it, or a plain one of the same size in the nation's paint.
			if (!MainTank(Where, Metre, Tag, Turn, false))
			{
				Tank(Where, Metre * 2.3, Vehicles, Turn);
			}
			break;
		case 1:
			if (!Vehicle(MainGunOf(Tag), Where, Metre, Turn, false))
			{
				FieldGun(Where, Metre * 2.0, Vehicles, Turn);
			}
			break;
		case 2:
			if (!Vehicle(MainCarrierOf(Tag), Where, Metre, Turn, false))
			{
				Lorry(Where, Metre * 2.4, Vehicles, Turn);
			}
			break;
		case 3: Bunker(Where, Metre * 2.0, Nation, Turn); break;
		default:
			// The engineers' bridging gear: a pontoon on its side timbers, and a stack of decking beside it.
			Ball(Where + FVector(0.0, 0.0, 0.55 * Metre), FVector(5.6, 1.7, 0.9) * Metre, FLinearColor(FColor::FromHex(TEXT("6A7068"))), 5, 12);
			Box(Where + FVector(0.0, 0.0, 0.1 * Metre), FVector(1.2, 1.9, 0.2) * Metre, FLinearColor(FColor::FromHex(TEXT("7A5C3A"))), Turn);
			Box(Where + FVector(1.9 * Metre, 0.9 * Metre, 0.25 * Metre), FVector(2.6, 0.9, 0.5) * Metre, FLinearColor(FColor::FromHex(TEXT("8A6A44"))), Turn + 8.0);
			break;
		}
	}

	// The men fill what room is left. With nothing big on the base they stand in three ranks of four;
	// otherwise a few stand about it.
	TArray<FVector2D> Places;
	if (!bHeavy)
	{
		for (int32 Rank = 0; Rank < 3; ++Rank)
		{
			for (int32 File = 0; File < 4; ++File)
			{
				Places.Add(FVector2D(-1.7 + Rank * 1.35, (File - 1.5) * 1.2 + (Rank % 2 ? 0.25 : 0.0)));
			}
		}
	}
	else if (Heavy == 0 || Heavy == 2)
	{
		Places = { { -2.3, -2.3 }, { -0.9, -3.1 }, { 2.9, 1.6 }, { 3.2, 0.3 } };
	}
	else
	{
		Places = { { 1.6, -1.2 }, { 1.9, 1.4 }, { -2.3, -2.3 }, { 2.9, 0.2 }, { -0.9, -3.1 } };
	}
	int32 Slot = 0;
	if (Mountain > 0)
	{
		// Mountain troops stand by a rock, at the back.
		Crag(On(2.3, 2.3), Metre * 1.6);
	}
	if (Horse > 0 && !bHeavy)
	{
		// Cavalry: troopers on their horses in the front, three for one corps and five for more; any foot stand behind.
		const TArray<FVector2D> Mounts = { { -1.5, -2.0 }, { -1.5, 0.0 }, { -1.5, 2.0 }, { 1.0, -1.0 }, { 1.0, 1.0 } };
		const int32 Riders = Horse > 1 || Foot + Mountain == 0 ? 5 : 3;
		for (int32 K = 0; K < Riders; ++K)
		{
			Rider(Tag, On(Mounts[K].X, Mounts[K].Y), Metre, ((K * 7) % 5 - 2) * 4.0, false);
		}
		Places = Riders == 5 ? TArray<FVector2D>{ { 2.9, -1.8 }, { 2.9, -0.6 }, { 2.9, 0.6 }, { 2.9, 1.8 } } : TArray<FVector2D>{ { 0.8, -1.8 }, { 0.8, -0.6 }, { 0.8, 0.6 }, { 0.8, 1.8 }, { 2.2, -1.2 }, { 2.2, 0.0 }, { 2.2, 1.2 } };
	}
	// Riflemen for the corps on foot: two ranks for one corps, all three for more. A tank or a gun has a few
	// men about it whatever else the army holds.
	int32 Men = FMath::Min(bHeavy ? 2 + 2 * (Foot + Mountain) : 4 + 4 * (Foot + Mountain), Places.Num() - Slot);
	if (!bHeavy && Foot + Mountain == 0 && Horse > 0)
	{
		Men = 0; // cavalry alone
	}
	if (Heavy == 4)
	{
		Men = FMath::Min(Places.Num() - Slot, 5); // the engineers themselves
	}
	for (int32 K = 0; K < Men && Places.IsValidIndex(Slot); ++K, ++Slot)
	{
		if (Heavy == 4)
		{
			Pioneer(Tag, On(Places[Slot].X, Places[Slot].Y), Metre, ((K * 7) % 5 - 2) * 5.0, false);
		}
		else
		{
			Soldier(Tag, On(Places[Slot].X, Places[Slot].Y), Metre, ((K * 7) % 5 - 2) * 5.0, false);
		}
	}
}
