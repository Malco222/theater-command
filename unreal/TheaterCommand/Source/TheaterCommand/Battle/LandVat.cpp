#include "Battle/LandVat.h"

#include "Battle/LandMen.h"
#include "Battle/LandVatPack.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "TextureResource.h"
#include "TheaterCommand.h"
#include "UObject/UObjectGlobals.h"

#include <vector>

namespace
{
	/** The most points a set may have here: a point's number rides the mesh as a half float, exact to 2,048. */
	constexpr int32 MostPoints = LandVatPack::MostPoints;
	/** The instance's own numbers: two poses and the second's share, the same a frame before, his face's tone. */
	constexpr int32 Numbers = 9;
	/** His skin's paint (LandMen's MakeLook: the face's tone multiplies it). */
	constexpr uint8 SkinPaint = 7;

	/**
	 * A mesh of the men's on Ink: the points (metres, his own axes, which BankedAt turns into Unreal's: see LandBattle's
	 * BankManMesh) times U, their facings, and the look's triangles and colours (sRGB bytes, made linear by the
	 * material). Posed (a body set at rest, M_LandCrowd): the first texture coordinate's U is 1 on his skin, and the
	 * second's U the point's number, by which the material finds its column of the poses' picture. Null where the points
	 * and the look do not agree.
	 */
	UStaticMesh* ManMesh(UObject* Owner, const float* At, const float* Facing, const LandMen::Look& Paint, UMaterialInterface* Ink, double U, bool bPosed)
	{
		const int32 Count = Paint.Points;
		if (!Owner || !Ink || !At || !Facing || Count <= 0 || Paint.Triangles.empty() || (bPosed && Count > MostPoints))
		{
			return nullptr;
		}
		FMeshDescription D;
		FStaticMeshAttributes Attributes(D);
		Attributes.Register();
		auto Positions = Attributes.GetVertexPositions();
		auto Normals = Attributes.GetVertexInstanceNormals();
		auto Tangents = Attributes.GetVertexInstanceTangents();
		auto Signs = Attributes.GetVertexInstanceBinormalSigns();
		auto Colours = Attributes.GetVertexInstanceColors();
		auto Uvs = Attributes.GetVertexInstanceUVs();
		Uvs.SetNumChannels(bPosed ? 2 : 1);
		const FPolygonGroupID Group = D.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Ink");
		D.ReserveNewVertices(Count);
		D.ReserveNewVertexInstances(Count);
		D.ReserveNewTriangles(static_cast<int32>(Paint.Triangles.size() / 3));
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const size_t K = static_cast<size_t>(I) * 3;
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(static_cast<float>(At[K] * U), static_cast<float>(At[K + 1] * U), static_cast<float>(At[K + 2] * U));
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			const FVector3f Normal = FVector3f(Facing[K], Facing[K + 1], Facing[K + 2]).GetSafeNormal(1e-6f, FVector3f::UpVector);
			Normals[Corner] = Normal;
			// (no normal map: any tangent across the normal will do; a posed man's facing is the material's own)
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
			Tangents[Corner] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal(1e-6f, FVector3f::ForwardVector);
			Signs[Corner] = 1.0f;
			// (the alpha: release 2's roughness by part where the look carries it, tc.Look.Worn; 255 otherwise, as before)
			const FColor Shade = K + 2 < Paint.Rgb.size() ? FColor(Paint.Rgb[K], Paint.Rgb[K + 1], Paint.Rgb[K + 2], Paint.Alpha(static_cast<size_t>(I))) : FColor(90, 90, 80, 255);
			Colours[Corner] = FVector4f(FLinearColor(Shade));
			const bool bSkin = static_cast<size_t>(I) < Paint.Paint.size() && Paint.Paint[static_cast<size_t>(I)] == SkinPaint;
			Uvs.Set(Corner, 0, FVector2f(bPosed && bSkin ? 1.0f : 0.0f, 0.0f));
			if (bPosed)
			{
				Uvs.Set(Corner, 1, FVector2f(static_cast<float>(I), 0.0f));
			}
			Corners[I] = Corner;
		}
		int32 Laid = 0;
		for (size_t K = 0; K + 2 < Paint.Triangles.size(); K += 3)
		{
			const uint32_t A = Paint.Triangles[K], B = Paint.Triangles[K + 1], C = Paint.Triangles[K + 2];
			if (A >= static_cast<uint32_t>(Count) || B >= static_cast<uint32_t>(Count) || C >= static_cast<uint32_t>(Count))
			{
				return nullptr;
			}
			const FVertexInstanceID Three[3] = { Corners[static_cast<int32>(A)], Corners[static_cast<int32>(B)], Corners[static_cast<int32>(C)] };
			D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
			++Laid;
		}
		if (Laid == 0)
		{
			return nullptr;
		}
		UStaticMesh* Made = NewObject<UStaticMesh>(Owner, MakeUniqueObjectName(Owner, UStaticMesh::StaticClass(), FName(TEXT("LandVatMesh"))));
		Made->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Ink")));
		TArray<const FMeshDescription*> Levels;
		Levels.Add(&D);
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Made->BuildFromMeshDescriptions(Levels, Params);
		return Made;
	}
}

FLandVat::FLandVat(AActor* InOwner, USceneComponent* InParent, UMaterialInterface* InCrowd, UMaterialInterface* InInk, double InUnitsPerMetre,
	TArray<TObjectPtr<UObject>>& InKeep)
	: Owner(InOwner)
	, Parent(InParent)
	, CrowdInk(InCrowd)
	, Ink(InInk)
	, U(InUnitsPerMetre)
	, Keep(&InKeep)
{
	OneAt.SetNum(1);
	OneBefore.SetNum(1);
}

bool FLandVat::Make(const LandMen::Crowd& Men)
{
	const double Began = FPlatformTime::Seconds();
	Drawn.SetNum(FMath::Max(0, Men.Count()));
	if (!CrowdInk.IsValid() || !Ink.IsValid() || !Owner.IsValid() || Men.BankPoses() <= 0)
	{
		return false;
	}
	// The sets met on men, and the helmets they wear (a set's finish: key, then finish).
	TSet<int64> Worn;
	for (int32 Man = 0; Man < Men.Count(); ++Man)
	{
		const int32 Key = Men.BodyKey(Man);
		if (Key < 0)
		{
			continue;
		}
		Keys.Add(Key);
		const int32 Finish = Men.HelmetOf(Man);
		if (Finish >= 0)
		{
			Worn.Add((static_cast<int64>(Key) << 32) | static_cast<int64>(static_cast<uint32>(Finish)));
		}
	}
	int32 Bodies = 0, Helmets = 0;
	for (const int32 Key : Keys)
	{
		for (const int32 Level : { static_cast<int32>(LandMen::Mid), static_cast<int32>(LandMen::Far) })
		{
			Bodies += MakeBody(Men, Key, Level) ? 1 : 0;
		}
	}
	for (const int64 Both : Worn)
	{
		for (const int32 Level : { static_cast<int32>(LandMen::Mid), static_cast<int32>(LandMen::Far) })
		{
			Helmets += MakeHelmet(Men, static_cast<int32>(Both >> 32), static_cast<int32>(Both & 0xFFFFFFFF), Level) ? 1 : 0;
		}
	}
	MadeMsTotal = (FPlatformTime::Seconds() - Began) * 1000.0;
	GrownTo = Men.Count();
	WornMet = Worn;
	UE_LOG(LogTheater, Display, TEXT("Land battle: the mid and far men as one crowd (SO1): %d body sets, %d of their %d parts made (mid and far), %d helmets' parts of %d; the poses' pictures %.1f MB on the card; made in %.0f ms"),
		Keys.Num(), Bodies, Keys.Num() * 2, Helmets, Worn.Num() * 2, PicturesMB(), MadeMsTotal);
	return Bodies > 0;
}

int32 FLandVat::Grow(const LandMen::Crowd& Men, bool bAgain)
{
	if (!CrowdInk.IsValid() || !Ink.IsValid() || !Owner.IsValid() || Men.BankPoses() <= 0)
	{
		return 0;
	}
	const double Began = FPlatformTime::Seconds();
	if (bAgain)
	{
		for (int32 Man = FMath::Max(0, Men.Count()); Man < Drawn.Num(); ++Man)
		{
			Out(Man);
		}
		if (Drawn.Num() > Men.Count())
		{
			Drawn.SetNum(FMath::Max(0, Men.Count()));
		}
		GrownTo = 0;
	}
	if (Drawn.Num() < Men.Count())
	{
		Drawn.SetNum(Men.Count());
	}
	// (the men added since: their sets and helmets)
	for (int32 Man = FMath::Clamp(GrownTo, 0, Men.Count()); Man < Men.Count(); ++Man)
	{
		const int32 Key = Men.BodyKey(Man);
		if (Key < 0)
		{
			continue;
		}
		Keys.Add(Key);
		const int32 Finish = Men.HelmetOf(Man);
		if (Finish >= 0)
		{
			WornMet.Add((static_cast<int64>(Key) << 32) | static_cast<int64>(static_cast<uint32>(Finish)));
		}
	}
	GrownTo = Men.Count();
	// (every set's level banked and not yet tried: a man of a set met only far may stand at mid later)
	int32 Made = 0;
	for (const int32 Key : Keys)
	{
		for (const int32 Level : { static_cast<int32>(LandMen::Mid), static_cast<int32>(LandMen::Far) })
		{
			const int64 Id = BodyId(Key, Level);
			if (PartOf.Contains(Id) || Tried.Contains(Id) || Men.BankBodyPoints(Key, static_cast<LandMen::Lod>(Level)) <= 0)
			{
				continue;
			}
			Tried.Add(Id);
			if (MakeBody(Men, Key, Level))
			{
				++Made;
			}
			else if (Level == static_cast<int32>(LandMen::Mid))
			{
				bMidMissing = true;
			}
		}
	}
	for (const int64 Both : WornMet)
	{
		const int32 Key = static_cast<int32>(Both >> 32), Finish = static_cast<int32>(Both & 0xFFFFFFFF);
		for (const int32 Level : { static_cast<int32>(LandMen::Mid), static_cast<int32>(LandMen::Far) })
		{
			const int64 Id = HelmetId(Key, Finish, Level);
			if (PartOf.Contains(Id) || Tried.Contains(Id) || !PartOf.Contains(BodyId(Key, Level)))
			{
				continue;
			}
			Tried.Add(Id);
			Made += MakeHelmet(Men, Key, Finish, Level) ? 1 : 0;
		}
	}
	if (Made > 0)
	{
		const double Took = (FPlatformTime::Seconds() - Began) * 1000.0;
		MadeMsTotal += Took;
		UE_LOG(LogTheater, Display, TEXT("Land battle (live): the men's crowd (SO1) grown by %d parts: %d body sets now, %d parts in all; the poses' pictures %.1f MB on the card; %.0f ms"),
			Made, Keys.Num(), PartOf.Num(), PicturesMB(), Took);
	}
	return Made;
}

bool FLandVat::HasAll(int32 Level) const
{
	for (const int32 Key : Keys)
	{
		if (!PartOf.Contains(BodyId(Key, Level)))
		{
			return false;
		}
	}
	return Keys.Num() > 0;
}

bool FLandVat::MakeBody(const LandMen::Crowd& Men, int32 Key, int32 Level)
{
	const LandMen::Lod L = static_cast<LandMen::Lod>(Level);
	const int32 Poses = Men.BankPoses();
	const int32 Points = Men.BankBodyPoints(Key, L);
	if (Points <= 0)
	{
		return false;
	}
	if (Points > MostPoints)
	{
		UE_LOG(LogTheater, Warning, TEXT("Land battle: body set %d has %d points at level %d (over %d): its men are drawn as before there."), Key, Points, Level, MostPoints);
		return false;
	}
	const LandMen::Look& Paint = Men.BankBodyLook(Key, L);
	std::vector<float> At, Facing;
	if (!Men.BankBody(Key, L, At, Facing) || Paint.Points != Points)
	{
		return false;
	}
	// The poses' picture (LandVatPack, checked in the cloud by Tools/land_show/land_vat_pack_test.cpp): a column a point;
	// rows 0 to Poses - 1 each point's way from his rest (the first pose his file has; Unreal's units, his own axes),
	// rows Poses to 2 Poses - 1 its facing. A pose his file lacks is his rest (never shown: BankShow picks only poses he
	// has).
	std::vector<bool> Has(static_cast<size_t>(Poses), false);
	for (int32 K = 0; K < Poses; ++K)
	{
		Has[static_cast<size_t>(K)] = Men.BankHas(Key, K);
	}
	LandVatPack::Packed Pic;
	if (!LandVatPack::Pack(At, Facing, Has, Points, Poses, U, Pic))
	{
		return false;
	}
	const size_t Each = static_cast<size_t>(Points) * 3;
	const float* RestAt = At.data() + static_cast<size_t>(Pic.Rest) * Each;
	const float* RestFacing = Facing.data() + static_cast<size_t>(Pic.Rest) * Each;
	UTexture2D* Picture = UTexture2D::CreateTransient(Pic.Width, Pic.Height, PF_FloatRGBA);
	if (!Picture || !Picture->GetPlatformData() || Picture->GetPlatformData()->Mips.Num() == 0)
	{
		return false;
	}
	Picture->CompressionSettings = TC_HDR;
	Picture->SRGB = false;
	Picture->Filter = TF_Nearest;
	Picture->AddressX = TA_Clamp;
	Picture->AddressY = TA_Clamp;
	Picture->NeverStream = true;
	{
		FTexture2DMipMap& Mip = Picture->GetPlatformData()->Mips[0];
		const int64 Bytes = static_cast<int64>(Pic.Texels.size()) * static_cast<int64>(sizeof(uint16));
		Mip.BulkData.Lock(LOCK_READ_WRITE);
		void* Out = Mip.BulkData.Realloc(Bytes);
		FMemory::Memcpy(Out, Pic.Texels.data(), Bytes);
		Mip.BulkData.Unlock();
		PictureBytes += static_cast<double>(Bytes);
	}
	Picture->UpdateResource();
	Keep->Add(Picture);

	// Its ink: the men's settings, the picture and its measure.
	UMaterialInstanceDynamic* PartInk = UMaterialInstanceDynamic::Create(CrowdInk.Get(), Owner.Get());
	if (!PartInk)
	{
		return false;
	}
	if (UMaterialInstance* From = Cast<UMaterialInstance>(Ink.Get()))
	{
		PartInk->CopyParameterOverrides(From);
	}
	PartInk->SetTextureParameterValue(TEXT("Poses"), Picture);
	PartInk->SetScalarParameterValue(TEXT("Columns"), static_cast<float>(Points));
	PartInk->SetScalarParameterValue(TEXT("Rows"), static_cast<float>(2 * Poses));
	PartInk->SetScalarParameterValue(TEXT("PoseCount"), static_cast<float>(Poses));
	Keep->Add(PartInk);

	// The mesh at rest, its bounds grown to hold every pose (the card culls an instance by them).
	UStaticMesh* Mesh = ManMesh(Owner.Get(), RestAt, RestFacing, Paint, PartInk, U, true);
	if (!Mesh)
	{
		return false;
	}
	// (a centimetre more each way for the halfs' rounding)
	const FVector Over(FMath::Max(0.0f, Pic.AllHi[0] - Pic.RestHi[0]) + 1.0f, FMath::Max(0.0f, Pic.AllHi[1] - Pic.RestHi[1]) + 1.0f,
		FMath::Max(0.0f, Pic.AllHi[2] - Pic.RestHi[2]) + 1.0f);
	const FVector Under(FMath::Max(0.0f, Pic.RestLo[0] - Pic.AllLo[0]) + 1.0f, FMath::Max(0.0f, Pic.RestLo[1] - Pic.AllLo[1]) + 1.0f,
		FMath::Max(0.0f, Pic.RestLo[2] - Pic.AllLo[2]) + 1.0f);
	Mesh->SetPositiveBoundsExtension(Over);
	Mesh->SetNegativeBoundsExtension(Under);
	Mesh->CalculateExtendedBounds();
	const int32 Index = AddPart(Mesh, PartInk, Level, true);
	if (Index < 0)
	{
		return false;
	}
	PartOf.Add(BodyId(Key, Level), Index);
	return true;
}

bool FLandVat::MakeHelmet(const LandMen::Crowd& Men, int32 Key, int32 Finish, int32 Level)
{
	const LandMen::Lod L = static_cast<LandMen::Lod>(Level);
	const LandMen::Look& Paint = Men.HelmetLook(Key, Finish, L);
	std::vector<float> At, Facing;
	if (Paint.Points <= 0 || !Men.HelmetMesh(Key, Finish, L, At, Facing) || At.size() != static_cast<size_t>(Paint.Points) * 3 || Facing.size() != At.size())
	{
		return false;
	}
	UStaticMesh* Mesh = ManMesh(Owner.Get(), At.data(), Facing.data(), Paint, Ink.Get(), U, false);
	const int32 Index = Mesh ? AddPart(Mesh, Ink.Get(), Level, false) : -1;
	if (Index < 0)
	{
		return false;
	}
	PartOf.Add(HelmetId(Key, Finish, Level), Index);
	return true;
}

int32 FLandVat::AddPart(UStaticMesh* Mesh, UMaterialInterface* PartInk, int32 Level, bool bPosed)
{
	AActor* Scene = Owner.Get();
	if (!Mesh || !PartInk || !Scene)
	{
		return -1;
	}
	Keep->Add(Mesh);
	UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(Scene,
		MakeUniqueObjectName(Scene, UInstancedStaticMeshComponent::StaticClass(), FName(TEXT("LandVat"))));
	if (USceneComponent* On = Parent.Get())
	{
		Copies->SetupAttachment(On);
	}
	Copies->SetStaticMesh(Mesh);
	Copies->SetMaterial(0, PartInk);
	Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Copies->SetCanEverAffectNavigation(false);
	// (a mid man casts his shadow, a far one none: GS3)
	Copies->SetCastShadow(Level == static_cast<int32>(LandMen::Mid));
	if (bPosed)
	{
		Copies->SetNumCustomDataFloats(Numbers);
	}
	// (GS7: places removed off the end only; swap-removal keeps its arrays from shrinking and growing again)
	Copies->SetRemoveSwap();
	// (each instance's place a frame before kept, for the walk's motion vector: set before the component is registered,
	// as its draw is made then; never on a static component, which the engine forbids)
	Copies->SetMobility(EComponentMobility::Movable);
	Copies->SetHasPerInstancePrevTransforms(true);
	// (hidden until a man is placed in it)
	Copies->SetVisibility(false);
	Copies->RegisterComponent();
	Keep->Add(Copies);
	FPart& Part = Parts.AddDefaulted_GetRef();
	Part.Copies = Copies;
	Part.bPosed = bPosed;
	return Parts.Num() - 1;
}

bool FLandVat::Place(const LandMen::Crowd& Men, int32 Man, const FTransform& Body, const FTransform* Helmet)
{
	if (!Drawn.IsValidIndex(Man))
	{
		return false;
	}
	const int32 Level = Men.BankLevel(Man);
	const int32 Key = Men.BodyKey(Man);
	const LandMen::Crowd::Show Now = Men.BankShow(Man);
	const int32* Part = Level >= 0 && Key >= 0 && Now.A >= 0 ? PartOf.Find(BodyId(Key, Level)) : nullptr;
	if (!Part)
	{
		return false;
	}
	LandMen::Crowd::Show Was = Men.BankShowBefore(Man);
	if (Was.A < 0)
	{
		Was = Now;
	}
	const LandMen::Vec3 Tone = Men.FaceTone(Man);
	const float Data[Numbers] = { static_cast<float>(Now.A), static_cast<float>(Now.B >= 0 ? Now.B : Now.A), Now.W,
		static_cast<float>(Was.A), static_cast<float>(Was.B >= 0 ? Was.B : Was.A), Was.W, Tone.X, Tone.Y, Tone.Z };
	Put(Man, false, *Part, Body, Data);
	const int32 Finish = Helmet ? Men.HelmetOf(Man) : -1;
	const int32* Worn = Finish >= 0 ? PartOf.Find(HelmetId(Key, Finish, Level)) : nullptr;
	if (Worn)
	{
		Put(Man, true, *Worn, *Helmet, nullptr);
	}
	else
	{
		Drop(Man, true);
	}
	return true;
}

void FLandVat::Put(int32 Man, bool bHelmet, int32 Wanted, const FTransform& At, const float* Data)
{
	FMan& M = Drawn[Man];
	int32& Part = bHelmet ? M.Helmet : M.Body;
	int32& Slot = bHelmet ? M.HelmetSlot : M.BodySlot;
	FTransform& Last = bHelmet ? M.HelmetAt : M.BodyAt;
	FPart& P = Parts[Wanted];
	UInstancedStaticMeshComponent* Copies = P.Copies.Get();
	if (!Copies)
	{
		Drop(Man, bHelmet);
		return;
	}
	if (Part != Wanted || Slot < 0)
	{
		// (a new part: his old place given back, and the place after his part's last man taken: one left over from a man
		// given back this frame, before Send removes it, or one added)
		Drop(Man, bHelmet);
		Slot = P.Holder.Add(Man);
		if (Slot < Copies->GetInstanceCount())
		{
			// (a place left by a man given back this frame: its place a frame before is his, so it is set here too, to
			// this man's place now: no move)
			SetPlace(Copies, Slot, At, At);
		}
		else
		{
			// (a place added: the engine gives it its place now as its place a frame before)
			const int32 Added = Copies->AddInstance(At, false);
			ensureMsgf(Added == Slot, TEXT("LandVat: a part's instance %d added for its man %d"), Added, Slot);
		}
		Part = Wanted;
		P.bDirty = true;
		Last = At;
		(bHelmet ? M.HelmetBefore : M.BodyBefore) = At;
		(bHelmet ? M.HelmetMoved : M.BodyMoved) = 0;
		if (Data && !bHelmet)
		{
			Copies->SetCustomData(Slot, TArrayView<const float>(Data, Numbers), false);
			FMemory::Memcpy(M.Data, Data, sizeof(float) * Numbers);
		}
		return;
	}
	// (only what changed: a man lying still is not sent again. A move is sent with where he stood the frame before, so
	// his walk has its motion vector for the anti-aliasing, as his limbs have theirs from the poses a frame before; one
	// who stops is made still at Send. A place new to him, or filled from the part's end, is sent with no move, above
	// and in Drop)
	if (!Last.Equals(At, 1e-3))
	{
		SetPlace(Copies, Slot, At, Last);
		(bHelmet ? M.HelmetBefore : M.BodyBefore) = Last;
		Last = At;
		uint32& Moved = bHelmet ? M.HelmetMoved : M.BodyMoved;
		if (Moved != Frame)
		{
			Moved = Frame;
			MovingNow.Add(Man * 2 + (bHelmet ? 1 : 0));
		}
		P.bDirty = true;
	}
	if (Data && !bHelmet && FMemory::Memcmp(M.Data, Data, sizeof(float) * Numbers) != 0)
	{
		Copies->SetCustomData(Slot, TArrayView<const float>(Data, Numbers), false);
		FMemory::Memcpy(M.Data, Data, sizeof(float) * Numbers);
		P.bDirty = true;
	}
}

void FLandVat::Drop(int32 Man, bool bHelmet)
{
	FMan& M = Drawn[Man];
	int32& Part = bHelmet ? M.Helmet : M.Body;
	int32& Slot = bHelmet ? M.HelmetSlot : M.BodySlot;
	if (Parts.IsValidIndex(Part) && Slot >= 0)
	{
		// (kept packed, GS7: his part's last man moved into his place, so the places past the last man hold nobody and
		// Send removes them; nothing hidden is drawn)
		FPart& P = Parts[Part];
		const int32 LastSlot = P.Holder.Num() - 1;
		if (Slot < LastSlot && P.Holder.IsValidIndex(Slot))
		{
			const int32 Moved = P.Holder[LastSlot];
			FMan& O = Drawn[Moved];
			if (UInstancedStaticMeshComponent* Copies = P.Copies.Get())
			{
				// (his place and his own place a frame before, not the given-back man's: his move as it was, no jump)
				SetPlace(Copies, Slot, bHelmet ? O.HelmetAt : O.BodyAt, bHelmet ? O.HelmetBefore : O.BodyBefore);
				if (P.bPosed)
				{
					Copies->SetCustomData(Slot, TArrayView<const float>(O.Data, Numbers), false);
				}
			}
			P.Holder[Slot] = Moved;
			(bHelmet ? O.HelmetSlot : O.BodySlot) = Slot;
		}
		if (LastSlot >= 0)
		{
			P.Holder.Pop(EAllowShrinking::No);
		}
		P.bDirty = true;
	}
	Part = -1;
	Slot = -1;
	(bHelmet ? M.HelmetMoved : M.BodyMoved) = 0;
}

void FLandVat::SetPlace(UInstancedStaticMeshComponent* Copies, int32 Slot, const FTransform& At, const FTransform& Before)
{
	// (the engine's batch update is the one that takes the place a frame before by the instance's place in the
	// component; in the component's own frame, as the scene's places are; sent with the rest at Send)
	OneAt[0] = At;
	OneBefore[0] = Before;
	Copies->BatchUpdateInstancesTransforms(Slot, OneAt, OneBefore, false, false, false);
}

void FLandVat::Out(int32 Man)
{
	if (!Drawn.IsValidIndex(Man))
	{
		return;
	}
	Drop(Man, false);
	Drop(Man, true);
}

void FLandVat::Send()
{
	// (a man sent moving the frame before and not this frame: stopped, or left where he was out of the picture. His
	// place a frame before is made his place now, once, or the card would draw him smeared along his last step for as
	// long as he stands)
	for (const int32 Entry : MovingBefore)
	{
		const int32 Man = Entry / 2;
		const bool bHelmet = (Entry & 1) != 0;
		if (!Drawn.IsValidIndex(Man))
		{
			continue;
		}
		FMan& M = Drawn[Man];
		uint32& Moved = bHelmet ? M.HelmetMoved : M.BodyMoved;
		const int32 Part = bHelmet ? M.Helmet : M.Body;
		const int32 Slot = bHelmet ? M.HelmetSlot : M.BodySlot;
		if (Moved == 0 || Moved == Frame || !Parts.IsValidIndex(Part) || Slot < 0)
		{
			continue;
		}
		Moved = 0;
		const FTransform& At = bHelmet ? M.HelmetAt : M.BodyAt;
		(bHelmet ? M.HelmetBefore : M.BodyBefore) = At;
		FPart& P = Parts[Part];
		if (UInstancedStaticMeshComponent* Copies = P.Copies.Get())
		{
			if (Slot < Copies->GetInstanceCount())
			{
				SetPlace(Copies, Slot, At, At);
				P.bDirty = true;
			}
		}
	}
	Swap(MovingBefore, MovingNow);
	MovingNow.Reset();
	++Frame;
	if (Frame == 0)
	{
		Frame = 1;
	}
	for (FPart& P : Parts)
	{
		if (P.bDirty)
		{
			if (UInstancedStaticMeshComponent* Copies = P.Copies.Get())
			{
				// (the places past the last man, given back this frame, removed off the end, last first: GS7)
				const int32 Engine = Copies->GetInstanceCount();
				if (Engine > P.Holder.Num())
				{
					TArray<int32> Gone;
					Gone.Reserve(Engine - P.Holder.Num());
					for (int32 Slot = Engine - 1; Slot >= P.Holder.Num(); --Slot)
					{
						Gone.Add(Slot);
					}
					Copies->RemoveInstances(Gone, true);
				}
				Copies->SetVisibility(P.Holder.Num() > 0);
				Copies->MarkRenderInstancesDirty();
			}
		}
		P.bDirty = false;
	}
}
