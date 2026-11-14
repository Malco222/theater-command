#include "Campaign/CampaignCameraPawn.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

ACampaignCameraPawn::ACampaignCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	// Move after the player's orders for this frame have been read.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->bDoCollisionTest = false;
	Arm->bEnableCameraLag = false;
	Arm->bInheritPitch = false;
	Arm->bInheritYaw = false;
	Arm->bInheritRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(42.0f);
	Camera->bConstrainAspectRatio = false;

	// The look of the picture: fixed exposure (a table under steady light does
	// not need the eye to adapt), a gentle vignette, and focus that falls off.
	FPostProcessSettings& Look = Camera->PostProcessSettings;
	Look.bOverride_AutoExposureMethod = true;
	Look.AutoExposureMethod = AEM_Manual;
	Look.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Look.AutoExposureApplyPhysicalCameraExposure = false;
	Look.bOverride_AutoExposureBias = true;
	Look.AutoExposureBias = 0.0f;
	Look.bOverride_VignetteIntensity = true;
	Look.VignetteIntensity = 0.5f;
	Look.bOverride_BloomIntensity = true;
	Look.BloomIntensity = 0.25f;
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		// Only what is truly bright blooms: the lamp's glint on water, and snow.
		Look.BloomIntensity = 0.4f;
		Look.bOverride_BloomThreshold = true;
		Look.BloomThreshold = 1.1f;
	}
	// The grade of the picture: a little more colour and contrast, the lit side warm and the shadows cool,
	// as a photograph of a model under a lamp comes out.
	if (!FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		Look.bOverride_ColorSaturation = true;
		Look.ColorSaturation = FVector4(1.07, 1.07, 1.07, 1.0);
		Look.bOverride_ColorContrast = true;
		Look.ColorContrast = FVector4(1.08, 1.08, 1.08, 1.0);
		Look.bOverride_ColorGainHighlights = true;
		Look.ColorGainHighlights = FVector4(1.03, 1.0, 0.96, 1.0);
		Look.bOverride_ColorGainShadows = true;
		Look.ColorGainShadows = FVector4(0.96, 0.99, 1.05, 1.0);
		Look.bOverride_FilmToe = true;
		Look.FilmToe = 0.6f;
	}
	Look.bOverride_MotionBlurAmount = true;
	Look.MotionBlurAmount = 0.0f;
	Look.bOverride_DepthOfFieldFocalDistance = true;
	Look.bOverride_DepthOfFieldFstop = true;
	Look.bOverride_DepthOfFieldSensorWidth = true;
	Camera->PostProcessBlendWeight = 1.0f;

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

float ACampaignCameraPawn::GetDistance() const
{
	return MinDistance * FMath::Pow(MaxDistance / MinDistance, Zoom);
}

float ACampaignCameraPawn::GetPitch() const
{
	// Close in, look across the table; far out, look down on it.
	const float Base = FMath::Lerp(-40.0f, -74.0f, FMath::Pow(Zoom, 0.85f));
	return FMath::Clamp(Base + Tilt, -88.0f, FlattestPitch);
}

void ACampaignCameraPawn::SetLens(float FieldOfViewDegrees)
{
	Camera->SetFieldOfView(FieldOfViewDegrees);
}

float ACampaignCameraPawn::GetHorizontalFov() const
{
	return Camera->FieldOfView;
}

void ACampaignCameraPawn::Pan(const FVector2D& Input, float DeltaSeconds)
{
	if (Input.IsNearlyZero())
	{
		return;
	}
	const FRotator Heading(0.0f, TargetYaw, 0.0f);
	const FVector Forward = Heading.Vector();
	const FVector Right = FRotationMatrix(Heading).GetUnitAxis(EAxis::Y);
	// Cross about one screen a second, whatever the zoom.
	const float Speed = GetDistance() * 0.9f;
	TargetFocus += (Forward * Input.Y + Right * Input.X) * Speed * DeltaSeconds;
}

void ACampaignCameraPawn::PanWorld(const FVector& Delta)
{
	TargetFocus += FVector(Delta.X, Delta.Y, 0.0);
	Focus += FVector(Delta.X, Delta.Y, 0.0);
}

void ACampaignCameraPawn::ZoomBy(float Steps)
{
	// A trackpad sends scroll events in bursts of dozens. However many arrive, the zoom being asked for is
	// never allowed to get more than a fifth of the whole range ahead of where the view has got to.
	const float Asked = FMath::Clamp(TargetZoom + Steps * 0.045f, 0.0f, 1.0f);
	TargetZoom = FMath::Clamp(Asked, Zoom - 0.2f, Zoom + 0.2f);
}

void ACampaignCameraPawn::RotateBy(float Degrees)
{
	TargetYaw += Degrees;
}

void ACampaignCameraPawn::TiltBy(float Degrees)
{
	TargetTilt = FMath::Clamp(TargetTilt + Degrees, -30.0f, 30.0f);
}

void ACampaignCameraPawn::SetHeading(float YawDegrees, float TiltDegrees)
{
	TargetYaw = YawDegrees;
	TargetTilt = FMath::Clamp(TiltDegrees, -30.0f, 90.0f);
}

void ACampaignCameraPawn::GoTo(const FVector& WorldFocus, float InZoom, bool bImmediately)
{
	TargetFocus = FVector(WorldFocus.X, WorldFocus.Y, 0.0);
	if (bWraps)
	{
		// (round the world the short way: the place is the same one a turn of the world east or west)
		const double Span = WrapEast - WrapWest;
		while (TargetFocus.Y - Focus.Y > Span * 0.5) { TargetFocus.Y -= Span; }
		while (TargetFocus.Y - Focus.Y < -Span * 0.5) { TargetFocus.Y += Span; }
	}
	if (InZoom >= 0.0f)
	{
		TargetZoom = FMath::Clamp(InZoom, 0.0f, TopZoom);
	}
	if (bImmediately)
	{
		Focus = TargetFocus;
		Zoom = TargetZoom;
		Yaw = TargetYaw;
		Tilt = TargetTilt;
		ApplyView();
	}
}

void ACampaignCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TargetFocus.X = FMath::Clamp(TargetFocus.X, bBoxed ? BoundsMin.X : -HalfExtent.X, bBoxed ? BoundsMax.X : HalfExtent.X);
	if (bWraps)
	{
		// Past one end of the world the eye comes in again at the other, with everything it was doing.
		const double Span = WrapEast - WrapWest;
		while (TargetFocus.Y >= WrapEast) { TargetFocus.Y -= Span; Focus.Y -= Span; }
		while (TargetFocus.Y < WrapWest) { TargetFocus.Y += Span; Focus.Y += Span; }
	}
	else
	{
		TargetFocus.Y = FMath::Clamp(TargetFocus.Y, bBoxed ? BoundsMin.Y : -HalfExtent.Y, bBoxed ? BoundsMax.Y : HalfExtent.Y);
	}

	Focus = FMath::VInterpTo(Focus, TargetFocus, DeltaSeconds, Glide > 0.0f ? Glide : (bHurry ? 26.0f : 9.0f));
	Zoom = FMath::FInterpTo(Zoom, TargetZoom, DeltaSeconds, Glide > 0.0f ? Glide : (bHurry ? 20.0f : 8.0f));
	Yaw = FMath::FInterpTo(Yaw, TargetYaw, DeltaSeconds, 9.0f);
	Tilt = FMath::FInterpTo(Tilt, TargetTilt, DeltaSeconds, 9.0f);
	ApplyView();
}

void ACampaignCameraPawn::ApplyView()
{
	const float Distance = GetDistance();
	SetActorLocation(Focus + FVector(0.0, 0.0, FocusHeight));
	Arm->TargetArmLength = Distance;
	Arm->SetWorldRotation(FRotator(GetPitch(), Yaw, 0.0f));

	// Focus on the point being looked at. A real lens this far from a
	// life-size scene would hold everything sharp, so the sensor is treated as
	// many times larger than a real one: that is what gives the shallow focus
	// of a close-up photograph of a model.
	FPostProcessSettings& Look = Camera->PostProcessSettings;
	// (close to, what is looked at is a piece standing on the table, not the table: the plane of focus is brought
	// forward to the height of its figures, or it lay behind them)
	const float Stands = 30.0f * (1.0f - FMath::SmoothStep(1200.0f, 3200.0f, Distance));
	Look.DepthOfFieldFocalDistance = Distance - Stands * FMath::Abs(FMath::Sin(FMath::DegreesToRadians(GetPitch())));
	Look.DepthOfFieldFstop = 2.0f;
	Look.DepthOfFieldSensorWidth = 24.576f * FMath::Max(MiniatureStrength, 1.0f);
}

void ACampaignCameraPawn::SetNewsreel(bool bOn)
{
	bFilm = bOn;
	FPostProcessSettings& Look = Camera->PostProcessSettings;
	Look.bOverride_ColorSaturation = true;
	Look.ColorSaturation = bOn ? FVector4(0.0, 0.0, 0.0, 1.0) : FVector4(1.07, 1.07, 1.07, 1.0);
	Look.bOverride_VignetteIntensity = true;
	Look.VignetteIntensity = bOn ? 1.1f : 0.5f;
	Look.bOverride_FilmGrainIntensity = true;
	Look.FilmGrainIntensity = bOn ? 0.75f : 0.0f;
	Look.bOverride_ColorGamma = true;
	Look.ColorGamma = bOn ? FVector4(0.92, 0.92, 0.92, 1.0) : FVector4(1.0, 1.0, 1.0, 1.0);
	if (!bOn)
	{
		SetNight(NightKept, bLampKept); // (back to the colour of the hour)
	}
}

void ACampaignCameraPawn::SetNight(float Night, bool bLamplight)
{
	NightKept = Night;
	bLampKept = bLamplight;
	if (bFilm || FParse::Param(FCommandLine::Get(), TEXT("TheaterOldLook")))
	{
		return;
	}
	const double Moon = bLamplight ? 0.0 : Night;
	FPostProcessSettings& Look = Camera->PostProcessSettings;
	Look.bOverride_ColorSaturation = true;
	// (only a little: the moon's blue is in the light, so the towns' lamps keep their warmth)
	const double Colour = FMath::Lerp(1.07, 0.95, Moon);
	Look.ColorSaturation = FVector4(Colour, Colour, Colour, 1.0);
}
