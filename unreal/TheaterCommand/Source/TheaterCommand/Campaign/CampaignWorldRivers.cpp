#include "Campaign/CampaignWorldRivers.h"

#include "TheaterCommand.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

const FCampaignWorldRivers& FCampaignWorldRivers::Get()
{
	static FCampaignWorldRivers Kept;
	static bool bLooked = false;
	if (!bLooked)
	{
		bLooked = true;
		Kept.Load();
	}
	return Kept;
}

float FCampaignWorldRivers::HalfBreadth(float Water)
{
	// (the least a course carries to be in the file at all is 3,500)
	// (The art director's ruling of round 25: three ranks by the water a course carries. From a river of the Rhine's
	// or the Volga's size up it is as Europe's own are drawn, and the few far greater ones, the Mississippi, the
	// Congo, the Amazon, are broader again: the Amazon is ten kilometres of water. Everything much smaller is a
	// hairline; between the two a course swells from the one to the other.)
	const float Doublings = FMath::Log2(FMath::Max(Water, 3500.0f) / 3500.0f);
	if (Doublings >= 6.0f)
	{
		return FMath::Min(0.59f + 0.2f * (Doublings - 6.0f), 1.6f);
	}
	return FMath::Lerp(0.1f, 0.59f, FMath::SmoothStep(4.3f, 6.0f, Doublings));
}

float FCampaignWorldRivers::FCourse::BreadthFor(float WaterThere) const
{
	float Half = HalfBreadth(WaterThere);
	if (bGreat && Water.Num() > 0)
	{
		// (The art director's ruling of round 25: a great river keeps its breadth in every country. The Niger through
		// the Sahel is of the Nile's class, however little rain falls there. Only toward its source does it thin away.)
		Half = FMath::Max(Half, 0.62f * FMath::SmoothStep(0.04f, 0.25f, WaterThere / Water.Last()));
	}
	if (Joins != INDEX_NONE && Water.Num() > 0)
	{
		// (A tributary is at its broadest where it joins its river, and narrows from there up: more quickly than the
		// water it carries would have it, or a long tributary is one even band from end to end.)
		const float AtMouth = FMath::Max(HalfBreadth(Water.Last()), 0.1f);
		Half = FMath::Min(Half, FMath::Max(HalfCap * FMath::Pow(Half / AtMouth, 1.6f), 0.1f));
	}
	return FMath::Min(Half, HalfCap);
}

float FCampaignWorldRivers::FCourse::BreadthAt(int32 Point, float& OutWater) const
{
	OutWater = Water[Point];
	// (a river rises from nothing: its first few points are narrower)
	const float Half = BreadthFor(OutWater) * FMath::Min(1.0f, Point / 4.0f);
	if (bGreat)
	{
		OutWater = FMath::Max(OutWater, 200000.0f * FMath::SmoothStep(0.04f, 0.25f, OutWater / Water.Last()));
	}
	return Half;
}

void FCampaignWorldRivers::Load()
{
	Courses.Reset();
	TArray<uint8> Bytes;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Campaign/rivers-world.bin"));
	if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || Bytes.Num() < 8 || FMemory::Memcmp(Bytes.GetData(), "TCR2", 4) != 0)
	{
		UE_LOG(LogTheater, Display, TEXT("No rivers of the world at %s: the ground beyond the map has none."), *Path);
		return;
	}
	uint32 Count = 0;
	FMemory::Memcpy(&Count, Bytes.GetData() + 4, 4);
	int64 At = 8;
	int32 Points = 0;
	for (uint32 K = 0; K < Count && At + 4 <= Bytes.Num(); ++K)
	{
		uint32 Length = 0;
		int32 Joins = INDEX_NONE;
		float JoinsWater = 0.0f;
		if (At + 12 > Bytes.Num())
		{
			break;
		}
		FMemory::Memcpy(&Length, Bytes.GetData() + At, 4);
		FMemory::Memcpy(&Joins, Bytes.GetData() + At + 4, 4);
		FMemory::Memcpy(&JoinsWater, Bytes.GetData() + At + 8, 4);
		At += 12;
		if (At + static_cast<int64>(Length) * 12 > Bytes.Num())
		{
			break;
		}
		FCourse& Course = Courses.AddDefaulted_GetRef();
		Course.Joins = Joins;
		Course.JoinsWater = JoinsWater;
		Course.Points.SetNumUninitialized(Length);
		Course.Water.SetNumUninitialized(Length);
		Course.MinX = Course.MinY = TNumericLimits<float>::Max();
		Course.MaxX = Course.MaxY = TNumericLimits<float>::Lowest();
		for (uint32 P = 0; P < Length; ++P, At += 12)
		{
			float Three[3];
			FMemory::Memcpy(Three, Bytes.GetData() + At, 12);
			Course.Points[P] = FVector2f(Three[0], Three[1]);
			Course.Water[P] = Three[2];
			Course.MinX = FMath::Min(Course.MinX, Three[0]);
			Course.MaxX = FMath::Max(Course.MaxX, Three[0]);
			Course.MinY = FMath::Min(Course.MinY, Three[1]);
			Course.MaxY = FMath::Max(Course.MaxY, Three[1]);
		}
		Points += Length;
	}
	// The art director's ruling of round 25, three ranks: the river that reaches the sea is the great one; a
	// tributary is never more than about half as broad as the river it joins, where it joins it; the rest are hairlines.
	// (A river comes before its tributaries in the file, since it carries more water than any of them: the few that
	// do not are worked out when they are met.)
	TArray<int8> Done;
	Done.Init(0, Courses.Num());
	TFunction<void(int32, int32)> Rank = [&](int32 Index, int32 Depth)
	{
		if (Done[Index])
		{
			return;
		}
		Done[Index] = 1;
		FCourse& Course = Courses[Index];
		if (!Courses.IsValidIndex(Course.Joins) || Course.Joins == Index || Depth > 64)
		{
			Course.Joins = INDEX_NONE;
			Course.HalfCap = 1.6f;
			Course.bGreat = Course.Water.Num() > 0 && Course.Water.Last() >= 200000.0f;
			return;
		}
		Rank(Course.Joins, Depth + 1);
		Course.HalfCap = 0.36f * Courses[Course.Joins].BreadthFor(Course.JoinsWater);
	};
	int32 Rivers = 0;
	for (int32 Index = 0; Index < Courses.Num(); ++Index)
	{
		Rank(Index, 0);
		Rivers += Courses[Index].Joins == INDEX_NONE ? 1 : 0;
	}
	UE_LOG(LogTheater, Log, TEXT("Rivers of the world: %d courses (%d reach the sea or a basin), %d points"), Courses.Num(), Rivers, Points);
}
