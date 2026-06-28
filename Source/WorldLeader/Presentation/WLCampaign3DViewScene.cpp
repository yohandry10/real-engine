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

bool AWLCampaign3DView::ResolveBorderOutpostVisualTarget(const FString& TargetKey, FVector2D& OutLonLat) const
{
	const FString OutpostKey = TargetKey.TrimStartAndEnd().ToUpper();
	struct FOutpostCameraTarget
	{
		const TCHAR* Token;
		float Lon;
		float Lat;
	};
	static const FOutpostCameraTarget Targets[] = {
		{ TEXT("VENEZUELA"), -72.166f, 11.138f },
		{ TEXT("TACHIRA"), -72.325f, 7.967f },
		{ TEXT("GUAJIRA"), -72.166f, 11.138f },
		{ TEXT("ARAUCA"), -70.604f, 7.203f },
		{ TEXT("ORINOCO"), -67.449f, 5.900f },
		{ TEXT("SANTA"), -61.310f, 4.498f },
		{ TEXT("PACARAIMA"), -61.310f, 4.498f },
		{ TEXT("GUAYANA"), -60.240f, 7.875f },
		{ TEXT("COLOMBIA"), -77.680f, 0.820f },
		{ TEXT("RUMICHACA"), -77.680f, 0.820f },
		{ TEXT("LETICIA"), -69.940f, -4.235f },
		{ TEXT("ECUADOR"), -80.340f, -3.500f },
		{ TEXT("HUAQUILLAS"), -80.340f, -3.500f },
		{ TEXT("PERU"), -70.280f, -18.250f },
		{ TEXT("TACNA"), -70.280f, -18.250f },
		{ TEXT("INAPARI"), -68.950f, -11.800f },
		{ TEXT("DESAGUADERO"), -69.530f, -16.200f },
		{ TEXT("CHILE"), -69.800f, -33.000f },
		{ TEXT("LIBERTADORES"), -69.800f, -33.000f },
		{ TEXT("SAMORE"), -72.100f, -41.300f },
		{ TEXT("ARGENTINA"), -54.585f, -25.570f },
		{ TEXT("LAQUIACA"), -65.590f, -22.095f },
		{ TEXT("ENCARNACION"), -55.885f, -27.350f },
		{ TEXT("FRAY"), -58.600f, -33.500f },
		{ TEXT("IGUAZU"), -54.585f, -25.570f },
		{ TEXT("BOLIVIA"), -57.725f, -18.995f },
		{ TEXT("CORUMBA"), -57.725f, -18.995f },
		{ TEXT("CHACO"), -60.490f, -22.275f },
		{ TEXT("URUGUAY"), -55.540f, -30.895f },
		{ TEXT("RIVERA"), -55.540f, -30.895f },
		{ TEXT("CHUY"), -53.455f, -33.690f },
		{ TEXT("PARAGUAY"), -55.735f, -22.540f },
		{ TEXT("GUAIRA"), -54.275f, -24.070f },
		{ TEXT("PEDRO"), -55.735f, -22.540f },
		{ TEXT("PJC"), -55.735f, -22.540f },
		{ TEXT("BRASIL"), -61.310f, 4.498f },
		{ TEXT("BRAZIL"), -61.310f, 4.498f },
		{ TEXT("LETHEM"), -60.235f, 3.100f },
		{ TEXT("OIAPOQUE"), -51.815f, 3.865f }
	};
	for (const FOutpostCameraTarget& Target : Targets)
	{
		if (OutpostKey.Contains(Target.Token))
		{
			OutLonLat = FVector2D(Target.Lon, Target.Lat);
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::ResolvePortFacilityVisualTarget(const FString& TargetKey, FVector2D& OutLonLat) const
{
	const FString PortKey = TargetKey.TrimStartAndEnd().ToUpper();
	struct FPortCameraTarget
	{
		const TCHAR* Token;
		float Lon;
		float Lat;
	};
	static const FPortCameraTarget Targets[] = {
		{ TEXT("VENEZUELA"), -68.01f, 10.47f },
		{ TEXT("VE"), -68.01f, 10.47f },
		{ TEXT("PUERTO_CABELLO"), -68.01f, 10.47f },
		{ TEXT("PUERTO CABELLO"), -68.01f, 10.47f },
		{ TEXT("COLOMBIA"), -75.50f, 10.40f },
		{ TEXT("CO"), -75.50f, 10.40f },
		{ TEXT("CARTAGENA"), -75.50f, 10.40f },
		{ TEXT("ECUADOR"), -80.72f, -0.95f },
		{ TEXT("EC"), -80.72f, -0.95f },
		{ TEXT("MANTA"), -80.72f, -0.95f },
		{ TEXT("GUAYAQUIL"), -79.90f, -2.19f },
		{ TEXT("PERU"), -79.03f, -8.11f },
		{ TEXT("PE"), -79.03f, -8.11f },
		{ TEXT("TRUJILLO"), -79.03f, -8.11f },
		{ TEXT("CHILE"), -71.62f, -33.05f },
		{ TEXT("CL"), -71.62f, -33.05f },
		{ TEXT("VALPARAISO"), -71.62f, -33.05f },
		{ TEXT("ARGENTINA"), -57.55f, -38.00f },
		{ TEXT("AR"), -57.55f, -38.00f },
		{ TEXT("MAR_DEL_PLATA"), -57.55f, -38.00f },
		{ TEXT("MAR DEL PLATA"), -57.55f, -38.00f },
		{ TEXT("BOLIVIA"), -57.80f, -18.98f },
		{ TEXT("BO"), -57.80f, -18.98f },
		{ TEXT("PUERTO_SUAREZ"), -57.80f, -18.98f },
		{ TEXT("PUERTO SUAREZ"), -57.80f, -18.98f },
		{ TEXT("URUGUAY"), -57.84f, -34.47f },
		{ TEXT("UY"), -57.84f, -34.47f },
		{ TEXT("COLONIA"), -57.84f, -34.47f },
		{ TEXT("PARAGUAY"), -57.86f, -25.50f },
		{ TEXT("PY"), -57.86f, -25.50f },
		{ TEXT("ASUNCION"), -57.86f, -25.50f },
		{ TEXT("BRASIL"), -43.20f, -22.91f },
		{ TEXT("BRAZIL"), -43.20f, -22.91f },
		{ TEXT("BR"), -43.20f, -22.91f },
		{ TEXT("RIO"), -43.20f, -22.91f }
	};
	for (const FPortCameraTarget& Target : Targets)
	{
		const FString Token(Target.Token);
		const bool bExactToken = PortKey.Equals(Token, ESearchCase::IgnoreCase);
		const bool bNamedToken = Token.Len() > 2 && PortKey.Contains(Token);
		if (bExactToken || bNamedToken)
		{
			OutLonLat = FVector2D(Target.Lon, Target.Lat);
			return true;
		}
	}
	return false;
}

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
		ViewDirectionalLight->GetLightComponent()->SetIntensity(5.25f);
		ViewDirectionalLight->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.86f));
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
		SkyComp->SetLowerHemisphereColor(FLinearColor(0.22f, 0.25f, 0.24f));
		SkyComp->SetIntensity(1.35f);
		SkyComp->RecaptureSky();
	}

	const FVector Center = ProjectLonLat(-69.3f, 7.05f) + FVector(0.f, 0.f, 2600.f);
	ViewFog = World->SpawnActor<AExponentialHeightFog>(
		AExponentialHeightFog::StaticClass(),
		Center + FVector(0.f, 0.f, 1200.f),
		FRotator::ZeroRotator);
	if (ViewFog && ViewFog->GetComponent())
	{
		ViewFog->GetComponent()->SetFogDensity(0.0f);
		ViewFog->GetComponent()->SetFogHeightFalloff(0.060f);
		ViewFog->GetComponent()->SetFogMaxOpacity(0.0f);
		ViewFog->GetComponent()->SetStartDistance(220000.f);
		ViewFog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.018f, 0.250f, 0.305f));
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
		else if (CityKey.Contains(TEXT("PUERTO")) || CityKey.Contains(TEXT("CABELLO")))
		{
			CityLon = -68.01f;
			CityLat = 10.47f;
		}
		else if (CityKey.Contains(TEXT("PUNTO")) || CityKey.Contains(TEXT("FIJO")))
		{
			CityLon = -70.20f;
			CityLat = 11.70f;
		}
		else if (CityKey.Contains(TEXT("BARQUISIMETO")))
		{
			CityLon = -69.32f;
			CityLat = 10.07f;
		}

		float TestHeight = 52000.f;
		FParse::Value(FCommandLine::Get(), TEXT("WLCityVisualHeight="), TestHeight);
		TestHeight = FMath::Clamp(TestHeight, 42000.f, 620000.f);
		const float Pitch = TestHeight <= 60000.f
			? FMath::GetMappedRangeValueClamped(FVector2D(15000.f, 60000.f), FVector2D(-64.f, -50.f), TestHeight)
			: FMath::GetMappedRangeValueClamped(FVector2D(60000.f, 320000.f), FVector2D(-50.f, -84.f), TestHeight);
		const FVector Focus = ProjectLonLat(CityLon, CityLat) + FVector(0.f, 0.f, 1800.f);
		const float ForwardOffset = TestHeight / FMath::Tan(FMath::DegreesToRadians(FMath::Abs(Pitch)));
		CameraLocation = Focus + FVector(-ForwardOffset, 0.f, TestHeight);
		CameraRotation = FRotator(Pitch, 0.f, 0.f);
	}
	FString BorderOutpostVisualTest;
	if (FParse::Value(FCommandLine::Get(), TEXT("WLBorderOutpostVisualTest="), BorderOutpostVisualTest))
	{
		FVector2D OutpostLonLat(-72.166f, 11.138f);
		const FString OutpostKey = BorderOutpostVisualTest.TrimStartAndEnd();
		if (!ResolveBorderOutpostVisualTarget(OutpostKey, OutpostLonLat))
		{
			OutpostLonLat = FVector2D(-72.166f, 11.138f);
		}

		float TestHeight = 42000.f;
		FParse::Value(FCommandLine::Get(), TEXT("WLBorderOutpostVisualHeight="), TestHeight);
		TestHeight = FMath::Clamp(TestHeight, 18000.f, 180000.f);
		const FVector Focus = ProjectLonLat(OutpostLonLat.X, OutpostLonLat.Y) + FVector(0.f, 0.f, 1600.f);
		CameraLocation = Focus + FVector(0.f, 0.f, TestHeight);
		CameraRotation = FRotator(-90.f, 0.f, 0.f);
		UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D border outpost visual test camera: %s lon=%.3f lat=%.3f height=%.0f world=(%.0f,%.0f,%.0f)"),
			*OutpostKey,
			OutpostLonLat.X,
			OutpostLonLat.Y,
			TestHeight,
			CameraLocation.X,
			CameraLocation.Y,
			CameraLocation.Z);
	}
	FString PortFacilityVisualTest;
	if (FParse::Value(FCommandLine::Get(), TEXT("WLPortFacilityVisualTest="), PortFacilityVisualTest))
	{
		FVector2D PortLonLat(-68.01f, 10.47f);
		const FString PortKey = PortFacilityVisualTest.TrimStartAndEnd();
		if (!ResolvePortFacilityVisualTarget(PortKey, PortLonLat))
		{
			PortLonLat = FVector2D(-68.01f, 10.47f);
		}

		float TestHeight = 36000.f;
		FParse::Value(FCommandLine::Get(), TEXT("WLPortFacilityVisualHeight="), TestHeight);
		TestHeight = FMath::Clamp(TestHeight, 18000.f, 120000.f);
		const FVector Focus = ProjectLonLat(PortLonLat.X, PortLonLat.Y) + FVector(0.f, 0.f, 2200.f);
		CameraLocation = Focus + FVector(0.f, 0.f, TestHeight);
		CameraRotation = FRotator(-90.f, 0.f, 0.f);
		UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D port facility visual test camera: %s lon=%.3f lat=%.3f height=%.0f world=(%.0f,%.0f,%.0f)"),
			*PortKey,
			PortLonLat.X,
			PortLonLat.Y,
			TestHeight,
			CameraLocation.X,
			CameraLocation.Y,
			CameraLocation.Z);
	}
	DefaultCameraLocation = CameraLocation;
	DefaultCameraRotation = CameraRotation;
	ViewCamera = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		CameraLocation,
		CameraRotation);
	if (ViewCamera && ViewCamera->GetCameraComponent())
	{
		UCameraComponent* CameraComp = ViewCamera->GetCameraComponent();
		CameraComp->SetFieldOfView(46.f);
		CameraComp->PostProcessBlendWeight = 1.f;
		FPostProcessSettings& PP = CameraComp->PostProcessSettings;
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = -0.55f;
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(1.28f, 1.24f, 1.18f, 1.f);
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(0.98f, 0.92f, 0.88f, 1.f);
		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(0.93f, 0.94f, 0.96f, 1.f);
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
