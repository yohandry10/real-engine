// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

#include "WorldLeader.h"
#include "Campaign/WLDataRegistry.h"
#include "Presentation/WLCampaignOverviewBuilder.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "Presentation/WLCampaignRouteBuilder.h"
#include "Presentation/WLCampaignSettlementBuilder.h"
#include "Presentation/WLCampaignTerrainBuilder.h"
#include "Presentation/WLCampaignVisualStyle.h"
#include "Presentation/WLCampaignWaterBuilder.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ConstructorHelpers.h"
#include "UnrealClient.h"

void AWLCampaign3DView::SetupLightingAndCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DestroyPresentationActors();

	ViewDirectionalLight = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(),
		FVector(-12000.f, -16000.f, 45000.f),
		FRotator(-46.f, -12.f, 0.f));
	if (ViewDirectionalLight && ViewDirectionalLight->GetLightComponent())
	{
		// Bajada de 6.0 -> 4.6: sombras menos duras. El terreno es UNLIT (siempre claro) pero las
		// ciudades son LIT; con sol muy fuerte, las que caian en sombra del relieve salian oscuras.
		ViewDirectionalLight->GetLightComponent()->SetIntensity(4.6f);
		ViewDirectionalLight->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.97f, 0.92f));
	}
	ViewSkyLight = World->SpawnActor<ASkyLight>(
		ASkyLight::StaticClass(),
		FVector(0.f, 0.f, 26000.f),
		FRotator::ZeroRotator);
	if (USkyLightComponent* SkyComp = ViewSkyLight ? Cast<USkyLightComponent>(ViewSkyLight->GetLightComponent()) : nullptr)
	{
		// Ambiente REAL desde un cubemap HDRI del engine (no captura el fondo negro de
		// la escena). Esto es lo que faltaba: sin ambiente, las mallas LIT (edificios,
		// arboles) salian negras. Con esto se ven a color como en el visor del asset.
		if (UTextureCube* Ambient = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap")))
		{
			SkyComp->SourceType = SLS_SpecifiedCubemap;
			SkyComp->Cubemap = Ambient;
		}
		SkyComp->bLowerHemisphereIsBlack = false;
		// Restaurado al valor estable (2.9): el terreno vuelve a ser unlit (olivo).
		SkyComp->SetLowerHemisphereColor(FLinearColor(0.55f, 0.57f, 0.58f));
		SkyComp->SetIntensity(2.9f);
		SkyComp->RecaptureSky();
	}

	const FVector Center = ProjectLonLat(-69.3f, 7.05f) + FVector(0.f, 0.f, 2600.f);
	ViewFog = World->SpawnActor<AExponentialHeightFog>(
		AExponentialHeightFog::StaticClass(),
		Center + FVector(0.f, 0.f, 1200.f),
		FRotator::ZeroRotator);
	if (ViewFog && ViewFog->GetComponent())
	{
		ViewFog->GetComponent()->SetFogDensity(0.00034f);
		ViewFog->GetComponent()->SetFogHeightFalloff(0.060f);
		ViewFog->GetComponent()->SetFogMaxOpacity(0.14f);
		ViewFog->GetComponent()->SetStartDistance(118000.f);
		ViewFog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.010f, 0.018f, 0.019f));
	}

	FVector CameraLocation(Center.X, Center.Y, 320000.f);
	FRotator CameraRotation(-90.f, 0.f, 0.f);
	FString CityVisualTest;
	if (FParse::Value(FCommandLine::Get(), TEXT("WLCityVisualTest="), CityVisualTest))
	{
		float CityLon = -66.90f;
		float CityLat = 10.50f;
		const FString CityKey = CityVisualTest.TrimStartAndEnd().ToUpper();
		if (CityKey.Contains(TEXT("MARACAIBO")))
		{
			CityLon = -71.60f;
			CityLat = 10.60f;
		}
		else if (CityKey.Contains(TEXT("VALENCIA")))
		{
			CityLon = -68.00f;
			CityLat = 10.20f;
		}
		else if (CityKey.Contains(TEXT("BARQUISIMETO")))
		{
			CityLon = -69.32f;
			CityLat = 10.07f;
		}

		float TestHeight = 52000.f;
		FParse::Value(FCommandLine::Get(), TEXT("WLCityVisualHeight="), TestHeight);
		TestHeight = FMath::Clamp(TestHeight, 42000.f, 120000.f);
		const float Pitch = -46.f;
		const FVector Focus = ProjectLonLat(CityLon, CityLat) + FVector(0.f, 0.f, 1800.f);
		const float ForwardOffset = TestHeight / FMath::Tan(FMath::DegreesToRadians(FMath::Abs(Pitch)));
		CameraLocation = Focus + FVector(-ForwardOffset, 0.f, TestHeight);
		CameraRotation = FRotator(Pitch, 0.f, 0.f);
	}
	DefaultCameraLocation = CameraLocation;
	DefaultCameraRotation = CameraRotation;
	ViewCamera = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		CameraLocation,
		CameraRotation);
	if (ViewCamera && ViewCamera->GetCameraComponent())
	{
		ViewCamera->GetCameraComponent()->SetFieldOfView(46.f);
	}
}

void AWLCampaign3DView::DestroyPresentationActors()
{
	if (TerritoryLayer)
	{
		TerritoryLayer->ClearLayer();
	}
	if (ViewCamera)
	{
		ViewCamera->Destroy();
		ViewCamera = nullptr;
	}
	if (ViewDirectionalLight)
	{
		ViewDirectionalLight->Destroy();
		ViewDirectionalLight = nullptr;
	}
	if (ViewSkyLight)
	{
		ViewSkyLight->Destroy();
		ViewSkyLight = nullptr;
	}
	if (ViewFog)
	{
		ViewFog->Destroy();
		ViewFog = nullptr;
	}
}
