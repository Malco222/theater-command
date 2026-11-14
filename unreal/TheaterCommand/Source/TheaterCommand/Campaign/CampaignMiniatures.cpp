#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignMiniatureParts.h"
#include "TheaterCommand.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
			static_cast<uint8>(FMath::Lerp(static_cast<float>(Colour.B), static_cast<float>(Dust.B), Much)), Finish));
	}
	else
	{
		Colors.Add(FColor(Colour.R, Colour.G, Colour.B, Finish));
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

void FMiniatureMesh::MarkWear(bool bAll)
{
	if (Vertices.Num() == 0)
	{
		return;
	}
	const FBox Bounds(Vertices);
	const double Long = FMath::Max(Bounds.GetSize().X, 0.01), HalfWide = FMath::Max(FMath::Max(FMath::Abs(Bounds.Min.Y), FMath::Abs(Bounds.Max.Y)), 0.01), High = FMath::Max(Bounds.Max.Z, 0.01);
	for (int32 K = 0; K < Vertices.Num(); ++K)
	{
		if (!bAll && Colors[K].A != 128)
		{
			continue;
		}
		const FVector& V = Vertices[K];
		// The nose (the vehicle faces -X), the track guards, and what lies flat near the top: hatches and roof edges.
		const double Nose = 0.9 * FMath::SmoothStep(0.6, 0.95, (Bounds.Max.X - V.X) / Long);
		const double Guard = FMath::Abs(V.Y) > 0.74 * HalfWide && V.Z > 0.34 * High && V.Z < 0.58 * High ? 0.8 : 0.0;
		const double Hatch = Normals[K].Z > 0.6 && V.Z > 0.7 * High ? 0.75 : 0.0;
		Colors[K].A = static_cast<uint8>(128.0 - 100.0 * FMath::Max3(Nose, Guard, Hatch));
	}
}

void FMiniatureMesh::Numerals(const FVector& Where, double High, int32 Number, const FVector& Facing)
{
	// Each figure is made of up to seven strokes, as a signwriter blocks figures in: a top, a middle, a bottom and
	// four uprights. Laid out flat, then stood against the surface as a marking is. (Seen from outside, the flat
	// sheet's +Y runs to the reader's right and its -X is up.)
	static const uint8 Strokes[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F }; // bits: a top, b upper right, c lower right, d bottom, e lower left, f upper left, g middle
	const FString Text = FString::FromInt(FMath::Abs(Number));
	const double Wide = High * 0.56, Gap = High * 0.2, Thick = High * 0.15, All = Text.Len() * Wide + (Text.Len() - 1) * Gap;
	const FColor White = FLinearColor(FColor::FromHex(TEXT("F2F0E8"))).ToFColor(true);
	FMiniatureMesh Flat;
	auto Stroke = [&](double X0, double Y0, double X1, double Y1)
	{
		// (a rectangle from (X0, Y0) to (X1, Y1) in the figure's own measure: x to the reader's right, y up)
		auto At = [&](double X, double Y) { return FVector(-(Y - High * 0.5), X - All * 0.5, High * 0.02); };
		Flat.Quad(At(X0, Y0), At(X1, Y0), At(X1, Y1), At(X0, Y1), FVector::UpVector, White);
	};
	for (int32 K = 0; K < Text.Len(); ++K)
	{
		const int32 Digit = FMath::Clamp(static_cast<int32>(Text[K] - TEXT('0')), 0, 9);
		const uint8 Bits = Strokes[Digit];
		const double X = K * (Wide + Gap), Mid = High * 0.5;
		if (Bits & 0x01) { Stroke(X, High - Thick, X + Wide, High); }
		if (Bits & 0x02) { Stroke(X + Wide - Thick, Mid, X + Wide, High); }
		if (Bits & 0x04) { Stroke(X + Wide - Thick, 0.0, X + Wide, Mid); }
		if (Bits & 0x08) { Stroke(X, 0.0, X + Wide, Thick); }
		if (Bits & 0x10) { Stroke(X, 0.0, X + Thick, Mid); }
		if (Bits & 0x20) { Stroke(X, Mid, X + Thick, High); }
		if (Bits & 0x40) { Stroke(X, Mid - Thick * 0.5, X + Wide, Mid + Thick * 0.5); }
	}
	const FVector Out = Facing.GetSafeNormal();
	const FVector Skyward = FMath::Abs(Out.Z) > 0.8 ? FVector(-1.0, 0.0, 0.0) : FVector::UpVector;
	const FVector Top = (Skyward - Out * (Skyward | Out)).GetSafeNormal();
	const FVector Side = Top ^ Out;
	const int32 First = Vertices.Num();
	for (int32 K = 0; K < Flat.Vertices.Num(); ++K)
	{
		const FVector V = Flat.Vertices[K];
		Vertices.Add(Where + Top * -V.X + Side * V.Y + Out * V.Z);
		Normals.Add(Out);
		Colors.Add(Flat.Colors[K]);
	}
	// (each stroke is drawn to be seen from either side, since which way round it lies depends on the surface)
	for (int32 K = 0; K + 2 < Flat.Triangles.Num(); K += 3)
	{
		Triangles.Append({ First + Flat.Triangles[K], First + Flat.Triangles[K + 1], First + Flat.Triangles[K + 2], First + Flat.Triangles[K], First + Flat.Triangles[K + 2], First + Flat.Triangles[K + 1] });
	}
}

bool FMiniatureMesh::bTrialTank = false;
bool FMiniatureMesh::bOldPieces = false;

bool FMiniatureMesh::Sculpted(const FString& Name, const FString& Tag, const FVector& Where, double Metre, double Turn, bool bFar)
{
	// The numbers as the script wrote them, read once: points, which way each faces, its paint, and the triangles.
	struct FRaw
	{
		TArray<FVector> Points, Facings;
		TArray<int32> Paints, Triangles;
	};
	static TMap<FString, TSharedPtr<FRaw>> Read; // (a model that is not there is remembered as nothing, and not looked for again)
	static TMap<FString, FMiniatureMesh> Painted;
	const FString Which = Name + (bFar ? TEXT("|far") : TEXT("|near"));
	if (!Read.Contains(Which))
	{
		TSharedPtr<FRaw> Raw;
		FString Text;
		TSharedPtr<FJsonObject> File;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Models") / (Name + TEXT(".json"));
		if (FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), File) && File.IsValid())
		{
			const TSharedPtr<FJsonObject>* Part = nullptr;
			const TArray<TSharedPtr<FJsonValue>> *PointList = nullptr, *FacingList = nullptr, *PaintList = nullptr, *TriangleList = nullptr;
			if (File->TryGetObjectField(bFar ? TEXT("far") : TEXT("near"), Part) && (*Part)->TryGetArrayField(TEXT("points"), PointList) && (*Part)->TryGetArrayField(TEXT("facings"), FacingList)
				&& (*Part)->TryGetArrayField(TEXT("paints"), PaintList) && (*Part)->TryGetArrayField(TEXT("triangles"), TriangleList)
				&& PointList->Num() == PaintList->Num() * 3 && FacingList->Num() == PointList->Num() && TriangleList->Num() % 3 == 0)
			{
				Raw = MakeShared<FRaw>();
				for (int32 K = 0; K + 2 < PointList->Num(); K += 3)
				{
					// (the file has x forward and y to the right; the game's vehicles are built nose toward -X with their right hand at -Y)
					Raw->Points.Add(FVector(-(*PointList)[K]->AsNumber(), -(*PointList)[K + 1]->AsNumber(), (*PointList)[K + 2]->AsNumber()));
					Raw->Facings.Add(FVector(-(*FacingList)[K]->AsNumber(), -(*FacingList)[K + 1]->AsNumber(), (*FacingList)[K + 2]->AsNumber()).GetSafeNormal());
				}
				for (const TSharedPtr<FJsonValue>& Value : *PaintList)
				{
					Raw->Paints.Add(static_cast<int32>(Value->AsNumber()));
				}
				for (const TSharedPtr<FJsonValue>& Value : *TriangleList)
				{
					Raw->Triangles.Add(static_cast<int32>(Value->AsNumber()));
				}
				UE_LOG(LogTheater, Log, TEXT("Sculpted model %s read: %d points, %d triangles"), *Which, Raw->Points.Num(), Raw->Triangles.Num() / 3);
			}
		}
		Read.Add(Which, Raw);
	}
	const TSharedPtr<FRaw> Raw = Read[Which];
	if (!Raw.IsValid() || Raw->Points.Num() == 0)
	{
		return false;
	}
	const FString Key = Which + TEXT("|") + Tag;
	FMiniatureMesh* Model = Painted.Find(Key);
	if (!Model)
	{
		// Painted for the nation: its vehicles' own colour and a darker shade of it, and the same steel, rubber,
		// wood and canvas for everyone. Painted metal is marked as such, so that the pieces' material chips it.
		const FLinearColor Paint = VehicleColour(Tag);
		const FLinearColor Colours[9] = {
			Paint, Paint * 0.7f, FLinearColor(FColor::FromHex(TEXT("3B3733"))), FLinearColor(FColor::FromHex(TEXT("1F1F21"))), FLinearColor(FColor::FromHex(TEXT("7A5C3A"))),
			FLinearColor(FColor::FromHex(TEXT("6C6E72"))), FLinearColor(FColor::FromHex(TEXT("857F5C"))), FLinearColor(FColor::FromHex(TEXT("27313A"))), FLinearColor(FColor::FromHex(TEXT("6B4A32"))) };
		static const bool bMetal[9] = { true, true, true, false, false, true, false, false, true };
		FMiniatureMesh Fresh;
		// (the dust of the road on the tracks and the lower hull, as on the code-built vehicles)
		Fresh.DustFoot = 0.0;
		Fresh.DustTop = 1.0;
		Fresh.DustAmount = 0.85f;
		Fresh.Dust = FColor(124, 102, 74);
		for (int32 K = 0; K < Raw->Points.Num(); ++K)
		{
			const int32 Kind = FMath::Clamp(Raw->Paints[K], 0, 8);
			Fresh.Finish = bMetal[Kind] ? 128 : 255;
			Fresh.Add(Raw->Points[K], Raw->Facings[K], Colours[Kind].ToFColor(true));
		}
		for (int32 K = 0; K + 2 < Raw->Triangles.Num(); K += 3)
		{
			const int32 A = Raw->Triangles[K], B = Raw->Triangles[K + 1], C = Raw->Triangles[K + 2];
			if (Raw->Points.IsValidIndex(A) && Raw->Points.IsValidIndex(B) && Raw->Points.IsValidIndex(C))
			{
				Fresh.Face(A, B, C, Raw->Facings[A] + Raw->Facings[B] + Raw->Facings[C]);
			}
		}
		// Its markings: the nation's own on each side of the turret, and a number behind it.
		{
			const FBox Bounds(Fresh.Vertices);
			const double TurretUp = Bounds.Max.Z * 0.76, TurretOut = FMath::Max(FMath::Abs(Bounds.Min.Y), Bounds.Max.Y) * 0.56;
			for (const double Hand : { -1.0, 1.0 })
			{
				TGuardValue<uint8> Metal(Fresh.Finish, 128);
				Fresh.Marking(FVector(0.15, Hand * TurretOut, TurretUp), 0.22, Tag == TEXT("SOV") ? FString(TEXT("SOV-ARMY")) : Tag == TEXT("USA") ? FString(TEXT("USA-ARMY")) : Tag == TEXT("UK") ? FString(TEXT("UK-FLASH")) : Tag, FVector(0.0, Hand, 0.12));
				Fresh.Numerals(FVector(0.82, Hand * TurretOut * 0.96, TurretUp), 0.3, 100 + static_cast<int32>(GetTypeHash(Tag) % 500), FVector(0.0, Hand, 0.12));
			}
		}
		Fresh.MarkWear(false);
		Model = &Painted.Add(Key, MoveTemp(Fresh));
	}
	TGuardValue<uint8> AsPainted(Finish, 255); // (each point keeps what the model says it is made of)
	Stamp(*Model, Where, Metre, Turn);
	return true;
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
		Colors.Add(Finish != 255 ? FColor(Model.Colors[K].R, Model.Colors[K].G, Model.Colors[K].B, Finish) : Model.Colors[K]);
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

void FMiniatureMesh::Fleet(int32 Battleships, int32 Carriers, int32 Cruisers, int32 Destroyers, int32 Submarines, double Unit, const FLinearColor& /*Nation*/, const FString& Tag, bool bUnderWay)
{
	TGuardValue<uint8> Painted(Finish, 128); // (ships are painted metal)
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
		const double Turn = -90.0 + (K - 1) * 4.0;
		Vehicle(FString(Show[K]) + Tag, Where, Metre, Turn);
		if (bUnderWay)
		{
			// A ship at sea is under way: foam at her bows and a wake astern, painted on the water as on a model's base,
			// white by the hull and fading into the sea behind her.
			const FString Kind(Show[K]);
			const double Half = (Kind.StartsWith(TEXT("Battleship")) ? 112.0 : Kind.StartsWith(TEXT("Carrier")) ? 122.0 : Kind.StartsWith(TEXT("Cruiser")) ? 92.0 : Kind.StartsWith(TEXT("Destroyer")) ? 54.0 : 36.0) * Metre;
			const double Beam = (Kind.StartsWith(TEXT("Battleship")) || Kind.StartsWith(TEXT("Carrier")) ? 30.0 : Kind.StartsWith(TEXT("Cruiser")) ? 20.0 : Kind.StartsWith(TEXT("Destroyer")) ? 11.0 : 7.0) * Metre;
			const double R = FMath::DegreesToRadians(Turn);
			// (a model's front is toward -X before it is turned: Ahead is the way her bows point on the table)
			const FVector Ahead(-FMath::Cos(R), -FMath::Sin(R), 0.0), Abeam(-Ahead.Y, Ahead.X, 0.0);
			const FLinearColor Foam(FColor::FromHex(TEXT("EEF4F2"))), Water(FColor::FromHex(TEXT("2B6E88")));
			const FVector Sea(Where.X, Where.Y, 0.012 * Unit);
			const int32 Pieces = 14;
			const FVector Up(0.0, 0.0, 1.0);
			for (int32 Piece = 0; Piece < Pieces; ++Piece)
			{
				// The wake astern: one smooth tapering strip, widening a little and fading into the sea as it falls behind.
				const double S0 = static_cast<double>(Piece) / Pieces, S1 = static_cast<double>(Piece + 1) / Pieces;
				const double W0 = Beam * (0.34 + 0.75 * S0), W1 = Beam * (0.34 + 0.75 * S1);
				const FVector P0 = Sea - Ahead * (Half * (0.9 + 1.7 * S0)), P1 = Sea - Ahead * (Half * (0.9 + 1.7 * S1));
				const FLinearColor Shade = FMath::Lerp(Foam, Water, FMath::Pow((S0 + S1) * 0.5, 0.55));
				Quad(P0 - Abeam * W0, P0 + Abeam * W0, P1 + Abeam * W1, P1 - Abeam * W1, Up, Shade.ToFColor(true)); // (corners in order round the strip)
			}
			for (const double Side : { -1.0, 1.0 })
			{
				// The bow wave: a thin line of foam thrown out on each side from the stem.
				for (int32 Piece = 0; Piece < 4; ++Piece)
				{
					const double Share = (Piece + 0.5) / 4.0;
					const FVector At = Sea + Ahead * (Half * (0.95 - 1.15 * Share)) + Abeam * Side * (Beam * (0.35 + 1.25 * Share));
					Box(At, FVector(Half * 0.3, Beam * 0.16, 0.012 * Unit), FMath::Lerp(Foam, Water, 0.25 + 0.6 * Share), Turn + Side * 24.0);
				}
			}
		}
	}
}

void FMiniatureMesh::AirFleet(int32 Fighters, int32 Bombers, double Unit, const FLinearColor& Nation, const FString& Tag)
{
	// A patch of airfield: mown grass with a runway across it.
	Pillar(FVector(0.0, 0.0, 0.05 * Unit), 5.0 * Unit, 4.9 * Unit, 0.14 * Unit, FLinearColor(FColor::FromHex(TEXT("70804F"))), 28);
	Box(FVector(0.0, 0.0, 0.2 * Unit), FVector(1.2, 9.0, 0.03) * Unit, FLinearColor(FColor::FromHex(TEXT("8E8A80"))), 28.0);
	TGuardValue<uint8> Painted(Finish, 128); // (aircraft are painted metal)
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
	if (Windows)
	{
		// A window or two in each long wall, a row for each storey of a tall house.
		const int32 Storeys = FMath::Clamp(FMath::RoundToInt32(WallHeight / (Width * 0.75)), 1, 4);
		const int32 Across2 = Length > Width * 2.2 ? 3 : 2;
		for (const double Side : { -1.0, 1.0 })
		{
			for (int32 Storey = 0; Storey < Storeys; ++Storey)
			{
				for (int32 K = 0; K < Across2; ++K)
				{
					Windows->Add(At(Length * ((K + 0.5) / Across2 - 0.5) * 0.8, Side * (Width * 0.5 + Width * 0.02), WallHeight * (Storey + 0.55) / Storeys));
					Windows->Add(Across * Side);
				}
			}
		}
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

void FMiniatureMesh::Civic(const FVector& Where, double Unit, int32 Line, int32 Level, bool bGoing, const FLinearColor& Nation)
{
	const double S = Unit;
	auto P = [&Where, S](double North, double East, double Up) { return Where + FVector(North, East, Up) * S; };
	auto Hex = [](const TCHAR* Code) { return FLinearColor(FColor::FromHex(Code)); };
	const FLinearColor Cream = Hex(TEXT("E6DCC4")), Ashlar = Hex(TEXT("D9D2C0")), White = Hex(TEXT("EFEBE0")), Brick = Hex(TEXT("8E5644")), Tile = Hex(TEXT("A04A31"));
	const FLinearColor Slate = Hex(TEXT("555A60")), Wood = Hex(TEXT("8A6A45")), Cobble = Hex(TEXT("8F887A")), Copper = Hex(TEXT("6FA08C")), Gilt = Hex(TEXT("C9A227"));
	const FLinearColor Tenting = Hex(TEXT("C9BE9A")), Wheat = Hex(TEXT("D2B052")), Crop = Hex(TEXT("7C9A52")), Straw = Hex(TEXT("D8C277")), Tin = Hex(TEXT("A9ADB0"));
	const FLinearColor Red = Hex(TEXT("BE2A26")), Drill = Hex(TEXT("C9B487")), Hut = Hex(TEXT("6B7355")), Lawn = Hex(TEXT("6E8B4C")), Leaf = Hex(TEXT("4F6B3A"));
	TArray<FVector>* const Lit = Windows; // (the scaffolding and the small things have no windows to light)

	// Builders at work: a half-built wall inside a cage of poles, with a derrick over it.
	auto Scaffold = [&](double North, double East)
	{
		Windows = nullptr;
		Box(P(North, East, 0.17), FVector(0.5, 0.5, 0.34) * S, Brick);
		for (const double A : { -1.0, 1.0 })
		{
			for (const double B : { -1.0, 1.0 })
			{
				Rod(P(North + A * 0.32, East + B * 0.32, 0.0), P(North + A * 0.32, East + B * 0.32, 0.8), 0.018 * S, Wood, 4);
			}
			for (const double High : { 0.3, 0.62 })
			{
				Rod(P(North + A * 0.32, East - 0.32, High), P(North + A * 0.32, East + 0.32, High), 0.014 * S, Wood, 4);
				Rod(P(North - 0.32, East + A * 0.32, High), P(North + 0.32, East + A * 0.32, High), 0.014 * S, Wood, 4);
			}
		}
		Rod(P(North + 0.32, East + 0.32, 0.8), P(North - 0.1, East - 0.1, 1.05), 0.016 * S, Wood, 4);
		Rod(P(North - 0.1, East - 0.1, 1.05), P(North - 0.1, East - 0.1, 0.55), 0.006 * S, Slate, 4);
		Windows = Lit;
	};
	// A flag on a pole, in the holder's colour.
	auto Flag = [&](double North, double East, double High)
	{
		Rod(P(North, East, 0.0), P(North, East, High), 0.016 * S, Ashlar, 5);
		Box(P(North, East + 0.17, High - 0.1), FVector(0.02, 0.34, 0.2) * S, Nation);
	};
	if (Level <= 0)
	{
		Box(P(0.0, 0.0, 0.015), FVector(1.1, 1.1, 0.03) * S, Cobble);
		Scaffold(0.0, 0.0);
		return;
	}
	switch (Line)
	{
	case 0: // commerce: a market hall with its stalls, then a bank, then a domed exchange
	{
		Box(P(0.0, 0.0, 0.015), FVector(1.9, 2.4, 0.03) * S, Cobble);
		House(P(0.3, 0.0, 0.0), 1.5 * S, 0.66 * S, 0.4 * S, 0.34 * S, Cream, Tile, 90.0);
		Windows = nullptr;
		static const TCHAR* Awnings[] = { TEXT("B8342B"), TEXT("ECE3CC"), TEXT("2F6C96"), TEXT("D39B3A") };
		for (int32 Stall = 0; Stall < 4; ++Stall)
		{
			const double East = (Stall - 1.5) * 0.42;
			Box(P(-0.5, East, 0.07), FVector(0.2, 0.24, 0.14) * S, Wood);
			Box(P(-0.5, East, 0.2), FVector(0.3, 0.32, 0.03) * S, Hex(Awnings[Stall]), 0.0, 12.0);
		}
		Windows = Lit;
		if (Level >= 2)
		{
			Box(P(0.25, -1.45, 0.48), FVector(0.72, 0.6, 0.96) * S, Ashlar);
			Box(P(0.25, -1.45, 0.99), FVector(0.8, 0.68, 0.06) * S, Slate);
			for (int32 Column = 0; Column < 4; ++Column)
			{
				Pillar(P(-0.16, -1.45 + (Column - 1.5) * 0.16, 0.0), 0.035 * S, 0.03 * S, 0.8 * S, White, 6);
			}
		}
		if (Level >= 3)
		{
			Pillar(P(0.25, 1.5, 0.0), 0.44 * S, 0.44 * S, 0.62 * S, Ashlar, 12);
			Ball(P(0.25, 1.5, 0.74), FVector(0.86, 0.86, 0.62) * S, Copper, 5, 10);
			Pillar(P(0.25, 1.5, 1.0), 0.04 * S, 0.0, 0.34 * S, Gilt, 6);
		}
		if (bGoing) { Scaffold(-0.45, Level >= 2 ? 1.5 : -1.45); }
		break;
	}
	case 1: // farming: a barn among striped fields, then silos, then a canning works
	{
		Windows = nullptr;
		for (int32 Strip = 0; Strip < 6; ++Strip)
		{
			Box(P((Strip - 2.5) * 0.34, 0.95, 0.012), FVector(0.3, 1.5, 0.024) * S, Strip % 2 == 0 ? Wheat : Crop);
		}
		House(P(0.1, -0.5, 0.0), 1.0 * S, 0.58 * S, 0.36 * S, 0.38 * S, Hex(TEXT("8C3B2E")), Hex(TEXT("4B4038")), 0.0);
		Ball(P(-0.75, -0.2, 0.1), FVector(0.3, 0.3, 0.24) * S, Straw, 4, 8);
		Ball(P(-0.75, -0.62, 0.1), FVector(0.3, 0.3, 0.24) * S, Straw, 4, 8);
		if (Level >= 2)
		{
			for (const double East : { -1.25, -1.72 })
			{
				Pillar(P(0.35, East, 0.0), 0.2 * S, 0.2 * S, 0.8 * S, Tin, 10);
				Pillar(P(0.35, East, 0.8), 0.22 * S, 0.0, 0.2 * S, Slate, 10);
			}
		}
		if (Level >= 3)
		{
			Windows = Lit;
			House(P(1.05, -0.7, 0.0), 1.5 * S, 0.6 * S, 0.44 * S, 0.18 * S, Brick, Slate, 90.0);
			Windows = nullptr;
			Pillar(P(1.05, 0.25, 0.0), 0.09 * S, 0.065 * S, 1.25 * S, Brick, 8);
		}
		Windows = Lit;
		if (bGoing) { Scaffold(-0.6, -1.4); }
		break;
	}
	case 2: // learning: a schoolhouse with a bell tower, then wings, then an observatory and a mast
	{
		Box(P(0.0, 0.0, 0.012), FVector(1.9, 2.3, 0.024) * S, Lawn);
		House(P(0.2, 0.0, 0.0), 1.25 * S, 0.6 * S, 0.5 * S, 0.32 * S, Brick, Slate, 90.0);
		Box(P(0.2, 0.0, 0.55), FVector(0.3, 0.3, 1.1) * S, Brick);
		Pillar(P(0.2, 0.0, 1.1), 0.24 * S, 0.0, 0.36 * S, Slate, 4);
		if (Level >= 2)
		{
			for (const double Side : { -1.0, 1.0 })
			{
				House(P(-0.25, Side * 0.86, 0.0), 0.95 * S, 0.5 * S, 0.5 * S, 0.28 * S, Brick, Slate, 0.0);
			}
		}
		if (Level >= 3)
		{
			Pillar(P(0.75, -1.25, 0.0), 0.3 * S, 0.3 * S, 0.5 * S, White, 10);
			Ball(P(0.75, -1.25, 0.6), FVector(0.6, 0.6, 0.5) * S, White, 5, 10);
			Windows = nullptr;
			Rod(P(0.75, 1.3, 0.0), P(0.75, 1.3, 1.7), 0.02 * S, Slate, 4);
			for (const double High : { 0.6, 1.1, 1.5 })
			{
				Rod(P(0.75, 1.3 - 0.16, High), P(0.75, 1.3 + 0.16, High), 0.012 * S, Slate, 4);
			}
			Windows = Lit;
		}
		if (bGoing) { Scaffold(-0.95, 0.0); }
		break;
	}
	case 3: // training: a drill ground with tents and a flag, then barrack huts, then an officers' school
	{
		Windows = nullptr;
		Box(P(0.0, 0.0, 0.012), FVector(2.0, 2.3, 0.024) * S, Drill);
		Flag(0.75, 0.0, 1.25);
		if (Level == 1)
		{
			for (int32 Tent = 0; Tent < 4; ++Tent)
			{
				House(P(-0.55, (Tent - 1.5) * 0.48, 0.0), 0.5 * S, 0.34 * S, 0.04 * S, 0.24 * S, Tenting, Tenting, 0.0);
			}
		}
		Windows = Lit;
		if (Level >= 2)
		{
			for (int32 Row = 0; Row < 3; ++Row)
			{
				House(P(-0.72 + Row * 0.42, -0.25, 0.0), 1.5 * S, 0.32 * S, 0.24 * S, 0.14 * S, Hut, Hex(TEXT("3F4338")), 90.0);
			}
		}
		if (Level >= 3)
		{
			House(P(0.1, 0.85, 0.0), 1.1 * S, 0.56 * S, 0.56 * S, 0.28 * S, Ashlar, Slate, 0.0);
			Box(P(0.55, 0.85, 0.5), FVector(0.3, 0.3, 1.0) * S, Ashlar);
			Pillar(P(0.55, 0.85, 1.0), 0.22 * S, 0.0, 0.3 * S, Slate, 4);
		}
		if (bGoing) { Scaffold(-0.7, 0.95); }
		break;
	}
	case 4: // medicine: an aid post under canvas, then a hospital with two wings, then a rest home in a garden
	{
		// A red cross laid out on the ground, as on the roof of every hospital, where it can be read from above.
		Windows = nullptr;
		Box(P(-0.75, 0.0, 0.02), FVector(0.66, 0.66, 0.04) * S, White);
		Box(P(-0.75, 0.0, 0.045), FVector(0.5, 0.15, 0.02) * S, Red);
		Box(P(-0.75, 0.0, 0.045), FVector(0.15, 0.5, 0.02) * S, Red);
		if (Level == 1)
		{
			House(P(0.15, 0.0, 0.0), 1.0 * S, 0.62 * S, 0.12 * S, 0.4 * S, White, Tenting, 90.0);
			House(P(0.15, 0.85, 0.0), 0.5 * S, 0.36 * S, 0.05 * S, 0.26 * S, Tenting, Tenting, 90.0);
		}
		Windows = Lit;
		if (Level >= 2)
		{
			Box(P(0.25, 0.0, 0.4), FVector(0.6, 1.7, 0.8) * S, White);
			Box(P(0.25, 0.0, 0.83), FVector(0.68, 1.78, 0.06) * S, Slate);
			for (const double Side : { -1.0, 1.0 })
			{
				Box(P(-0.05, Side * 0.7, 0.3), FVector(0.9, 0.42, 0.6) * S, White);
				Box(P(-0.05, Side * 0.7, 0.63), FVector(0.98, 0.5, 0.06) * S, Slate);
			}
			if (Lit)
			{
				for (int32 K = 0; K < 5; ++K)
				{
					Lit->Add(P(-0.06, (K - 2) * 0.3, 0.5));
					Lit->Add(FVector(-1.0, 0.0, 0.0));
				}
			}
		}
		if (Level >= 3)
		{
			House(P(0.2, 1.55, 0.0), 0.8 * S, 0.56 * S, 0.42 * S, 0.3 * S, Cream, Tile, 0.0);
			Windows = nullptr;
			Box(P(-0.55, 1.5, 0.012), FVector(0.7, 0.9, 0.024) * S, Lawn);
			Broadleaf(P(-0.5, 1.25, 0.0), 0.5 * S, 0.2 * S, Leaf);
			Broadleaf(P(-0.62, 1.75, 0.0), 0.44 * S, 0.18 * S, Leaf);
			Windows = Lit;
		}
		if (bGoing) { Scaffold(0.9, -1.2); }
		break;
	}
	default: // administration: a town hall with a clock tower, then a colonnade and a wing, then the governor's block
	{
		Box(P(0.0, 0.0, 0.015), FVector(1.9, 2.2, 0.03) * S, Cobble);
		House(P(0.25, 0.0, 0.0), 1.3 * S, 0.7 * S, 0.55 * S, 0.36 * S, Cream, Tile, 90.0);
		Box(P(0.25, 0.0, 0.75), FVector(0.34, 0.34, 1.5) * S, Cream);
		Box(P(0.07, 0.0, 1.2), FVector(0.02, 0.2, 0.2) * S, White); // the clock
		Pillar(P(0.25, 0.0, 1.5), 0.27 * S, 0.0, 0.46 * S, Copper, 4);
		Windows = nullptr;
		Flag(-0.7, 0.0, Level >= 3 ? 1.5 : 1.1);
		if (Level >= 2)
		{
			for (int32 Column = 0; Column < 6; ++Column)
			{
				Pillar(P(-0.2, (Column - 2.5) * 0.22, 0.0), 0.035 * S, 0.03 * S, 0.5 * S, White, 6);
			}
			Box(P(-0.2, 0.0, 0.53), FVector(0.16, 1.3, 0.06) * S, Ashlar);
			Windows = Lit;
			House(P(0.25, 1.05, 0.0), 0.8 * S, 0.6 * S, 0.5 * S, 0.3 * S, Cream, Tile, 0.0);
		}
		if (Level >= 3)
		{
			Windows = Lit;
			Box(P(0.85, -0.2, 0.5), FVector(0.5, 1.9, 1.0) * S, Ashlar);
			Box(P(0.85, -0.2, 1.03), FVector(0.58, 1.98, 0.06) * S, Slate);
			House(P(0.25, -1.05, 0.0), 0.8 * S, 0.6 * S, 0.5 * S, 0.3 * S, Cream, Tile, 0.0);
		}
		Windows = Lit;
		if (bGoing) { Scaffold(-0.75, -0.95); }
		break;
	}
	}
	Windows = Lit;
}

void FMiniatureMesh::Town(const FVector& Where, double Unit, int32 Houses, bool bCapital, bool bCity, int32 Works, int32 Seed, TArray<FVector>* OutChimneys, bool bHarbour)
{
	FRandomStream Random(Seed * 7919 + 13);
	static const TCHAR* WallPaints[] = { TEXT("E6DCC4"), TEXT("D8C8A6"), TEXT("CEC6B6"), TEXT("E2D2AE"), TEXT("C9B79A") };
	static const TCHAR* RoofPaints[] = { TEXT("9A4A32"), TEXT("7C3F2E"), TEXT("5E5A58"), TEXT("8B5A3C"), TEXT("A65B3A") };
	const FLinearColor StoneWork(FColor::FromHex(TEXT("D9D2C0")));
	const FLinearColor Copper(FColor::FromHex(TEXT("6FA08C")));
	const FLinearColor Brick(FColor::FromHex(TEXT("7A5547")));
	const FLinearColor Soot(FColor::FromHex(TEXT("3B3532")));
	const FLinearColor Slate(FColor::FromHex(TEXT("5E5A58")));
	const FLinearColor Cobble(FColor::FromHex(TEXT("8F887A")));
	const FLinearColor Gilt(FColor::FromHex(TEXT("C9A227")));
	const FLinearColor Leaf(FColor::FromHex(TEXT("4F6B3A"))), LeafLight(FColor::FromHex(TEXT("668249")));
	auto Out = [&Where](double Angle, double Distance) { return Where + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Distance; };

	// The great building in the middle: a domed palace for a capital, a church with a tower for a city.
	double Clear = 0.0;
	const bool bTown = !bCapital && !bCity;
	if (bCapital)
	{
		// The palace: a long front with a wing at each end, a row of columns before it, a flight of steps, and the dome.
		Box(Where + FVector(0.0, 0.0, 0.04 * Unit), FVector(2.9, 3.6, 0.08) * Unit, Cobble); // the square it stands in
		Box(Where + FVector(0.0, 0.0, 0.45 * Unit), FVector(1.5, 2.3, 0.9) * Unit, StoneWork);
		for (const double Wing : { -1.0, 1.0 })
		{
			Box(Where + FVector(0.25 * Unit, Wing * 1.35 * Unit, 0.38 * Unit), FVector(1.9, 0.55, 0.76) * Unit, StoneWork);
			Box(Where + FVector(0.25 * Unit, Wing * 1.35 * Unit, 0.79 * Unit), FVector(2.0, 0.65, 0.06) * Unit, Slate);
		}
		for (int32 Column = 0; Column < 6; ++Column)
		{
			Pillar(Where + FVector(0.86 * Unit, (Column - 2.5) * 0.36 * Unit, 0.08 * Unit), 0.06 * Unit, 0.05 * Unit, 0.72 * Unit, StoneWork, 6);
		}
		Box(Where + FVector(0.86 * Unit, 0.0, 0.84 * Unit), FVector(0.24, 2.1, 0.1) * Unit, StoneWork); // the entablature they carry
		for (int32 Step = 0; Step < 3; ++Step)
		{
			Box(Where + FVector((1.05 + Step * 0.09) * Unit, 0.0, (0.15 - Step * 0.04) * Unit), FVector(0.1, 1.9, 0.06) * Unit, Cobble);
		}
		Box(Where + FVector(0.0, 0.0, 0.93 * Unit), FVector(1.6, 2.4, 0.06) * Unit, Slate);
		Pillar(Where + FVector(0.0, 0.0, 0.9 * Unit), 0.5 * Unit, 0.5 * Unit, 0.35 * Unit, StoneWork, 12);
		Ball(Where + FVector(0.0, 0.0, 1.25 * Unit), FVector(1.0, 1.0, 0.9) * Unit, Copper, 5, 10);
		Pillar(Where + FVector(0.0, 0.0, 1.65 * Unit), 0.06 * Unit, 0.0, 0.5 * Unit, Gilt, 6);
		if (Windows)
		{
			for (int32 K = 0; K < 7; ++K)
			{
				for (const double Side : { -1.0, 1.0 })
				{
					Windows->Add(Where + FVector(Side * 0.76 * Unit, (K - 3) * 0.3 * Unit, 0.52 * Unit));
					Windows->Add(FVector(Side, 0.0, 0.0));
				}
			}
		}
		Clear = 2.1 * Unit;
	}
	else if (bCity)
	{
		// The church with its tower, the town hall across the square from it, and the railway station on the edge.
		Box(Where + FVector(0.0, 0.0, 0.03 * Unit), FVector(2.2, 2.6, 0.06) * Unit, Cobble);
		House(Where + FVector(0.0, -0.5 * Unit, 0.0), 1.5 * Unit, 0.7 * Unit, 0.6 * Unit, 0.45 * Unit, StoneWork, Slate, 0.0);
		Box(Where + FVector(0.0, 0.45 * Unit, 0.7 * Unit), FVector(0.5, 0.5, 1.4) * Unit, StoneWork);
		Pillar(Where + FVector(0.0, 0.45 * Unit, 1.4 * Unit), 0.36 * Unit, 0.0, 0.8 * Unit, Slate, 4);
		Pillar(Where + FVector(0.0, 0.45 * Unit, 2.2 * Unit), 0.03 * Unit, 0.0, 0.25 * Unit, Gilt, 4);
		House(Where + FVector(-0.75 * Unit, 0.2 * Unit, 0.0), 1.0 * Unit, 0.6 * Unit, 0.62 * Unit, 0.25 * Unit, FLinearColor(FColor::FromHex(TEXT("C9B79A"))), FLinearColor(FColor::FromHex(TEXT("7C3F2E"))), 90.0);
		Box(Where + FVector(-0.75 * Unit, 0.2 * Unit, 0.95 * Unit), FVector(0.22, 0.22, 0.4) * Unit, FLinearColor(FColor::FromHex(TEXT("C9B79A"))));
		Pillar(Where + FVector(-0.75 * Unit, 0.2 * Unit, 1.15 * Unit), 0.17 * Unit, 0.0, 0.3 * Unit, Copper, 4);
		Clear = 1.5 * Unit;
	}
	else if (Houses >= 6)
	{
		// A town of some size has its church: a nave and a steeple.
		House(Where, 0.9 * Unit, 0.45 * Unit, 0.42 * Unit, 0.3 * Unit, StoneWork, Slate, 20.0);
		Box(Where + FVector(0.5 * Unit, 0.18 * Unit, 0.45 * Unit), FVector(0.3, 0.3, 0.9) * Unit, StoneWork, 20.0);
		Pillar(Where + FVector(0.5 * Unit, 0.18 * Unit, 0.9 * Unit), 0.22 * Unit, 0.0, 0.6 * Unit, Slate, 4);
		Clear = 0.75 * Unit;
	}

	// Houses round it, closer packed toward the middle, each turned its own way. In a city and a capital the
	// inner ones are tall tenements with flat-pitched roofs.
	const double Spread = (0.9 + 0.36 * FMath::Sqrt(static_cast<double>(Houses))) * Unit;
	const int32 Tall = bCapital ? 6 : bCity ? 4 : 0;
	const int32 All = Houses + Tall;
	for (int32 K = 0; K < All; ++K)
	{
		const double Angle = K * 2.39996 + Random.FRandRange(-0.3f, 0.3f); // the golden angle scatters them evenly
		const double Distance = Clear + Spread * FMath::Sqrt((K + 0.6) / All);
		const FVector At = Out(Angle, Distance);
		const FLinearColor Wall(FColor::FromHex(WallPaints[Random.RandRange(0, 4)])), Roof(FColor::FromHex(RoofPaints[Random.RandRange(0, 4)]));
		if (K < Tall)
		{
			// A block of flats: three or four storeys round a stair, facing the square.
			House(At, Random.FRandRange(0.8f, 1.1f) * Unit, Random.FRandRange(0.5f, 0.62f) * Unit, Random.FRandRange(0.75f, 1.05f) * Unit, 0.16 * Unit, Wall, Slate, FMath::RadiansToDegrees(Angle) + 90.0);
			continue;
		}
		const double Length = Random.FRandRange(0.62f, 0.95f) * Unit, Width = Random.FRandRange(0.42f, 0.58f) * Unit;
		const double Turn = Random.FRandRange(0.0f, 180.0f);
		House(At, Length, Width, Random.FRandRange(0.3f, 0.5f) * Unit, Random.FRandRange(0.22f, 0.34f) * Unit, Wall, Roof, Turn);
		// A chimney on the ridge of some, and a tree in the garden of others.
		if (K % 3 == 0)
		{
			Box(At + FVector(0.0, 0.0, 0.62 * Unit), FVector(0.09, 0.09, 0.3) * Unit, Brick, Turn);
		}
		if (K % 2 == 1)
		{
			const FVector Garden = Out(Angle + 0.5, Distance + 0.55 * Unit);
			Broadleaf(Garden, Random.FRandRange(0.45f, 0.7f) * Unit, Random.FRandRange(0.16f, 0.24f) * Unit, K % 4 == 1 ? Leaf : LeafLight);
		}
	}

	// The roads through it: two that cross in the middle, laid on the ground under everything.
	const double Reach = Clear + Spread + 0.5 * Unit;
	const double Lane = Random.FRandRange(0.0f, 60.0f);
	Box(Where + FVector(0.0, 0.0, 0.012 * Unit), FVector(Reach * 2.0, 0.2 * Unit, 0.024 * Unit), Cobble, Lane);
	Box(Where + FVector(0.0, 0.0, 0.014 * Unit), FVector(Reach * 1.7, 0.18 * Unit, 0.028 * Unit), Cobble, Lane + 78.0);

	// A city's railway station, out along one of the roads: a long hall with a glazed roof and a clock turret.
	if (bCity || bCapital)
	{
		const double Angle = FMath::DegreesToRadians(Lane) + PI;
		const FVector At = Out(Angle, Reach + 0.2 * Unit);
		const double Turn = Lane + 90.0;
		House(At, 1.7 * Unit, 0.6 * Unit, 0.4 * Unit, 0.2 * Unit, FLinearColor(FColor::FromHex(TEXT("B9A27E"))), FLinearColor(FColor::FromHex(TEXT("6E7C80"))), Turn);
		Box(At + FVector(0.0, 0.0, 0.62 * Unit), FVector(0.24, 0.24, 0.44) * Unit, FLinearColor(FColor::FromHex(TEXT("B9A27E"))), Turn);
		Pillar(At + FVector(0.0, 0.0, 0.84 * Unit), 0.19 * Unit, 0.0, 0.22 * Unit, Slate, 4);
	}

	// A harbour town has a quay with a crane and a warehouse or two, on the side the works are not.
	if (bHarbour)
	{
		const double Angle = 0.9 + PI;
		const FVector At = Out(Angle, Reach + 0.3 * Unit);
		const double Turn = FMath::RadiansToDegrees(Angle) + 90.0;
		Box(At + FVector(0.0, 0.0, 0.05 * Unit), FVector(2.2, 0.7, 0.1) * Unit, Cobble, Turn);
		House(At + FVector(0.0, 0.0, 0.1 * Unit), 0.9 * Unit, 0.45 * Unit, 0.3 * Unit, 0.14 * Unit, Brick, Soot, Turn);
		const FVector Crane = Out(Angle + 0.32, Reach + 0.3 * Unit) + FVector(0.0, 0.0, 0.1 * Unit);
		Pillar(Crane, 0.1 * Unit, 0.06 * Unit, 0.9 * Unit, Soot, 4);
		Rod(Crane + FVector(0.0, 0.0, 0.85 * Unit), Crane + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * 0.7 * Unit + FVector(0.0, 0.0, 1.15 * Unit), 0.035 * Unit, Soot, 4);
	}

	// The works, out at the edge: a long shed with a chimney.
	for (int32 K = 0; K < Works; ++K)
	{
		const double Angle = 0.9 + K * 2.1;
		const FVector At = Out(Angle, Reach + 0.5 * Unit);
		const double Turn = FMath::RadiansToDegrees(Angle) + 90.0;
		House(At, 1.5 * Unit, 0.8 * Unit, 0.5 * Unit, 0.22 * Unit, Brick, Soot, Turn);
		// A saw-toothed north-light roof on a second shed beside it.
		const FVector Beside = At + FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0) * 0.95 * Unit;
		Box(Beside + FVector(0.0, 0.0, 0.2 * Unit), FVector(1.2, 0.7, 0.4) * Unit, Brick, Turn);
		for (int32 Tooth = 0; Tooth < 3; ++Tooth)
		{
			Box(Beside + FVector(FMath::Cos(FMath::DegreesToRadians(Turn)), FMath::Sin(FMath::DegreesToRadians(Turn)), 0.0) * ((Tooth - 1) * 0.4 * Unit) + FVector(0.0, 0.0, 0.46 * Unit), FVector(0.3, 0.7, 0.1) * Unit, Soot, Turn, 22.0);
		}
		Pillar(At + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * 0.6 * Unit, 0.13 * Unit, 0.09 * Unit, 1.5 * Unit, Brick, 7);
		if (OutChimneys)
		{
			OutChimneys->Add(At + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * 0.6 * Unit + FVector(0.0, 0.0, 1.5 * Unit));
		}
	}
	(void)bTown;
}

void FMiniatureMesh::Township(const FTownSite& Site, TArray<FVector>* OutChimneys, TArray<FVector2D>* OutQuarters, FVector* OutBeacon)
{
	FRandomStream Dice(Site.Seed * 7919 + 13);
	const double U = Site.Unit * (Site.bPacked ? 0.85 : 1.0); // a house is under one of these long (a little smaller where the town is packed into a small province)
	const double PerMap = Site.MapUnit, Reach = Site.Radius; // (the plan is laid out in map units, and built in the table's)
	const double Um = U / PerMap; // one house-unit, in map units

	// ---- The paint of the place: walls and roofs by its part of Europe.
	static const TCHAR* WallsOf[5][5] = {
		{ TEXT("E6DCC4"), TEXT("D8C8A6"), TEXT("CEC6B6"), TEXT("E2D2AE"), TEXT("C9B79A") }, // middle Europe: plaster
		{ TEXT("8E5A48"), TEXT("A0705A"), TEXT("C9B79A"), TEXT("B8A890"), TEXT("7A5547") }, // the north-west: brick
		{ TEXT("F0E8D8"), TEXT("E8DCC0"), TEXT("F2E4C8"), TEXT("E0D0B0"), TEXT("EDE0CC") }, // the south: whitewash
		{ TEXT("7A3B2E"), TEXT("8A5A3C"), TEXT("6B5236"), TEXT("C9B79A"), TEXT("9A6A4A") }, // the north and east: painted timber
		{ TEXT("EDE3CF"), TEXT("E6D8BC"), TEXT("F3EAD6"), TEXT("DCCFB4"), TEXT("E9DEC8") }, // the far south: pale plaster
	};
	static const TCHAR* RoofsOf[5][5] = {
		{ TEXT("9A4A32"), TEXT("7C3F2E"), TEXT("5E5A58"), TEXT("8B5A3C"), TEXT("A65B3A") }, // red tile, some slate
		{ TEXT("4E5560"), TEXT("5E5A58"), TEXT("444B55"), TEXT("6A6660"), TEXT("3F454D") }, // slate
		{ TEXT("C2643A"), TEXT("B4552F"), TEXT("CC7040"), TEXT("A84E30"), TEXT("BD6A45") }, // terracotta
		{ TEXT("3E4640"), TEXT("4A4440"), TEXT("55504A"), TEXT("2F3A36"), TEXT("5A5248") }, // dark shingle and tin
		{ TEXT("D9CDB4"), TEXT("CFC2A6"), TEXT("E0D5BE"), TEXT("C6B99C"), TEXT("D3C7AE") }, // flat roofs, the colour of their walls
	};
	const int32 Look = FMath::Clamp(Site.Look, 0, 4);
	const bool bFlatRoofs = Look == 4;
	auto WallPaint = [&]() { return FLinearColor(FColor::FromHex(WallsOf[Look][Dice.RandRange(0, 4)])); };
	auto RoofPaint = [&]() { return FLinearColor(FColor::FromHex(RoofsOf[Look][Dice.RandRange(0, 4)])); };
	const FLinearColor Ashlar(FColor::FromHex(TEXT("D9D2C0"))), Verdigris(FColor::FromHex(TEXT("6FA08C"))), RedBrick(FColor::FromHex(TEXT("7A5547")));
	const FLinearColor Sooty(FColor::FromHex(TEXT("3B3532"))), LeadGrey(FColor::FromHex(TEXT("5E5A58"))), Cobbles(FColor::FromHex(TEXT("8F887A")));
	const FLinearColor Gilding(FColor::FromHex(TEXT("C9A227"))), Paving(FColor::FromHex(TEXT("7C766A"))), Lawn(FColor::FromHex(TEXT("46553A")));
	const FLinearColor LeafDark(FColor::FromHex(TEXT("4F6B3A"))), LeafPale(FColor::FromHex(TEXT("668249"))), Glazing(FColor::FromHex(TEXT("6E7C80")));
	const FLinearColor TowerWhite(FColor::FromHex(TEXT("EFE9DC"))), Ochre(FColor::FromHex(TEXT("B9A27E"))), Rust(FColor::FromHex(TEXT("8A3B2A")));

	// ---- The plan's own frame: its streets run a little off the compass, each town its own way.
	const double Sway = Site.Tier >= 3 ? 10.0 : 24.0; // (a capital's lie nearer the compass, so that its landmark faces the usual eye)
	const double Yaw0 = Dice.FRandRange(-1.0f, 1.0f) * Sway; // which way its long streets run, in degrees east of north
	const double C0 = FMath::Cos(FMath::DegreesToRadians(Yaw0)), S0 = FMath::Sin(FMath::DegreesToRadians(Yaw0));
	// (A and B are along and across its streets, in map units. On the table north is +X and east is +Y.)
	auto MapAt = [&](double A, double B) { return FVector2D(Site.Centre.X + (A * S0 + B * C0), Site.Centre.Y - (A * C0 - B * S0)); };
	auto Stand = [&](double A, double B, double Lift = 0.02) { const FVector2D M = MapAt(A, B); return Site.Ground(M.X, M.Y, Lift); };
	auto BearingOf = [&](double A, double B) { const FVector2D M = MapAt(A, B) - Site.Centre; return FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(M.X, -M.Y)) + 360.0, 360.0); };
	// Ground kept for the buildings the town's owner will give it: a rectangle south of its middle, turned as the map says.
	const double PlotC = FMath::Cos(FMath::DegreesToRadians(Site.PlotsTurn)), PlotS = FMath::Sin(FMath::DegreesToRadians(Site.PlotsTurn));
	auto InPlots = [&](const FVector2D& M)
	{
		if (!Site.bPlots)
		{
			return false;
		}
		const FVector2D D = M - Site.Centre;
		const double X = D.X * PlotC + D.Y * PlotS, Y = -D.X * PlotS + D.Y * PlotC;
		return X > -4.0 && X < 22.0 && Y > Site.PlotsNear - 4.0 && Y < Site.PlotsNear + 11.5;
	};
	auto NearQuay = [&](const FVector2D& M) { return Site.bQuay && FVector2D::DistSquared(M, Site.Quay) < FMath::Square(2.6 * Um); };
	auto CanBuild = [&](double A, double B)
	{
		const FVector2D M = MapAt(A, B);
		return (!Site.Dry || Site.Dry(M.X, M.Y)) && !InPlots(M) && !NearQuay(M);
	};
	TArray<FVector>* const AllWindows = Windows;
	// (Not every house keeps a lamp in its window: a great city would be all lamps.)
	const float Lit = Site.Tier >= 3 ? 0.3f : Site.Tier == 2 ? 0.42f : 0.8f;
	// (The far town is laid out exactly as the near one, so that nothing moves when the eye rises past the height
	// where one gives way to the other: it throws the same dice in the same order, and builds fewer pieces. While
	// "bMute" is set a dwelling is reckoned and not built.)
	const bool bFar = Site.bFar;
	bool bMute = false;
	auto Dwelling = [&](double A, double B, double Long, double Wide, double High, double Roof, const FLinearColor& WallCol, const FLinearColor& RoofCol, double Turn, bool bAlwaysLit = false)
	{
		Windows = (bAlwaysLit || Dice.FRand() < Lit) ? AllWindows : nullptr;
		if (!bMute)
		{
			House(Stand(A, B), Long * U, Wide * U, High * U, (bFlatRoofs ? 0.04 : Roof) * U, WallCol, RoofCol, Yaw0 + Turn);
		}
		Windows = AllWindows;
	};
	// (a colour for a far block, from the place's own paints, chosen by where it stands and not by the dice)
	auto PaintAt = [&](const TCHAR* const Paints[5][5], int32 I, int32 J) { return FLinearColor(FColor::FromHex(Paints[Look][((I * 7 + J * 13 + Site.Seed) % 5 + 5) % 5])); };
	auto Tree = [&](double A, double B)
	{
		if (CanBuild(A, B))
		{
			const double High = Dice.FRandRange(0.45f, 0.7f) * U, Round = Dice.FRandRange(0.16f, 0.24f) * U;
			const bool bDark = Dice.FRand() < 0.5f;
			if (!bFar)
			{
				Broadleaf(Stand(A, B), High, Round, bDark ? LeafDark : LeafPale);
			}
		}
	};
	// A street: a chain of short lengths of cobble, each lying on the ground where it is.
	auto Street = [&](double A0, double B0, double A1, double B1, double Wide, bool bAcrossPlots = false)
	{
		if (bFar)
		{
			return; // (a street is not seen from high up)
		}
		const double Long = FMath::Sqrt(FMath::Square(A1 - A0) + FMath::Square(B1 - B0));
		const int32 Lengths = FMath::Max(1, FMath::CeilToInt32(Long / (1.6 * Um)));
		const double Turn = FMath::RadiansToDegrees(FMath::Atan2(B1 - B0, A1 - A0));
		for (int32 K = 0; K < Lengths; ++K)
		{
			const double T = (K + 0.5) / Lengths, A = FMath::Lerp(A0, A1, T), B = FMath::Lerp(B0, B1, T);
			// (a road may cross the ground kept for the owner's buildings: it is flat, and a works can stand beside it)
			const FVector2D M = MapAt(A, B);
			if (bAcrossPlots ? (!Site.Dry || Site.Dry(M.X, M.Y)) : CanBuild(A, B))
			{
				Box(Stand(A, B, 0.012), FVector(Long / Lengths * PerMap * 1.06, Wide * U, 0.024 * U), Cobbles, Yaw0 + Turn);
			}
		}
	};

	// ---- The church, by the country's own manner of building one: a spire, a dome and a bell tower, or onion domes.
	const int32 Manner = Look == 2 || Look == 4 ? 1 : Look == 3 ? 2 : 0;
	auto Church = [&](double A, double B, double Size, double Turn, bool bCopper = false)
	{
		const FVector At = Stand(A, B);
		const double R = FMath::DegreesToRadians(Yaw0 + Turn);
		auto Off = [&](double Along, double Across, double Up) { return At + FVector(FMath::Cos(R) * Along - FMath::Sin(R) * Across, FMath::Sin(R) * Along + FMath::Cos(R) * Across, Up) * (Size * U); };
		if (Manner == 0)
		{
			House(At, 0.95 * Size * U, 0.46 * Size * U, 0.44 * Size * U, 0.32 * Size * U, Ashlar, LeadGrey, Yaw0 + Turn);
			Box(Off(0.55, 0.0, 0.65), FVector(0.3, 0.3, 1.3) * Size * U, Ashlar, Yaw0 + Turn);
			Pillar(Off(0.55, 0.0, 1.3), 0.23 * Size * U, 0.0, 0.95 * Size * U, bCopper ? Verdigris : LeadGrey, 4);
			Pillar(Off(0.55, 0.0, 2.25), 0.025 * Size * U, 0.0, 0.2 * Size * U, Gilding, 4);
		}
		else if (Manner == 1)
		{
			House(At, 0.9 * Size * U, 0.5 * Size * U, 0.42 * Size * U, 0.16 * Size * U, TowerWhite, FLinearColor(FColor::FromHex(TEXT("B4552F"))), Yaw0 + Turn);
			Pillar(Off(-0.2, 0.0, 0.5), 0.2 * Size * U, 0.2 * Size * U, 0.16 * Size * U, TowerWhite, 10);
			Ball(Off(-0.2, 0.0, 0.66), FVector(0.46, 0.46, 0.4) * Size * U, FLinearColor(FColor::FromHex(TEXT("B4552F"))), 4, 10);
			Box(Off(0.58, 0.3, 0.95), FVector(0.24, 0.24, 1.9) * Size * U, TowerWhite, Yaw0 + Turn);
			Pillar(Off(0.58, 0.3, 1.9), 0.2 * Size * U, 0.0, 0.34 * Size * U, FLinearColor(FColor::FromHex(TEXT("B4552F"))), 4);
		}
		else
		{
			Box(Off(0.0, 0.0, 0.26), FVector(0.7, 0.62, 0.52) * Size * U, TowerWhite, Yaw0 + Turn);
			for (int32 Dome = 0; Dome < 3; ++Dome)
			{
				// (the middle dome stands on a tall drum, so that it is seen over the roofs round it)
				const double Along = Dome == 0 ? 0.0 : (Dome == 1 ? 0.24 : -0.24), Big = Dome == 0 ? 1.0 : 0.6, Drum = Dome == 0 ? 0.95 : 0.22;
				Pillar(Off(Along, 0.0, 0.52), 0.11 * Big * Size * U, 0.1 * Big * Size * U, Drum * Size * U, TowerWhite, 8);
				Ball(Off(Along, 0.0, 0.52 + Drum + 0.08 * Big), FVector(0.3, 0.3, 0.34) * Big * Size * U, Dome == 0 ? Gilding : Verdigris, 5, 8);
				Pillar(Off(Along, 0.0, 0.52 + Drum + 0.24 * Big), 0.02 * Size * U, 0.0, 0.16 * Big * Size * U, Gilding, 4);
			}
		}
	};
	// The works: a long shed with a saw-toothed annex and a stack, the stack taller for a place of much industry.
	auto Works = [&](double A, double B, double Turn)
	{
		if (!CanBuild(A, B))
		{
			return;
		}
		const FVector At = Stand(A, B);
		const double R = FMath::DegreesToRadians(Yaw0 + Turn);
		auto Off = [&](double Along, double Across, double Up) { return At + FVector(FMath::Cos(R) * Along - FMath::Sin(R) * Across, FMath::Sin(R) * Along + FMath::Cos(R) * Across, Up) * U; };
		House(At, 1.5 * U, 0.8 * U, 0.5 * U, 0.22 * U, RedBrick, Sooty, Yaw0 + Turn);
		Box(Off(0.0, 0.95, 0.2), FVector(1.2, 0.7, 0.4) * U, RedBrick, Yaw0 + Turn);
		for (int32 Tooth = 0; Tooth < 3 && !bFar; ++Tooth)
		{
			Box(Off((Tooth - 1) * 0.4, 0.95, 0.46), FVector(0.3, 0.7, 0.1) * U, Glazing, Yaw0 + Turn, 18.0);
		}
		const double Tall = (1.3 + 0.1 * FMath::Min(Site.Factories, 6)) * U;
		Pillar(Off(0.95, 0.0, 0.0), 0.13 * U, 0.09 * U, Tall, RedBrick, 7);
		if (OutChimneys)
		{
			OutChimneys->Add(Off(0.95, 0.0, 0.0) + FVector(0.0, 0.0, Tall));
		}
	};

	// (a landmark in a packed capital is built smaller, by as much as its own breadth asks, to stand in its small square)
	static const double FitsSmallSquare[9] = { 0.86, 0.9, 0.71, 0.84, 0.86, 0.68, 0.77, 0.6, 0.86 };
	const double LU = U * (Site.bPacked ? FitsSmallSquare[FMath::Clamp(Site.Landmark, 0, 8)] : 1.0);
	// ---- A capital's own great building, known from the air. It stands in the great square.
	auto Landmark = [&](int32 Kind, double StandA)
	{
		// (it stands on the far side of its square and is turned once to face the eye as a campaign begins, which
		// looks north: its front is to the south. The long hall and the arena are stood broadside on.)
		const double LYaw = Yaw0 + 180.0 + (Kind == 5 || Kind == 7 ? 90.0 : 0.0);
		const FVector At = Stand(StandA, 0.0, 0.1);
		const double R = FMath::DegreesToRadians(LYaw);
		auto Off = [&](double Along, double Across, double Up) { return At + FVector(FMath::Cos(R) * Along - FMath::Sin(R) * Across, FMath::Sin(R) * Along + FMath::Cos(R) * Across, Up) * LU; };
		auto Palace = [&](double Along, double Across)
		{
			// A long front with a wing at each end, a row of columns before it, and the dome.
			Box(Off(Along, Across, 0.45), FVector(1.5, 2.3, 0.9) * LU, Ashlar, LYaw);
			for (const double Wing : { -1.0, 1.0 })
			{
				Box(Off(Along + 0.25, Across + Wing * 1.35, 0.38), FVector(1.9, 0.55, 0.76) * LU, Ashlar, LYaw);
				Box(Off(Along + 0.25, Across + Wing * 1.35, 0.79), FVector(2.0, 0.65, 0.06) * LU, LeadGrey, LYaw);
			}
			for (int32 Column = 0; Column < 6; ++Column)
			{
				Pillar(Off(Along + 0.86, Across + (Column - 2.5) * 0.36, 0.08), 0.06 * LU, 0.05 * LU, 0.72 * LU, Ashlar, 6);
			}
			Box(Off(Along + 0.86, Across, 0.84), FVector(0.24, 2.1, 0.1) * LU, Ashlar, LYaw);
			Box(Off(Along, Across, 0.93), FVector(1.6, 2.4, 0.06) * LU, LeadGrey, LYaw);
			Pillar(Off(Along, Across, 0.9), 0.5 * LU, 0.5 * LU, 0.35 * LU, Ashlar, 12);
			Ball(Off(Along, Across, 1.25), FVector(1.0, 1.0, 0.9) * LU, Verdigris, 5, 10);
			Pillar(Off(Along, Across, 1.65), 0.06 * LU, 0.0, 0.5 * LU, Gilding, 6);
			if (AllWindows)
			{
				for (int32 K = 0; K < 7; ++K)
				{
					AllWindows->Add(Off(Along + 0.76, Across + (K - 3) * 0.3, 0.52));
					AllWindows->Add(FVector(FMath::Cos(R), FMath::Sin(R), 0.0));
				}
			}
		};
		switch (Kind)
		{
		case 1: // a cathedral with two west towers
			House(Off(0.0, 0.0, 0.0), 2.8 * LU, 0.9 * LU, 1.0 * LU, 0.62 * LU, Ashlar, LeadGrey, LYaw);
			House(Off(-0.35, 0.0, 0.0), 0.8 * LU, 1.9 * LU, 0.95 * LU, 0.55 * LU, Ashlar, LeadGrey, LYaw);
			for (const double Side : { -1.0, 1.0 })
			{
				Box(Off(1.5, Side * 0.42, 1.0), FVector(0.5, 0.5, 2.0) * LU, Ashlar, LYaw);
				Pillar(Off(1.5, Side * 0.42, 2.0), 0.36 * LU, 0.0, 1.1 * LU, LeadGrey, 4);
			}
			Pillar(Off(-0.35, 0.0, 1.5), 0.12 * LU, 0.0, 0.9 * LU, Verdigris, 6);
			break;
		case 2: // a castle: a keep within four towers and a wall, on its mound
			Pillar(Off(0.0, 0.0, 0.0), 2.0 * LU, 1.75 * LU, 0.3 * LU, Lawn, 14);
			for (int32 K = 0; K < 4; ++K)
			{
				const double SA = K < 2 ? 1.0 : -1.0, SB = K % 2 == 0 ? 1.0 : -1.0;
				Pillar(Off(SA * 1.1, SB * 1.1, 0.3), 0.3 * LU, 0.28 * LU, 1.0 * LU, Ashlar, 8);
				Pillar(Off(SA * 1.1, SB * 1.1, 1.3), 0.36 * LU, 0.0, 0.5 * LU, Rust, 8);
				Box(Off(K < 2 ? SA * 1.1 : 0.0, K < 2 ? 0.0 : SB * 1.1, 0.62), (K < 2 ? FVector(0.16, 2.2, 0.64) : FVector(2.2, 0.16, 0.64)) * LU, Ashlar, LYaw);
			}
			Box(Off(-1.1, 0.0, 0.62), FVector(0.16, 2.2, 0.64) * LU, Ashlar, LYaw);
			Box(Off(0.0, -1.1, 0.62), FVector(2.2, 0.16, 0.64) * LU, Ashlar, LYaw);
			Box(Off(0.0, 0.0, 1.05), FVector(0.95, 0.95, 1.5) * LU, Ashlar, LYaw);
			Pillar(Off(0.0, 0.0, 1.8), 0.72 * LU, 0.0, 0.7 * LU, Rust, 4);
			Pillar(Off(0.0, 0.0, 2.5), 0.03 * LU, 0.0, 0.5 * LU, Gilding, 4);
			break;
		case 3: // a parliament house: a long front with a dome and end pavilions, and a column before it
			Box(Off(-0.4, 0.0, 0.42), FVector(1.0, 3.4, 0.84) * LU, Ashlar, LYaw);
			House(Off(-0.4, 0.0, 0.84), 0.9 * LU, 3.3 * LU, 0.02 * LU, 0.3 * LU, Ashlar, Rust, LYaw + 90.0);
			for (const double End : { -1.0, 1.0 })
			{
				Box(Off(-0.4, End * 1.6, 0.6), FVector(1.25, 0.5, 1.2) * LU, Ashlar, LYaw);
				Pillar(Off(-0.4, End * 1.6, 1.2), 0.42 * LU, 0.0, 0.6 * LU, Rust, 4);
			}
			Pillar(Off(-0.4, 0.0, 0.84), 0.42 * LU, 0.42 * LU, 0.5 * LU, Ashlar, 12);
			Ball(Off(-0.4, 0.0, 1.5), FVector(0.86, 0.86, 1.0) * LU, Rust, 5, 10);
			Pillar(Off(-0.4, 0.0, 2.0), 0.04 * LU, 0.0, 0.6 * LU, Gilding, 5);
			Pillar(Off(1.3, 0.0, 0.0), 0.34 * LU, 0.3 * LU, 0.2 * LU, Ashlar, 10);
			Pillar(Off(1.3, 0.0, 0.2), 0.1 * LU, 0.08 * LU, 2.0 * LU, Ashlar, 8);
			Ball(Off(1.3, 0.0, 2.32), FVector(0.24, 0.24, 0.3) * LU, Gilding, 4, 6);
			break;
		case 4: // a tower of iron lattice on four legs, and the palace beyond it
		{
			Palace(-1.2, 0.0);
			const FLinearColor Iron(FColor::FromHex(TEXT("4A4038")));
			const FVector Foot = Off(1.05, 0.0, 0.0);
			auto Up = [&](double Along, double Across, double High) { return Foot + FVector(FMath::Cos(R) * Along - FMath::Sin(R) * Across, FMath::Sin(R) * Along + FMath::Cos(R) * Across, High) * LU; };
			for (int32 K = 0; K < 4; ++K)
			{
				const double SA = K < 2 ? 1.0 : -1.0, SB = K % 2 == 0 ? 1.0 : -1.0;
				Rod(Up(SA * 0.8, SB * 0.8, 0.0), Up(SA * 0.34, SB * 0.34, 1.3), 0.07 * LU, Iron, 4);
				Rod(Up(SA * 0.34, SB * 0.34, 1.3), Up(SA * 0.14, SB * 0.14, 2.6), 0.055 * LU, Iron, 4);
				Rod(Up(SA * 0.14, SB * 0.14, 2.6), Up(0.0, 0.0, 4.4), 0.04 * LU, Iron, 4);
				Rod(Up(SA * 0.8, SB * 0.8, 0.0), Up(K < 2 ? SA * 0.5 : 0.0, K < 2 ? 0.0 : SB * 0.5, 0.8), 0.04 * LU, Iron, 4);
			}
			Box(Up(0.0, 0.0, 1.3), FVector(0.86, 0.86, 0.1) * LU, Iron, LYaw);
			Box(Up(0.0, 0.0, 2.6), FVector(0.42, 0.42, 0.08) * LU, Iron, LYaw);
			Pillar(Up(0.0, 0.0, 4.4), 0.03 * LU, 0.0, 0.5 * LU, Iron, 4);
			break;
		}
		case 5: // a clock tower beside a long hall on the water-front
			House(Off(-0.2, 0.0, 0.0), 3.2 * LU, 0.85 * LU, 0.9 * LU, 0.3 * LU, Ochre, LeadGrey, LYaw);
			Box(Off(1.75, 0.0, 1.5), FVector(0.52, 0.52, 3.0) * LU, Ochre, LYaw);
			Box(Off(1.75, 0.0, 3.1), FVector(0.62, 0.62, 0.36) * LU, Ashlar, LYaw);
			Pillar(Off(1.75, 0.0, 3.28), 0.44 * LU, 0.0, 0.8 * LU, LeadGrey, 4);
			Pillar(Off(1.75, 0.0, 4.08), 0.03 * LU, 0.0, 0.3 * LU, Gilding, 4);
			Box(Off(-1.9, 0.0, 0.7), FVector(0.5, 0.5, 1.4) * LU, Ochre, LYaw);
			Pillar(Off(-1.9, 0.0, 1.4), 0.36 * LU, 0.0, 0.5 * LU, LeadGrey, 4);
			break;
		case 6: // a walled citadel: red walls, towers with green caps, and gold domes within
		{
			const FLinearColor WallRed(FColor::FromHex(TEXT("8E3A30"))), CapGreen(FColor::FromHex(TEXT("3F6A52")));
			for (int32 K = 0; K < 4; ++K)
			{
				const double SA = K < 2 ? 1.0 : -1.0, SB = K % 2 == 0 ? 1.0 : -1.0;
				Pillar(Off(SA * 1.7, SB * 1.7, 0.0), 0.3 * LU, 0.26 * LU, 1.1 * LU, WallRed, 8);
				Pillar(Off(SA * 1.7, SB * 1.7, 1.1), 0.3 * LU, 0.0, 0.8 * LU, CapGreen, 8);
			}
			for (const double Side : { -1.0, 1.0 })
			{
				Box(Off(Side * 1.7, 0.0, 0.3), FVector(0.18, 3.4, 0.6) * LU, WallRed, LYaw);
				Box(Off(0.0, Side * 1.7, 0.3), FVector(3.4, 0.18, 0.6) * LU, WallRed, LYaw);
			}
			Box(Off(0.0, 0.0, 0.45), FVector(1.3, 1.1, 0.9) * LU, TowerWhite, LYaw);
			for (int32 Dome = 0; Dome < 5; ++Dome)
			{
				const double Along = Dome == 0 ? 0.0 : (Dome < 3 ? 0.42 : -0.42), Across = Dome == 0 ? 0.0 : (Dome % 2 == 1 ? 0.36 : -0.36), Big = Dome == 0 ? 1.0 : 0.62;
				Pillar(Off(Along, Across, 0.9), 0.16 * Big * LU, 0.15 * Big * LU, 0.34 * Big * LU, TowerWhite, 8);
				Ball(Off(Along, Across, 0.9 + 0.46 * Big), FVector(0.44, 0.44, 0.5) * Big * LU, Gilding, 5, 8);
				Pillar(Off(Along, Across, 0.9 + 0.7 * Big), 0.025 * LU, 0.0, 0.24 * Big * LU, Gilding, 4);
			}
			Box(Off(1.0, -0.9, 0.9), FVector(0.4, 0.4, 1.8) * LU, TowerWhite, LYaw);
			Ball(Off(1.0, -0.9, 1.95), FVector(0.36, 0.36, 0.42) * LU, Gilding, 5, 8);
			break;
		}
		case 7: // an arena of the ancients, and a domed temple beside it
		{
			const FLinearColor Travertine(FColor::FromHex(TEXT("CDBF9F"))), Sand(FColor::FromHex(TEXT("B79C6C")));
			Pillar(Off(0.9, 0.0, 0.0), 1.55 * LU, 1.5 * LU, 0.75 * LU, Travertine, 22);
			Pillar(Off(0.9, 0.0, 0.752), 1.2 * LU, 1.2 * LU, 0.01 * LU, FLinearColor(FColor::FromHex(TEXT("9A8D74"))), 22);
			Pillar(Off(0.9, 0.0, 0.764), 0.7 * LU, 0.7 * LU, 0.01 * LU, Sand, 22);
			Pillar(Off(-1.6, 0.0, 0.0), 0.8 * LU, 0.8 * LU, 0.62 * LU, Travertine, 16);
			Ball(Off(-1.6, 0.0, 0.62), FVector(1.5, 1.5, 0.8) * LU, FLinearColor(FColor::FromHex(TEXT("8E8A7C"))), 4, 14);
			House(Off(-0.75, 0.0, 0.0), 0.5 * LU, 1.0 * LU, 0.5 * LU, 0.2 * LU, Travertine, Travertine, LYaw + 90.0);
			break;
		}
		case 8: // a triumphal gate before the palace
			Palace(-1.0, 0.0);
			for (const double Side : { -1.0, 1.0 })
			{
				Box(Off(2.0, Side * 0.95, 0.6), FVector(0.5, 0.45, 1.2) * LU, Ashlar, LYaw);
			}
			for (int32 Column = 0; Column < 4; ++Column)
			{
				Pillar(Off(2.0, (Column - 1.5) * 0.36, 0.0), 0.07 * LU, 0.06 * LU, 1.1 * LU, Ashlar, 6);
			}
			Box(Off(2.0, 0.0, 1.25), FVector(0.55, 2.4, 0.3) * LU, Ashlar, LYaw);
			Box(Off(2.0, 0.0, 1.5), FVector(0.3, 0.6, 0.24) * LU, Verdigris, LYaw);
			break;
		default:
			Palace(0.0, 0.0);
			break;
		}
	};

	// ---- The harbour: a quay at the water's edge, warehouses and cranes on it, and ships alongside.
	if (Site.bQuay)
	{
		{
			// Where the water lies beyond the town's last houses, a road runs down to the quay.
			const FVector2D Way = Site.Quay - Site.Centre;
			const double Far = Way.Size();
			if (Far > Reach + 1.0)
			{
				// (the map's east and south, turned into the plan's along and across)
				const double QA = -Way.Y * C0 + Way.X * S0, QB = Way.Y * S0 + Way.X * C0;
				const double From = (Reach - 0.4 * Um) / Far, To = (Far - 0.5 * Um) / Far;
				Street(QA * From, QB * From, QA * To, QB * To, 0.3, true);
				// ...lined with warehouses and sheds, now on this side and now on that, so that the port and its town are one place.
				const double RoadTurn = FMath::RadiansToDegrees(FMath::Atan2(QB, QA));
				double Side = 1.0;
				for (double T = From + 0.5 * Um / Far; T < To - 0.5 * Um / Far; T += 0.66 * Um / Far, Side = -Side)
				{
					const double HA = QA * T - QB / Far * Side * 0.62 * Um, HB = QB * T + QA / Far * Side * 0.62 * Um;
					if (CanBuild(HA, HB))
					{
						const bool bShed = Dice.FRand() < 0.5f;
						Dwelling(HA, HB, bShed ? 1.0 : 0.8, 0.5, bShed ? 0.34 : 0.42, bShed ? 0.16 : 0.26, bShed ? RedBrick : WallPaint(), bShed ? Sooty : RoofPaint(), RoadTurn);
					}
				}
			}
		}
		const double Sea = FMath::DegreesToRadians(Site.SeaBearing);
		const FVector Seaward(FMath::Cos(Sea), FMath::Sin(Sea), 0.0), Shoreline(-FMath::Sin(Sea), FMath::Cos(Sea), 0.0); // (on the table: north is +X, east +Y)
		const FVector Edge = Site.Ground(Site.Quay.X, Site.Quay.Y, 0.03);
		const double ShoreYaw = Site.SeaBearing + 90.0;
		Box(Edge - Seaward * (0.25 * U) + FVector(0.0, 0.0, 0.05 * U), FVector(3.4, 0.8, 0.1) * U, Cobbles, ShoreYaw);
		for (const double Along : { -0.9, 0.5 })
		{
			House(Edge - Seaward * (0.95 * U) + Shoreline * (Along * U), 1.1 * U, 0.5 * U, 0.34 * U, 0.16 * U, RedBrick, Sooty, ShoreYaw);
		}
		for (const double Along : { -1.2, 0.0, 1.2 })
		{
			if (bFar)
			{
				break; // (cranes are not seen from high up)
			}
			const FVector Foot = Edge + Shoreline * (Along * U) + FVector(0.0, 0.0, 0.1 * U);
			Pillar(Foot, 0.1 * U, 0.06 * U, 0.95 * U, Sooty, 4);
			Rod(Foot + FVector(0.0, 0.0, 0.9 * U), Foot + Seaward * (0.75 * U) + FVector(0.0, 0.0, 1.2 * U), 0.035 * U, Sooty, 4);
			Rod(Foot + FVector(0.0, 0.0, 0.9 * U), Foot - Seaward * (0.3 * U) + FVector(0.0, 0.0, 0.75 * U), 0.035 * U, Sooty, 4);
		}
		const FLinearColor HullBlack(FColor::FromHex(TEXT("2B2D31"))), FunnelRed(FColor::FromHex(TEXT("9A3A2E")));
		{
			// The lighthouse at the end of the quay: a white tower with a red band, a gallery and a lantern. (Its
			// light is lit by the map at night, where the country is at peace.)
			const FVector Light = Edge + Shoreline * (2.05 * U) - Seaward * (0.1 * U) + FVector(0.0, 0.0, 0.08 * U);
			Pillar(Light, 0.26 * U, 0.22 * U, 0.12 * U, Cobbles, 8);
			Pillar(Light + FVector(0.0, 0.0, 0.12 * U), 0.19 * U, 0.12 * U, 1.5 * U, TowerWhite, 8);
			Pillar(Light + FVector(0.0, 0.0, 0.62 * U), 0.172 * U, 0.152 * U, 0.34 * U, FunnelRed, 8);
			Pillar(Light + FVector(0.0, 0.0, 1.62 * U), 0.2 * U, 0.2 * U, 0.05 * U, Sooty, 8);
			Pillar(Light + FVector(0.0, 0.0, 1.67 * U), 0.1 * U, 0.1 * U, 0.2 * U, FLinearColor(FColor::FromHex(TEXT("E9D9A0"))), 6);
			Pillar(Light + FVector(0.0, 0.0, 1.87 * U), 0.14 * U, 0.0, 0.15 * U, Sooty, 6);
			if (OutBeacon)
			{
				*OutBeacon = Light + FVector(0.0, 0.0, 1.78 * U);
			}
		}
		// Two merchant ships lying alongside: a black hull, a white house amidships, a funnel and two masts.
		for (const double Along : { -1.05, 1.15 })
		{
			const FVector Berth = Edge + Seaward * (0.72 * U) + Shoreline * (Along * U);
			const FVector Afloat(Berth.X, Berth.Y, Edge.Z - 0.02 * U);
			const double Long = 1.9 * U;
			Slab(Afloat, Long, ShoreYaw, HullPlan(0.2), 0.0, 0.085, HullBlack);
			Slab(Afloat, Long, ShoreYaw, HullPlan(0.16), 0.085, 0.092, DeckPlanks);
			Box(Afloat + Shoreline * (-0.08 * Long) + FVector(0.0, 0.0, 0.13 * Long), FVector(0.2, 0.13, 0.08) * Long, TowerWhite, ShoreYaw);
			Pillar(Afloat + Shoreline * (-0.12 * Long) + FVector(0.0, 0.0, 0.17 * Long), 0.028 * Long, 0.024 * Long, 0.1 * Long, FunnelRed, 7);
			for (const double Mast : { -0.32, 0.26 })
			{
				if (!bFar)
				{
					Pillar(Afloat + Shoreline * (Mast * Long) + FVector(0.0, 0.0, 0.09 * Long), 0.008 * Long, 0.005 * Long, 0.2 * Long, Sooty, 4);
				}
			}
		}
	}

	// ---- A small place, and a town: a church and its streets.
	if (Site.Tier <= 1)
	{
		const bool bTown = Site.Tier == 1;
		const double End = Reach - 0.55 * Um;
		int32 FarCount = 0;
		Street(-End, 0.0, End, 0.0, 0.2);
		if (bTown)
		{
			Street(0.0, -End * 0.8, 0.0, End * 0.8, 0.18);
		}
		if (CanBuild(0.55 * Um, 0.62 * Um))
		{
			Church(0.55 * Um, 0.62 * Um, bTown ? 1.0 : 0.72, 0.0);
		}
		// Houses along the street, a row on either side, gable to the road; and along the cross street of a town.
		for (int32 Row = 0; Row < (bTown ? 2 : 1); ++Row)
		{
			for (const double Side : { -1.0, 1.0 })
			{
				for (double Along = -End + 0.5 * Um; Along < End - 0.2 * Um; Along += Dice.FRandRange(0.92f, 1.2f) * Um)
				{
					const double Back = Side * Dice.FRandRange(0.5f, 0.66f) * Um;
					const double A = Row == 0 ? Along : Back, B = Row == 0 ? Back : Along * 0.8;
					if ((Row == 1 && FMath::Abs(Along) < 0.75 * Um) || (Row == 0 && bTown && FMath::Abs(Along) < 0.5 * Um))
					{
						continue; // (the crossing is kept open)
					}
					if (FVector2D(A - 0.55 * Um, B - 0.62 * Um).SizeSquared() < FMath::Square(0.95 * Um) || !CanBuild(A, B))
					{
						continue; // the church stands there
					}
					// (from high up every other house of a small place is enough, each a little larger)
					bMute = bFar && (++FarCount % 2 == 0);
					Dwelling(A, B, Dice.FRandRange(0.62f, 0.9f) * (bFar ? 1.3 : 1.0), Dice.FRandRange(0.42f, 0.56f) * (bFar ? 1.2 : 1.0), Dice.FRandRange(0.3f, 0.48f), Dice.FRandRange(0.22f, 0.34f), WallPaint(), RoofPaint(), (Row == 0 ? 90.0 : 0.0) + Dice.FRandRange(-8.0f, 8.0f));
					bMute = false;
					if (Dice.FRand() < 0.4f)
					{
						Tree(A + (Row == 0 ? 0.0 : Side * 0.6 * Um), B + (Row == 0 ? Side * 0.62 * Um : 0.0));
					}
				}
			}
		}
		for (int32 K = 0; K < FMath::Min(Site.Factories, 2); ++K)
		{
			Works(-End * 0.55 + K * 1.9 * Um, -End * 0.78, 0.0);
		}
		if (OutQuarters)
		{
			OutQuarters->Add(Site.Centre);
		}
		return;
	}

	// ---- A city, and a capital: blocks on a grid of streets, closed and tall at the heart, lower terraces round
	// that, and houses standing apart at the edge. Its two avenues cross in its square, running between the blocks;
	// its quarters do not all lie the same way; and its outline is no circle: the town runs out along its roads
	// and railways and stops short between them.
	const bool bCapital = Site.Tier >= 3;
	const bool bPacked = Site.bPacked;
	const double PitchA = 1.95 * Um, PitchB = 1.5 * Um; // a block and its street, along and across
	const double BlockA = PitchA - 0.3 * Um, BlockB = PitchB - 0.28 * Um; // the block itself
	// The square: two blocks deep, and four wide before a capital's landmark.
	const int32 SquareDeep = 2, SquareWide = bCapital && !bPacked ? 4 : 2;
	const double SquareHalfA = SquareDeep * 0.5 * PitchA, SquareHalfB = SquareWide * 0.5 * PitchB;
	const double Heart = bCapital ? 0.5 : 0.44, Middle = 0.78;
	// How far the town runs on each bearing: far along the roads that leave it for the provinces round it, a
	// little less along its own avenues, and well short between them. (The works lie out along the west avenue and
	// the station along the east, so those two run further than the others.)
	auto ReachAt = [&](double Bearing)
	{
		if (bPacked)
		{
			return Reach; // (a town packed into a small province fills what ground it has)
		}
		double Star = 0.0;
		for (const double Arm : Site.Arms)
		{
			Star = FMath::Max(Star, FMath::Exp(-FMath::Square(FRotator::NormalizeAxis(Bearing - Arm) / 15.0)));
		}
		for (int32 K = 0; K < 4; ++K)
		{
			const double Weight = K == 3 && Site.Factories > 0 ? 1.0 : (K == 1 ? 0.75 : 0.5);
			Star = FMath::Max(Star, Weight * FMath::Exp(-FMath::Square(FRotator::NormalizeAxis(Bearing - (Yaw0 + 90.0 * K)) / 15.0)));
		}
		// (a capital has a second ring of terraces between its arms, and its arms run a little further)
		const double Out = Reach * (bCapital ? 0.78 + 0.44 * Star : 0.64 + 0.5 * Star);
		// (the armies of the province stand to the north of its town, clear of its roofs: on that side no arm
		// runs out past the town's own measure to meet them)
		return FMath::Abs(FRotator::NormalizeAxis(Bearing - 15.0)) < 50.0 ? FMath::Min(Out, Reach * 1.06) : Out;
	};
	// The square is not all open ground: about a third of it is given to blocks. Before a capital's landmark the
	// near half of the square is only as wide as the avenue's two blocks, with a block standing in each outer
	// corner (where the landmark is not one that fills the square itself); and in a city a block stands in the
	// corner of the square beside its great church.
	const bool bLandmarkFills = Site.Landmark == 2 || Site.Landmark == 6 || Site.Landmark == 7;
	auto BuiltInSquare = [&](double A, double B)
	{
		if (bPacked)
		{
			return false;
		}
		return bCapital ? (!bLandmarkFills && A < 0.0 && FMath::Abs(B) > PitchB) : (A > 0.0 && B > 0.0);
	};
	auto InSquare = [&](double A, double B) { return FMath::Abs(A) < SquareHalfA && FMath::Abs(B) < SquareHalfB; };
	// (In a packed capital the square is left open on the side the eye looks from, a forecourt one block deep, so
	// that the tall blocks round it do not stand in front of its landmark.)
	auto InForecourt = [&](double A, double B) { return bPacked && bCapital && A < -SquareHalfA && A > -SquareHalfA - PitchA && FMath::Abs(B) < SquareHalfB; };
	const FLinearColor CobbleDark(FColor::FromHex(TEXT("777166"))), Earth(FColor::FromHex(TEXT("67664F")));
	const FLinearColor Leads = Look == 2 || Look == 4 ? FLinearColor(FColor::FromHex(RoofsOf[Look][0])) : LeadGrey;

	// The square, its two avenues, and what stands in it.
	Street(-ReachAt(Yaw0 + 180.0) + 0.3 * Um, 0.0, ReachAt(Yaw0) - 0.3 * Um, 0.0, 0.3);
	Street(0.0, -ReachAt(Yaw0 + 270.0) + 0.3 * Um, 0.0, ReachAt(Yaw0 + 90.0) - 0.3 * Um, 0.3);
	{
		// (paved a block at a time, all at the height of its middle, leaving out the corners that are built on)
		const FVector Mid = Stand(0.0, 0.0, 0.0);
		for (int32 J = -SquareWide / 2; J < SquareWide / 2; ++J)
		{
			for (int32 I = -SquareDeep / 2; I < SquareDeep / 2; ++I)
			{
				const double A = (I + 0.5) * PitchA, B = (J + 0.5) * PitchB;
				if (!BuiltInSquare(A, B))
				{
					const FVector At = Stand(A, B, 0.0);
					Box(FVector(At.X, At.Y, Mid.Z), FVector(PitchA * PerMap * 1.02, PitchB * PerMap * 1.02, 0.22 * U), Paving, Yaw0);
				}
			}
		}
	}
	if (bPacked && bCapital)
	{
		Box(Stand(-SquareHalfA - 0.5 * PitchA, 0.0, 0.0), FVector(PitchA * PerMap * 1.02, SquareWide * PitchB * PerMap * 1.02, 0.2 * U), Paving, Yaw0);
	}
	if (bCapital)
	{
		Landmark(Site.Landmark, SquareHalfA * 0.24);
	}
	else
	{
		// The great church on the far side of the square, its tower to the fore; and the town hall beside it.
		Church(SquareHalfA * 0.42, -SquareHalfB * 0.28, 1.6, 180.0);
		Dwelling(-SquareHalfA * 0.42, SquareHalfB * 0.4, 1.1, 0.62, 0.64, 0.26, Ochre, Rust, 90.0, true);
		Box(Stand(-SquareHalfA * 0.42, SquareHalfB * 0.4) + FVector(0.0, 0.0, 0.98 * U), FVector(0.22, 0.22, 0.4) * U, Ochre, Yaw0);
		Pillar(Stand(-SquareHalfA * 0.42, SquareHalfB * 0.4) + FVector(0.0, 0.0, 1.18 * U), 0.17 * U, 0.0, 0.3 * U, Verdigris, 4);
	}
	// The station, out along the east avenue: a long hall with a glazed roof and a clock turret.
	const double StationA = 0.85 * Um, StationB = ReachAt(Yaw0 + 90.0) * 0.8;
	const bool bStation = !bPacked && CanBuild(StationA, StationB);
	if (bStation)
	{
		Dwelling(StationA, StationB, 1.75, 0.62, 0.42, 0.2, Ochre, Glazing, 90.0, true);
		Box(Stand(StationA, StationB) + FVector(0.0, 0.0, 0.64 * U), FVector(0.24, 0.24, 0.44) * U, Ochre, Yaw0);
		Pillar(Stand(StationA, StationB) + FVector(0.0, 0.0, 0.86 * U), 0.19 * U, 0.0, 0.22 * U, LeadGrey, 4);
	}
	auto AtStation = [&](double A, double B) { return bStation && FMath::Square(A - StationA) + FMath::Square(B - StationB) < FMath::Square(1.3 * Um); };
	auto InWorksQuarter = [&](double Ring, double Bearing) { return Ring > 0.48 && FMath::Abs(FRotator::NormalizeAxis(Bearing - (Yaw0 + 270.0))) < 30.0; };

	const int32 Span = FMath::CeilToInt32(Reach * 1.1 / FMath::Min(PitchA, PitchB)) + 1;
	// The parish churches, each in its own quarter of the town and well inside it, so that with the great church
	// (or a capital's landmark) two or three spires stand over the roofs. Their blocks are chosen before any is built.
	TArray<FIntPoint> Parishes;
	{
		const int32 Wanted = bCapital && !bPacked ? 3 : 2;
		static const double QuarterA[3] = { 1.0, -1.0, 1.0 }, QuarterB[3] = { 1.0, -1.0, -1.0 };
		for (int32 Q = 0; Q < Wanted; ++Q)
		{
			double Best = 1.0e9;
			FIntPoint Pick(MAX_int32, 0);
			for (int32 J = -Span; J < Span; ++J)
			{
				for (int32 I = -Span; I < Span; ++I)
				{
					const double A = (I + 0.5) * PitchA, B = (J + 0.5) * PitchB;
					if (A * QuarterA[Q] <= 0.0 || B * QuarterB[Q] <= 0.0 || InSquare(A, B) || InForecourt(A, B))
					{
						continue;
					}
					const double Far = FMath::Sqrt(A * A + B * B), Ring = Far / Reach, Bearing = BearingOf(A, B);
					if (Far > ReachAt(Bearing) * 0.9 - 0.35 * Um || !CanBuild(A, B) || AtStation(A, B) || (!bPacked && Site.Factories > 0 && InWorksQuarter(Ring - 0.06, Bearing)))
					{
						continue;
					}
					const double Score = FMath::Abs(Ring - (bPacked ? 0.66 : 0.46));
					if (Score < Best)
					{
						Best = Score;
						Pick = FIntPoint(I, J);
					}
				}
			}
			if (Pick.X != MAX_int32)
			{
				Parishes.Add(Pick);
			}
		}
	}

	int32 FactoriesLeft = bPacked ? 0 : FMath::Clamp(Site.Factories, 0, 6);
	TArray<FVector2D> Bright;
	for (int32 J = -Span; J < Span; ++J)
	{
		for (int32 I = -Span; I < Span; ++I)
		{
			const double A = (I + 0.5) * PitchA, B = (J + 0.5) * PitchB;
			const bool bSquareCorner = InSquare(A, B) && BuiltInSquare(A, B);
			if ((InSquare(A, B) && !bSquareCorner) || InForecourt(A, B))
			{
				continue; // the square
			}
			const double Far = FMath::Sqrt(A * A + B * B), Ring = Far / Reach, Bearing = BearingOf(A, B);
			const double Limit = ReachAt(Bearing);
			if (Far > Limit * (0.96 + 0.08 * Dice.FRand()) - 0.35 * Um || !CanBuild(A, B))
			{
				continue; // beyond the town, or in the water, or ground kept for something else
			}
			if (AtStation(A, B))
			{
				continue; // the station stands there
			}
			const bool bParish = Parishes.Contains(FIntPoint(I, J));
			// The works quarter, out along the west avenue: a shed and a stack for each factory.
			if (FactoriesLeft > 0 && !bParish && InWorksQuarter(Ring, Bearing))
			{
				Works(A, B, (FactoriesLeft % 2) * 90.0);
				--FactoriesLeft;
				continue;
			}
			// How near this block is to the town's edge on its own bearing: the town thins out there, wherever
			// that edge lies.
			const double Local = Far / Limit;
			// The floor of the town: cobbles that meet the next block's at the heart, and toward the edge a smaller
			// and lower patch of trodden earth under each block, so that the town has no hard square edge.
			const double Fade = bPacked ? 0.8 * FMath::SmoothStep(0.66, 1.0, Ring) : FMath::Max(FMath::SmoothStep(Heart * 0.85, Middle, Ring), FMath::SmoothStep(0.6, 0.94, Local));
			if (Fade < 0.97)
			{
				Box(Stand(A, B, 0.0), FVector(FMath::Lerp(PitchA, BlockA, Fade) * PerMap * 1.02, FMath::Lerp(PitchB, BlockB, Fade) * PerMap * 1.02, FMath::Lerp(0.2, 0.06, Fade) * U),
					FMath::Lerp(CobbleDark, Earth, static_cast<float>(Fade)), Yaw0);
			}
			// A parish church, its copper spire over the roofs.
			if (bParish)
			{
				Church(A, B, 1.42, 180.0, true);
				continue;
			}
			// Which way this quarter's rows lie: one of two directions at right angles, a district at a time.
			const bool bTurned = ((((I >> 1) * 73856093) ^ ((J >> 1) * 19349663) ^ (Site.Seed * 83492791)) & 4) != 0;
			// (A town packed into a small province is built tight and tall: closed blocks round its square, half as
			// tall again, and tall terraces to its edge, with no parks and no houses standing apart.)
			const bool bClosed = bSquareCorner || (bPacked ? (FMath::Abs(A) < SquareHalfA + PitchA && FMath::Abs(B) < SquareHalfB + PitchB) : (Ring < Heart && Local < 0.86));
			const double Storeys = bPacked ? 1.3 : 1.0;
			if (bClosed)
			{
				// The heart: a closed block, four wings round a courtyard, taller toward the square.
				const double Tall = bPacked ? 1.45 : FMath::Lerp(1.25, 0.88, FMath::Min(Ring / Heart, 1.0));
				bMute = bFar;
				for (const double Side : { -1.0, 1.0 })
				{
					Dwelling(A + Side * (BlockA * 0.5 - 0.21 * Um), B, BlockB / Um, 0.42, Tall * Dice.FRandRange(0.86f, 1.1f), 0.14, WallPaint(), Leads, 90.0);
					Dwelling(A, B + Side * (BlockB * 0.5 - 0.21 * Um), BlockA / Um - 0.86, 0.42, Tall * Dice.FRandRange(0.86f, 1.1f), 0.14, WallPaint(), Leads, 0.0);
				}
				bMute = false;
				if (bFar)
				{
					// (from high up: the whole block as one piece)
					House(Stand(A, B), BlockA / Um * U, BlockB / Um * U, Tall * U, (bFlatRoofs ? 0.04 : 0.14) * U, PaintAt(WallsOf, I, J), Leads, Yaw0);
				}
				if (Dice.FRand() < 0.34f)
				{
					Bright.Add(MapAt(A, B));
				}
			}
			else if (bPacked || Local < (bCapital ? 0.88 : 0.8))
			{
				if (!bPacked && Dice.FRand() < 0.08f)
				{
					// A park: a lawn with trees round it.
					Box(Stand(A, B, 0.015), FVector(1.6, 1.15, 0.03) * U, Lawn, Yaw0);
					for (int32 K = 0; K < 4; ++K)
					{
						Tree(A + (K < 2 ? -0.5 : 0.5) * Um, B + (K % 2 == 0 ? -0.34 : 0.34) * Um);
					}
					continue;
				}
				bMute = bFar;
				if (bFar)
				{
					// (from high up: each row of a terrace as one long house)
					for (const double Side : { -1.0, 1.0 })
					{
						if (!bTurned)
						{
							House(Stand(A, B + Side * 0.3 * Um), 1.54 * U, 0.42 * U, Storeys * 0.5 * U, (bFlatRoofs ? 0.04 : 0.24) * U, PaintAt(WallsOf, I, J + (Side > 0.0 ? 1 : 0)), PaintAt(RoofsOf, I + (Side > 0.0 ? 2 : 0), J), Yaw0);
						}
						else
						{
							House(Stand(A + Side * 0.42 * Um, B), 1.12 * U, 0.5 * U, Storeys * 0.5 * U, (bFlatRoofs ? 0.04 : 0.24) * U, PaintAt(WallsOf, I, J + (Side > 0.0 ? 1 : 0)), PaintAt(RoofsOf, I + (Side > 0.0 ? 2 : 0), J), Yaw0 + 90.0);
						}
					}
				}
				if (!bTurned)
				{
					// Terraces along the street: two rows of three houses, their gardens between them.
					for (const double Side : { -1.0, 1.0 })
					{
						for (const double Along : { -0.52, 0.0, 0.52 })
						{
							Dwelling(A + Along * Um, B + Side * 0.3 * Um, 0.5, 0.42, Storeys * Dice.FRandRange(0.4f, 0.62f), Dice.FRandRange(0.2f, 0.28f), WallPaint(), RoofPaint(), 0.0);
						}
					}
				}
				else
				{
					// The same turned a quarter round: two rows across the block.
					for (const double Side : { -1.0, 1.0 })
					{
						for (const double Across : { -0.31, 0.31 })
						{
							Dwelling(A + Side * 0.42 * Um, B + Across * Um, 0.56, 0.5, Storeys * Dice.FRandRange(0.4f, 0.62f), Dice.FRandRange(0.2f, 0.28f), WallPaint(), RoofPaint(), 90.0);
						}
					}
				}
				bMute = false;
			}
			else
			{
				// The edge: houses standing apart, each its own way, with trees in their gardens.
				const int32 Homes = Dice.RandRange(1, 3);
				for (int32 K = 0; K < Homes; ++K)
				{
					const double HA = A + Dice.FRandRange(-0.55f, 0.55f) * Um, HB = B + Dice.FRandRange(-0.4f, 0.4f) * Um;
					if (CanBuild(HA, HB))
					{
						bMute = bFar && K > 0;
						Dwelling(HA, HB, Dice.FRandRange(0.62f, 0.9f), Dice.FRandRange(0.42f, 0.56f), Dice.FRandRange(0.3f, 0.46f), Dice.FRandRange(0.22f, 0.32f), WallPaint(), RoofPaint(), Dice.FRandRange(0.0f, 180.0f));
						bMute = false;
					}
				}
				if (Dice.FRand() < 0.6f)
				{
					Tree(A + Dice.FRandRange(-0.7f, 0.7f) * Um, B + Dice.FRandRange(-0.5f, 0.5f) * Um);
				}
			}
		}
	}
	// (Where the plots are kept the ground is left open, a common inside the town, until something is built on it.)
	if (OutQuarters)
	{
		OutQuarters->Add(Site.Centre);
		const int32 Most = bCapital ? 5 : 2;
		for (int32 K = 0; K < Bright.Num() && K < Most; ++K)
		{
			OutQuarters->Add(Bright[K]);
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
	bool bHeavyTanks = false; // a heavy tank corps is among them
	for (const FString& Kind : Kinds)
	{
		if (Kind == TEXT("arm")) ++Armour;
		else if (Kind == TEXT("heavy")) { ++Armour; bHeavyTanks = true; }
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
			// (a heavy tank corps of a nation that has a heavy tank modelled shows that one)
			// (while the trial tank is being judged, it stands in for the nation's own on every armoured base)
			if (bTrialTank && Sculpted(TEXT("TrialTank"), Tag, Where, Metre, Turn))
			{
				break;
			}
			if (!(bHeavyTanks && Tag == TEXT("GER") && Vehicle(TEXT("Tiger"), Where, Metre, Turn, false)) && !MainTank(Where, Metre, Tag, Turn, false))
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
	// (Art fix 12: five figures to a base, as a wargamer mounts them, each a little over life size beside the
	// vehicles so that it reads from working height: three to the fore and two behind.)
	const double FigureSize = bHeavy || bOldPieces ? 1.0 : 1.38;
	if (!bHeavy && bOldPieces)
	{
		for (int32 Rank = 0; Rank < 3; ++Rank)
		{
			for (int32 File = 0; File < 4; ++File)
			{
				Places.Add(FVector2D(-1.7 + Rank * 1.35, (File - 1.5) * 1.2 + (Rank % 2 ? 0.25 : 0.0)));
			}
		}
	}
	else if (!bHeavy)
	{
		Places = { { -1.55, -2.1 }, { -1.75, 0.0 }, { -1.55, 2.1 }, { 0.95, -1.1 }, { 0.95, 1.1 } };
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
	int32 Men = FMath::Min(bHeavy ? 2 + 2 * (Foot + Mountain) : (bOldPieces ? 4 + 4 * (Foot + Mountain) : 5), Places.Num() - Slot);
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
			Pioneer(Tag, On(Places[Slot].X, Places[Slot].Y), Metre * FigureSize, ((K * 7) % 5 - 2) * 5.0, false);
		}
		else
		{
			Soldier(Tag, On(Places[Slot].X, Places[Slot].Y), Metre * FigureSize, ((K * 7) % 5 - 2) * 5.0, false);
		}
	}
}
