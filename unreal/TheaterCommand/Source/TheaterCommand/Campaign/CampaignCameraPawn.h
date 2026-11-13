#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CampaignCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * The player's eye over the campaign table. It always looks at a point on the
 * table (the focus) from a distance (the zoom) and a compass direction (the
 * yaw). Zoomed out it looks almost straight down, like someone leaning over
 * the map; zoomed in it drops toward the table to look across at the pieces.
 *
 * Every change is eased, so the view glides instead of jumping.
 */
UCLASS()
class THEATERCOMMAND_API ACampaignCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ACampaignCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** The camera's focus stays within this distance of the middle of the table. */
	void SetBounds(const FVector2D& InHalfExtent) { HalfExtent = InHalfExtent; }

	/** Slides the focus: X to the viewer's right, Y away from the viewer, each -1 to 1. */
	void Pan(const FVector2D& Input, float DeltaSeconds);

	/** Moves the focus at once by a distance on the table, for dragging the map by hand. */
	void PanWorld(const FVector& Delta);

	/** Positive zooms out. One step is one notch of a mouse wheel. */
	void ZoomBy(float Steps);

	void RotateBy(float Degrees);
	void TiltBy(float Degrees);

	/** Sets the compass direction the view faces and any extra tilt. Takes effect at the next GoTo or eases in. */
	void SetHeading(float YawDegrees, float TiltDegrees = 0.0f);

	/** The look of a newsreel: black and white, grainy, dark at the corners. */
	void SetNewsreel(bool bOn);

	/** While hurried the view travels several times faster, for following the other nations' month. */
	void SetHurry(bool bInHurry) { bHurry = bInHurry; }

	/** Sends the view to a place on the table. Zoom is 0 (closest) to 1 (furthest); negative leaves it alone. */
	void GoTo(const FVector& WorldFocus, float InZoom = -1.0f, bool bImmediately = false);

	const FVector& GetFocus() const { return TargetFocus; }
	float GetZoom() const { return TargetZoom; }
	float GetDistance() const;
	float GetYaw() const { return Yaw; }
	float GetPitch() const;
	float GetHorizontalFov() const;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinDistance = 750.0f;

	/** How high above the table the point being looked at is, and the flattest the view may go (degrees below level). Changed only for looking at models. */
	float FocusHeight = 0.0f;
	void SetLens(float FieldOfViewDegrees);
	float FlattestPitch = -22.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxDistance = 30000.0f;

	/** How strongly the far and near parts of the view fall out of focus. This is what makes the table read as a model. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float MiniatureStrength = 13.0f;

private:
	void ApplyView();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> Arm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	FVector2D HalfExtent = FVector2D(8500.0, 7500.0);
	FVector Focus = FVector::ZeroVector;
	FVector TargetFocus = FVector::ZeroVector;
	float Zoom = 0.62f;
	float TargetZoom = 0.62f;
	float Yaw = 0.0f;
	float TargetYaw = 0.0f;
	float Tilt = 0.0f;
	float TargetTilt = 0.0f;
	bool bHurry = false;
};
