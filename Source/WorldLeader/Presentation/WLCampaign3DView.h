// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "Presentation/WLCampaignTerritoryLayerComponent.h"
#include "GameFramework/Actor.h"
#include "WLCampaign3DView.generated.h"

class ACameraActor;
class ADirectionalLight;
class AExponentialHeightFog;
class ASkyLight;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWLCampaignTerritoryLayerComponent;
class UWLDataRegistry;
enum class EWLCampaignSettlementType : uint8;

UENUM(BlueprintType)
enum class EWLCampaign3DZoomLOD : uint8
{
	Close,
	Theater,
	Region,
	Global
};

USTRUCT(BlueprintType)
struct FWLCampaign3DProvinceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Region;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FWLCampaign3DCityView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString TerritoryId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString TerritoryName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CityType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString StrategicRole;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString DetailLevel;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bCapital = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bPort = false;
};

USTRUCT(BlueprintType)
struct FWLCampaign3DForceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ForceType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString MarkerCategory;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString LocationName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ProvinceId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ProvinceName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString NearbyCity;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Mobility;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString OperationalState;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Supply;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Morale;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Posture;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString StrategicRole;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString DetailLevel;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") TArray<FString> DisabledActions;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lon = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lat = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") int32 EstimatedStrength = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bAir = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bNaval = false;
};

/**
 * Presentation layer principal de campania. Es una escena 3D regional
 * Colombia/Venezuela que lee datos compartidos; no contiene reglas de juego.
 */
UCLASS()
class WORLDLEADER_API AWLCampaign3DView : public AActor
{
	GENERATED_BODY()

public:
	AWLCampaign3DView();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign3D")
	void BuildView(const FString& PlayerNationIso);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign3D")
	void SetPresentationActive(bool bActive, bool bSetCamera = true);

	bool TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DProvinceView& OutProvince) const;
	bool TryGetTerritoryForComponent(const UPrimitiveComponent* Component, FWLCampaignTerritoryRegionView& OutTerritory) const;
	bool TryGetTerritoryAtWorldLocation(const FVector& WorldLocation, FWLCampaignTerritoryRegionView& OutTerritory) const;
	bool TryGetCityForComponent(const UPrimitiveComponent* Component, FWLCampaign3DCityView& OutCity) const;
	bool TryGetCityNearWorldLocation(const FVector& WorldLocation, float MaxDistance, FWLCampaign3DCityView& OutCity) const;
	bool TryGetForceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DForceView& OutForce) const;
	bool TryGetForceNearWorldLocation(const FVector& WorldLocation, float MaxDistance, FWLCampaign3DForceView& OutForce) const;
	void SetSelectedProvinceHighlight(const FString& ProvinceId);
	void SetSelectedCityHighlight(const FString& CityId);
	void SetSelectedForceHighlight(const FString& ForceId);
	void SetHoveredForceHighlight(const FString& ForceId);
	void ClearSelectionHighlight();
	FBox2D GetViewBounds2D() const;
	FBox2D GetCameraBounds2D(float CameraHeight) const;
	ACameraActor* GetViewCamera() const { return ViewCamera; }
	FVector GetDefaultCameraLocation() const { return DefaultCameraLocation; }
	FRotator GetDefaultCameraRotation() const { return DefaultCameraRotation; }
	FVector GetTheaterFocusPoint() const;
	EWLCampaign3DZoomLOD GetCurrentZoomLOD() const { return CurrentZoomLOD; }
	FString GetCurrentZoomLODLabel() const;
	void ApplyZoomLOD(float CameraHeight);

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") FVector2D TheaterCenterLonLat = FVector2D(-68.6f, 7.2f);
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float GeoScale = 9000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMinLon = -82.6f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMaxLon = -57.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMinLat = -5.2f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMaxLat = 17.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") bool bAutoBuildOnBeginPlay = false;

private:
	UPROPERTY() USceneComponent* SceneRoot = nullptr;
	UPROPERTY() UProceduralMeshComponent* TerrainMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SeaMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SeaDetailMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* OverviewMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* BoundaryMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* RoadMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SettlementMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SelectionHighlightMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;
	UPROPERTY() UMaterialInterface* VertexColorMaterial = nullptr;
	UPROPERTY() UStaticMesh* CityMesh = nullptr;
	UPROPERTY() UStaticMesh* RouteMesh = nullptr;
	UPROPERTY() UStaticMesh* CubeMesh = nullptr;
	UPROPERTY() UStaticMesh* ConeMesh = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* SettlementBlockInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* SettlementTowerInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* TreeInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* BrushInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* PortInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* ArmyMarkerInstances = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> ProvinceMarkers;
	UPROPERTY() TArray<UPrimitiveComponent*> CitySelectionMarkers;
	UPROPERTY() TArray<UStaticMeshComponent*> ForceMarkerComponents;
	UPROPERTY() TArray<UPrimitiveComponent*> ForceSelectionMarkers;
	UPROPERTY() TArray<UTextRenderComponent*> ForceMarkerLabels;
	UPROPERTY() TArray<UStaticMeshComponent*> VisualComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> RouteSegments;
	UPROPERTY() TArray<UTextRenderComponent*> Labels;
	UPROPERTY() TArray<UTextRenderComponent*> OverviewLabels;
	TArray<uint8> OverviewLabelVisibilityMasks;
	UPROPERTY() TArray<FWLCampaign3DProvinceView> ProvinceViews;
	UPROPERTY() TArray<FWLCampaign3DCityView> CityViews;
	UPROPERTY() TArray<FWLCampaign3DForceView> ForceViews;
	UPROPERTY() UWLCampaignTerritoryLayerComponent* TerritoryLayer = nullptr;
	UPROPERTY() ADirectionalLight* ViewDirectionalLight = nullptr;
	UPROPERTY() ASkyLight* ViewSkyLight = nullptr;
	UPROPERTY() AExponentialHeightFog* ViewFog = nullptr;
	UPROPERTY() ACameraActor* ViewCamera = nullptr;

	FString ActivePlayerNationIso;
	FBox2D Bounds;
	FBox2D OverviewBounds;
	FVector DefaultCameraLocation = FVector::ZeroVector;
	FRotator DefaultCameraRotation = FRotator::ZeroRotator;
	EWLCampaign3DZoomLOD CurrentZoomLOD = EWLCampaign3DZoomLOD::Theater;
	FString SelectedProvinceHighlightId;
	FString SelectedCityHighlightId;
	FString SelectedForceHighlightId;
	FString HoveredForceHighlightId;
	TArray<FVector> ForceMarkerBaseScales;
	TArray<FLinearColor> ForceMarkerBaseColors;
	bool bHasBuiltView = false;
	float WaterAnimationTime = 0.f;

	UWLDataRegistry* GetRegistry() const;
	FVector ProjectLonLat(float Lon, float Lat) const;
	float SampleTerrainHeight(float Lon, float Lat) const;
	FLinearColor TerrainColor(EWLTerrainType Terrain, const FString& CountryIso) const;
	UMaterialInstanceDynamic* MakeColorMaterial(const FLinearColor& Color);
	void BuildSea();
	void BuildOverviewLayer();
	void BuildTerrain();
	void BuildCampaignVisualLayer();
	void AddCountryTerrain(const TArray<TArray<FVector2D>>& Rings, const FLinearColor& Color, bool bCoreCountry);
	void AddTerrainPatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset);
	void AddPolylineSegment(const FVector& Start, const FVector& End, const FLinearColor& Color, float RadiusScale);
	void AddCoastline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale);
	void AddBoundaryRibbon(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float WidthWorld, float ZOffset);
	void AddPathPolyline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale, float ZOffset);
	void AddBiomePatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset);
	void AddSettlementCluster(
		const FString& CityId,
		const FString& Name,
		const FString& CountryIso,
		const FString& TerritoryId,
		const FString& TerritoryName,
		float Lon,
		float Lat,
		EWLCampaignSettlementType Type,
		const FLinearColor& AccentColor);
	void AddCitySelectionProxy(const FWLCampaign3DCityView& City, float RadiusScale);
	void AddMilitaryForceMarkers();
	void AddMilitaryForceMarker(const FWLCampaign3DForceView& Force);
	void RefreshMilitaryForceMarkerVisuals();
	void RebuildPointSelectionHighlight(const FVector& Location, float Radius, const FLinearColor& Color);
	void AddVegetationScatter(float MinLon, float MaxLon, float MinLat, float MaxLat, int32 Columns, int32 Rows, bool bDenseJungle);
	void AddInstance(UInstancedStaticMeshComponent* Component, const FVector& Location, const FRotator& Rotation, const FVector& Scale);
	void AddOverviewLabel(
		const FString& Text,
		float Lon,
		float Lat,
		float ZOffset,
		float WorldSize,
		const FColor& Color,
		bool bShowInGlobal,
		bool bShowInRegion);
	void UpdateOverviewLabelVisibility(bool bStrategicVisible);
	void ConfigureInstancedComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, const FLinearColor& Color);
	void AddProvinceMarkers();
	void AddProvinceMarker(const FWLProvinceData& Province);
	void AddRoutes();
	void AddRouteSegment(const FWLProvinceData& A, const FWLProvinceData& B);
	void SetupLightingAndCamera();
	void DestroyPresentationActors();
	void SetComponentSetActive(bool bActive);
	void SetDetailedLayerVisible(bool bVisible);
	void SetStrategicLayerVisible(bool bVisible);
};
