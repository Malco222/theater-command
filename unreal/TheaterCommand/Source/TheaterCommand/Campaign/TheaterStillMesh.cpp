#include "Campaign/TheaterStillMesh.h"

#include "Campaign/TheaterSpeed.h"
#include "DynamicMeshBuilder.h"
#include "LocalVertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "UObject/UObjectIterator.h"

namespace
{
	/** One section's buffers on the drawing thread, kept as the procedural mesh's own proxy keeps them. */
	struct FTheaterStillSection
	{
		explicit FTheaterStillSection(ERHIFeatureLevel::Type InFeatureLevel)
			: VertexFactory(InFeatureLevel, "FTheaterStillSection")
		{
		}

		UMaterialInterface* Material = nullptr;
		FStaticMeshVertexBuffers VertexBuffers;
		FDynamicMeshIndexBuffer32 IndexBuffer;
		FLocalVertexFactory VertexFactory;
		bool bVisible = true;
	};

	/**
	 * The procedural mesh's proxy with its drawing moved from GetDynamicMeshElements (every frame, every view, every
	 * shadow) to DrawStaticElements (once, when the proxy is made): the same buffers, filled the same way, the same
	 * vertex factory, the same materials and the same view relevance, but static where it was dynamic.
	 */
	class FTheaterStillProxy final : public FPrimitiveSceneProxy
	{
	public:
		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		explicit FTheaterStillProxy(UTheaterStillMesh* Component)
			: FPrimitiveSceneProxy(Component)
			, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
		{
			// (as the procedural mesh's own proxy: the sections' drawing orders use this proxy's own uniform buffer, so a
			// move only updates that buffer and the kept orders stand)
			bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

			const ERHIFeatureLevel::Type ChipLevel = GetScene().GetFeatureLevel();
			const int32 Count = Component->GetNumSections();
			Sections.SetNumZeroed(Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FProcMeshSection* Source = Component->GetProcMeshSection(Index);
				if (!Source || Source->ProcIndexBuffer.Num() == 0 || Source->ProcVertexBuffer.Num() == 0)
				{
					continue;
				}
				FTheaterStillSection* Made = new FTheaterStillSection(ChipLevel);

				// The points, as the procedural mesh's proxy converts them (ConvertProcMeshToDynMeshVertex).
				TArray<FDynamicMeshVertex> Vertices;
				Vertices.SetNumUninitialized(Source->ProcVertexBuffer.Num());
				for (int32 Point = 0; Point < Vertices.Num(); ++Point)
				{
					const FProcMeshVertex& From = Source->ProcVertexBuffer[Point];
					FDynamicMeshVertex& To = Vertices[Point];
					To.Position = FVector3f(From.Position);
					To.Color = From.Color;
					To.TextureCoordinate[0] = FVector2f(From.UV0);
					To.TextureCoordinate[1] = FVector2f(From.UV1);
					To.TextureCoordinate[2] = FVector2f(From.UV2);
					To.TextureCoordinate[3] = FVector2f(From.UV3);
					To.TangentX = FVector3f(From.Tangent.TangentX);
					To.TangentZ = FVector3f(From.Normal);
					To.TangentZ.Vector.W = From.Tangent.bFlipTangentY ? -127 : 127;
				}
				Made->IndexBuffer.Indices = Source->ProcIndexBuffer;
				Made->VertexBuffers.InitFromDynamicVertex(&Made->VertexFactory, Vertices, 4);
				BeginInitResource(&Made->VertexBuffers.PositionVertexBuffer);
				BeginInitResource(&Made->VertexBuffers.StaticMeshVertexBuffer);
				BeginInitResource(&Made->VertexBuffers.ColorVertexBuffer);
				BeginInitResource(&Made->IndexBuffer);
				BeginInitResource(&Made->VertexFactory);

				Made->Material = Component->GetMaterial(Index);
				if (!Made->Material)
				{
					Made->Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}
				Made->bVisible = Source->bSectionVisible;
				Sections[Index] = Made;
			}
		}

		virtual ~FTheaterStillProxy() override
		{
			for (FTheaterStillSection* Section : Sections)
			{
				if (Section)
				{
					Section->VertexBuffers.PositionVertexBuffer.ReleaseResource();
					Section->VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
					Section->VertexBuffers.ColorVertexBuffer.ReleaseResource();
					Section->IndexBuffer.ReleaseResource();
					Section->VertexFactory.ReleaseResource();
					delete Section;
				}
			}
		}

		/** Each shown section, handed over once: Unreal keeps its drawing orders for every pass until the proxy goes. */
		virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
		{
			int32 Shown = 0;
			for (const FTheaterStillSection* Section : Sections)
			{
				Shown += Section && Section->bVisible ? 1 : 0;
			}
			PDI->ReserveMemoryForMeshes(Shown);
			for (const FTheaterStillSection* Section : Sections)
			{
				if (!Section || !Section->bVisible)
				{
					continue;
				}
				// (the batch the procedural mesh's proxy makes each frame, made once; its primitive uniform buffer is left
				// to the scene, which gives a static batch the proxy's own)
				FMeshBatch Mesh;
				FMeshBatchElement& Element = Mesh.Elements[0];
				Element.IndexBuffer = &Section->IndexBuffer;
				Element.FirstIndex = 0;
				Element.NumPrimitives = Section->IndexBuffer.Indices.Num() / 3;
				Element.MinVertexIndex = 0;
				Element.MaxVertexIndex = Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
				Mesh.VertexFactory = &Section->VertexFactory;
				Mesh.MaterialRenderProxy = Section->Material->GetRenderProxy();
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Mesh.LODIndex = 0; // (one level of detail, always drawn)
				PDI->DrawMesh(Mesh, FLT_MAX);
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			// (the procedural mesh's own relevance, with static drawing in place of dynamic)
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bShadowRelevance = IsShadowCast(View);
			Result.bStaticRelevance = true;
			Result.bDynamicRelevance = false;
			Result.bRenderInMainPass = ShouldRenderInMainPass();
			Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
			Result.bRenderCustomDepth = ShouldRenderCustomDepth();
			Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
			MaterialRelevance.SetPrimitiveViewRelevance(Result);
			Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
			return Result;
		}

		virtual bool CanBeOccluded() const override
		{
			return !MaterialRelevance.bDisableDepthTest;
		}

		virtual uint32 GetMemoryFootprint() const override
		{
			return static_cast<uint32>(sizeof(*this) + GetAllocatedSize());
		}

	private:
		TArray<FTheaterStillSection*> Sections;
		FMaterialRelevance MaterialRelevance;
	};
}

UTheaterStillMesh::UTheaterStillMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FPrimitiveSceneProxy* UTheaterStillMesh::CreateSceneProxy()
{
	if (!TheaterSpeed::StillMeshes())
	{
		return Super::CreateSceneProxy();
	}
	// (Speed log: how many still models are handed to the draw thread in a frame, and their points. This is done at the
	// frame's end, outside every timed job; where it is done on the game thread, it is timed here as one.)
	if (TheaterSpeed::IsLogging())
	{
		int32 Points = 0;
		for (int32 Index = 0; Index < GetNumSections(); ++Index)
		{
			const FProcMeshSection* Section = GetProcMeshSection(Index);
			Points += Section ? Section->ProcVertexBuffer.Num() : 0;
		}
		TheaterSpeed::NoteMade(TheaterSpeed::EMade::StillModels);
		TheaterSpeed::NoteMade(TheaterSpeed::EMade::StillPoints, Points);
		TheaterSpeed::NoteStillKind(FName(*GetFName().GetPlainNameString()), Points); // ("LandTile_12" is of the kind "LandTile")
		if (IsInGameThread())
		{
			THEATER_SPEED_SCOPE(StillModelHandOver);
			return new FTheaterStillProxy(this);
		}
	}
	return new FTheaterStillProxy(this);
}

UTheaterStillMesh* UTheaterStillMesh::Make(UObject* Outer, const TCHAR* Kind)
{
	return NewObject<UTheaterStillMesh>(Outer, MakeUniqueObjectName(Outer, UTheaterStillMesh::StaticClass(), FName(Kind)));
}

void UTheaterStillMesh::RemakeAll()
{
	for (TObjectIterator<UTheaterStillMesh> It; It; ++It)
	{
		if (It->IsRegistered())
		{
			It->MarkRenderStateDirty();
		}
	}
}
