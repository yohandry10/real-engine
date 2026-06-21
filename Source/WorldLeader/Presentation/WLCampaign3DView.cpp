// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

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
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FString RouteKey(const FString& A, const FString& B)
	{
		return A < B ? FString::Printf(TEXT("%s|%s"), *A, *B) : FString::Printf(TEXT("%s|%s"), *B, *A);
	}

	FString SettlementTypeToPanelText(EWLCampaignSettlementType Type)
	{
		switch (Type)
		{
		case EWLCampaignSettlementType::Capital: return TEXT("capital");
		case EWLCampaignSettlementType::LargeCity: return TEXT("large city");
		case EWLCampaignSettlementType::Port: return TEXT("port");
		case EWLCampaignSettlementType::Frontier: return TEXT("border city");
		case EWLCampaignSettlementType::Industrial: return TEXT("industrial city");
		default: return TEXT("settlement");
		}
	}

	FString SettlementStrategicRole(EWLCampaignSettlementType Type)
	{
		switch (Type)
		{
		case EWLCampaignSettlementType::Capital: return TEXT("capital command hub");
		case EWLCampaignSettlementType::LargeCity: return TEXT("urban economic hub");
		case EWLCampaignSettlementType::Port: return TEXT("port and coastal logistics");
		case EWLCampaignSettlementType::Frontier: return TEXT("border control point");
		case EWLCampaignSettlementType::Industrial: return TEXT("industry and energy hub");
		default: return TEXT("regional settlement");
		}
	}

	FString ReadCampaignMilitaryForceStringField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		const FString& DefaultValue = TEXT(""))
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
	}

	float ReadCampaignMilitaryForceNumberField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		float DefaultValue = 0.f)
	{
		double Value = static_cast<double>(DefaultValue);
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(FieldName, Value);
		}
		return static_cast<float>(Value);
	}

	int32 ReadCampaignMilitaryForceIntField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		int32 DefaultValue = 0)
	{
		double Value = static_cast<double>(DefaultValue);
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(FieldName, Value);
		}
		return FMath::RoundToInt(Value);
	}

	bool ReadCampaignMilitaryForceBoolField(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName,
		bool bDefaultValue = false)
	{
		bool bValue = bDefaultValue;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	TArray<FString> ReadCampaignMilitaryForceStringArray(
		const TSharedPtr<FJsonObject>& Obj,
		const TCHAR* FieldName)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(FieldName, Values) || !Values)
		{
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
			{
				Result.Add(Text);
			}
		}
		return Result;
	}

	FLinearColor CampaignMilitaryForceColor(const FWLCampaign3DForceView& Force)
	{
		const FString Category = Force.MarkerCategory.ToLower();
		if (Category.Contains(TEXT("naval")))
		{
			return FLinearColor(0.20f, 0.70f, 0.88f, 1.f);
		}
		if (Category.Contains(TEXT("air")))
		{
			return FLinearColor(0.72f, 0.86f, 0.96f, 1.f);
		}
		if (Category.Contains(TEXT("mechanized")))
		{
			return FLinearColor(0.91f, 0.62f, 0.24f, 1.f);
		}
		if (Category.Contains(TEXT("frontier")))
		{
			return FLinearColor(0.86f, 0.74f, 0.32f, 1.f);
		}
		if (Category.Contains(TEXT("urban")))
		{
			return FLinearColor(0.58f, 0.66f, 0.68f, 1.f);
		}
		return Force.CountryIso.Equals(TEXT("VE"), ESearchCase::IgnoreCase)
			? FLinearColor(0.84f, 0.50f, 0.30f, 1.f)
			: FLinearColor(0.74f, 0.68f, 0.36f, 1.f);
	}

	FVector CampaignMilitaryForceScale(const FWLCampaign3DForceView& Force)
	{
		const FString Category = Force.MarkerCategory.ToLower();
		if (Category.Contains(TEXT("naval")))
		{
			return FVector(10.5f, 10.5f, 8.6f);
		}
		if (Category.Contains(TEXT("air")))
		{
			return FVector(9.2f, 9.2f, 10.8f);
		}
		if (Category.Contains(TEXT("mechanized")))
		{
			return FVector(9.5f, 9.5f, 14.0f);
		}
		if (Category.Contains(TEXT("frontier")))
		{
			return FVector(8.4f, 8.4f, 12.0f);
		}
		if (Category.Contains(TEXT("urban")))
		{
			return FVector(8.0f, 8.0f, 10.8f);
		}
		return FVector(9.0f, 9.0f, 13.0f);
	}

	void LoadCampaignMilitaryForceDefinitions(TArray<FWLCampaign3DForceView>& OutForces)
	{
		OutForces.Reset();
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("MilitaryForces.json");
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *Path))
		{
			return;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root->TryGetArrayField(TEXT("forces"), Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
			{
				continue;
			}

			FWLCampaign3DForceView Force;
			Force.Id = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("id"));
			Force.Name = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("name"));
			Force.CountryIso = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("country_iso")).ToUpper();
			Force.CountryName = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("country_name"));
			Force.ForceType = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("force_type"));
			Force.MarkerCategory = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("marker_category"), TEXT("land"));
			Force.MovementNodeId = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("movement_node_id"));
			Force.MovementStatus = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("movement_status"), TEXT("detenido"));
			Force.LocationName = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("location"));
			Force.ProvinceId = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("province_id"));
			Force.ProvinceName = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("province"));
			Force.NearbyCity = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("nearby_city"));
			Force.Lon = ReadCampaignMilitaryForceNumberField(*ObjPtr, TEXT("lon"));
			Force.Lat = ReadCampaignMilitaryForceNumberField(*ObjPtr, TEXT("lat"));
			Force.EstimatedStrength = FMath::Max(0, ReadCampaignMilitaryForceIntField(*ObjPtr, TEXT("strength")));
			Force.Mobility = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("mobility"), TEXT("media"));
			Force.OperationalState = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("operational_state"), TEXT("placeholder"));
			Force.Supply = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("supply"), TEXT("sin datos"));
			Force.Morale = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("morale"), TEXT("sin datos"));
			Force.Posture = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("posture"), TEXT("observacion"));
			Force.StrategicRole = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("strategic_role"), TEXT("presencia militar placeholder"));
			Force.DetailLevel = ReadCampaignMilitaryForceStringField(*ObjPtr, TEXT("detail_level"), TEXT("placeholder force marker"));
			Force.DisabledActions = ReadCampaignMilitaryForceStringArray(*ObjPtr, TEXT("disabled_actions"));
			if (Force.DisabledActions.IsEmpty())
			{
				Force.DisabledActions = {
					TEXT("Mover"),
					TEXT("Atacar"),
					TEXT("Reorganizar"),
					TEXT("Reforzar"),
					TEXT("Ver composicion"),
					TEXT("Abrir logistica"),
					TEXT("Auto-resolve")
				};
			}

			const FString Category = Force.MarkerCategory.ToLower();
			Force.bAir = Category.Contains(TEXT("air"));
			Force.bNaval = Category.Contains(TEXT("naval"));
			Force.bMovable = ReadCampaignMilitaryForceBoolField(*ObjPtr, TEXT("movable"), !Force.bAir);
			if (!Force.Id.IsEmpty()
				&& !Force.Name.IsEmpty()
				&& FWLCampaignRegionGeometry::IsTheaterIso(Force.CountryIso)
				&& !FMath::IsNearlyZero(Force.Lon)
				&& !FMath::IsNearlyZero(Force.Lat))
			{
				OutForces.Add(MoveTemp(Force));
			}
		}
	}

	bool CampaignPointInTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
	{
		const float Area = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
		const float S = ((A.Y - C.Y) * (P.X - C.X) + (C.X - A.X) * (P.Y - C.Y)) / Area;
		const float T = ((C.Y - B.Y) * (P.X - C.X) + (B.X - C.X) * (P.Y - C.Y)) / Area;
		const float U = 1.f - S - T;
		return S >= 0.f && T >= 0.f && U >= 0.f;
	}

	float SignedArea(const TArray<FVector2D>& Poly)
	{
		float Area = 0.f;
		for (int32 Index = 0; Index < Poly.Num(); ++Index)
		{
			const FVector2D& A = Poly[Index];
			const FVector2D& B = Poly[(Index + 1) % Poly.Num()];
			Area += (A.X * B.Y) - (B.X * A.Y);
		}
		return Area * 0.5f;
	}

	void TriangulateSimplePolygon(const TArray<FVector2D>& Contour, TArray<int32>& OutTris)
	{
		OutTris.Reset();
		const int32 Num = Contour.Num();
		if (Num < 3)
		{
			return;
		}

		TArray<int32> V;
		V.Reserve(Num);
		const bool bClockwise = SignedArea(Contour) < 0.f;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			V.Add(bClockwise ? (Num - 1 - Index) : Index);
		}

		int32 Guard = 0;
		while (V.Num() > 2 && Guard++ < Num * Num)
		{
			bool bClipped = false;
			for (int32 Index = 0; Index < V.Num(); ++Index)
			{
				const int32 Prev = V[(Index + V.Num() - 1) % V.Num()];
				const int32 Curr = V[Index];
				const int32 Next = V[(Index + 1) % V.Num()];
				const FVector2D& A = Contour[Prev];
				const FVector2D& B = Contour[Curr];
				const FVector2D& C = Contour[Next];

				const float Cross = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
				if (Cross <= 0.f)
				{
					continue;
				}

				bool bEar = true;
				for (int32 TestIndex : V)
				{
					if (TestIndex == Prev || TestIndex == Curr || TestIndex == Next)
					{
						continue;
					}
					if (CampaignPointInTriangle(A, B, C, Contour[TestIndex]))
					{
						bEar = false;
						break;
					}
				}

				if (bEar)
				{
					OutTris.Add(Prev);
					OutTris.Add(Curr);
					OutTris.Add(Next);
					V.RemoveAt(Index);
					bClipped = true;
					break;
				}
			}

			if (!bClipped)
			{
				break;
			}
		}
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
	BuildSea();
	BuildOverviewLayer();
	BuildTerrain();
	BuildCampaignVisualLayer();
	if (TerritoryLayer)
	{
		TerritoryLayer->BuildLayer(SceneRoot, VertexColorMaterial, [this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		});
	}
	ApplyZoomLOD(DefaultCameraLocation.Z);
	SetPresentationActive(true, true);
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

bool AWLCampaign3DView::TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DProvinceView& OutProvince) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ProvinceMarkers.Num() && Index < ProvinceViews.Num(); ++Index)
	{
		if (ProvinceMarkers[Index] == Component)
		{
			OutProvince = ProvinceViews[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetTerritoryForComponent(
	const UPrimitiveComponent* Component,
	FWLCampaignTerritoryRegionView& OutTerritory) const
{
	return TerritoryLayer && TerritoryLayer->TryGetTerritoryForComponent(Component, OutTerritory);
}

bool AWLCampaign3DView::TryGetTerritoryAtWorldLocation(
	const FVector& WorldLocation,
	FWLCampaignTerritoryRegionView& OutTerritory) const
{
	return TerritoryLayer && TerritoryLayer->TryGetTerritoryAtWorldLocation(WorldLocation, true, OutTerritory);
}

bool AWLCampaign3DView::TryGetCityForComponent(const UPrimitiveComponent* Component, FWLCampaign3DCityView& OutCity) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < CitySelectionMarkers.Num() && Index < CityViews.Num(); ++Index)
	{
		if (CitySelectionMarkers[Index] == Component)
		{
			OutCity = CityViews[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetCityNearWorldLocation(
	const FVector& WorldLocation,
	float MaxDistance,
	FWLCampaign3DCityView& OutCity) const
{
	float BestDistanceSq = FMath::Square(FMath::Max(1000.f, MaxDistance));
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < CityViews.Num(); ++Index)
	{
		const FWLCampaign3DCityView& City = CityViews[Index];
		const float DistanceSq = FVector::DistSquared2D(WorldLocation, City.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	OutCity = CityViews[BestIndex];
	return true;
}

bool AWLCampaign3DView::TryGetForceById(const FString& ForceId, FWLCampaign3DForceView& OutForce) const
{
	for (const FWLCampaign3DForceView& Force : ForceViews)
	{
		if (Force.Id.Equals(ForceId, ESearchCase::IgnoreCase))
		{
			OutForce = Force;
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetForceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DForceView& OutForce) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		const bool bProxyHit = ForceSelectionMarkers.IsValidIndex(Index) && ForceSelectionMarkers[Index] == Component;
		const bool bVisualHit = ForceMarkerComponents.IsValidIndex(Index) && ForceMarkerComponents[Index] == Component;
		if (bProxyHit || bVisualHit)
		{
			OutForce = ForceViews[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetForceNearWorldLocation(
	const FVector& WorldLocation,
	float MaxDistance,
	FWLCampaign3DForceView& OutForce) const
{
	float BestDistanceSq = FMath::Square(FMath::Max(1000.f, MaxDistance));
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		const FWLCampaign3DForceView& Force = ForceViews[Index];
		const float DistanceSq = FVector::DistSquared2D(WorldLocation, Force.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	OutForce = ForceViews[BestIndex];
	return true;
}

bool AWLCampaign3DView::TryGetMovementDestinationForComponent(
	const UPrimitiveComponent* Component,
	FWLCampaign3DMovementNodeView& OutNode) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ActiveMovementDestinations.Num(); ++Index)
	{
		const bool bProxyHit = MovementDestinationSelectionMarkers.IsValidIndex(Index)
			&& MovementDestinationSelectionMarkers[Index] == Component;
		const bool bVisualHit = MovementDestinationComponents.IsValidIndex(Index)
			&& MovementDestinationComponents[Index] == Component;
		if (bProxyHit || bVisualHit)
		{
			OutNode = ActiveMovementDestinations[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetMovementDestinationNearWorldLocation(
	const FVector& WorldLocation,
	float MaxDistance,
	FWLCampaign3DMovementNodeView& OutNode) const
{
	float BestDistanceSq = FMath::Square(FMath::Max(1000.f, MaxDistance));
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ActiveMovementDestinations.Num(); ++Index)
	{
		const FWLCampaign3DMovementNodeView& Node = ActiveMovementDestinations[Index];
		const float DistanceSq = FVector::DistSquared2D(WorldLocation, Node.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	OutNode = ActiveMovementDestinations[BestIndex];
	return true;
}

bool AWLCampaign3DView::BuildMovementRouteForForce(
	const FString& ForceId,
	const FString& DestinationNodeId,
	TArray<FWLCampaign3DMovementNodeView>& OutRouteNodes,
	int32& OutEstimatedTurns) const
{
	OutRouteNodes.Reset();
	OutEstimatedTurns = 0;

	FWLCampaign3DForceView Force;
	if (!TryGetForceById(ForceId, Force) || !Force.bMovable)
	{
		return false;
	}

	const FString OriginNodeId = Force.MovementNodeId.IsEmpty() ? FindNearestMovementNodeId(Force) : Force.MovementNodeId;
	if (OriginNodeId.IsEmpty() || DestinationNodeId.IsEmpty() || OriginNodeId.Equals(DestinationNodeId, ESearchCase::IgnoreCase))
	{
		return false;
	}

	const FWLCampaign3DMovementNodeView* OriginNode = FindMovementNodeById(OriginNodeId);
	const FWLCampaign3DMovementNodeView* DestinationNode = FindMovementNodeById(DestinationNodeId);
	if (!OriginNode || !DestinationNode)
	{
		return false;
	}

	const auto NodeAllowedForForce = [&Force](const FWLCampaign3DMovementNodeView& Node)
	{
		if (Force.bAir)
		{
			return false;
		}
		if (Force.bNaval)
		{
			return Node.bPort;
		}
		return true;
	};

	if (!NodeAllowedForForce(*DestinationNode))
	{
		return false;
	}

	TArray<FString> Queue;
	TMap<FString, FString> CameFrom;
	Queue.Add(OriginNodeId);
	CameFrom.Add(OriginNodeId, TEXT(""));
	for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
	{
		const FString Current = Queue[Cursor];
		if (Current.Equals(DestinationNodeId, ESearchCase::IgnoreCase))
		{
			break;
		}

		const TArray<FString>* Neighbors = MovementAdjacency.Find(Current);
		if (!Neighbors)
		{
			continue;
		}
		for (const FString& NeighborId : *Neighbors)
		{
			if (CameFrom.Contains(NeighborId))
			{
				continue;
			}
			const FWLCampaign3DMovementNodeView* NeighborNode = FindMovementNodeById(NeighborId);
			if (!NeighborNode || !NodeAllowedForForce(*NeighborNode))
			{
				continue;
			}
			CameFrom.Add(NeighborId, Current);
			Queue.Add(NeighborId);
		}
	}

	if (!CameFrom.Contains(DestinationNodeId))
	{
		return false;
	}

	TArray<FString> ReverseNodeIds;
	FString Current = DestinationNodeId;
	while (!Current.IsEmpty())
	{
		ReverseNodeIds.Add(Current);
		const FString* Previous = CameFrom.Find(Current);
		Current = Previous ? *Previous : FString();
	}

	for (int32 Index = ReverseNodeIds.Num() - 1; Index >= 0; --Index)
	{
		if (const FWLCampaign3DMovementNodeView* Node = FindMovementNodeById(ReverseNodeIds[Index]))
		{
			OutRouteNodes.Add(*Node);
		}
	}

	OutEstimatedTurns = FMath::Max(1, OutRouteNodes.Num() - 1);
	return OutRouteNodes.Num() >= 2;
}

bool AWLCampaign3DView::UpdateForceMovementLocation(
	const FString& ForceId,
	const FString& DestinationNodeId,
	FWLCampaign3DForceView& OutForce)
{
	const FWLCampaign3DMovementNodeView* DestinationNode = FindMovementNodeById(DestinationNodeId);
	if (!DestinationNode)
	{
		return false;
	}

	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		FWLCampaign3DForceView& Force = ForceViews[Index];
		if (!Force.Id.Equals(ForceId, ESearchCase::IgnoreCase) || !Force.bMovable)
		{
			continue;
		}

		Force.MovementNodeId = DestinationNode->Id;
		Force.MovementStatus = TEXT("reubicado");
		Force.LocationName = DestinationNode->Name;
		Force.ProvinceId = DestinationNode->ProvinceId;
		Force.ProvinceName = DestinationNode->ProvinceName;
		Force.NearbyCity = DestinationNode->Name;
		Force.OperationalState = TEXT("reubicado en destino");
		Force.Posture = Force.bNaval ? TEXT("patrulla en nuevo puerto") : TEXT("presencia en nuevo nodo");
		Force.WorldLocation = GetForceMarkerLocationForNode(Force, *DestinationNode);
		Force.Lon = DestinationNode->Lon;
		Force.Lat = DestinationNode->Lat;

		if (ForceMarkerComponents.IsValidIndex(Index) && ForceMarkerComponents[Index])
		{
			ForceMarkerComponents[Index]->SetWorldLocation(Force.WorldLocation);
		}
		if (ForceSelectionMarkers.IsValidIndex(Index) && ForceSelectionMarkers[Index])
		{
			ForceSelectionMarkers[Index]->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, 1400.f));
		}
		if (ForceMarkerLabels.IsValidIndex(Index) && ForceMarkerLabels[Index])
		{
			ForceMarkerLabels[Index]->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, 2550.f));
			ForceMarkerLabels[Index]->SetText(FText::FromString(Force.NearbyCity.IsEmpty() ? Force.Name : Force.NearbyCity));
		}

		OutForce = Force;
		if (SelectedForceHighlightId.Equals(Force.Id, ESearchCase::IgnoreCase))
		{
			SetSelectedForceHighlight(Force.Id);
		}
		return true;
	}

	return false;
}

void AWLCampaign3DView::ShowMovementDestinationOptions(const FString& ForceId)
{
	ActiveMovementForceId = ForceId;
	PreviewMovementDestinationNodeId.Reset();
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->ClearAllMeshSections();
	}

	FWLCampaign3DForceView Force;
	TArray<FWLCampaign3DMovementNodeView> Destinations;
	if (TryGetForceById(ForceId, Force))
	{
		GetValidMovementDestinations(Force, Destinations);
	}
	RebuildMovementDestinationMarkers(Destinations);
}

void AWLCampaign3DView::SetMovementDestinationPreview(const FString& ForceId, const FString& DestinationNodeId)
{
	if (ForceId.IsEmpty() || DestinationNodeId.IsEmpty())
	{
		if (PreviewMovementDestinationNodeId.IsEmpty())
		{
			return;
		}
		if (MovementRoutePreviewMesh)
		{
			MovementRoutePreviewMesh->ClearAllMeshSections();
		}
		PreviewMovementDestinationNodeId.Reset();
		RebuildMovementDestinationMarkers(ActiveMovementDestinations);
		return;
	}

	if (PreviewMovementDestinationNodeId.Equals(DestinationNodeId, ESearchCase::IgnoreCase))
	{
		return;
	}

	TArray<FWLCampaign3DMovementNodeView> RouteNodes;
	int32 EstimatedTurns = 0;
	if (!BuildMovementRouteForForce(ForceId, DestinationNodeId, RouteNodes, EstimatedTurns))
	{
		return;
	}

	PreviewMovementDestinationNodeId = DestinationNodeId;
	RebuildMovementDestinationMarkers(ActiveMovementDestinations);
	RebuildMovementRoutePreview(RouteNodes);
}

void AWLCampaign3DView::ClearMovementPreview()
{
	ActiveMovementForceId.Reset();
	PreviewMovementDestinationNodeId.Reset();
	ActiveMovementDestinations.Reset();
	DestroyMovementDestinationMarkers();
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetSelectedProvinceHighlight(const FString& ProvinceId)
{
	SelectedProvinceHighlightId = ProvinceId;
	SelectedCityHighlightId.Reset();
	SelectedForceHighlightId.Reset();
	RefreshMilitaryForceMarkerVisuals();
	if (TerritoryLayer)
	{
		TerritoryLayer->SetSelectedTerritory(ProvinceId);
	}

	for (const FWLCampaign3DProvinceView& Province : ProvinceViews)
	{
		if (Province.Id.Equals(ProvinceId, ESearchCase::IgnoreCase))
		{
			RebuildPointSelectionHighlight(Province.WorldLocation + FVector(0.f, 0.f, 820.f), 6200.f, FLinearColor(0.96f, 0.78f, 0.34f, 1.f));
			return;
		}
	}

	if (TerritoryLayer)
	{
		FWLCampaignTerritoryRegionView Territory;
		if (TerritoryLayer->GetRegionById(ProvinceId, Territory))
		{
			RebuildPointSelectionHighlight(Territory.WorldLocation + FVector(0.f, 0.f, 900.f), 5200.f, FLinearColor(0.96f, 0.78f, 0.34f, 1.f));
			return;
		}
	}

	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetSelectedCityHighlight(const FString& CityId)
{
	SelectedCityHighlightId = CityId;
	SelectedProvinceHighlightId.Reset();
	SelectedForceHighlightId.Reset();
	RefreshMilitaryForceMarkerVisuals();

	for (const FWLCampaign3DCityView& City : CityViews)
	{
		if (City.Id.Equals(CityId, ESearchCase::IgnoreCase))
		{
			if (TerritoryLayer && !City.TerritoryId.IsEmpty())
			{
				TerritoryLayer->SetSelectedTerritory(City.TerritoryId);
			}
			RebuildPointSelectionHighlight(City.WorldLocation + FVector(0.f, 0.f, 1240.f), City.bCapital ? 7600.f : 6200.f,
				City.bPort ? FLinearColor(0.46f, 0.82f, 0.86f, 1.f) : FLinearColor(0.96f, 0.78f, 0.34f, 1.f));
			return;
		}
	}

	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetSelectedForceHighlight(const FString& ForceId)
{
	SelectedForceHighlightId = ForceId;
	SelectedProvinceHighlightId.Reset();
	SelectedCityHighlightId.Reset();
	if (TerritoryLayer)
	{
		TerritoryLayer->SetSelectedTerritory(TEXT(""));
	}
	RefreshMilitaryForceMarkerVisuals();

	for (const FWLCampaign3DForceView& Force : ForceViews)
	{
		if (Force.Id.Equals(ForceId, ESearchCase::IgnoreCase))
		{
			RebuildPointSelectionHighlight(Force.WorldLocation + FVector(0.f, 0.f, 880.f),
				Force.bNaval ? 7600.f : 6600.f,
				Force.bAir ? FLinearColor(0.72f, 0.86f, 0.98f, 1.f) : FLinearColor(0.96f, 0.78f, 0.34f, 1.f));
			return;
		}
	}

	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetHoveredForceHighlight(const FString& ForceId)
{
	if (HoveredForceHighlightId.Equals(ForceId, ESearchCase::IgnoreCase))
	{
		return;
	}

	HoveredForceHighlightId = ForceId;
	RefreshMilitaryForceMarkerVisuals();
}

void AWLCampaign3DView::ClearSelectionHighlight()
{
	SelectedProvinceHighlightId.Reset();
	SelectedCityHighlightId.Reset();
	SelectedForceHighlightId.Reset();
	HoveredForceHighlightId.Reset();
	RefreshMilitaryForceMarkerVisuals();
	if (TerritoryLayer)
	{
		TerritoryLayer->SetSelectedTerritory(TEXT(""));
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

FBox2D AWLCampaign3DView::GetViewBounds2D() const
{
	return Bounds;
}

FBox2D AWLCampaign3DView::GetCameraBounds2D(float CameraHeight) const
{
	if (!OverviewBounds.bIsValid)
	{
		return Bounds;
	}

	if (!Bounds.bIsValid)
	{
		return OverviewBounds;
	}

	if (CameraHeight < 155000.f)
	{
		return Bounds;
	}

	FBox2D Combined = Bounds;
	const float Blend = FMath::Clamp((CameraHeight - 155000.f) / 350000.f, 0.f, 1.f);
	const FVector2D Center = Bounds.GetCenter();
	const FVector2D OverviewMin = FMath::Lerp(Bounds.Min, OverviewBounds.Min, Blend);
	const FVector2D OverviewMax = FMath::Lerp(Bounds.Max, OverviewBounds.Max, Blend);
	Combined.Min = FVector2D(FMath::Min(OverviewMin.X, Center.X), FMath::Min(OverviewMin.Y, Center.Y));
	Combined.Max = FVector2D(FMath::Max(OverviewMax.X, Center.X), FMath::Max(OverviewMax.Y, Center.Y));
	return Combined;
}

FVector AWLCampaign3DView::GetTheaterFocusPoint() const
{
	return ProjectLonLat(-69.3f, 7.05f) + FVector(0.f, 0.f, 2600.f);
}

FString AWLCampaign3DView::GetCurrentZoomLODLabel() const
{
	switch (CurrentZoomLOD)
	{
	case EWLCampaign3DZoomLOD::Close:
		return TEXT("Cercano");
	case EWLCampaign3DZoomLOD::Theater:
		return TEXT("Teatro");
	case EWLCampaign3DZoomLOD::Region:
		return TEXT("Region");
	case EWLCampaign3DZoomLOD::Global:
		return TEXT("America");
	default:
		return TEXT("Teatro");
	}
}

UWLDataRegistry* AWLCampaign3DView::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FVector AWLCampaign3DView::ProjectLonLat(float Lon, float Lat) const
{
	const float X = (Lat - TheaterCenterLonLat.Y) * GeoScale;
	const float Y = (Lon - TheaterCenterLonLat.X) * GeoScale;
	return FVector(X, Y, SampleTerrainHeight(Lon, Lat));
}

float AWLCampaign3DView::SampleTerrainHeight(float Lon, float Lat) const
{
	const float ColombianCentralRidge = FMath::Exp(-FMath::Square((Lon + 74.4f + (Lat - 6.0f) * 0.10f) * 0.82f))
		* FMath::Exp(-FMath::Square((Lat - 6.0f) * 0.18f));
	const float ColombianEasternRidge = FMath::Exp(-FMath::Square((Lon + 72.5f + (Lat - 7.0f) * 0.07f) * 0.88f))
		* FMath::Exp(-FMath::Square((Lat - 7.3f) * 0.20f));
	const float VenezuelanCoastalRange = FMath::Exp(-FMath::Square((Lat - 10.45f) * 1.85f))
		* FMath::Exp(-FMath::Square((Lon + 67.7f) * 0.28f));
	const float GuianaShield = FMath::Exp(-FMath::Square((Lon + 63.6f) * 0.34f))
		* FMath::Exp(-FMath::Square((Lat - 6.6f) * 0.26f));
	const float CoastalTerrace = FMath::Clamp((Lat - 1.2f) / 11.5f, 0.f, 1.f) * 0.05f;
	const float ReliefNoise = (FMath::Sin(Lon * 2.6f + Lat * 1.4f) + FMath::Cos(Lon * 1.2f - Lat * 2.8f)) * 0.035f;
	const float Height = ColombianCentralRidge * 0.92f
		+ ColombianEasternRidge * 0.68f
		+ VenezuelanCoastalRange * 0.45f
		+ GuianaShield * 0.36f
		+ CoastalTerrace
		+ ReliefNoise;
	return FMath::Max(0.f, Height * 9800.f);
}

FLinearColor AWLCampaign3DView::TerrainColor(EWLTerrainType Terrain, const FString& CountryIso) const
{
	if (Terrain == EWLTerrainType::Urban)
	{
		return FLinearColor(0.31f, 0.33f, 0.30f);
	}
	if (Terrain == EWLTerrainType::Coastal)
	{
		return FLinearColor(0.28f, 0.42f, 0.25f);
	}
	if (Terrain == EWLTerrainType::Jungle)
	{
		return FLinearColor(0.12f, 0.35f, 0.19f);
	}
	if (CountryIso.Equals(TEXT("CO"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.20f, 0.36f, 0.22f);
	}
	return FLinearColor(0.17f, 0.31f, 0.20f);
}

UMaterialInstanceDynamic* AWLCampaign3DView::MakeColorMaterial(const FLinearColor& Color)
{
	if (!BaseMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Mat->SetVectorParameterValue(TEXT("Base Color"), Color);
		Mat->SetVectorParameterValue(TEXT("DiffuseColor"), Color);
		Mat->SetVectorParameterValue(TEXT("Tint"), Color);
		Mat->SetVectorParameterValue(TEXT("Albedo"), Color);
	}
	return Mat;
}

void AWLCampaign3DView::BuildSea()
{
	FWLCampaignWaterBuildParams Params;
	Params.Center = ProjectLonLat(TheaterCenterLonLat.X, TheaterCenterLonLat.Y);
	Params.HalfSize = 3600000.f;
	Params.SeaZ = -2350.f;
	Params.TileCount = 60;
	Params.WaterMaterial = VertexColorMaterial;
	Params.DetailMaterial = VertexColorMaterial;

	FWLCampaignWaterBuilder::Build(SeaMesh, SeaDetailMesh, Params, [this](float Lon, float Lat)
	{
		return ProjectLonLat(Lon, Lat);
	});
}

void AWLCampaign3DView::BuildOverviewLayer()
{
	if (!OverviewMesh)
	{
		return;
	}

	FWLCampaignOverviewBuildParams Params;
	Params.Material = VertexColorMaterial;
	Params.LandZ = 9000.f;
	Params.BorderZ = 11800.f;
	Params.BorderWidth = 900.f;
	Params.CityZ = 14800.f;
	Params.CityMarkerSize = 0.f;

	TArray<FWLCampaignOverviewLabelSpec> OverviewLabelSpecs;
	FWLCampaignOverviewBuilder::Build(OverviewMesh, Params, OverviewLabelSpecs, [this](float Lon, float Lat)
	{
		return ProjectLonLat(Lon, Lat);
	});
	OverviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const TArray<FVector2D> OverviewCorners = {
		FVector2D(-172.f, 84.f),
		FVector2D(-10.f, 84.f),
		FVector2D(-10.f, -56.f),
		FVector2D(-172.f, -56.f)
	};
	for (const FVector2D& Corner : OverviewCorners)
	{
		const FVector P = ProjectLonLat(Corner.X, Corner.Y);
		OverviewBounds += FVector2D(P.X, P.Y);
	}

	for (const FWLCampaignOverviewLabelSpec& Label : OverviewLabelSpecs)
	{
		AddOverviewLabel(
			Label.Text,
			Label.Lon,
			Label.Lat,
			Label.ZOffset,
			Label.WorldSize,
			Label.Color,
			Label.bShowInGlobal,
			Label.bShowInRegion);
	}
}

void AWLCampaign3DView::BuildTerrain()
{
	FWLCampaignTerrainBuildParams Params;
	Params.RegionMinLon = RegionMinLon;
	Params.RegionMaxLon = RegionMaxLon;
	Params.RegionMinLat = RegionMinLat;
	Params.RegionMaxLat = RegionMaxLat;
	Params.TerrainMaterial = VertexColorMaterial;
	Params.BoundaryMaterial = VertexColorMaterial;

	FWLCampaignTerrainBuilder::Build(
		TerrainMesh,
		BoundaryMesh,
		Params,
		[this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		},
		Bounds);
}

void AWLCampaign3DView::AddTerrainPatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset)
{
	if (!TerrainMesh || LonLatPoints.Num() < 3)
	{
		return;
	}

	TArray<int32> Tris;
	TriangulateSimplePolygon(LonLatPoints, Tris);
	if (Tris.Num() < 3)
	{
		return;
	}

	TArray<FVector> Verts;
	TArray<FVector2D> UVs;
	Verts.Reserve(LonLatPoints.Num());
	UVs.Reserve(LonLatPoints.Num());
	for (const FVector2D& LonLat : LonLatPoints)
	{
		FVector P = ProjectLonLat(LonLat.X, LonLat.Y);
		P.Z += ZOffset;
		Verts.Add(P);
		UVs.Add(FVector2D((LonLat.X - RegionMinLon) / (RegionMaxLon - RegionMinLon), (LonLat.Y - RegionMinLat) / (RegionMaxLat - RegionMinLat)));
		Bounds += FVector2D(P.X, P.Y);
	}

	TArray<int32> FinalTris;
	FinalTris.Reserve(Tris.Num() * 2);
	for (int32 Index = 0; Index + 2 < Tris.Num(); Index += 3)
	{
		FinalTris.Add(Tris[Index]);
		FinalTris.Add(Tris[Index + 1]);
		FinalTris.Add(Tris[Index + 2]);
		FinalTris.Add(Tris[Index]);
		FinalTris.Add(Tris[Index + 2]);
		FinalTris.Add(Tris[Index + 1]);
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FColor> Colors;
	Colors.Init(FWLCampaignVisualStyle::ToVertexFColor(Color), Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	const int32 SectionIndex = TerrainMesh->GetNumSections();
	TerrainMesh->CreateMeshSection(SectionIndex, Verts, FinalTris, Normals, UVs, Colors, Tangents, true);
	if (VertexColorMaterial)
	{
		TerrainMesh->SetMaterial(SectionIndex, VertexColorMaterial);
	}
}

void AWLCampaign3DView::AddPolylineSegment(const FVector& Start, const FVector& End, const FLinearColor& Color, float RadiusScale)
{
	if (!BoundaryMesh)
	{
		return;
	}

	const FVector Direction = End - Start;
	const float Length = Direction.Size();
	if (Length < 1.f)
	{
		return;
	}

	const FVector FlatDirection(Direction.X, Direction.Y, 0.f);
	if (FlatDirection.SizeSquared() < 1.f)
	{
		return;
	}

	const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal()) * (RadiusScale * 360.f);
	TArray<FVector> Verts = {
		Start - Side,
		Start + Side,
		End + Side,
		End - Side
	};
	TArray<int32> Tris = { 0, 1, 2, 0, 2, 3 };
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs = { FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(1.f, 0.f) };
	TArray<FColor> Colors;
	Colors.Init(FWLCampaignVisualStyle::ToVertexFColor(Color), Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	const int32 SectionIndex = BoundaryMesh->GetNumSections();
	BoundaryMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (VertexColorMaterial)
	{
		BoundaryMesh->SetMaterial(SectionIndex, VertexColorMaterial);
	}
}

void AWLCampaign3DView::AddCoastline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale)
{
	if (LonLatPoints.Num() < 2)
	{
		return;
	}

	for (int32 Index = 0; Index < LonLatPoints.Num(); ++Index)
	{
		const FVector2D& A = LonLatPoints[Index];
		const FVector2D& B = LonLatPoints[(Index + 1) % LonLatPoints.Num()];
		const FVector Start = ProjectLonLat(A.X, A.Y) + FVector(0.f, 0.f, 320.f);
		const FVector End = ProjectLonLat(B.X, B.Y) + FVector(0.f, 0.f, 320.f);
		AddPolylineSegment(Start, End, Color, RadiusScale);
	}
}

void AWLCampaign3DView::BuildCampaignVisualLayer()
{
	// Geometria de tierra para empujar las ciudades costeras hacia el interior
	// (evita que los marcadores queden sobre la costa/el mar).
	SettlementLandGeometry.Reset();
	FWLCampaignRegionGeometry::LoadCountries(
		RegionMinLon - 3.f, RegionMaxLon + 3.f, RegionMinLat - 3.f, RegionMaxLat + 3.f,
		SettlementLandGeometry);

	AddBiomePatch({
		FVector2D(-72.05f, 10.92f),
		FVector2D(-71.52f, 10.74f),
		FVector2D(-71.05f, 10.30f),
		FVector2D(-70.70f, 9.65f),
		FVector2D(-70.86f, 9.05f),
		FVector2D(-71.25f, 8.62f),
		FVector2D(-71.76f, 8.48f),
		FVector2D(-72.20f, 8.82f),
		FVector2D(-72.42f, 9.38f),
		FVector2D(-72.36f, 10.10f)
	}, FLinearColor(0.020f, 0.430f, 0.520f, 0.92f), 3600.f);

	FWLCampaignRouteBuilder::BuildDefaultTheaterRoutes(
		RoadMesh,
		VertexColorMaterial,
		[this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		});

	const auto AddTheaterCountryLabel = [this](const FString& Text, float Lon, float Lat, const FColor& Color, float WorldSize)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		if (!Label)
		{
			return;
		}
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetWorldLocation(ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, 11800.f));
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(WorldSize);
		Label->SetText(FText::FromString(Text));
		Label->SetTextRenderColor(Color);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Labels.Add(Label);
	};

	AddTheaterCountryLabel(TEXT("COLOMBIA"), -74.2f, 5.7f, FColor(232, 206, 126), 3400.f);
	AddTheaterCountryLabel(TEXT("VENEZUELA"), -66.4f, 7.8f, FColor(232, 206, 126), 3400.f);

	AddSettlementCluster(TEXT("CO-BOGOTA"), TEXT("Bogota"), TEXT("CO"), TEXT("CO-DC"), TEXT("Bogota D.C."), -74.1f, 4.6f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CO-MEDELLIN"), TEXT("Medellin"), TEXT("CO"), TEXT("CO-ANT"), TEXT("Antioquia"), -75.58f, 6.25f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("CO-CALI"), TEXT("Cali"), TEXT("CO"), TEXT("CO-VAC"), TEXT("Valle del Cauca"), -76.53f, 3.45f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("CO-CARTAGENA"), TEXT("Cartagena"), TEXT("CO"), TEXT("CO-BOL"), TEXT("Bolivar"), -75.5f, 10.4f, EWLCampaignSettlementType::Port, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("CO-BARRANQUILLA"), TEXT("Barranquilla"), TEXT("CO"), TEXT("CO-ATL"), TEXT("Atlantico"), -74.8f, 10.98f, EWLCampaignSettlementType::Port, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("CO-SANTA-MARTA"), TEXT("Santa Marta"), TEXT("CO"), TEXT("CO-MAG"), TEXT("Magdalena"), -74.2f, 11.24f, EWLCampaignSettlementType::Port, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("CO-BUCARAMANGA"), TEXT("Bucaramanga"), TEXT("CO"), TEXT("CO-SAN"), TEXT("Santander"), -73.12f, 7.12f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("CO-RIOHACHA"), TEXT("Riohacha"), TEXT("CO"), TEXT("CO-LGJ"), TEXT("La Guajira"), -72.9f, 11.5f, EWLCampaignSettlementType::Port, FLinearColor(0.66f, 0.62f, 0.46f));
	AddSettlementCluster(TEXT("CO-CUCUTA"), TEXT("Cucuta"), TEXT("CO"), TEXT("CO-NSA"), TEXT("Norte de Santander"), -72.5f, 7.9f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("VE-MARACAIBO"), TEXT("Maracaibo"), TEXT("VE"), TEXT("VE-ZU"), TEXT("Zulia"), -71.6f, 10.6f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("VE-CARACAS"), TEXT("Caracas"), TEXT("VE"), TEXT("VE-DC"), TEXT("Distrito Capital"), -66.9f, 10.5f, EWLCampaignSettlementType::Capital, FLinearColor(0.92f, 0.58f, 0.34f));
	AddSettlementCluster(TEXT("VE-VALENCIA"), TEXT("Valencia"), TEXT("VE"), TEXT("VE-CAR"), TEXT("Carabobo"), -68.0f, 10.2f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.78f, 0.52f, 0.34f));
	AddSettlementCluster(TEXT("VE-BARQUISIMETO"), TEXT("Barquisimeto"), TEXT("VE"), TEXT("VE-LAR"), TEXT("Lara"), -69.32f, 10.07f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.78f, 0.52f, 0.34f));
	AddSettlementCluster(TEXT("VE-MARACAY"), TEXT("Maracay"), TEXT("VE"), TEXT("VE-ARA"), TEXT("Aragua"), -67.60f, 10.25f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.78f, 0.52f, 0.34f));
	AddSettlementCluster(TEXT("VE-SAN-CRISTOBAL"), TEXT("San Cristobal"), TEXT("VE"), TEXT("VE-TAC"), TEXT("Tachira"), -72.23f, 7.77f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.62f, 0.34f));
	AddSettlementCluster(TEXT("VE-PUERTO-LA-CRUZ"), TEXT("Puerto La Cruz"), TEXT("VE"), TEXT("VE-ANZ"), TEXT("Anzoategui"), -64.63f, 10.21f, EWLCampaignSettlementType::Port, FLinearColor(0.78f, 0.52f, 0.34f));
	AddSettlementCluster(TEXT("VE-CIUDAD-GUAYANA"), TEXT("Ciudad Guayana"), TEXT("VE"), TEXT("VE-BO"), TEXT("Bolivar"), -62.6f, 8.3f, EWLCampaignSettlementType::Industrial, FLinearColor(0.62f, 0.54f, 0.36f));

	// Lote 1 (Andes norte) - Ecuador. Mismo pipeline que CO/VE; CO/VE quedan intactos.
	AddTheaterCountryLabel(TEXT("ECUADOR"), -78.3f, -1.45f, FColor(232, 206, 126), 2400.f);
	AddSettlementCluster(TEXT("EC-QUITO"), TEXT("Quito"), TEXT("EC"), TEXT("EC-PIC"), TEXT("Pichincha"), -78.52f, -0.22f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("EC-GUAYAQUIL"), TEXT("Guayaquil"), TEXT("EC"), TEXT("EC-GUA"), TEXT("Guayas"), -79.90f, -2.19f, EWLCampaignSettlementType::Port, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("EC-CUENCA"), TEXT("Cuenca"), TEXT("EC"), TEXT("EC-AZU"), TEXT("Azuay"), -79.00f, -2.90f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-SANTO-DOMINGO"), TEXT("Santo Domingo"), TEXT("EC"), TEXT("EC-SDT"), TEXT("Santo Domingo"), -79.17f, -0.25f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-MANTA"), TEXT("Manta"), TEXT("EC"), TEXT("EC-MAN"), TEXT("Manabi"), -80.72f, -0.95f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("EC-MACHALA"), TEXT("Machala"), TEXT("EC"), TEXT("EC-ELO"), TEXT("El Oro"), -79.97f, -3.26f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("EC-AMBATO"), TEXT("Ambato"), TEXT("EC"), TEXT("EC-TUN"), TEXT("Tungurahua"), -78.62f, -1.24f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-LOJA"), TEXT("Loja"), TEXT("EC"), TEXT("EC-LOJ"), TEXT("Loja"), -79.20f, -3.99f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	// Lote 1 (Andes norte) - Peru.
	AddTheaterCountryLabel(TEXT("PERU"), -75.5f, -10.0f, FColor(232, 206, 126), 4200.f);
	AddSettlementCluster(TEXT("PE-LIMA"), TEXT("Lima"), TEXT("PE"), TEXT("PE-LIM"), TEXT("Lima"), -77.04f, -12.05f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("PE-TRUJILLO"), TEXT("Trujillo"), TEXT("PE"), TEXT("PE-LAL"), TEXT("La Libertad"), -79.03f, -8.11f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("PE-CHICLAYO"), TEXT("Chiclayo"), TEXT("PE"), TEXT("PE-LAM"), TEXT("Lambayeque"), -79.84f, -6.77f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("PE-PIURA"), TEXT("Piura"), TEXT("PE"), TEXT("PE-PIU"), TEXT("Piura"), -80.63f, -5.19f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("PE-AREQUIPA"), TEXT("Arequipa"), TEXT("PE"), TEXT("PE-ARE"), TEXT("Arequipa"), -71.54f, -16.41f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("PE-CUSCO"), TEXT("Cusco"), TEXT("PE"), TEXT("PE-CUS"), TEXT("Cusco"), -71.97f, -13.53f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("PE-IQUITOS"), TEXT("Iquitos"), TEXT("PE"), TEXT("PE-LOR"), TEXT("Loreto"), -73.25f, -3.75f, EWLCampaignSettlementType::Port, FLinearColor(0.66f, 0.62f, 0.46f));
	AddSettlementCluster(TEXT("PE-PUNO"), TEXT("Puno"), TEXT("PE"), TEXT("PE-PUN"), TEXT("Puno"), -70.02f, -15.84f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	// Lote 1 (Andes norte) - Bolivia. Mediterranea (sin puertos).
	AddTheaterCountryLabel(TEXT("BOLIVIA"), -65.0f, -17.4f, FColor(232, 206, 126), 3600.f);
	AddSettlementCluster(TEXT("BO-LA-PAZ"), TEXT("La Paz"), TEXT("BO"), TEXT("BO-LPZ"), TEXT("La Paz"), -68.15f, -16.50f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("BO-SANTA-CRUZ"), TEXT("Santa Cruz"), TEXT("BO"), TEXT("BO-SCZ"), TEXT("Santa Cruz"), -63.18f, -17.78f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BO-COCHABAMBA"), TEXT("Cochabamba"), TEXT("BO"), TEXT("BO-CBB"), TEXT("Cochabamba"), -66.16f, -17.39f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BO-SUCRE"), TEXT("Sucre"), TEXT("BO"), TEXT("BO-CHU"), TEXT("Chuquisaca"), -65.26f, -19.03f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BO-POTOSI"), TEXT("Potosi"), TEXT("BO"), TEXT("BO-PTS"), TEXT("Potosi"), -65.75f, -19.58f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BO-ORURO"), TEXT("Oruro"), TEXT("BO"), TEXT("BO-ORU"), TEXT("Oruro"), -67.10f, -17.98f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BO-TARIJA"), TEXT("Tarija"), TEXT("BO"), TEXT("BO-TJA"), TEXT("Tarija"), -64.73f, -21.53f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	// Lote 2 - Brasil.
	AddTheaterCountryLabel(TEXT("BRASIL"), -50.0f, -10.0f, FColor(232, 206, 126), 6000.f);
	AddSettlementCluster(TEXT("BR-BRASILIA"), TEXT("Brasilia"), TEXT("BR"), TEXT("BR-DF"), TEXT("Distrito Federal"), -47.88f, -15.79f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("BR-SAO-PAULO"), TEXT("Sao Paulo"), TEXT("BR"), TEXT("BR-SP"), TEXT("Sao Paulo"), -46.63f, -23.55f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BR-RIO"), TEXT("Rio de Janeiro"), TEXT("BR"), TEXT("BR-RJ"), TEXT("Rio de Janeiro"), -43.20f, -22.91f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-BELO-HORIZONTE"), TEXT("Belo Horizonte"), TEXT("BR"), TEXT("BR-MG"), TEXT("Minas Gerais"), -43.94f, -19.92f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BR-SALVADOR"), TEXT("Salvador"), TEXT("BR"), TEXT("BR-BA"), TEXT("Bahia"), -38.51f, -12.97f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-FORTALEZA"), TEXT("Fortaleza"), TEXT("BR"), TEXT("BR-CE"), TEXT("Ceara"), -38.54f, -3.73f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-RECIFE"), TEXT("Recife"), TEXT("BR"), TEXT("BR-PE"), TEXT("Pernambuco"), -34.88f, -8.05f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-MANAUS"), TEXT("Manaus"), TEXT("BR"), TEXT("BR-AM"), TEXT("Amazonas"), -60.02f, -3.10f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BR-BELEM"), TEXT("Belem"), TEXT("BR"), TEXT("BR-PA"), TEXT("Para"), -48.50f, -1.46f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-PORTO-ALEGRE"), TEXT("Porto Alegre"), TEXT("BR"), TEXT("BR-RS"), TEXT("Rio Grande do Sul"), -51.23f, -30.03f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-CURITIBA"), TEXT("Curitiba"), TEXT("BR"), TEXT("BR-PR"), TEXT("Parana"), -49.27f, -25.43f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));

	// Lote 3 - Cono Sur (Argentina, Chile, Uruguay, Paraguay).
	AddTheaterCountryLabel(TEXT("ARGENTINA"), -65.0f, -38.0f, FColor(232, 206, 126), 6000.f);
	AddSettlementCluster(TEXT("AR-BUENOS-AIRES"), TEXT("Buenos Aires"), TEXT("AR"), TEXT("AR-BA"), TEXT("Buenos Aires"), -58.38f, -34.60f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("AR-CORDOBA"), TEXT("Cordoba"), TEXT("AR"), TEXT("AR-CB"), TEXT("Cordoba"), -64.18f, -31.42f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("AR-ROSARIO"), TEXT("Rosario"), TEXT("AR"), TEXT("AR-SF"), TEXT("Santa Fe"), -60.64f, -32.95f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("AR-MENDOZA"), TEXT("Mendoza"), TEXT("AR"), TEXT("AR-MZ"), TEXT("Mendoza"), -68.84f, -32.89f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("AR-MAR-DEL-PLATA"), TEXT("Mar del Plata"), TEXT("AR"), TEXT("AR-BAP"), TEXT("Buenos Aires"), -57.55f, -38.00f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("AR-TUCUMAN"), TEXT("Tucuman"), TEXT("AR"), TEXT("AR-TM"), TEXT("Tucuman"), -65.22f, -26.82f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("AR-COMODORO"), TEXT("Comodoro Rivadavia"), TEXT("AR"), TEXT("AR-CT"), TEXT("Chubut"), -67.48f, -45.86f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("AR-USHUAIA"), TEXT("Ushuaia"), TEXT("AR"), TEXT("AR-TF"), TEXT("Tierra del Fuego"), -68.30f, -54.80f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	AddTheaterCountryLabel(TEXT("CHILE"), -72.0f, -38.5f, FColor(232, 206, 126), 2800.f);
	AddSettlementCluster(TEXT("CL-SANTIAGO"), TEXT("Santiago"), TEXT("CL"), TEXT("CL-RM"), TEXT("Region Metropolitana"), -70.65f, -33.45f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CL-VALPARAISO"), TEXT("Valparaiso"), TEXT("CL"), TEXT("CL-VS"), TEXT("Valparaiso"), -71.62f, -33.05f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CL-CONCEPCION"), TEXT("Concepcion"), TEXT("CL"), TEXT("CL-BI"), TEXT("Biobio"), -73.05f, -36.83f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CL-ANTOFAGASTA"), TEXT("Antofagasta"), TEXT("CL"), TEXT("CL-AN"), TEXT("Antofagasta"), -70.40f, -23.65f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("CL-PUERTO-MONTT"), TEXT("Puerto Montt"), TEXT("CL"), TEXT("CL-LL"), TEXT("Los Lagos"), -72.94f, -41.47f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CL-PUNTA-ARENAS"), TEXT("Punta Arenas"), TEXT("CL"), TEXT("CL-MA"), TEXT("Magallanes"), -70.92f, -53.16f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	AddTheaterCountryLabel(TEXT("URUGUAY"), -56.0f, -33.0f, FColor(232, 206, 126), 2200.f);
	AddSettlementCluster(TEXT("UY-MONTEVIDEO"), TEXT("Montevideo"), TEXT("UY"), TEXT("UY-MO"), TEXT("Montevideo"), -56.16f, -34.90f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("UY-SALTO"), TEXT("Salto"), TEXT("UY"), TEXT("UY-SA"), TEXT("Salto"), -57.96f, -31.38f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("UY-PUNTA-DEL-ESTE"), TEXT("Punta del Este"), TEXT("UY"), TEXT("UY-MA"), TEXT("Maldonado"), -54.95f, -34.97f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	AddTheaterCountryLabel(TEXT("PARAGUAY"), -58.0f, -23.5f, FColor(232, 206, 126), 2800.f);
	AddSettlementCluster(TEXT("PY-ASUNCION"), TEXT("Asuncion"), TEXT("PY"), TEXT("PY-AS"), TEXT("Central"), -57.64f, -25.30f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("PY-CIUDAD-DEL-ESTE"), TEXT("Ciudad del Este"), TEXT("PY"), TEXT("PY-AP"), TEXT("Alto Parana"), -54.61f, -25.51f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("PY-ENCARNACION"), TEXT("Encarnacion"), TEXT("PY"), TEXT("PY-IT"), TEXT("Itapua"), -55.87f, -27.33f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));

	// Escudo guayanes - Guyana y Surinam.
	AddTheaterCountryLabel(TEXT("GUYANA"), -58.9f, 4.8f, FColor(232, 206, 126), 2000.f);
	AddSettlementCluster(TEXT("GY-GEORGETOWN"), TEXT("Georgetown"), TEXT("GY"), TEXT("GY-DE"), TEXT("Demerara-Mahaica"), -58.16f, 6.80f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("GY-LINDEN"), TEXT("Linden"), TEXT("GY"), TEXT("GY-UD"), TEXT("Upper Demerara"), -58.30f, 6.00f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));

	AddTheaterCountryLabel(TEXT("SURINAM"), -55.9f, 4.0f, FColor(232, 206, 126), 2000.f);
	AddSettlementCluster(TEXT("SR-PARAMARIBO"), TEXT("Paramaribo"), TEXT("SR"), TEXT("SR-PM"), TEXT("Paramaribo"), -55.17f, 5.87f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("SR-NICKERIE"), TEXT("Nieuw Nickerie"), TEXT("SR"), TEXT("SR-NI"), TEXT("Nickerie"), -56.97f, 5.95f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Lote 4 - Mexico.
	AddTheaterCountryLabel(TEXT("MEXICO"), -102.0f, 23.5f, FColor(232, 206, 126), 6000.f);
	AddSettlementCluster(TEXT("MX-CIUDAD-DE-MEXICO"), TEXT("Ciudad de Mexico"), TEXT("MX"), TEXT("MX-CMX"), TEXT("Ciudad de Mexico"), -99.13f, 19.43f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("MX-GUADALAJARA"), TEXT("Guadalajara"), TEXT("MX"), TEXT("MX-JAL"), TEXT("Jalisco"), -103.35f, 20.67f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("MX-MONTERREY"), TEXT("Monterrey"), TEXT("MX"), TEXT("MX-NLE"), TEXT("Nuevo Leon"), -100.31f, 25.67f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("MX-TIJUANA"), TEXT("Tijuana"), TEXT("MX"), TEXT("MX-BCN"), TEXT("Baja California"), -117.04f, 32.51f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("MX-CANCUN"), TEXT("Cancun"), TEXT("MX"), TEXT("MX-ROO"), TEXT("Quintana Roo"), -86.85f, 21.16f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("MX-VERACRUZ"), TEXT("Veracruz"), TEXT("MX"), TEXT("MX-VER"), TEXT("Veracruz"), -96.13f, 19.17f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("MX-MERIDA"), TEXT("Merida"), TEXT("MX"), TEXT("MX-YUC"), TEXT("Yucatan"), -89.59f, 20.97f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("MX-CIUDAD-JUAREZ"), TEXT("Ciudad Juarez"), TEXT("MX"), TEXT("MX-CHH"), TEXT("Chihuahua"), -106.49f, 31.74f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("MX-ACAPULCO"), TEXT("Acapulco"), TEXT("MX"), TEXT("MX-GRO"), TEXT("Guerrero"), -99.82f, 16.85f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Lote 5 - Caribe (Antillas Mayores + Bahamas + Trinidad).
	AddTheaterCountryLabel(TEXT("CUBA"), -79.2f, 21.9f, FColor(232, 206, 126), 2600.f);
	AddSettlementCluster(TEXT("CU-HABANA"), TEXT("La Habana"), TEXT("CU"), TEXT("CU-HAB"), TEXT("La Habana"), -82.38f, 23.13f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CU-SANTIAGO"), TEXT("Santiago de Cuba"), TEXT("CU"), TEXT("CU-SCU"), TEXT("Santiago"), -75.82f, 20.02f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));

	AddTheaterCountryLabel(TEXT("BAHAMAS"), -77.0f, 24.7f, FColor(232, 206, 126), 1700.f);
	AddSettlementCluster(TEXT("BS-NASSAU"), TEXT("Nassau"), TEXT("BS"), TEXT("BS-NP"), TEXT("New Providence"), -77.35f, 25.06f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));

	AddTheaterCountryLabel(TEXT("JAMAICA"), -77.3f, 17.6f, FColor(232, 206, 126), 1500.f);
	AddSettlementCluster(TEXT("JM-KINGSTON"), TEXT("Kingston"), TEXT("JM"), TEXT("JM-KIN"), TEXT("Kingston"), -76.79f, 17.97f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));

	AddTheaterCountryLabel(TEXT("HAITI"), -72.6f, 19.4f, FColor(232, 206, 126), 1500.f);
	AddSettlementCluster(TEXT("HT-PORT-AU-PRINCE"), TEXT("Puerto Principe"), TEXT("HT"), TEXT("HT-OU"), TEXT("Ouest"), -72.33f, 18.54f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));

	AddTheaterCountryLabel(TEXT("R. DOMINICANA"), -70.2f, 19.3f, FColor(232, 206, 126), 1500.f);
	AddSettlementCluster(TEXT("DO-SANTO-DOMINGO"), TEXT("Santo Domingo"), TEXT("DO"), TEXT("DO-DN"), TEXT("Distrito Nacional"), -69.93f, 18.47f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));

	AddTheaterCountryLabel(TEXT("PUERTO RICO"), -66.4f, 18.0f, FColor(232, 206, 126), 1400.f);
	AddSettlementCluster(TEXT("PR-SAN-JUAN"), TEXT("San Juan"), TEXT("PR"), TEXT("PR-SJ"), TEXT("San Juan"), -66.11f, 18.47f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));

	AddTheaterCountryLabel(TEXT("TRINIDAD"), -61.3f, 10.4f, FColor(232, 206, 126), 1400.f);
	AddSettlementCluster(TEXT("TT-PORT-OF-SPAIN"), TEXT("Puerto Espana"), TEXT("TT"), TEXT("TT-POS"), TEXT("Puerto Espana"), -61.51f, 10.65f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));

	// Lote 6 - Estados Unidos.
	AddTheaterCountryLabel(TEXT("ESTADOS UNIDOS"), -98.0f, 39.5f, FColor(232, 206, 126), 7000.f);
	AddSettlementCluster(TEXT("US-WASHINGTON"), TEXT("Washington D.C."), TEXT("US"), TEXT("US-DC"), TEXT("District of Columbia"), -77.04f, 38.91f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("US-NEW-YORK"), TEXT("Nueva York"), TEXT("US"), TEXT("US-NY"), TEXT("New York"), -74.01f, 40.71f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("US-LOS-ANGELES"), TEXT("Los Angeles"), TEXT("US"), TEXT("US-CA"), TEXT("California"), -118.24f, 34.05f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-CHICAGO"), TEXT("Chicago"), TEXT("US"), TEXT("US-IL"), TEXT("Illinois"), -87.63f, 41.88f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("US-HOUSTON"), TEXT("Houston"), TEXT("US"), TEXT("US-TX"), TEXT("Texas"), -95.37f, 29.76f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("US-MIAMI"), TEXT("Miami"), TEXT("US"), TEXT("US-FL"), TEXT("Florida"), -80.19f, 25.76f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("US-SAN-FRANCISCO"), TEXT("San Francisco"), TEXT("US"), TEXT("US-NCA"), TEXT("California"), -122.42f, 37.77f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-SEATTLE"), TEXT("Seattle"), TEXT("US"), TEXT("US-WA"), TEXT("Washington"), -122.33f, 47.61f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("US-DALLAS"), TEXT("Dallas"), TEXT("US"), TEXT("US-NTX"), TEXT("Texas"), -96.80f, 32.78f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-DENVER"), TEXT("Denver"), TEXT("US"), TEXT("US-CO"), TEXT("Colorado"), -104.99f, 39.74f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-ATLANTA"), TEXT("Atlanta"), TEXT("US"), TEXT("US-GA"), TEXT("Georgia"), -84.39f, 33.75f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-BOSTON"), TEXT("Boston"), TEXT("US"), TEXT("US-MA"), TEXT("Massachusetts"), -71.06f, 42.36f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Canada.
	AddTheaterCountryLabel(TEXT("CANADA"), -100.0f, 56.0f, FColor(232, 206, 126), 7000.f);
	AddSettlementCluster(TEXT("CA-OTTAWA"), TEXT("Ottawa"), TEXT("CA"), TEXT("CA-ON"), TEXT("Ontario"), -75.70f, 45.42f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CA-TORONTO"), TEXT("Toronto"), TEXT("CA"), TEXT("CA-ONT"), TEXT("Ontario"), -79.38f, 43.65f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("CA-MONTREAL"), TEXT("Montreal"), TEXT("CA"), TEXT("CA-QC"), TEXT("Quebec"), -73.57f, 45.50f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-VANCOUVER"), TEXT("Vancouver"), TEXT("CA"), TEXT("CA-BC"), TEXT("British Columbia"), -123.12f, 49.28f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CA-CALGARY"), TEXT("Calgary"), TEXT("CA"), TEXT("CA-AB"), TEXT("Alberta"), -114.07f, 51.05f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-EDMONTON"), TEXT("Edmonton"), TEXT("CA"), TEXT("CA-ABN"), TEXT("Alberta"), -113.49f, 53.55f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-WINNIPEG"), TEXT("Winnipeg"), TEXT("CA"), TEXT("CA-MB"), TEXT("Manitoba"), -97.14f, 49.90f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-HALIFAX"), TEXT("Halifax"), TEXT("CA"), TEXT("CA-NS"), TEXT("Nova Scotia"), -63.58f, 44.65f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Lote 4 (resto) - Centroamerica a core.
	AddTheaterCountryLabel(TEXT("GUATEMALA"), -90.4f, 15.5f, FColor(232, 206, 126), 1900.f);
	AddSettlementCluster(TEXT("GT-CIUDAD"), TEXT("Ciudad de Guatemala"), TEXT("GT"), TEXT("GT-GU"), TEXT("Guatemala"), -90.51f, 14.63f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddTheaterCountryLabel(TEXT("BELICE"), -88.7f, 17.2f, FColor(232, 206, 126), 1300.f);
	AddSettlementCluster(TEXT("BZ-BELMOPAN"), TEXT("Belmopan"), TEXT("BZ"), TEXT("BZ-CY"), TEXT("Cayo"), -88.77f, 17.25f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddTheaterCountryLabel(TEXT("HONDURAS"), -86.9f, 14.8f, FColor(232, 206, 126), 1900.f);
	AddSettlementCluster(TEXT("HN-TEGUCIGALPA"), TEXT("Tegucigalpa"), TEXT("HN"), TEXT("HN-FM"), TEXT("Francisco Morazan"), -87.21f, 14.10f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("HN-SAN-PEDRO-SULA"), TEXT("San Pedro Sula"), TEXT("HN"), TEXT("HN-CR"), TEXT("Cortes"), -88.03f, 15.50f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddTheaterCountryLabel(TEXT("EL SALVADOR"), -88.9f, 13.6f, FColor(232, 206, 126), 1400.f);
	AddSettlementCluster(TEXT("SV-SAN-SALVADOR"), TEXT("San Salvador"), TEXT("SV"), TEXT("SV-SS"), TEXT("San Salvador"), -89.22f, 13.69f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddTheaterCountryLabel(TEXT("NICARAGUA"), -85.2f, 12.9f, FColor(232, 206, 126), 1900.f);
	AddSettlementCluster(TEXT("NI-MANAGUA"), TEXT("Managua"), TEXT("NI"), TEXT("NI-MN"), TEXT("Managua"), -86.27f, 12.13f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddTheaterCountryLabel(TEXT("COSTA RICA"), -84.2f, 9.9f, FColor(232, 206, 126), 1600.f);
	AddSettlementCluster(TEXT("CR-SAN-JOSE"), TEXT("San Jose"), TEXT("CR"), TEXT("CR-SJ"), TEXT("San Jose"), -84.09f, 9.93f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddTheaterCountryLabel(TEXT("PANAMA"), -80.4f, 8.6f, FColor(232, 206, 126), 1800.f);
	AddSettlementCluster(TEXT("PA-CIUDAD"), TEXT("Ciudad de Panama"), TEXT("PA"), TEXT("PA-PA"), TEXT("Panama"), -79.52f, 8.98f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("PA-COLON"), TEXT("Colon"), TEXT("PA"), TEXT("PA-CO"), TEXT("Colon"), -79.90f, 9.36f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	AddVegetationScatter(-75.5f, -70.0f, 0.8f, 6.6f, 7, 6, true);
	AddVegetationScatter(-68.0f, -62.2f, 3.4f, 7.4f, 7, 5, true);
	AddVegetationScatter(-70.5f, -64.8f, 7.2f, 9.5f, 6, 4, false);
	AddVegetationScatter(-76.8f, -73.5f, 8.5f, 11.8f, 4, 3, false);

	BuildIntercityRoads();
	BuildMovementNodesAndEdges();
	AddMilitaryForceMarkers();
}

void AWLCampaign3DView::AddPathPolyline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale, float ZOffset)
{
	if (LonLatPoints.Num() < 2)
	{
		return;
	}

	for (int32 Index = 0; Index + 1 < LonLatPoints.Num(); ++Index)
	{
		const FVector2D& A = LonLatPoints[Index];
		const FVector2D& B = LonLatPoints[Index + 1];
		AddPolylineSegment(ProjectLonLat(A.X, A.Y) + FVector(0.f, 0.f, ZOffset), ProjectLonLat(B.X, B.Y) + FVector(0.f, 0.f, ZOffset), Color, RadiusScale);
	}
}

void AWLCampaign3DView::AddBiomePatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset)
{
	AddTerrainPatch(LonLatPoints, Color, ZOffset);
}

FVector2D AWLCampaign3DView::NudgeSettlementToLand(float Lon, float Lat) const
{
	if (SettlementLandGeometry.Num() == 0)
	{
		return FVector2D(Lon, Lat);
	}

	auto IsLand = [this](float L, float T) -> bool
	{
		for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
		{
			if (FWLCampaignRegionGeometry::PointInAnyLonLatRing(FVector2D(L, T), Country.Rings))
			{
				return true;
			}
		}
		return false;
	};

	// Muestreamos 8 direcciones; si la ciudad esta junto a la costa, parte de las
	// muestras caen en el mar y el vector resultante apunta tierra adentro.
	const float Probe = 0.45f;
	static const FVector2D Dirs[8] = {
		FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f),
		FVector2D(0.707f, 0.707f), FVector2D(-0.707f, 0.707f), FVector2D(0.707f, -0.707f), FVector2D(-0.707f, -0.707f)
	};
	FVector2D LandDir(0.f, 0.f);
	int32 LandHits = 0;
	for (const FVector2D& Dir : Dirs)
	{
		if (IsLand(Lon + Dir.X * Probe, Lat + Dir.Y * Probe))
		{
			LandDir += Dir;
			++LandHits;
		}
	}

	// Interior (rodeada de tierra), isla diminuta (<=1 muestra) o sin datos: no mover.
	if (LandHits <= 1 || LandHits >= 7 || LandDir.IsNearlyZero())
	{
		return FVector2D(Lon, Lat);
	}

	LandDir.Normalize();
	const float Step = 0.38f;
	return FVector2D(Lon + LandDir.X * Step, Lat + LandDir.Y * Step);
}

void AWLCampaign3DView::AddSettlementCluster(
	const FString& CityId,
	const FString& Name,
	const FString& CountryIso,
	const FString& TerritoryId,
	const FString& TerritoryName,
	float Lon,
	float Lat,
	EWLCampaignSettlementType Type,
	const FLinearColor& AccentColor)
{
	const FVector2D LandLonLat = NudgeSettlementToLand(Lon, Lat);
	Lon = LandLonLat.X;
	Lat = LandLonLat.Y;

	FWLCampaignSettlementSpec Spec;
	Spec.Name = Name;
	Spec.Lon = Lon;
	Spec.Lat = Lat;
	Spec.Type = Type;
	Spec.AccentColor = AccentColor;
	FWLCampaignSettlementBuilder::AddSettlement(
		SettlementMesh,
		VertexColorMaterial,
		Spec,
		[this](float InLon, float InLat)
		{
			return ProjectLonLat(InLon, InLat);
		});

	const bool bCapital = Type == EWLCampaignSettlementType::Capital;
	const bool bSmall = Type == EWLCampaignSettlementType::Frontier;
	const FVector Base = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, bCapital ? 2450.f : 2050.f);
	FWLCampaign3DCityView City;
	City.Id = CityId;
	City.Name = Name;
	City.CountryIso = CountryIso;
	City.TerritoryId = TerritoryId;
	City.TerritoryName = TerritoryName;
	City.CityType = SettlementTypeToPanelText(Type);
	City.StrategicRole = SettlementStrategicRole(Type);
	City.DetailLevel = TEXT("high-detail theater");
	City.WorldLocation = Base;
	City.bCapital = bCapital;
	City.bPort = Type == EWLCampaignSettlementType::Port;
	AddCitySelectionProxy(City, bCapital ? 1.28f : (City.bPort ? 1.16f : 1.0f));

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetWorldLocation(Base + FVector(0.f, 0.f, bCapital ? 2350.f : 1720.f));
	Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(bCapital ? 720.f : (bSmall ? 430.f : 540.f));
	Label->SetText(FText::FromString(Name));
	Label->SetTextRenderColor(bCapital ? FColor(230, 210, 140) : FColor(195, 205, 190));
	SettlementLabels.Add(Label);
}

void AWLCampaign3DView::AddCitySelectionProxy(const FWLCampaign3DCityView& City, float RadiusScale)
{
	if (City.Id.IsEmpty())
	{
		return;
	}

	USphereComponent* Marker = NewObject<USphereComponent>(this);
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetWorldLocation(City.WorldLocation + FVector(0.f, 0.f, City.bCapital ? 2400.f : 1800.f));
	Marker->SetSphereRadius((City.bCapital ? 12500.f : 9800.f) * FMath::Max(0.65f, RadiusScale));
	Marker->SetVisibility(false, true);
	Marker->SetHiddenInGame(false, true);
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DCity"));
	Marker->ComponentTags.Add(FName(*City.Id));
	CitySelectionMarkers.Add(Marker);
	CityViews.Add(City);
}

void AWLCampaign3DView::AddMilitaryForceMarkers()
{
	TArray<FWLCampaign3DForceView> LoadedForces;
	LoadCampaignMilitaryForceDefinitions(LoadedForces);
	LoadedForces.Sort([](const FWLCampaign3DForceView& A, const FWLCampaign3DForceView& B)
	{
		return A.Id < B.Id;
	});

	for (FWLCampaign3DForceView Force : LoadedForces)
	{
		Force.WorldLocation = ProjectLonLat(Force.Lon, Force.Lat) + FVector(0.f, 0.f, Force.bNaval ? 2350.f : 2600.f);
		AddMilitaryForceMarker(Force);
	}
}

void AWLCampaign3DView::AddMilitaryForceMarker(const FWLCampaign3DForceView& Force)
{
	if (Force.Id.IsEmpty() || !ConeMesh)
	{
		return;
	}

	const int32 ForceIndex = ForceViews.Num();
	const FLinearColor BaseColor = CampaignMilitaryForceColor(Force);
	const FVector BaseScale = CampaignMilitaryForceScale(Force);

	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	if (!Marker)
	{
		return;
	}
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetStaticMesh(ConeMesh);
	Marker->SetWorldLocation(Force.WorldLocation);
	Marker->SetWorldRotation(FRotator(0.f, Force.bNaval ? 45.f : 0.f, 0.f));
	Marker->SetWorldScale3D(BaseScale);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DForce"));
	Marker->ComponentTags.Add(FName(*Force.Id));
	if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(BaseColor))
	{
		Marker->SetMaterial(0, Mat);
	}

	USphereComponent* SelectionProxy = NewObject<USphereComponent>(this);
	if (!SelectionProxy)
	{
		Marker->DestroyComponent();
		return;
	}
	SelectionProxy->SetupAttachment(SceneRoot);
	SelectionProxy->RegisterComponent();
	SelectionProxy->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, 1400.f));
	SelectionProxy->SetSphereRadius(Force.bNaval ? 14500.f : (Force.bAir ? 13200.f : 11800.f));
	SelectionProxy->SetVisibility(false, true);
	SelectionProxy->SetHiddenInGame(false, true);
	SelectionProxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionProxy->SetCollisionObjectType(ECC_WorldDynamic);
	SelectionProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionProxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SelectionProxy->ComponentTags.Add(TEXT("WorldLeaderCampaign3DForce"));
	SelectionProxy->ComponentTags.Add(FName(*Force.Id));

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	if (Label)
	{
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetWorldLocation(Force.WorldLocation + FVector(0.f, 0.f, 2550.f));
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(Force.bNaval ? 430.f : 390.f);
		Label->SetText(FText::FromString(Force.NearbyCity.IsEmpty() ? Force.Name : Force.NearbyCity));
		Label->SetTextRenderColor(Force.bAir ? FColor(200, 225, 245) : FColor(230, 214, 148));
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ForceMarkerLabels.Add(Label);
	}

	ForceViews.Add(Force);
	ForceMarkerComponents.Add(Marker);
	ForceSelectionMarkers.Add(SelectionProxy);
	ForceMarkerBaseScales.Add(BaseScale);
	ForceMarkerBaseColors.Add(BaseColor);

	if (ForceMarkerComponents.IsValidIndex(ForceIndex))
	{
		ForceMarkerComponents[ForceIndex]->SetVisibility(!IsHidden(), true);
	}
}

void AWLCampaign3DView::RefreshMilitaryForceMarkerVisuals()
{
	for (int32 Index = 0; Index < ForceMarkerComponents.Num(); ++Index)
	{
		UStaticMeshComponent* Marker = ForceMarkerComponents[Index];
		if (!Marker || !ForceViews.IsValidIndex(Index))
		{
			continue;
		}

		const FWLCampaign3DForceView& Force = ForceViews[Index];
		const bool bSelected = !SelectedForceHighlightId.IsEmpty() && Force.Id.Equals(SelectedForceHighlightId, ESearchCase::IgnoreCase);
		const bool bHovered = !HoveredForceHighlightId.IsEmpty() && Force.Id.Equals(HoveredForceHighlightId, ESearchCase::IgnoreCase);
		const FVector BaseScale = ForceMarkerBaseScales.IsValidIndex(Index) ? ForceMarkerBaseScales[Index] : CampaignMilitaryForceScale(Force);
		const FLinearColor BaseColor = ForceMarkerBaseColors.IsValidIndex(Index) ? ForceMarkerBaseColors[Index] : CampaignMilitaryForceColor(Force);
		Marker->SetWorldScale3D(BaseScale * (bSelected ? 1.42f : (bHovered ? 1.22f : 1.0f)));

		FLinearColor DisplayColor = BaseColor;
		if (bSelected)
		{
			DisplayColor = (BaseColor * 0.58f) + FLinearColor(0.96f, 0.78f, 0.28f, 1.f) * 0.42f;
			DisplayColor.A = 1.f;
		}
		else if (bHovered)
		{
			DisplayColor = (BaseColor * 0.72f) + FLinearColor(0.82f, 0.96f, 1.0f, 1.f) * 0.28f;
			DisplayColor.A = 1.f;
		}

		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(DisplayColor))
		{
			Marker->SetMaterial(0, Mat);
		}
	}
}

void AWLCampaign3DView::BuildMovementNodesAndEdges()
{
	MovementNodes.Reset();
	MovementAdjacency.Reset();

	for (const FWLCampaign3DCityView& City : CityViews)
	{
		if (City.Id.IsEmpty())
		{
			continue;
		}

		FWLCampaign3DMovementNodeView Node;
		Node.Id = City.Id;
		Node.Name = City.Name;
		Node.CountryIso = City.CountryIso;
		Node.ProvinceId = City.TerritoryId;
		Node.ProvinceName = City.TerritoryName;
		Node.NodeType = City.bPort ? TEXT("puerto") : City.CityType;
		Node.WorldLocation = City.WorldLocation;
		Node.bPort = City.bPort
			|| City.Id.Equals(TEXT("VE-MARACAIBO"), ESearchCase::IgnoreCase)
			|| City.Id.Equals(TEXT("VE-PUERTO-LA-CRUZ"), ESearchCase::IgnoreCase);
		const FVector2D LonLat = FVector2D(
			TheaterCenterLonLat.X + City.WorldLocation.Y / GeoScale,
			TheaterCenterLonLat.Y + City.WorldLocation.X / GeoScale);
		Node.Lon = LonLat.X;
		Node.Lat = LonLat.Y;
		MovementNodes.Add(Node);
	}

	AddMovementEdge(TEXT("CO-BOGOTA"), TEXT("CO-MEDELLIN"));
	AddMovementEdge(TEXT("CO-BOGOTA"), TEXT("CO-CALI"));
	AddMovementEdge(TEXT("CO-BOGOTA"), TEXT("CO-BUCARAMANGA"));
	AddMovementEdge(TEXT("CO-BOGOTA"), TEXT("CO-CUCUTA"));
	AddMovementEdge(TEXT("CO-BARRANQUILLA"), TEXT("CO-CARTAGENA"));
	AddMovementEdge(TEXT("CO-BUCARAMANGA"), TEXT("CO-CUCUTA"));
	AddMovementEdge(TEXT("CO-RIOHACHA"), TEXT("VE-MARACAIBO"));
	AddMovementEdge(TEXT("CO-CUCUTA"), TEXT("VE-SAN-CRISTOBAL"));
	AddMovementEdge(TEXT("VE-CARACAS"), TEXT("VE-VALENCIA"));
	AddMovementEdge(TEXT("VE-VALENCIA"), TEXT("VE-MARACAY"));
	AddMovementEdge(TEXT("VE-CARACAS"), TEXT("VE-PUERTO-LA-CRUZ"));
	AddMovementEdge(TEXT("VE-MARACAIBO"), TEXT("VE-SAN-CRISTOBAL"));
	AddMovementEdge(TEXT("VE-CIUDAD-GUAYANA"), TEXT("VE-PUERTO-LA-CRUZ"));
	AddMovementEdge(TEXT("CO-CARTAGENA"), TEXT("VE-MARACAIBO"));
	AddMovementEdge(TEXT("VE-PUERTO-LA-CRUZ"), TEXT("VE-MARACAIBO"));
}

void AWLCampaign3DView::BuildIntercityRoads()
{
	// Agrupa las ciudades por pais (derivando lon/lat de su posicion proyectada) y
	// las conecta con una red minima (arbol de expansion, Prim). CO/VE ya tienen
	// rutas curadas, asi que se omiten. Da caminos a todo el continente sin datos a mano.
	struct FRoadCity
	{
		float Lon = 0.f;
		float Lat = 0.f;
		bool bCapital = false;
	};

	TMap<FString, TArray<FRoadCity>> ByCountry;
	for (const FWLCampaign3DCityView& City : CityViews)
	{
		if (City.CountryIso.IsEmpty()
			|| City.CountryIso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
			|| City.CountryIso.Equals(TEXT("VE"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		FRoadCity RC;
		RC.Lon = TheaterCenterLonLat.X + City.WorldLocation.Y / GeoScale;
		RC.Lat = TheaterCenterLonLat.Y + City.WorldLocation.X / GeoScale;
		RC.bCapital = City.bCapital;
		ByCountry.FindOrAdd(City.CountryIso).Add(RC);
	}

	const float MaxEdgeDeg = 9.0f; // evita carreteras gigantes / cruces de mar
	TArray<FWLCampaignRouteSpec> Network;
	for (const TPair<FString, TArray<FRoadCity>>& Pair : ByCountry)
	{
		const TArray<FRoadCity>& Cities = Pair.Value;
		const int32 N = Cities.Num();
		if (N < 2)
		{
			continue;
		}

		// Prim: arbol de expansion minima sobre la distancia lon/lat.
		TArray<bool> InTree;   InTree.Init(false, N);
		TArray<float> BestDist; BestDist.Init(TNumericLimits<float>::Max(), N);
		TArray<int32> BestFrom; BestFrom.Init(-1, N);
		InTree[0] = true;
		for (int32 J = 1; J < N; ++J)
		{
			BestDist[J] = FVector2D::Distance(FVector2D(Cities[0].Lon, Cities[0].Lat), FVector2D(Cities[J].Lon, Cities[J].Lat));
			BestFrom[J] = 0;
		}

		for (int32 Added = 1; Added < N; ++Added)
		{
			int32 Next = -1;
			float NextDist = TNumericLimits<float>::Max();
			for (int32 J = 0; J < N; ++J)
			{
				if (!InTree[J] && BestDist[J] < NextDist)
				{
					NextDist = BestDist[J];
					Next = J;
				}
			}
			if (Next < 0)
			{
				break;
			}
			InTree[Next] = true;

			const int32 From = BestFrom[Next];
			if (From >= 0 && NextDist <= MaxEdgeDeg)
			{
				const FRoadCity& A = Cities[From];
				const FRoadCity& B = Cities[Next];
				FWLCampaignRouteSpec Spec;
				Spec.Name = Pair.Key + TEXT("-intercity");
				Spec.Type = (A.bCapital || B.bCapital) ? EWLCampaignRouteType::Primary : EWLCampaignRouteType::Secondary;
				Spec.Points = { FVector2D(A.Lon, A.Lat), FVector2D(B.Lon, B.Lat) };
				Spec.Smoothness = 2;
				Spec.bShowJunctions = false;
				Network.Add(Spec);
			}

			for (int32 J = 0; J < N; ++J)
			{
				if (!InTree[J])
				{
					const float D = FVector2D::Distance(FVector2D(Cities[Next].Lon, Cities[Next].Lat), FVector2D(Cities[J].Lon, Cities[J].Lat));
					if (D < BestDist[J])
					{
						BestDist[J] = D;
						BestFrom[J] = Next;
					}
				}
			}
		}
	}

	if (Network.Num() > 0)
	{
		FWLCampaignRouteBuilder::AppendRoutes(
			RoadMesh,
			VertexColorMaterial,
			Network,
			[this](float Lon, float Lat)
			{
				return ProjectLonLat(Lon, Lat);
			});
	}
}

void AWLCampaign3DView::AddMovementEdge(const FString& A, const FString& B)
{
	if (!FindMovementNodeById(A) || !FindMovementNodeById(B))
	{
		return;
	}

	MovementAdjacency.FindOrAdd(A).AddUnique(B);
	MovementAdjacency.FindOrAdd(B).AddUnique(A);
}

const FWLCampaign3DMovementNodeView* AWLCampaign3DView::FindMovementNodeById(const FString& NodeId) const
{
	for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
	{
		if (Node.Id.Equals(NodeId, ESearchCase::IgnoreCase))
		{
			return &Node;
		}
	}
	return nullptr;
}

FString AWLCampaign3DView::FindNearestMovementNodeId(const FWLCampaign3DForceView& Force) const
{
	float BestDistanceSq = TNumericLimits<float>::Max();
	FString BestNodeId;
	for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
	{
		if (Force.bNaval && !Node.bPort)
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared2D(Force.WorldLocation, Node.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestNodeId = Node.Id;
		}
	}
	return BestNodeId;
}

void AWLCampaign3DView::GetValidMovementDestinations(
	const FWLCampaign3DForceView& Force,
	TArray<FWLCampaign3DMovementNodeView>& OutDestinations) const
{
	OutDestinations.Reset();
	if (!Force.bMovable || Force.bAir)
	{
		return;
	}

	const FString OriginNodeId = Force.MovementNodeId.IsEmpty() ? FindNearestMovementNodeId(Force) : Force.MovementNodeId;
	const TArray<FString>* Neighbors = MovementAdjacency.Find(OriginNodeId);
	if (!Neighbors)
	{
		return;
	}

	for (const FString& NeighborId : *Neighbors)
	{
		const FWLCampaign3DMovementNodeView* Node = FindMovementNodeById(NeighborId);
		if (!Node)
		{
			continue;
		}
		if (Force.bNaval && !Node->bPort)
		{
			continue;
		}
		OutDestinations.Add(*Node);
	}
}

void AWLCampaign3DView::RebuildMovementDestinationMarkers(const TArray<FWLCampaign3DMovementNodeView>& Destinations)
{
	DestroyMovementDestinationMarkers();
	ActiveMovementDestinations = Destinations;

	for (const FWLCampaign3DMovementNodeView& Node : ActiveMovementDestinations)
	{
		const bool bPreview = !PreviewMovementDestinationNodeId.IsEmpty()
			&& Node.Id.Equals(PreviewMovementDestinationNodeId, ESearchCase::IgnoreCase);
		const FVector MarkerLocation = Node.WorldLocation + FVector(0.f, 0.f, bPreview ? 3300.f : 2850.f);

		UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
		if (!Marker)
		{
			continue;
		}
		Marker->SetupAttachment(SceneRoot);
		Marker->RegisterComponent();
		Marker->SetStaticMesh(ConeMesh ? ConeMesh : CityMesh);
		Marker->SetWorldLocation(MarkerLocation);
		Marker->SetWorldScale3D(bPreview ? FVector(9.5f, 9.5f, 8.0f) : FVector(7.0f, 7.0f, 5.8f));
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(bPreview
			? FLinearColor(0.96f, 0.78f, 0.30f, 1.f)
			: FLinearColor(0.28f, 0.72f, 0.64f, 1.f)))
		{
			Marker->SetMaterial(0, Mat);
		}
		MovementDestinationComponents.Add(Marker);

		USphereComponent* SelectionProxy = NewObject<USphereComponent>(this);
		if (SelectionProxy)
		{
			SelectionProxy->SetupAttachment(SceneRoot);
			SelectionProxy->RegisterComponent();
			SelectionProxy->SetWorldLocation(MarkerLocation + FVector(0.f, 0.f, 620.f));
			SelectionProxy->SetSphereRadius(12800.f);
			SelectionProxy->SetVisibility(false, true);
			SelectionProxy->SetHiddenInGame(false, true);
			SelectionProxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			SelectionProxy->SetCollisionObjectType(ECC_WorldDynamic);
			SelectionProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
			SelectionProxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			SelectionProxy->ComponentTags.Add(TEXT("WorldLeaderCampaign3DMovementDestination"));
			SelectionProxy->ComponentTags.Add(FName(*Node.Id));
			MovementDestinationSelectionMarkers.Add(SelectionProxy);
		}

		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		if (Label)
		{
			Label->SetupAttachment(SceneRoot);
			Label->RegisterComponent();
			Label->SetWorldLocation(MarkerLocation + FVector(0.f, 0.f, 1480.f));
			Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
			Label->SetHorizontalAlignment(EHTA_Center);
			Label->SetWorldSize(bPreview ? 520.f : 430.f);
			Label->SetText(FText::FromString(Node.Name));
			Label->SetTextRenderColor(bPreview ? FColor(238, 214, 132) : FColor(170, 220, 210));
			Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MovementDestinationLabels.Add(Label);
		}
	}
}

void AWLCampaign3DView::RebuildMovementRoutePreview(const TArray<FWLCampaign3DMovementNodeView>& RouteNodes)
{
	if (!MovementRoutePreviewMesh)
	{
		return;
	}

	MovementRoutePreviewMesh->ClearAllMeshSections();
	const FLinearColor RouteColor(0.26f, 0.70f, 0.64f, 0.96f);
	for (int32 Index = 0; Index + 1 < RouteNodes.Num(); ++Index)
	{
		const FVector Start = RouteNodes[Index].WorldLocation + FVector(0.f, 0.f, 2250.f);
		const FVector End = RouteNodes[Index + 1].WorldLocation + FVector(0.f, 0.f, 2250.f);
		AddMovementRoutePreviewSegment(Start, End, RouteColor, Index);
	}
	MovementRoutePreviewMesh->SetVisibility(!IsHidden(), true);
	MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWLCampaign3DView::AddMovementRoutePreviewSegment(
	const FVector& Start,
	const FVector& End,
	const FLinearColor& Color,
	int32 SectionIndex)
{
	if (!MovementRoutePreviewMesh)
	{
		return;
	}

	const FVector Direction = End - Start;
	const FVector FlatDirection(Direction.X, Direction.Y, 0.f);
	if (FlatDirection.SizeSquared() < 1.f)
	{
		return;
	}

	const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal()) * 760.f;
	TArray<FVector> Verts = {
		Start - Side,
		Start + Side,
		End + Side,
		End - Side
	};
	TArray<int32> Tris = { 0, 1, 2, 0, 2, 3 };
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs = { FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(1.f, 0.f) };
	TArray<FColor> Colors;
	Colors.Init(FWLCampaignVisualStyle::ToVertexFColor(Color), Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	MovementRoutePreviewMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (UMaterialInstanceDynamic* RouteMaterial = MakeColorMaterial(Color))
	{
		MovementRoutePreviewMesh->SetMaterial(SectionIndex, RouteMaterial);
	}
	else if (VertexColorMaterial)
	{
		MovementRoutePreviewMesh->SetMaterial(SectionIndex, VertexColorMaterial);
	}
}

void AWLCampaign3DView::DestroyMovementDestinationMarkers()
{
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	MovementDestinationComponents.Reset();

	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	MovementDestinationSelectionMarkers.Reset();

	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	MovementDestinationLabels.Reset();
}

FVector AWLCampaign3DView::GetForceMarkerLocationForNode(
	const FWLCampaign3DForceView& Force,
	const FWLCampaign3DMovementNodeView& Node) const
{
	return Node.WorldLocation + FVector(0.f, 0.f, Force.bNaval ? 450.f : 550.f);
}

void AWLCampaign3DView::RebuildPointSelectionHighlight(const FVector& Location, float Radius, const FLinearColor& Color)
{
	if (!SelectionHighlightMesh)
	{
		return;
	}

	SelectionHighlightMesh->ClearAllMeshSections();
	const int32 Segments = 36;
	const float OuterRadius = FMath::Max(1200.f, Radius);
	const float InnerRadius = OuterRadius * 0.74f;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Verts.Reserve(Segments * 2);
	Normals.Reserve(Segments * 2);
	UVs.Reserve(Segments * 2);
	Colors.Reserve(Segments * 2);

	const FColor VertexColor = Color.ToFColor(false);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float Angle = (static_cast<float>(Index) / static_cast<float>(Segments)) * 2.f * PI;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		Verts.Add(Location + Direction * OuterRadius);
		Verts.Add(Location + Direction * InnerRadius);
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(1.f, 0.f));
		UVs.Add(FVector2D(0.f, 0.f));
		Colors.Add(VertexColor);
		Colors.Add(VertexColor);
	}
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const int32 Next = (Index + 1) % Segments;
		const int32 A = Index * 2;
		const int32 B = A + 1;
		const int32 C = Next * 2;
		const int32 D = C + 1;
		Tris.Append({ A, C, B, B, C, D });
	}

	SelectionHighlightMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (VertexColorMaterial)
	{
		SelectionHighlightMesh->SetMaterial(0, VertexColorMaterial);
	}
	SelectionHighlightMesh->SetVisibility(!IsHidden(), true);
	SelectionHighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWLCampaign3DView::AddVegetationScatter(float MinLon, float MaxLon, float MinLat, float MaxLat, int32 Columns, int32 Rows, bool bDenseJungle)
{
	for (int32 Col = 0; Col < Columns; ++Col)
	{
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const float U = (static_cast<float>(Col) + 0.35f + 0.18f * FMath::Sin(Row * 1.7f)) / static_cast<float>(Columns);
			const float V = (static_cast<float>(Row) + 0.42f + 0.16f * FMath::Cos(Col * 1.3f)) / static_cast<float>(Rows);
			if (((Col * 11 + Row * 17) % (bDenseJungle ? 7 : 5)) == 0)
			{
				continue;
			}
			const float Lon = FMath::Lerp(MinLon, MaxLon, U);
			const float Lat = FMath::Lerp(MinLat, MaxLat, V);
			const FVector Location = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, bDenseJungle ? 760.f : 560.f);
			const float ScaleBase = (bDenseJungle ? 6.6f : 4.7f) * (0.82f + 0.26f * FMath::Frac(FMath::Sin((Col + 1) * 12.9898f + Row * 78.233f) * 43758.5453f));
			AddInstance(bDenseJungle ? TreeInstances : BrushInstances, Location, FRotator(0.f, 37.f * (Col + Row), 0.f), FVector(ScaleBase, ScaleBase, bDenseJungle ? 9.5f : 4.8f));
		}
	}
}

void AWLCampaign3DView::AddInstance(UInstancedStaticMeshComponent* Component, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (!Component || !Component->GetStaticMesh())
	{
		return;
	}

	Component->AddInstance(FTransform(Rotation, Location, Scale));
}

void AWLCampaign3DView::AddOverviewLabel(
	const FString& Text,
	float Lon,
	float Lat,
	float ZOffset,
	float WorldSize,
	const FColor& Color,
	bool bShowInGlobal,
	bool bShowInRegion)
{
	if (!SceneRoot)
	{
		return;
	}

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	if (!Label)
	{
		return;
	}

	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetWorldLocation(ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, ZOffset));
	Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(WorldSize);
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(Color);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverviewLabels.Add(Label);

	uint8 VisibilityMask = 0;
	if (bShowInGlobal)
	{
		VisibilityMask |= 1;
	}
	if (bShowInRegion)
	{
		VisibilityMask |= 2;
	}
	OverviewLabelVisibilityMasks.Add(VisibilityMask);
}

void AWLCampaign3DView::UpdateOverviewLabelVisibility(bool bStrategicVisible)
{
	for (int32 Index = 0; Index < OverviewLabels.Num(); ++Index)
	{
		UTextRenderComponent* Label = OverviewLabels[Index];
		if (!Label)
		{
			continue;
		}

		const uint8 VisibilityMask = OverviewLabelVisibilityMasks.IsValidIndex(Index)
			? OverviewLabelVisibilityMasks[Index]
			: 3;
		const bool bAllowedForZoom =
			(CurrentZoomLOD == EWLCampaign3DZoomLOD::Global && (VisibilityMask & 1) != 0)
			|| (CurrentZoomLOD == EWLCampaign3DZoomLOD::Region && (VisibilityMask & 2) != 0);
		Label->SetVisibility(bStrategicVisible && bAllowedForZoom, true);
	}
}

void AWLCampaign3DView::ConfigureInstancedComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, const FLinearColor& Color)
{
	if (!Component || !Mesh)
	{
		return;
	}

	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetMobility(EComponentMobility::Movable);
	if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(Color))
	{
		Component->SetMaterial(0, Mat);
	}
}

void AWLCampaign3DView::AddProvinceMarkers()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	TArray<FWLProvinceData> Provinces = Registry->GetAllProvinces();
	Provinces.Sort([](const FWLProvinceData& A, const FWLProvinceData& B)
	{
		return A.Id < B.Id;
	});

	for (const FWLProvinceData& Province : Provinces)
	{
		if (FWLCampaignRegionGeometry::IsRegionalProvince(Province))
		{
			AddProvinceMarker(Province);
		}
	}
}

void AWLCampaign3DView::AddProvinceMarker(const FWLProvinceData& Province)
{
	const FVector Location = ProjectLonLat(Province.MapLon, Province.MapLat) + FVector(0.f, 0.f, Province.bIsCapital ? 1050.f : 780.f);
	USphereComponent* Marker = NewObject<USphereComponent>(this);
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetWorldLocation(Location);
	Marker->SetSphereRadius(Province.bIsCapital ? 6200.f : 4800.f);
	Marker->SetVisibility(false, true);
	Marker->SetHiddenInGame(false, true);
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DProvince"));
	Marker->ComponentTags.Add(FName(*Province.Id));

	FWLCampaign3DProvinceView View;
	View.Id = Province.Id;
	View.Name = Province.Name;
	View.CountryIso = Province.CountryIso;
	View.Region = Province.Region;
	View.WorldLocation = Location;
	ProvinceMarkers.Add(Marker);
	ProvinceViews.Add(View);

	if (Province.bIsCapital)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetWorldLocation(Location + FVector(0.f, 0.f, 1500.f));
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(920.f);
		Label->SetText(FText::FromString(Province.Capital));
		Label->SetTextRenderColor(FColor(235, 210, 120));
		SettlementLabels.Add(Label);
	}
}

void AWLCampaign3DView::AddRoutes()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry || !RouteMesh)
	{
		return;
	}

	TSet<FString> Processed;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (!FWLCampaignRegionGeometry::IsRegionalProvince(Province))
		{
			continue;
		}

		for (const FString& NeighborId : Province.Neighbors)
		{
			FWLProvinceData Neighbor;
			if (!Registry->GetProvince(NeighborId, Neighbor) || !FWLCampaignRegionGeometry::IsRegionalProvince(Neighbor))
			{
				continue;
			}
			const FString Key = RouteKey(Province.Id, Neighbor.Id);
			if (Processed.Contains(Key))
			{
				continue;
			}
			Processed.Add(Key);
			AddRouteSegment(Province, Neighbor);
		}
	}
}

void AWLCampaign3DView::AddRouteSegment(const FWLProvinceData& A, const FWLProvinceData& B)
{
	const FVector Start = ProjectLonLat(A.MapLon, A.MapLat) + FVector(0.f, 0.f, 520.f);
	const FVector End = ProjectLonLat(B.MapLon, B.MapLat) + FVector(0.f, 0.f, 520.f);
	AddPolylineSegment(Start, End, FLinearColor(0.58f, 0.46f, 0.20f, 0.92f), 0.55f);
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
		FRotator(-52.f, -38.f, 0.f));
	ViewSkyLight = World->SpawnActor<ASkyLight>(
		ASkyLight::StaticClass(),
		FVector(0.f, 0.f, 26000.f),
		FRotator::ZeroRotator);

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

	const FVector CameraLocation(Center.X, Center.Y, 320000.f);
	DefaultCameraLocation = CameraLocation;
	DefaultCameraRotation = FRotator(-90.f, 0.f, 0.f);
	ViewCamera = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		CameraLocation,
		DefaultCameraRotation);
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

void AWLCampaign3DView::SetDetailedLayerVisible(bool bVisible)
{
	if (TerrainMesh)
	{
		TerrainMesh->SetVisibility(bVisible, true);
		TerrainMesh->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->SetVisibility(bVisible, true);
	}
	if (RoadMesh)
	{
		RoadMesh->SetVisibility(bVisible, true);
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bVisible, true);
	}
	for (UPrimitiveComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bVisible && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bVisible && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bVisible, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bVisible, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bVisible, true);
		}
	}
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bVisible, true);
		}
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
			Instanced->SetVisibility(bVisible, true);
		}
	}
}

void AWLCampaign3DView::SetStrategicLayerVisible(bool bVisible)
{
	if (OverviewMesh)
	{
		OverviewMesh->SetVisibility(bVisible, true);
		OverviewMesh->SetMeshSectionVisible(0, bVisible);
		OverviewMesh->SetMeshSectionVisible(1, bVisible);
		OverviewMesh->SetMeshSectionVisible(2, bVisible && CurrentZoomLOD == EWLCampaign3DZoomLOD::Region);
		OverviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	UpdateOverviewLabelVisibility(bVisible);
}

void AWLCampaign3DView::ApplyZoomLOD(float CameraHeight)
{
	const bool bActive = !IsHidden();
	if (CameraHeight >= 1200000.f)
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Global;
	}
	else if (CameraHeight >= 620000.f)
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Region;
	}
	else if (CameraHeight <= 76000.f)
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Close;
	}
	else
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Theater;
	}

	const bool bStrategic = bActive
		&& (CurrentZoomLOD == EWLCampaign3DZoomLOD::Region || CurrentZoomLOD == EWLCampaign3DZoomLOD::Global);
	const bool bTheaterTerrain = bActive && CurrentZoomLOD != EWLCampaign3DZoomLOD::Global;
	const bool bFineDetail = bActive
		&& (CurrentZoomLOD == EWLCampaign3DZoomLOD::Close || CurrentZoomLOD == EWLCampaign3DZoomLOD::Theater);

	SetStrategicLayerVisible(bStrategic);
	SetDetailedLayerVisible(bTheaterTerrain);

	if (RoadMesh)
	{
		RoadMesh->SetVisibility(bActive && CurrentZoomLOD != EWLCampaign3DZoomLOD::Global, true);
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bFineDetail, true);
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bFineDetail, true);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bFineDetail, true);
		}
	}
	// Etiquetas de PAIS: visibles al alejar (Region) y al acercar (Teatro/Cercano).
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail || CurrentZoomLOD == EWLCampaign3DZoomLOD::Region, true);
		}
	}
	// Etiquetas de CIUDAD: solo al acercar (Teatro/Cercano), nunca al alejar.
	for (UTextRenderComponent* Label : SettlementLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail, true);
		}
	}
	if (TreeInstances)
	{
		TreeInstances->SetVisibility(bFineDetail, true);
	}
	if (BrushInstances)
	{
		BrushInstances->SetVisibility(bFineDetail, true);
	}
	if (ArmyMarkerInstances)
	{
		ArmyMarkerInstances->SetVisibility(bFineDetail, true);
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bFineDetail, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bFineDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail, true);
		}
	}
	if (PortInstances)
	{
		PortInstances->SetVisibility(bFineDetail || CurrentZoomLOD == EWLCampaign3DZoomLOD::Region, true);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bFineDetail && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bFineDetail, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bFineDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail, true);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bFineDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bFineDetail && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
	}
	if (TerritoryLayer)
	{
		TerritoryLayer->ApplyVisibility(CameraHeight);
	}
}

void AWLCampaign3DView::SetComponentSetActive(bool bActive)
{
	if (SeaMesh)
	{
		SeaMesh->SetVisibility(bActive, true);
		SeaMesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (SeaDetailMesh)
	{
		SeaDetailMesh->SetVisibility(bActive && CurrentZoomLOD != EWLCampaign3DZoomLOD::Global, true);
		SeaDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (OverviewMesh)
	{
		OverviewMesh->SetVisibility(bActive, true);
		OverviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (TerrainMesh)
	{
		TerrainMesh->SetVisibility(bActive, true);
		TerrainMesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->SetVisibility(bActive, true);
		BoundaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (RoadMesh)
	{
		RoadMesh->SetVisibility(bActive, true);
		RoadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bActive, true);
		SettlementMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UPrimitiveComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bActive && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
		SelectionHighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bActive && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bActive, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bActive, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bActive, true);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
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
			Instanced->SetVisibility(bActive, true);
			Instanced->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bActive, true);
		}
	}
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UTextRenderComponent* Label : SettlementLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UTextRenderComponent* Label : OverviewLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}

	if (bActive)
	{
		const float CameraHeight = ViewCamera ? ViewCamera->GetActorLocation().Z : DefaultCameraLocation.Z;
		ApplyZoomLOD(CameraHeight);
	}
	else
	{
		SetDetailedLayerVisible(false);
		SetStrategicLayerVisible(false);
	}
}
