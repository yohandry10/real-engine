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

namespace
{
	UProceduralMeshComponent* FindRoadDetailMesh(AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}

		TArray<UProceduralMeshComponent*> Components;
		Owner->GetComponents<UProceduralMeshComponent>(Components);
		for (UProceduralMeshComponent* Component : Components)
		{
			if (Component && Component->GetFName() == TEXT("RoadDetailMesh"))
			{
				return Component;
			}
		}
		return nullptr;
	}
}

AWLCampaign3DView::AWLCampaign3DView()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SeaMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SeaMesh"));
	SeaMesh->SetupAttachment(SceneRoot);
	SeaDetailMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SeaDetailMesh"));
	SeaDetailMesh->SetupAttachment(SceneRoot);

	OverviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("OverviewMesh"));
	OverviewMesh->SetupAttachment(SceneRoot);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	TerrainMesh->SetupAttachment(SceneRoot);
	TerrainDetailMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainDetailMesh"));
	TerrainDetailMesh->SetupAttachment(SceneRoot);

	BoundaryMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BoundaryMesh"));
	BoundaryMesh->SetupAttachment(SceneRoot);

	RoadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadMesh"));
	RoadMesh->SetupAttachment(SceneRoot);
	UProceduralMeshComponent* RoadDetailMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadDetailMesh"));
	RoadDetailMesh->SetupAttachment(SceneRoot);

	SettlementMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SettlementMesh"));
	SettlementMesh->SetupAttachment(SceneRoot);
	BorderOutpostMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BorderOutpostMesh"));
	BorderOutpostMesh->SetupAttachment(SceneRoot);
	PortFacilityMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PortFacilityMesh"));
	PortFacilityMesh->SetupAttachment(SceneRoot);

	SelectionHighlightMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SelectionHighlightMesh"));
	SelectionHighlightMesh->SetupAttachment(SceneRoot);
	MovementRoutePreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MovementRoutePreviewMesh"));
	MovementRoutePreviewMesh->SetupAttachment(SceneRoot);

	TerritoryLayer = CreateDefaultSubobject<UWLCampaignTerritoryLayerComponent>(TEXT("TerritoryLayer"));

	SettlementBlockInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SettlementBlockInstances"));
	SettlementBlockInstances->SetupAttachment(SceneRoot);
	SettlementTowerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SettlementTowerInstances"));
	SettlementTowerInstances->SetupAttachment(SceneRoot);
	PortInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PortInstances"));
	PortInstances->SetupAttachment(SceneRoot);
	ArmyMarkerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArmyMarkerInstances"));
	ArmyMarkerInstances->SetupAttachment(SceneRoot);

	// Un ISM por tipo de naturaleza (EWLNatureKind). Nombres estables para CreateDefaultSubobject.
	static const TCHAR* NatureComponentNames[] = {
		TEXT("NatureConifer"), TEXT("NatureBroadleaf"), TEXT("NaturePalm"),
		TEXT("NatureRock"), TEXT("NaturePeak"), TEXT("NatureMount")
	};
	NatureInstances.SetNum(static_cast<int32>(EWLNatureKind::Count));
	for (int32 i = 0; i < static_cast<int32>(EWLNatureKind::Count); ++i)
	{
		NatureInstances[i] = CreateDefaultSubobject<UInstancedStaticMeshComponent>(NatureComponentNames[i]);
		NatureInstances[i]->SetupAttachment(SceneRoot);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexMatFinder(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexMatFinder.Succeeded())
	{
		VertexColorMaterial = VertexMatFinder.Object;
	}

	// Material LIT/PBR del terreno (M1). Si aun no esta importado, queda null y el terreno cae al
	// VertexColorMaterial unlit (no rompe).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TerrainLitFinder(TEXT("/Game/GenTerrain/M_TerrainLit.M_TerrainLit"));
	if (TerrainLitFinder.Succeeded())
	{
		TerrainLitMaterial = TerrainLitFinder.Object;
	}

	// PBR cercano rechazado para 90k: era pesado y empeoraba la lectura. Dejamos los slots
	// vacios para mantener el terreno base liviano de teatro en todos los zooms.
	TerrainPBRBiomeMaterials.SetNum(static_cast<int32>(EWLVisualBiome::Count));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CityMeshFinder.Succeeded())
	{
		CityMesh = CityMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RouteMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (RouteMeshFinder.Succeeded())
	{
		RouteMesh = RouteMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		CubeMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		ConeMesh = ConeMeshFinder.Object;
	}

	// Tokens de ejercito (NPC en el mapa): modelos de unidad unlit vertex-color de /Game/GenVehicle.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TokenLand(TEXT("/Game/GenVehicle/veh_mbt.veh_mbt"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TokenNaval(TEXT("/Game/GenVehicle/veh_ship.veh_ship"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TokenAir(TEXT("/Game/GenVehicle/veh_aircraft.veh_aircraft"));
	if (TokenLand.Succeeded())  { ForceTokenLandMesh = TokenLand.Object; }
	if (TokenNaval.Succeeded()) { ForceTokenNavalMesh = TokenNaval.Object; }
	if (TokenAir.Succeeded())   { ForceTokenAirMesh = TokenAir.Object; }

	ConfigureInstancedComponent(SettlementBlockInstances, CubeMesh, FLinearColor(0.34f, 0.305f, 0.235f));
	ConfigureInstancedComponent(SettlementTowerInstances, CubeMesh, FLinearColor(0.56f, 0.50f, 0.355f));
	ConfigureInstancedComponent(PortInstances, CubeMesh, FLinearColor(0.27f, 0.225f, 0.150f));
	ConfigureInstancedComponent(ArmyMarkerInstances, ConeMesh ? ConeMesh : CityMesh, FLinearColor(0.86f, 0.68f, 0.22f));

	// Modelos de CIUDAD 3D cohesiva generados en Blender (manzanas + calles + edificios modernos),
	// importados a /Game/GenCity. Se coloca UNO por asentamiento segun tamaño (ver BuildMeshCity).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityLarge(TEXT("/Game/GenCity/city_large.city_large"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityLargeB(TEXT("/Game/GenCity/city_large_b.city_large_b"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityLargeC(TEXT("/Game/GenCity/city_large_c.city_large_c"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityMedium(TEXT("/Game/GenCity/city_medium.city_medium"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityMediumB(TEXT("/Game/GenCity/city_medium_b.city_medium_b"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityMediumC(TEXT("/Game/GenCity/city_medium_c.city_medium_c"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CitySmall(TEXT("/Game/GenCity/city_small.city_small"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CitySmallB(TEXT("/Game/GenCity/city_small_b.city_small_b"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CitySmallC(TEXT("/Game/GenCity/city_small_c.city_small_c"));
	if (CityLarge.Succeeded())  { CityModelLarge  = CityLarge.Object; }
	if (CityMedium.Succeeded()) { CityModelMedium = CityMedium.Object; }
	if (CitySmall.Succeeded())  { CityModelSmall  = CitySmall.Object; }
	// Variantes (base + b + c). El picker en BuildMeshCity reparte por hash; si falta alguna, simplemente
	// hay menos variantes (no rompe).
	if (CityModelLarge)      { CityModelLargeVariants.Add(CityModelLarge); }
	if (CityLargeB.Object)   { CityModelLargeVariants.Add(CityLargeB.Object); }
	if (CityLargeC.Object)   { CityModelLargeVariants.Add(CityLargeC.Object); }
	if (CityModelMedium)     { CityModelMediumVariants.Add(CityModelMedium); }
	if (CityMediumB.Object)  { CityModelMediumVariants.Add(CityMediumB.Object); }
	if (CityMediumC.Object)  { CityModelMediumVariants.Add(CityMediumC.Object); }
	if (CityModelSmall)      { CityModelSmallVariants.Add(CityModelSmall); }
	if (CitySmallB.Object)   { CityModelSmallVariants.Add(CitySmallB.Object); }
	if (CitySmallC.Object)   { CityModelSmallVariants.Add(CitySmallC.Object); }

	// Naturaleza cercana desactivada: el scatter actual leia como hongos/rocas gigantes y rompia 90k.
	NatureMeshes.SetNum(static_cast<int32>(EWLNatureKind::Count));
	for (int32 i = 0; i < NatureInstances.Num(); ++i)
	{
		UInstancedStaticMeshComponent* Comp = NatureInstances[i];
		if (!Comp)
		{
			continue;
		}
		Comp->SetStaticMesh(nullptr);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetVisibility(false, true);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road0(TEXT("/Game/Cartoon_City_Free/Meshes/Roads/SM_road_001.SM_road_001"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road1(TEXT("/Game/Cartoon_City_Free/Meshes/Roads/SM_road_003.SM_road_003"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road2(TEXT("/Game/Cartoon_City_Free/Meshes/Roads/SM_road_009.SM_road_009"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road3(TEXT("/Game/Cartoon_City_Free/Meshes/Roads/SM_road_013.SM_road_013"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road4(TEXT("/Game/Cartoon_City_Free/Meshes/Roads/SM_road_019.SM_road_019"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road5(TEXT("/Game/Cartoon_City_Free/Meshes/Roads/SM_road_020.SM_road_020"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Road6(TEXT("/Game/Cartoon_City_Free/Meshes/Roads/SM_road_022.SM_road_022"));
	UStaticMesh* LoadedRoadMeshes[7] = { Road0.Object, Road1.Object, Road2.Object, Road3.Object, Road4.Object, Road5.Object, Road6.Object };
	for (int32 i = 0; i < 7; ++i)
	{
		if (LoadedRoadMeshes[i])
		{
			RoadAssetMeshes.Add(LoadedRoadMeshes[i]);
		}
	}

}

void AWLCampaign3DView::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoBuildOnBeginPlay && !bHasBuiltView)
	{
		BuildView(ActivePlayerNationIso);
	}
}

void AWLCampaign3DView::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPresentationActors();
	Super::EndPlay(EndPlayReason);
}

void AWLCampaign3DView::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PendingCityVisualScreenshotSeconds >= 0.f && !bCityVisualScreenshotRequested)
	{
		PendingCityVisualScreenshotSeconds -= DeltaSeconds;
		if (PendingCityVisualScreenshotSeconds <= 0.f)
		{
			bCityVisualScreenshotRequested = true;
			const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("Screenshots/Windows/CodexCityVisual.png");
			FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false, false, FIntRect(), true);
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D visual screenshot requested: %s"), *ScreenshotPath);
			if (bCityVisualQuitAfterScreenshot)
			{
				CityVisualQuitCountdownSeconds = 4.f;
			}
		}
	}

	if (CityVisualQuitCountdownSeconds >= 0.f)
	{
		CityVisualQuitCountdownSeconds -= DeltaSeconds;
		if (CityVisualQuitCountdownSeconds <= 0.f)
		{
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D visual screenshot complete; requesting exit."));
			FPlatformMisc::RequestExit(false);
			CityVisualQuitCountdownSeconds = -1.f;
		}
	}

	if (PendingBorderOutpostScreenshotIndex != INDEX_NONE)
	{
		PendingBorderOutpostScreenshotSeconds -= DeltaSeconds;
		if (PendingBorderOutpostScreenshotSeconds <= 0.f)
		{
			if (PendingBorderOutpostScreenshotIndex < PendingBorderOutpostScreenshotKeys.Num()
				&& PendingBorderOutpostScreenshotIndex < PendingBorderOutpostScreenshotLonLats.Num())
			{
				const FString& TargetKey = PendingBorderOutpostScreenshotKeys[PendingBorderOutpostScreenshotIndex];
				const FVector2D LonLat = PendingBorderOutpostScreenshotLonLats[PendingBorderOutpostScreenshotIndex];
				const FVector Focus = ProjectLonLat(LonLat.X, LonLat.Y) + FVector(0.f, 0.f, 1600.f);
				const FVector CameraLocation = Focus + FVector(0.f, 0.f, BorderOutpostScreenshotHeight);
				const FRotator CameraRotation(-90.f, 0.f, 0.f);
				if (ViewCamera)
				{
					ViewCamera->SetActorLocation(CameraLocation);
					ViewCamera->SetActorRotation(CameraRotation);
				}
				DefaultCameraLocation = CameraLocation;
				DefaultCameraRotation = CameraRotation;

				const FString ScreenshotPath = FPaths::ProjectSavedDir()
					/ FString::Printf(
						TEXT("Screenshots/Windows/BorderOutpost_%02d_%s.png"),
						PendingBorderOutpostScreenshotIndex + 1,
						*TargetKey);
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false, false, FIntRect(), true);
				UE_LOG(
					LogWorldLeader,
					Log,
					TEXT("Campaign3D border outpost screenshot requested: %s lon=%.3f lat=%.3f"),
					*ScreenshotPath,
					LonLat.X,
					LonLat.Y);

				++PendingBorderOutpostScreenshotIndex;
				PendingBorderOutpostScreenshotSeconds = 1.6f;
			}
			else
			{
				UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D border outpost screenshot sequence complete."));
				PendingBorderOutpostScreenshotIndex = INDEX_NONE;
				PendingBorderOutpostScreenshotSeconds = -1.f;
				if (bBorderOutpostScreenshotQuitAfterSequence)
				{
					BorderOutpostScreenshotQuitCountdownSeconds = 3.f;
				}
			}
		}
	}

	if (BorderOutpostScreenshotQuitCountdownSeconds >= 0.f)
	{
		BorderOutpostScreenshotQuitCountdownSeconds -= DeltaSeconds;
		if (BorderOutpostScreenshotQuitCountdownSeconds <= 0.f)
		{
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D border outpost screenshot sequence exit requested."));
			FPlatformMisc::RequestExit(false);
			BorderOutpostScreenshotQuitCountdownSeconds = -1.f;
		}
	}

	if (PendingPortFacilityScreenshotIndex != INDEX_NONE)
	{
		PendingPortFacilityScreenshotSeconds -= DeltaSeconds;
		if (PendingPortFacilityScreenshotSeconds <= 0.f)
		{
			if (PendingPortFacilityScreenshotIndex < PendingPortFacilityScreenshotKeys.Num()
				&& PendingPortFacilityScreenshotIndex < PendingPortFacilityScreenshotLonLats.Num())
			{
				const FString& TargetKey = PendingPortFacilityScreenshotKeys[PendingPortFacilityScreenshotIndex];
				const FVector2D LonLat = PendingPortFacilityScreenshotLonLats[PendingPortFacilityScreenshotIndex];
				const FVector Focus = ProjectLonLat(LonLat.X, LonLat.Y) + FVector(0.f, 0.f, 2200.f);
				// Vista OBLICUA (~52 grados) que reproduce el angulo real de juego, no cenital -> la captura
				// valida lo que ve el usuario (antes -90 cenital ocultaba el skyline y la escala real).
				const FVector CameraLocation = Focus + FVector(-PortFacilityScreenshotHeight * 0.78f, 0.f, PortFacilityScreenshotHeight);
				const FRotator CameraRotation(-52.f, 0.f, 0.f);
				if (ViewCamera)
				{
					ViewCamera->SetActorLocation(CameraLocation);
					ViewCamera->SetActorRotation(CameraRotation);
				}
				DefaultCameraLocation = CameraLocation;
				DefaultCameraRotation = CameraRotation;

				const FString ScreenshotPath = FPaths::ProjectSavedDir()
					/ FString::Printf(
						TEXT("Screenshots/Windows/PortFacility_%02d_%s.png"),
						PendingPortFacilityScreenshotIndex + 1,
						*TargetKey);
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false, false, FIntRect(), true);
				UE_LOG(
					LogWorldLeader,
					Log,
					TEXT("Campaign3D port facility screenshot requested: %s lon=%.3f lat=%.3f"),
					*ScreenshotPath,
					LonLat.X,
					LonLat.Y);

				++PendingPortFacilityScreenshotIndex;
				PendingPortFacilityScreenshotSeconds = 1.6f;
			}
			else
			{
				UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D port facility screenshot sequence complete."));
				PendingPortFacilityScreenshotIndex = INDEX_NONE;
				PendingPortFacilityScreenshotSeconds = -1.f;
				if (bPortFacilityScreenshotQuitAfterSequence)
				{
					PortFacilityScreenshotQuitCountdownSeconds = 3.f;
				}
			}
		}
	}

	if (PortFacilityScreenshotQuitCountdownSeconds >= 0.f)
	{
		PortFacilityScreenshotQuitCountdownSeconds -= DeltaSeconds;
		if (PortFacilityScreenshotQuitCountdownSeconds <= 0.f)
		{
			UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D port facility screenshot sequence exit requested."));
			FPlatformMisc::RequestExit(false);
			PortFacilityScreenshotQuitCountdownSeconds = -1.f;
		}
	}

	if (!SeaDetailMesh || IsHidden())
	{
		return;
	}

	WaterAnimationTime += DeltaSeconds;
	const FVector Drift(
		FMath::Sin(WaterAnimationTime * 0.17f) * 520.f,
		FMath::Cos(WaterAnimationTime * 0.13f) * 760.f,
		FMath::Sin(WaterAnimationTime * 0.31f) * 42.f);
	SeaDetailMesh->SetRelativeLocation(Drift);
}

void AWLCampaign3DView::BuildView(const FString& PlayerNationIso)
{
	ActivePlayerNationIso = PlayerNationIso.TrimStartAndEnd().ToUpper();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView start: nation=%s"), *ActivePlayerNationIso);
	bHasBuiltView = true;
	Bounds = FBox2D(ForceInit);
	OverviewBounds = FBox2D(ForceInit);
	ProvinceViews.Reset();
	CityViews.Reset();
	ForceViews.Reset();
	MovementNodes.Reset();
	ActiveMovementDestinations.Reset();
	MovementAdjacency.Reset();
	ActiveMovementForceId.Reset();
	PreviewMovementDestinationNodeId.Reset();
	SelectedProvinceHighlightId.Reset();
	SelectedCityHighlightId.Reset();
	SelectedForceHighlightId.Reset();
	HoveredForceHighlightId.Reset();
	ForceMarkerBaseScales.Reset();
	ForceMarkerBaseColors.Reset();

	if (SeaMesh)
	{
		SeaMesh->ClearAllMeshSections();
	}
	if (SeaDetailMesh)
	{
		SeaDetailMesh->ClearAllMeshSections();
	}
	if (OverviewMesh)
	{
		OverviewMesh->ClearAllMeshSections();
	}
	if (TerrainMesh)
	{
		TerrainMesh->ClearAllMeshSections();
	}
	if (TerrainDetailMesh)
	{
		TerrainDetailMesh->ClearAllMeshSections();
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->ClearAllMeshSections();
	}
	if (RoadMesh)
	{
		RoadMesh->ClearAllMeshSections();
	}
	if (UProceduralMeshComponent* RoadDetailMesh = FindRoadDetailMesh(this))
	{
		RoadDetailMesh->ClearAllMeshSections();
	}
	if (SettlementMesh)
	{
		SettlementMesh->ClearAllMeshSections();
	}
	if (BorderOutpostMesh)
	{
		BorderOutpostMesh->ClearAllMeshSections();
		BorderOutpostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (PortFacilityMesh)
	{
		PortFacilityMesh->ClearAllMeshSections();
		PortFacilityMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->ClearAllMeshSections();
	}
	DestroyMovementDestinationMarkers();
	for (UPrimitiveComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	ProvinceMarkers.Reset();
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	CitySelectionMarkers.Reset();
	for (UStaticMeshComponent* Component : CityBuildingComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	CityBuildingComponents.Reset();
	for (UStaticMeshComponent* Component : RoadAssetComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	RoadAssetComponents.Reset();
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	ForceMarkerComponents.Reset();
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	ForceSelectionMarkers.Reset();
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	ForceMarkerLabels.Reset();
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	VisualComponents.Reset();
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->DestroyComponent();
		}
	}
	RouteSegments.Reset();
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	Labels.Reset();
	CountryLabelBaseSizes.Reset();
	for (UTextRenderComponent* Label : SettlementLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	SettlementLabels.Reset();
	SettlementLabelBaseSizes.Reset();
	SettlementLabelMaxHeights.Reset();
	for (UTextRenderComponent* Label : OverviewLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	OverviewLabels.Reset();
	OverviewLabelVisibilityMasks.Reset();
	if (TerritoryLayer)
	{
		TerritoryLayer->ClearLayer();
	}
	for (UInstancedStaticMeshComponent* Instanced : {
		SettlementBlockInstances,
		SettlementTowerInstances,
		PortInstances,
		ArmyMarkerInstances
	})
	{
		if (Instanced)
		{
			Instanced->ClearInstances();
		}
	}
	for (UInstancedStaticMeshComponent* Nature : NatureInstances)
	{
		if (Nature)
		{
			Nature->ClearInstances();
		}
	}
	SetupLightingAndCamera();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: lighting/camera ready."));
	BuildSea();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: sea ready."));
	BuildOverviewLayer();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: overview ready."));
	// Pre-pasada: recopila los flat pads de cada ciudad (solo registra pads, no construye nada) para
	// que BuildTerrain APLANE el terreno bajo cada ciudad ANTES de mallarlo. Sin esto el relieve/ruido
	// (~2600u sobre la huella de la capital) inclina y "se come" la ciudad (un mesh rigido y plano).
	CityFlatPads.Reset();
	bCollectFlatPadsOnly = true;
	BuildCampaignVisualLayer();
	bCollectFlatPadsOnly = false;
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: city flat pads collected=%d."), CityFlatPads.Num());
	BuildTerrain();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: terrain ready (flattened under cities)."));
	if (TerrainDetailMesh)
	{
		TerrainDetailMesh->ClearAllMeshSections();
		TerrainDetailMesh->SetVisibility(false, true);
		TerrainDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: close PBR terrain detail disabled."));
	BuildCampaignVisualLayer();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: campaign visual layer ready. Cities=%d Forces=%d"), CityViews.Num(), ForceViews.Num());
	if (TerritoryLayer)
	{
		TerritoryLayer->BuildLayer(SceneRoot, VertexColorMaterial, [this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		});
		UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: territory layer ready."));
	}
	LogScaleAudit();
	ApplyZoomLOD(DefaultCameraLocation.Z);
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: zoom LOD applied."));
	SetPresentationActive(true, true);
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView complete."));

	float ScreenshotDelay = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("WLCityVisualScreenshot="), ScreenshotDelay))
	{
		PendingCityVisualScreenshotSeconds = FMath::Max(0.2f, ScreenshotDelay);
		bCityVisualScreenshotRequested = false;
		bCityVisualQuitAfterScreenshot = FParse::Param(FCommandLine::Get(), TEXT("WLCityVisualQuitAfterScreenshot"));
		CityVisualQuitCountdownSeconds = -1.f;
		UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D visual screenshot scheduled in %.2fs."), PendingCityVisualScreenshotSeconds);
	}

	FString OutpostSequenceValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("WLBorderOutpostScreenshotSequence="), OutpostSequenceValue))
	{
		PendingBorderOutpostScreenshotKeys.Reset();
		PendingBorderOutpostScreenshotLonLats.Reset();

		TArray<FString> RequestedTargets;
		const FString SequenceKey = OutpostSequenceValue.TrimStartAndEnd().ToUpper();
		if (SequenceKey.Equals(TEXT("COUNTRIES")))
		{
			const TCHAR* CountryTargets[] = {
				TEXT("VENEZUELA"), TEXT("COLOMBIA"), TEXT("ECUADOR"), TEXT("PERU"), TEXT("CHILE"),
				TEXT("ARGENTINA"), TEXT("BOLIVIA"), TEXT("URUGUAY"), TEXT("PARAGUAY"), TEXT("BRASIL")
			};
			for (const TCHAR* Target : CountryTargets)
			{
				RequestedTargets.Add(Target);
			}
		}
		else if (SequenceKey.Equals(TEXT("ALL")))
		{
			const TCHAR* AllTargets[] = {
				TEXT("BO-CO-VE-TACHIRA"), TEXT("BO-CO-VE-GUAJIRA"), TEXT("BO-CO-VE-ARAUCA"), TEXT("BO-CO-VE-ORINOCO"),
				TEXT("BO-VE-BR-SANTA-ELENA"), TEXT("BO-VE-GY-GUAYANA"), TEXT("BO-CO-EC-RUMICHACA"),
				TEXT("BO-CO-BR-LETICIA"), TEXT("BO-EC-PE-HUAQUILLAS"), TEXT("BO-PE-CL-TACNA"),
				TEXT("BO-PE-BR-INAPARI"), TEXT("BO-PE-BO-DESAGUADERO"), TEXT("BO-CL-AR-LIBERTADORES"),
				TEXT("BO-CL-AR-SAMORE"), TEXT("BO-AR-BO-LAQUIACA"), TEXT("BO-AR-PY-ENCARNACION"),
				TEXT("BO-AR-UY-FRAY-BENTOS"), TEXT("BO-AR-BR-IGUAZU"), TEXT("BO-BO-BR-CORUMBA"),
				TEXT("BO-BO-PY-CHACO"), TEXT("BO-UY-BR-RIVERA"), TEXT("BO-UY-BR-CHUY"),
				TEXT("BO-PY-BR-GUAIRA"), TEXT("BO-PY-BR-PJC"), TEXT("BO-BR-GY-LETHEM"),
				TEXT("BO-BR-GF-OIAPOQUE")
			};
			for (const TCHAR* Target : AllTargets)
			{
				RequestedTargets.Add(Target);
			}
		}
		else
		{
			OutpostSequenceValue.ParseIntoArray(RequestedTargets, TEXT(","), true);
		}

		for (FString Target : RequestedTargets)
		{
			Target = Target.TrimStartAndEnd().ToUpper();
			FVector2D LonLat;
			if (!ResolveBorderOutpostVisualTarget(Target, LonLat))
			{
				UE_LOG(LogWorldLeader, Warning, TEXT("Campaign3D outpost screenshot skipped unknown target: %s"), *Target);
				continue;
			}
			Target.ReplaceInline(TEXT(" "), TEXT("_"));
			Target.ReplaceInline(TEXT("/"), TEXT("_"));
			Target.ReplaceInline(TEXT("\\"), TEXT("_"));
			Target.ReplaceInline(TEXT(":"), TEXT("_"));
			PendingBorderOutpostScreenshotKeys.Add(Target);
			PendingBorderOutpostScreenshotLonLats.Add(LonLat);
		}

		if (!PendingBorderOutpostScreenshotKeys.IsEmpty())
		{
			FParse::Value(FCommandLine::Get(), TEXT("WLBorderOutpostVisualHeight="), BorderOutpostScreenshotHeight);
			BorderOutpostScreenshotHeight = FMath::Clamp(BorderOutpostScreenshotHeight, 18000.f, 180000.f);
			PendingBorderOutpostScreenshotIndex = 0;
			PendingBorderOutpostScreenshotSeconds = 2.f;
			BorderOutpostScreenshotQuitCountdownSeconds = -1.f;
			bBorderOutpostScreenshotQuitAfterSequence = FParse::Param(FCommandLine::Get(), TEXT("WLBorderOutpostQuitAfterSequence"));
			UE_LOG(
				LogWorldLeader,
				Log,
				TEXT("Campaign3D border outpost screenshot sequence scheduled: %d targets height=%.0f."),
				PendingBorderOutpostScreenshotKeys.Num(),
				BorderOutpostScreenshotHeight);
		}
	}

	FString PortSequenceValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("WLPortFacilityScreenshotSequence="), PortSequenceValue))
	{
		PendingPortFacilityScreenshotKeys.Reset();
		PendingPortFacilityScreenshotLonLats.Reset();

		TArray<FString> RequestedTargets;
		const FString SequenceKey = PortSequenceValue.TrimStartAndEnd().ToUpper();
		if (SequenceKey.Equals(TEXT("COUNTRIES")) || SequenceKey.Equals(TEXT("ALL")))
		{
			const TCHAR* CountryTargets[] = {
				TEXT("VENEZUELA"), TEXT("COLOMBIA"), TEXT("ECUADOR"), TEXT("PERU"), TEXT("CHILE"),
				TEXT("ARGENTINA"), TEXT("BOLIVIA"), TEXT("URUGUAY"), TEXT("PARAGUAY"), TEXT("BRASIL")
			};
			for (const TCHAR* Target : CountryTargets)
			{
				RequestedTargets.Add(Target);
			}
		}
		else
		{
			PortSequenceValue.ParseIntoArray(RequestedTargets, TEXT(","), true);
		}

		for (FString Target : RequestedTargets)
		{
			Target = Target.TrimStartAndEnd().ToUpper();
			FVector2D LonLat;
			if (!ResolvePortFacilityVisualTarget(Target, LonLat))
			{
				UE_LOG(LogWorldLeader, Warning, TEXT("Campaign3D port facility screenshot skipped unknown target: %s"), *Target);
				continue;
			}
			Target.ReplaceInline(TEXT(" "), TEXT("_"));
			Target.ReplaceInline(TEXT("/"), TEXT("_"));
			Target.ReplaceInline(TEXT("\\"), TEXT("_"));
			Target.ReplaceInline(TEXT(":"), TEXT("_"));
			PendingPortFacilityScreenshotKeys.Add(Target);
			PendingPortFacilityScreenshotLonLats.Add(LonLat);
		}

		if (!PendingPortFacilityScreenshotKeys.IsEmpty())
		{
			FParse::Value(FCommandLine::Get(), TEXT("WLPortFacilityVisualHeight="), PortFacilityScreenshotHeight);
			PortFacilityScreenshotHeight = FMath::Clamp(PortFacilityScreenshotHeight, 18000.f, 120000.f);
			PendingPortFacilityScreenshotIndex = 0;
			PendingPortFacilityScreenshotSeconds = 2.f;
			PortFacilityScreenshotQuitCountdownSeconds = -1.f;
			bPortFacilityScreenshotQuitAfterSequence = FParse::Param(FCommandLine::Get(), TEXT("WLPortFacilityQuitAfterSequence"));
			UE_LOG(
				LogWorldLeader,
				Log,
				TEXT("Campaign3D port facility screenshot sequence scheduled: %d targets height=%.0f."),
				PendingPortFacilityScreenshotKeys.Num(),
				PortFacilityScreenshotHeight);
		}
	}
}

void AWLCampaign3DView::SetPresentationActive(bool bActive, bool bSetCamera)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	SetActorTickEnabled(bActive);
	SetComponentSetActive(bActive);
	if (TerritoryLayer)
	{
		TerritoryLayer->SetPresentationActive(bActive);
	}

	if (ViewDirectionalLight)
	{
		ViewDirectionalLight->SetActorHiddenInGame(!bActive);
	}
	if (ViewSkyLight)
	{
		ViewSkyLight->SetActorHiddenInGame(!bActive);
	}
	if (ViewFog)
	{
		ViewFog->SetActorHiddenInGame(!bActive);
	}
	if (bActive && bSetCamera && ViewCamera)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->SetViewTargetWithBlend(ViewCamera, 0.35f);
		}
	}
}
