#include "Battle/LandCrowd.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "StaticMeshAttributes.h"
#include "TheaterCommand.h"
#include "UObject/UObjectGlobals.h"

#include <cmath>

namespace LandCrowdParts
{
	/**
	 * A far piece (LandKit::FarPiece::Merged: model axes, metres from its anchor's pivot; x forward, y right, z up, as
	 * Unreal's own) as a mesh of its own on Ink, for instancing: points and normals as LandBattle's FromBody gives the
	 * near meshes theirs, colours as the same sRGB bytes (see LandParts::ThingMesh: handed over linear, kept as these
	 * bytes), and each point's soot weight in the first texture coordinate's U (M_LandSteel reads it). A triangle that
	 * repeats a corner is left out (it draws nothing). Null where there is nothing to draw.
	 */
	static UStaticMesh* PieceMesh(UObject* Owner, const LandKit::Mesh& M, UMaterialInterface* Ink, double U)
	{
		const int32 Count = static_cast<int32>(M.Count());
		if (!Owner || !Ink || Count <= 0 || M.Triangles.empty() || M.Points.size() != static_cast<size_t>(Count) * 3
			|| M.Normals.size() != M.Points.size() || M.Colours.size() != static_cast<size_t>(Count) * 4)
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
		Uvs.SetNumChannels(1);
		const FPolygonGroupID Group = D.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Ink");
		D.ReserveNewVertices(Count);
		D.ReserveNewVertexInstances(Count);
		D.ReserveNewTriangles(static_cast<int32>(M.Triangles.size() / 3));
		TArray<FVertexInstanceID> Corners;
		Corners.SetNum(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const size_t K = static_cast<size_t>(I);
			const FVertexID Vertex = D.CreateVertex();
			Positions[Vertex] = FVector3f(static_cast<float>(M.Points[K * 3] * U), static_cast<float>(M.Points[K * 3 + 1] * U), static_cast<float>(M.Points[K * 3 + 2] * U));
			const FVertexInstanceID Corner = D.CreateVertexInstance(Vertex);
			const FVector3f Normal = FVector3f(M.Normals[K * 3], M.Normals[K * 3 + 1], M.Normals[K * 3 + 2]).GetSafeNormal(1e-6f, FVector3f::UpVector);
			Normals[Corner] = Normal;
			// (no normal map: any tangent across the normal will do)
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
			Tangents[Corner] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal(1e-6f, FVector3f::ForwardVector);
			Signs[Corner] = 1.0f;
			Colours[Corner] = FVector4f(FLinearColor(FColor(M.Colours[K * 4], M.Colours[K * 4 + 1], M.Colours[K * 4 + 2], M.Colours[K * 4 + 3])));
			const float Soot = K < M.Soot.size() ? M.Soot[K] : 0.0f;
			Uvs.Set(Corner, 0, FVector2f(Soot, 0.0f));
			Corners[I] = Corner;
		}
		int32 Made = 0;
		for (size_t K = 0; K + 2 < M.Triangles.size(); K += 3)
		{
			const uint32_t A = M.Triangles[K], B = M.Triangles[K + 1], C = M.Triangles[K + 2];
			if (A >= static_cast<uint32_t>(Count) || B >= static_cast<uint32_t>(Count) || C >= static_cast<uint32_t>(Count))
			{
				return nullptr;
			}
			if (A == B || B == C || A == C)
			{
				continue;
			}
			const FVertexInstanceID Three[3] = { Corners[static_cast<int32>(A)], Corners[static_cast<int32>(B)], Corners[static_cast<int32>(C)] };
			D.CreateTriangle(Group, TArrayView<const FVertexInstanceID>(Three, 3));
			++Made;
		}
		if (Made == 0)
		{
			return nullptr;
		}
		// (by number: a model's name may hold what an object's name may not)
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Owner, MakeUniqueObjectName(Owner, UStaticMesh::StaticClass(), FName(TEXT("LandCrowdMesh"))));
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Ink, TEXT("Ink")));
		TArray<const FMeshDescription*> Levels;
		Levels.Add(&D);
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions(Levels, Params);
		return Mesh;
	}

	/** Whether some of a vehicle is in the picture now (TK3): her middle, the four corners of her footprint and her nose
	 * and tail, a little high, with a margin (as the men's skins are; LandEye::InPicture); or she is close by. */
	static bool InSight(const LandKit::Vehicle& V, const LandEye::Pose& Pose, const LandPlay::Vec& EyeAt, double Aspect)
	{
		const LandKit::Model& M = V.Look();
		const LandPlay::Vec Middle(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
		// (close by, some of her is in the picture whichever way the lens looks, and a lens beside her sees her flank
		// though every one of these points is behind it)
		if ((Middle - EyeAt).Length() < 0.5 * FMath::Max(M.Long, M.Wide) + 6.0)
		{
			return true;
		}
		auto Seen = [&Pose, Aspect](const LandKit::V3& At, double Up)
		{
			return LandEye::InPicture(Pose, LandPlay::Vec(At.X, At.Y, At.Z + Up), 1.15, Aspect);
		};
		if (Seen(V.Root.At, 1.2))
		{
			return true;
		}
		for (const LandKit::V3& Corner : M.Foot)
		{
			if (Seen(V.Root.Point(Corner), 0.4))
			{
				return true;
			}
		}
		return Seen(V.Root.Point(LandKit::V3(0.5 * M.Long, 0.0, 0.0)), 0.8) || Seen(V.Root.Point(LandKit::V3(-0.5 * M.Long, 0.0, 0.0)), 0.8);
	}

	/** The picture's shape (wide for 1 high), as DrawMen reads it. */
	static double PictureAspect()
	{
		double Aspect = 16.0 / 9.0;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D Size;
			GEngine->GameViewport->GetViewportSize(Size);
			Aspect = Size.Y > 0.0 ? Size.X / Size.Y : Aspect;
		}
		return Aspect;
	}
}

FLandCrowd::FLandCrowd(AActor* InOwner, USceneComponent* InParent, UMaterialInterface* InSteel, double InUnitsPerMetre, WorldFn InWorld,
	TArray<TObjectPtr<UObject>>& InKeep)
	: Owner(InOwner), Parent(InParent), Steel(InSteel), U(InUnitsPerMetre), World(MoveTemp(InWorld)), Keep(&InKeep)
{
	bOff = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandNoCrowd"));
	bLinksAlways = FParse::Param(FCommandLine::Get(), TEXT("TheaterLandLinksAlways"));
	bPrev = !FParse::Param(FCommandLine::Get(), TEXT("TheaterLandCrowdNoPrev"));
	OneAt.SetNum(1);
	OneBefore.SetNum(1);
	UE_LOG(LogTheater, Display, TEXT("Land battle: far tanks, guns and lorries %s (TK1)%s; track links posed %s (TK3)."),
		bOff ? TEXT("each by its own meshes (-TheaterLandNoCrowd)") : TEXT("as instances of their models' far pieces past 140 m"),
		bOff ? TEXT("") : bPrev ? TEXT(", each with her place a frame before (her motion vector)") : TEXT(", drawn as still when they move (-TheaterLandCrowdNoPrev)"),
		bLinksAlways ? TEXT("on every near moving tank (-TheaterLandLinksAlways)") : TEXT("only while she is in the picture"));
}

UMaterialInterface* FLandCrowd::InkFor(const LandKit::Model& Model)
{
	UMaterialInterface* Plain = Steel.Get();
	AActor* Scene = Owner.Get();
	if (!bSootLooked)
	{
		bSootLooked = true;
		SootParent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_LandSteel.M_LandSteel"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!SootParent.IsValid())
		{
			UE_LOG(LogTheater, Warning, TEXT("Land battle: /Game/Campaign/Materials/M_LandSteel is missing (run Tools/build_content.sh): far tanks, guns and lorries are drawn on the steel's own ink, and a far wreck shows her paint unburnt."));
		}
	}
	UMaterialInterface* Sooty = SootParent.Get();
	if (!Sooty || !Scene)
	{
		return Plain;
	}
	// (the near meshes' settings, Roughness, Specular, Grain, Tooth, TopLight, as LoadInks gave them; and her model's
	// burnt colour, the bytes FromBody takes the near meshes' colours toward)
	UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Sooty, Scene);
	if (!Ink)
	{
		return Plain;
	}
	if (UMaterialInstance* From = Cast<UMaterialInstance>(Plain))
	{
		Ink->CopyParameterOverrides(From);
	}
	Ink->SetVectorParameterValue(TEXT("BurntColour"), FLinearColor(Model.BurntColour[0] / 255.0f, Model.BurntColour[1] / 255.0f, Model.BurntColour[2] / 255.0f, 1.0f));
	Keep->Add(Ink);
	return Ink;
}

int32 FLandCrowd::KindFor(const LandKit::Model& Model)
{
	const FString Name(UTF8_TO_TCHAR(Model.Name.c_str()));
	const FString Nation(UTF8_TO_TCHAR(Model.Dressed.Nation.c_str()));
	for (size_t K = 0; K < Kinds.size(); ++K)
	{
		if (Kinds[K].Model == &Model && Kinds[K].Name == Name && Kinds[K].Nation == Nation)
		{
			return Kinds[K].bMade ? static_cast<int32>(K) : -1;
		}
	}
	Kinds.emplace_back();
	const int32 Index = static_cast<int32>(Kinds.size()) - 1;
	FKind& Kind = Kinds.back();
	Kind.Model = &Model;
	Kind.Name = Name;
	Kind.Nation = Nation;
	// (a gun lays her barrel high and swings her trails for the tow, and is then drawn by her own far meshes: she keeps
	// them)
	Kind.bKeepsFar = Model.Kind == "gun";
	const double Took = FPlatformTime::Seconds();
	// (the scene's own rule for a body it hides while she stands, ALandBattle::MakeBodies' bWhileManned: a German roof
	// gun's cradle, drawn only while her commander mans it, is kept out of the pieces; the ground scene's merge)
	std::vector<uint8_t> KeepOut(Model.Bodies.size(), 0);
	for (size_t B = 0; B < Model.Bodies.size(); ++B)
	{
		KeepOut[B] = LandKit::ShownWhileManned(Model, Model.Bodies[B]);
	}
	LandKit::SplitFar(Model, Kind.Split, &KeepOut);
	AActor* Scene = Owner.Get();
	USceneComponent* On = Parent.Get();
	UMaterialInterface* Ink = InkFor(Model);
	if (!Scene || !On || !Ink || Kind.Split.Empty())
	{
		return -1;
	}
	for (int32 Piece = 0; Piece < PieceCount; ++Piece)
	{
		const LandKit::FarPiece& Of = SplitPiece(Kind.Split, Piece);
		if (Of.Empty())
		{
			continue;
		}
		UStaticMesh* Mesh = Of.Anchor >= 0 ? LandCrowdParts::PieceMesh(Scene, Of.Merged, Ink, U) : nullptr;
		if (!Mesh && Piece == LidUp)
		{
			// (the lid-up copy only shows the lid up: without it she shows it shut, as before)
			UE_LOG(LogTheater, Warning, TEXT("Land battle: the lid-up far piece of %s (%s) could not be made: her lid shows shut past the hand-over."), *Name, *Nation);
			continue;
		}
		if (!Mesh)
		{
			// (all or nothing: a piece missing would leave her bodies in it undrawn past the hand-over)
			UE_LOG(LogTheater, Warning, TEXT("Land battle: the far pieces of %s (%s) could not be made: drawn by their own meshes."), *Name, *Nation);
			return -1;
		}
		Keep->Add(Mesh);
		UInstancedStaticMeshComponent* Copies = NewObject<UInstancedStaticMeshComponent>(Scene,
			MakeUniqueObjectName(Scene, UInstancedStaticMeshComponent::StaticClass(), FName(TEXT("LandCrowd"))));
		Copies->SetupAttachment(On);
		Copies->SetStaticMesh(Mesh);
		Copies->SetMaterial(0, Ink);
		Copies->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copies->SetCanEverAffectNavigation(false);
		Copies->SetCastShadow(true);
		Copies->SetNumCustomDataFloats(1);
		// (places removed off the end only, as LandVat's: swap-removal keeps its arrays from shrinking and growing again)
		Copies->SetRemoveSwap();
		if (bPrev)
		{
			// (each instance's place a frame before kept, for a far vehicle's motion vector: set before the component is
			// registered, as its draw is made then; never on a static component, which the engine forbids. LandVat's)
			Copies->SetMobility(EComponentMobility::Movable);
			Copies->SetHasPerInstancePrevTransforms(true);
		}
		// (hidden until one of its vehicles is far)
		Copies->SetVisibility(false);
		Copies->RegisterComponent();
		Keep->Add(Copies);
		Kind.Copies[Piece] = Copies;
	}
	Kind.bMade = true;
	UE_LOG(LogTheater, Display, TEXT("Land battle: %s (%s) far as %d instanced pieces (hull %d triangles, turret %d)%s, made in %.1f ms (TK1)."), *Name, *Nation,
		Kind.Split.Count(), static_cast<int32>(Kind.Split.Pieces[0].Merged.Triangles.size() / 3), static_cast<int32>(Kind.Split.Pieces[1].Merged.Triangles.size() / 3),
		Kind.Copies[LidUp].IsValid() ? (Kind.Split.OpenOf == LandKit::FarSplit::HullPiece ? TEXT(" and her hull with the commander's lid up") : TEXT(" and her turret with the commander's lid up")) : TEXT(""),
		(FPlatformTime::Seconds() - Took) * 1000.0);
	return Index;
}

void FLandCrowd::SetPlace(UInstancedStaticMeshComponent* Copies, int32 Slot, const FVector& At, const FQuat& Q, const FVector& Before, const FQuat& BeforeQ)
{
	if (!bPrev)
	{
		Copies->UpdateInstanceTransform(Slot, FTransform(Q, At, FVector::OneVector), true, false, true);
		return;
	}
	// (the engine's batch update is the one that takes the place a frame before, by the instance's place in the
	// component; both put in the component's own frame here, from the world's this crowd keeps, so the place a frame
	// before is never left to the engine to turn; sent with the rest at the end of Draw. LandVat's SetPlace)
	const FTransform& Home = Copies->GetComponentTransform();
	OneAt[0] = FTransform(Q, At, FVector::OneVector).GetRelativeTransform(Home);
	OneBefore[0] = FTransform(BeforeQ, Before, FVector::OneVector).GetRelativeTransform(Home);
	Copies->BatchUpdateInstancesTransforms(Slot, OneAt, OneBefore, false, false, false);
}

void FLandCrowd::Put(int32 Vehicle, FOne& One, int32 Piece, const FVector& At, const FQuat& Q, const FVector& Before, const FQuat& BeforeQ)
{
	FKind& Kind = Kinds[static_cast<size_t>(One.Kind)];
	UInstancedStaticMeshComponent* Copies = Kind.Copies[Piece].Get();
	if (!Copies || One.Slot[Piece] >= 0)
	{
		return;
	}
	// (her place a frame before, where her anchor was drawn by her own meshes or the other piece: a move only if it is
	// one, as Draw counts a move)
	const bool bMoving = bPrev && (!Before.Equals(At, 0.5) || !BeforeQ.Equals(Q, 1e-5));
	const FVector& From = bMoving ? Before : At;
	const FQuat& FromQ = bMoving ? BeforeQ : Q;
	// (the place after the piece's last vehicle: one left over from a vehicle given back this frame, before Draw removes
	// it at its end, or one added)
	const int32 Slot = Kind.Holder[Piece].Add(Vehicle);
	if (Slot < Copies->GetInstanceCount())
	{
		// (its place a frame before was the given-back vehicle's: set here too, to hers)
		SetPlace(Copies, Slot, At, Q, From, FromQ);
	}
	else
	{
		// (a place added: the engine gives it its place now as its place a frame before; hers sent after, if she moved)
		const int32 Added = Copies->AddInstance(FTransform(Q, At, FVector::OneVector), true);
		ensureMsgf(Added == Slot, TEXT("LandCrowd: a piece's instance %d added for its vehicle's place %d"), Added, Slot);
		if (bMoving)
		{
			SetPlace(Copies, Slot, At, Q, From, FromQ);
		}
	}
	Copies->SetCustomDataValue(Slot, 0, FMath::Max(One.Soot, 0.0f), false);
	One.Slot[Piece] = Slot;
	One.At[Piece] = At;
	One.Q[Piece] = Q;
	One.Before[Piece] = From;
	One.BeforeQ[Piece] = FromQ;
	One.Moved[Piece] = bMoving ? Frame : 0;
	SentMoving += bMoving ? 1 : 0;
	Kind.bDirty[Piece] = true;
}

void FLandCrowd::Drop(FOne& One, int32 Piece)
{
	const int32 Slot = One.Slot[Piece];
	One.Slot[Piece] = -1;
	One.Moved[Piece] = 0;
	if (Slot < 0 || One.Kind < 0 || static_cast<size_t>(One.Kind) >= Kinds.size())
	{
		return;
	}
	FKind& Kind = Kinds[static_cast<size_t>(One.Kind)];
	TArray<int32>& Holder = Kind.Holder[Piece];
	const int32 LastSlot = Holder.Num() - 1;
	if (Slot < LastSlot)
	{
		// (kept packed: the piece's last vehicle moved into her place, with her place, her own place a frame before (her
		// move as it was, no jump) and soot, so the places past the last hold nobody and Draw removes them; nothing
		// hidden is drawn)
		const int32 Moved = Holder[LastSlot];
		FOne& Other = Ones[static_cast<size_t>(Moved)];
		if (UInstancedStaticMeshComponent* Copies = Kind.Copies[Piece].Get())
		{
			SetPlace(Copies, Slot, Other.At[Piece], Other.Q[Piece], Other.Before[Piece], Other.BeforeQ[Piece]);
			Copies->SetCustomDataValue(Slot, 0, FMath::Max(Other.Soot, 0.0f), false);
		}
		Holder[Slot] = Moved;
		Other.Slot[Piece] = Slot;
	}
	if (LastSlot >= 0)
	{
		Holder.Pop(EAllowShrinking::No);
	}
	Kind.bDirty[Piece] = true;
}

void FLandCrowd::Release(FOne& One)
{
	for (int32 Piece = 0; Piece < PieceCount; ++Piece)
	{
		Drop(One, Piece);
	}
	One = FOne();
}

bool FLandCrowd::Enlist(int32 Vehicle, const LandKit::Model& Model)
{
	if (Vehicle < 0)
	{
		return false;
	}
	if (static_cast<size_t>(Vehicle) >= Ones.size())
	{
		Ones.resize(static_cast<size_t>(Vehicle) + 1);
	}
	Release(Ones[static_cast<size_t>(Vehicle)]);
	if (bOff)
	{
		return false;
	}
	const int32 Index = KindFor(Model);
	if (Index < 0)
	{
		return false;
	}
	// (her instances are taken in Draw, while she is drawn by her pieces)
	Ones[static_cast<size_t>(Vehicle)].Kind = Index;
	return true;
}

bool FLandCrowd::TakesFar(int32 Vehicle, int32 Body) const
{
	if (Vehicle < 0 || static_cast<size_t>(Vehicle) >= Ones.size() || Ones[static_cast<size_t>(Vehicle)].Kind < 0)
	{
		return false;
	}
	const FKind& Kind = Kinds[static_cast<size_t>(Ones[static_cast<size_t>(Vehicle)].Kind)];
	return !Kind.bKeepsFar && Body >= 0 && static_cast<size_t>(Body) < Kind.Split.PieceOf.size() && Kind.Split.PieceOf[static_cast<size_t>(Body)] >= 0;
}

bool FLandCrowd::Holds(int32 Vehicle, int32 Body) const
{
	if (Vehicle < 0 || static_cast<size_t>(Vehicle) >= Ones.size())
	{
		return false;
	}
	const FOne& One = Ones[static_cast<size_t>(Vehicle)];
	if (!One.bHeld || One.Kind < 0)
	{
		return false;
	}
	const LandKit::FarSplit& Split = Kinds[static_cast<size_t>(One.Kind)].Split;
	return Body >= 0 && static_cast<size_t>(Body) < Split.PieceOf.size() && Split.PieceOf[static_cast<size_t>(Body)] >= 0;
}

bool FLandCrowd::LinksDue(int32 Vehicle, bool bRendered)
{
	const bool bDue = bLinksAlways || bRendered || Vehicle < 0 || static_cast<size_t>(Vehicle) >= Ones.size() || Ones[static_cast<size_t>(Vehicle)].bSeen;
	if (bDue)
	{
		++LinksPosed;
	}
	else
	{
		++LinksHeld;
	}
	return bDue;
}

void FLandCrowd::Draw(const std::vector<LandKit::Vehicle>& All, const LandEye::Pose& Pose, const LandPlay::Vec& EyeAt, TFunctionRef<bool(int32)> Hidden)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LandCrowd_Draw);
	// (a live battle's reset can leave fewer vehicles: those past the end give their instances back)
	while (Ones.size() > All.size())
	{
		Release(Ones.back());
		Ones.pop_back();
	}
	if (Ones.size() < All.size())
	{
		Ones.resize(All.size());
	}
	const double Aspect = LandCrowdParts::PictureAspect();
	int32 Shown = 0;
	for (size_t K = 0; K < All.size(); ++K)
	{
		const LandKit::Vehicle& V = All[K];
		FOne& One = Ones[K];
		One.bSeen = bLinksAlways || LandCrowdParts::InSight(V, Pose, EyeAt, Aspect);
		const bool bShown = V.Fade >= 0.5 && !Hidden(static_cast<int32>(K));
		const LandPlay::Vec Middle(V.Root.At.X, V.Root.At.Y, V.Root.At.Z);
		const double Far = (Middle - EyeAt).Length();
		if (bShown)
		{
			++Shown;
		}
		if (One.Kind < 0)
		{
			OwnFarFrames += (bShown && Far >= FarFrom) ? 1 : 0;
			continue;
		}
		FKind& Kind = Kinds[static_cast<size_t>(One.Kind)];
		if (Kind.Model != &V.Look())
		{
			// (her model changed under her without her bodies made again: drawn by her own meshes until they are)
			Release(One);
			continue;
		}
		// Far, shown, and her bodies near enough where her pieces draw them (their rest on her root and her turret): a
		// give of 0.25 m at the hand-over, 1.8 mm a metre beyond (about two pixels of the usual lens), taken in a fifth
		// tighter to come in than to stay, so she does not flicker between the two.
		bool bHold = false;
		if (bShown && Far >= FarFrom && V.Bodies().size() == Kind.Model->Bodies.size())
		{
			const double Off = LandKit::FarOff(*Kind.Model, Kind.Split, V.Bodies());
			const double Give = FMath::Max(0.25, 0.0018 * Far);
			bHold = Off <= (One.bHeld ? Give : 0.8 * Give);
		}
		One.bHeld = bHold;
		if (bHold)
		{
			++HeldFrames;
		}
		else if (bShown && Far >= FarFrom)
		{
			++OwnFarFrames;
		}
		// (her commander's lid half open or more: the lid-up copy drawn in the stead of the piece that holds it)
		const bool bLidUp = Kind.Copies[LidUp].IsValid() && Kind.Split.OpenOf >= 0 && V.HatchOpen >= LandKit::FarSplit::OpenFrom;
		const float Soot = static_cast<float>(FMath::Clamp(V.Soot, 0.0, 1.0));
		const bool bSootNew = bHold && FMath::Abs(Soot - One.Soot) > 0.002f;
		if (bSootNew)
		{
			One.Soot = Soot;
		}
		// Where each piece's anchor body is drawn now (as DrawArmour puts a body's own mesh), whether her pieces draw her
		// or not: kept for the frame after, as the place a frame before of a piece that then starts drawing her (at the
		// hand-over, or her lid's copy swapped in), where her own meshes or the other piece drew her.
		const bool bPlaced = bShown && V.Bodies().size() == Kind.Model->Bodies.size();
		const bool bWas = bPrev && One.WasFrame != 0 && One.WasFrame + 1 == Frame;
		FVector AtNow[PieceCount] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
		FQuat QNow[PieceCount] = { FQuat::Identity, FQuat::Identity, FQuat::Identity };
		for (int32 Piece = 0; Piece < PieceCount && bPlaced; ++Piece)
		{
			const int32 Anchor = SplitPiece(Kind.Split, Piece).Anchor;
			if (Anchor >= 0 && static_cast<size_t>(Anchor) < V.Bodies().size())
			{
				const LandKit::Place& Place = V.Bodies()[static_cast<size_t>(Anchor)];
				AtNow[Piece] = World(Place.At.X, Place.At.Y, Place.At.Z);
				QNow[Piece] = FQuat(Place.Q.X, Place.Q.Y, Place.Q.Z, Place.Q.W);
			}
		}
		for (int32 Piece = 0; Piece < PieceCount; ++Piece)
		{
			UInstancedStaticMeshComponent* Copies = Kind.Copies[Piece].Get();
			const int32 Anchor = SplitPiece(Kind.Split, Piece).Anchor;
			const bool bThisOne = Piece == LidUp ? bLidUp : !(bLidUp && Piece == Kind.Split.OpenOf);
			if (!Copies || Anchor < 0 || !bHold || !bThisOne)
			{
				Drop(One, Piece);
				continue;
			}
			// Her anchor's place, and her soot.
			const FVector& At = AtNow[Piece];
			const FQuat& Q = QNow[Piece];
			if (One.Slot[Piece] < 0)
			{
				Put(static_cast<int32>(K), One, Piece, At, Q, bWas ? One.Was[Piece] : At, bWas ? One.WasQ[Piece] : Q);
				continue;
			}
			if (!At.Equals(One.At[Piece], 0.5) || !Q.Equals(One.Q[Piece], 1e-5))
			{
				// (a move: sent with where her instance was drawn the frame before, her motion vector's other end)
				SetPlace(Copies, One.Slot[Piece], At, Q, One.At[Piece], One.Q[Piece]);
				One.Before[Piece] = One.At[Piece];
				One.BeforeQ[Piece] = One.Q[Piece];
				One.At[Piece] = At;
				One.Q[Piece] = Q;
				if (bPrev)
				{
					One.Moved[Piece] = Frame;
					++SentMoving;
				}
				Kind.bDirty[Piece] = true;
			}
			else if (One.Moved[Piece] != 0 && One.Moved[Piece] != Frame)
			{
				// (sent moving the frame before and not this frame: she stopped, or crept less than the threshold. Her
				// place a frame before made her place now, once, or she would be drawn smeared along her last step for
				// as long as she stands)
				SetPlace(Copies, One.Slot[Piece], At, Q, At, Q);
				One.At[Piece] = One.Before[Piece] = At;
				One.Q[Piece] = One.BeforeQ[Piece] = Q;
				One.Moved[Piece] = 0;
				++MadeStill;
				Kind.bDirty[Piece] = true;
			}
			if (bSootNew)
			{
				Copies->SetCustomDataValue(One.Slot[Piece], 0, Soot, false);
				Kind.bDirty[Piece] = true;
			}
		}
		if (bPlaced)
		{
			for (int32 Piece = 0; Piece < PieceCount; ++Piece)
			{
				One.Was[Piece] = AtNow[Piece];
				One.WasQ[Piece] = QNow[Piece];
			}
			One.WasFrame = Frame;
		}
	}
	// Each piece's mesh shown while it holds a vehicle, and sent once for all its changes.
	for (FKind& Kind : Kinds)
	{
		for (int32 Piece = 0; Piece < PieceCount; ++Piece)
		{
			UInstancedStaticMeshComponent* Copies = Kind.Copies[Piece].Get();
			if (!Copies)
			{
				continue;
			}
			if (Kind.bDirty[Piece])
			{
				// (the places past the last vehicle, given back this frame, removed off the end, last first)
				Kind.bDirty[Piece] = false;
				const int32 Count = Copies->GetInstanceCount();
				const int32 Holding = Kind.Holder[Piece].Num();
				if (Count > Holding)
				{
					TArray<int32> Gone;
					Gone.Reserve(Count - Holding);
					for (int32 Slot = Count - 1; Slot >= Holding; --Slot)
					{
						Gone.Add(Slot);
					}
					Copies->RemoveInstances(Gone, true);
				}
				Copies->MarkRenderInstancesDirty();
			}
			const bool bWanted = Kind.Holder[Piece].Num() > 0;
			if (bWanted != Kind.bShownNow[Piece])
			{
				Kind.bShownNow[Piece] = bWanted;
				Copies->SetVisibility(bWanted);
			}
		}
	}
	++Frames;
	++Frame;
	if (Frame == 0)
	{
		Frame = 1;
	}
	Tell(Shown);
}

void FLandCrowd::Tell(int32 Shown)
{
	const double Now = FPlatformTime::Seconds();
	if (TellAt < 0.0)
	{
		TellAt = Now;
		return;
	}
	if (Now - TellAt < 10.0 || Frames == 0)
	{
		return;
	}
	int32 Instances = 0, Meshes = 0;
	for (const FKind& Kind : Kinds)
	{
		for (int32 Piece = 0; Piece < PieceCount; ++Piece)
		{
			Instances += Kind.Holder[Piece].Num();
			Meshes += Kind.bShownNow[Piece] ? 1 : 0;
		}
	}
	const double PerFrame = 1.0 / static_cast<double>(Frames);
	UE_LOG(LogTheater, Display, TEXT("Land battle crowd: of %d vehicles shown, a frame %.1f far drawn as instances (TK1; %d instances on %d meshes now; places sent moving %.1f a frame, made still again %.1f%s) and %.1f far by their own meshes; track links posed %.1f a frame, held out of the picture %.1f (TK3)"),
		Shown, HeldFrames * PerFrame, Instances, Meshes, SentMoving * PerFrame, MadeStill * PerFrame, bPrev ? TEXT("") : TEXT(", no places a frame before"),
		OwnFarFrames * PerFrame, LinksPosed * PerFrame, LinksHeld * PerFrame);
	TellAt = Now;
	Frames = HeldFrames = OwnFarFrames = LinksPosed = LinksHeld = SentMoving = MadeStill = 0;
}
