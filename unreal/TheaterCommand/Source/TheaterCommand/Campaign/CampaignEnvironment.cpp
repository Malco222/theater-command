#include "Campaign/CampaignEnvironment.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "UObject/ConstructorHelpers.h"

ACampaignEnvironment::ACampaignEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// North is +X and east is +Y. The lamp stands low in the west-south-west, behind the left shoulder of
	// someone looking north across the table: pieces are lit from the front and their shadows fall away
	// to the right, and the low angle rakes across the relief.
	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeRotation(FRotator(-42.0f, 69.0f, 0.0f));
	KeyLight->SetIntensity(6.0f);
	KeyLight->bUseTemperature = true;
	KeyLight->Temperature = 5200.0f;
	KeyLight->SetCastShadows(true);
	KeyLight->ForwardShadingPriority = 1;
	KeyLight->DynamicShadowCascades = 4;
	KeyLight->DynamicShadowDistanceMovableLight = 60000.0f;

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetRelativeRotation(FRotator(-40.0f, -105.0f, 0.0f));
	FillLight->SetIntensity(0.6f);
	FillLight->ForwardShadingPriority = 0;
	FillLight->bUseTemperature = true;
	FillLight->Temperature = 8500.0f;
	FillLight->SetCastShadows(false);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = SLS_SpecifiedCubemap;
	static ConstructorHelpers::FObjectFinder<UTextureCube> Ambient(TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
	if (Ambient.Succeeded())
	{
		SkyLight->Cubemap = Ambient.Object;
	}
	SkyLight->SetIntensity(1.0f);
	SkyLight->bLowerHemisphereIsBlack = false;
}
