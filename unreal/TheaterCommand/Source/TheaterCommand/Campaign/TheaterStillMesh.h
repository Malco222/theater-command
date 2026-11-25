#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "TheaterStillMesh.generated.h"

/**
 * (Speed, 17 November 2026.) A hand-built model that Unreal draws the way it draws a stored one.
 *
 * A UProceduralMeshComponent is handed to the drawing thread afresh every frame. For each of its sections shown, for
 * the picture, for the depth and for every shadow, a new drawing order is made, used once and thrown away (its proxy
 * has only the "dynamic" path). That is right for smoke that changes every frame, and waste for the ground, the towns,
 * the trees and the lines on the map, which are made once and then left alone for minutes at a time.
 *
 * A still mesh is a procedural mesh in everything but its proxy: it is built, coloured and shown with the same calls,
 * and drawn from the same buffers through the same vertex factory. Only its drawing orders are handed over once, when
 * it is made, and Unreal keeps them for every frame after (the "static" path, as for a stored model). The picture is
 * the same; the drawing thread's work for it each frame is a look-up instead of a rebuild.
 *
 * Making or clearing a section, a new material, a move, showing or hiding the whole: all as before (a change to the
 * model makes its proxy again, as it does for a procedural mesh). Two calls must never be used on one, because the
 * engine's procedural mesh reaches into its own kind of proxy for them: SetMeshSectionVisible and UpdateMeshSection.
 * Use a plain UProceduralMeshComponent for anything that needs them.
 *
 * tc.Speed.StillMeshes 0 draws every still mesh the old way again, from the next frame.
 */
UCLASS()
class UTheaterStillMesh : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	explicit UTheaterStillMesh(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	/** A new still mesh for Outer, named after its kind ("LandTile_12"), so that the speed census can count by kind. */
	static UTheaterStillMesh* Make(UObject* Outer, const TCHAR* Kind);

	/** Every still mesh's proxy made again: after tc.Speed.StillMeshes has changed, so that the change is seen at once. */
	static void RemakeAll();
};
