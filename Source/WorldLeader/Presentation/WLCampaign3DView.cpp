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

	BoundaryMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BoundaryMesh"));
	BoundaryMesh->SetupAttachment(SceneRoot);

	RoadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadMesh"));
	RoadMesh->SetupAttachment(SceneRoot);

	SettlementMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SettlementMesh"));
	SettlementMesh->SetupAttachment(SceneRoot);

	SelectionHighlightMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SelectionHighlightMesh"));
	SelectionHighlightMesh->SetupAttachment(SceneRoot);
	MovementRoutePreviewMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MovementRoutePreviewMesh"));
	MovementRoutePreviewMesh->SetupAttachment(SceneRoot);

	TerritoryLayer = CreateDefaultSubobject<UWLCampaignTerritoryLayerComponent>(TEXT("TerritoryLayer"));

	SettlementBlockInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SettlementBlockInstances"));
	SettlementBlockInstances->SetupAttachment(SceneRoot);
	SettlementTowerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SettlementTowerInstances"));
	SettlementTowerInstances->SetupAttachment(SceneRoot);
	TreeInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeInstances"));
	TreeInstances->SetupAttachment(SceneRoot);
	BrushInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BrushInstances"));
	BrushInstances->SetupAttachment(SceneRoot);
	PortInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PortInstances"));
	PortInstances->SetupAttachment(SceneRoot);
	ArmyMarkerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArmyMarkerInstances"));
	ArmyMarkerInstances->SetupAttachment(SceneRoot);

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

	ConfigureInstancedComponent(SettlementBlockInstances, CubeMesh, FLinearColor(0.34f, 0.305f, 0.235f));
	ConfigureInstancedComponent(SettlementTowerInstances, CubeMesh, FLinearColor(0.56f, 0.50f, 0.355f));
	ConfigureInstancedComponent(TreeInstances, ConeMesh ? ConeMesh : CityMesh, FLinearColor(0.010f, 0.135f, 0.032f));
	ConfigureInstancedComponent(BrushInstances, ConeMesh ? ConeMesh : CityMesh, FLinearColor(0.125f, 0.170f, 0.070f));
	ConfigureInstancedComponent(PortInstances, CubeMesh, FLinearColor(0.27f, 0.225f, 0.150f));
	ConfigureInstancedComponent(ArmyMarkerInstances, ConeMesh ? ConeMesh : CityMesh, FLinearColor(0.86f, 0.68f, 0.22f));

	// Mallas reales de edificios (pack Cartoon_City_Free). Se conservan SUS materiales
	// (no se les aplica color plano). Si el pack no esta importado, la base
	// procedural sigue funcionando y BuildMeshCity simplemente no agrega nada.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityB0(TEXT("/Game/Cartoon_City_Free/Meshes/Buildings/SM_Eco_Building_Grid.SM_Eco_Building_Grid"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityB1(TEXT("/Game/Cartoon_City_Free/Meshes/Buildings/SM_Eco_Building_Slope.SM_Eco_Building_Slope"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityB2(TEXT("/Game/Cartoon_City_Free/Meshes/Buildings/SM_Eco_Building_Terrace.SM_Eco_Building_Terrace"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityB3(TEXT("/Game/Cartoon_City_Free/Meshes/Buildings/SM_Regular_Building_TwistedTower_Large.SM_Regular_Building_TwistedTower_Large"));
	UStaticMesh* LoadedCityMeshes[4] = { CityB0.Object, CityB1.Object, CityB2.Object, CityB3.Object };
	for (int32 i = 0; i < 4; ++i)
	{
		if (LoadedCityMeshes[i])
		{
			CityBuildingMeshes.Add(LoadedCityMeshes[i]);
		}
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
	if (BoundaryMesh)
	{
		BoundaryMesh->ClearAllMeshSections();
	}
	if (RoadMesh)
	{
		RoadMesh->ClearAllMeshSections();
	}
	if (SettlementMesh)
	{
		SettlementMesh->ClearAllMeshSections();
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
	for (UTextRenderComponent* Label : SettlementLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	SettlementLabels.Reset();
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
		TreeInstances,
		BrushInstances,
		PortInstances,
		ArmyMarkerInstances
	})
	{
		if (Instanced)
		{
			Instanced->ClearInstances();
		}
	}
	SetupLightingAndCamera();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: lighting/camera ready."));
	BuildSea();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: sea ready."));
	BuildOverviewLayer();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: overview ready."));
	BuildTerrain();
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D BuildView phase: terrain ready."));
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
