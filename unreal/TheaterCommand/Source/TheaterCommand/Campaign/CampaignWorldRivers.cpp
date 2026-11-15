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
	// (Up to a river of the Rhine's or the Volga's size it is as Europe's own are drawn. The few far greater ones, the
	// Mississippi, the Congo, the Amazon, are broader again: the Amazon is ten kilometres of water.)
	const float Doublings = FMath::Log2(FMath::Max(Water, 3500.0f) / 3500.0f);
	return FMath::Min(0.14f + 0.075f * FMath::Min(Doublings, 6.0f) + 0.22f * FMath::Max(Doublings - 6.0f, 0.0f), 1.6f);
}

void FCampaignWorldRivers::Load()
{
	Courses.Reset();
	TArray<uint8> Bytes;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Campaign/rivers-world.bin"));
	if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || Bytes.Num() < 8 || FMemory::Memcmp(Bytes.GetData(), "TCR1", 4) != 0)
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
		FMemory::Memcpy(&Length, Bytes.GetData() + At, 4);
		At += 4;
		if (At + static_cast<int64>(Length) * 12 > Bytes.Num())
		{
			break;
		}
		FCourse& Course = Courses.AddDefaulted_GetRef();
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
	UE_LOG(LogTheater, Log, TEXT("Rivers of the world: %d courses, %d points"), Courses.Num(), Points);
}
