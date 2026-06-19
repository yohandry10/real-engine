// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
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
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWLDataRegistry;

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

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign3D")
	void BuildView(const FString& PlayerNationIso);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign3D")
	void SetPresentationActive(bool bActive, bool bSetCamera = true);

	bool TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DProvinceView& OutProvince) const;
	FBox2D GetViewBounds2D() const;
	ACameraActor* GetViewCamera() const { return ViewCamera; }

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") FVector2D TheaterCenterLonLat = FVector2D(-68.6f, 7.2f);
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float GeoScale = 9000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMinLon = -77.5f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMaxLon = -59.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMinLat = 0.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMaxLat = 13.2f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") bool bAutoBuildOnBeginPlay = false;

private:
	UPROPERTY() USceneComponent* SceneRoot = nullptr;
	UPROPERTY() UProceduralMeshComponent* TerrainMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SeaMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* BoundaryMesh = nullptr;
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
	UPROPERTY() TArray<UStaticMeshComponent*> VisualComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> RouteSegments;
	UPROPERTY() TArray<UTextRenderComponent*> Labels;
	UPROPERTY() TArray<FWLCampaign3DProvinceView> ProvinceViews;
	UPROPERTY() ADirectionalLight* ViewDirectionalLight = nullptr;
	UPROPERTY() ASkyLight* ViewSkyLight = nullptr;
	UPROPERTY() AExponentialHeightFog* ViewFog = nullptr;
	UPROPERTY() ACameraActor* ViewCamera = nullptr;

	FString ActivePlayerNationIso;
	FBox2D Bounds;
	bool bHasBuiltView = false;

	UWLDataRegistry* GetRegistry() const;
	FVector ProjectLonLat(float Lon, float Lat) const;
	float SampleTerrainHeight(float Lon, float Lat) const;
	FLinearColor TerrainColor(EWLTerrainType Terrain, const FString& CountryIso) const;
	UMaterialInstanceDynamic* MakeColorMaterial(const FLinearColor& Color);
	void BuildSea();
	void BuildTerrain();
	void BuildCampaignVisualLayer();
	void AddCountryTerrain(const TArray<TArray<FVector2D>>& Rings, const FLinearColor& Color, bool bCoreCountry);
	void AddTerrainPatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset);
	void AddPolylineSegment(const FVector& Start, const FVector& End, const FLinearColor& Color, float RadiusScale);
	void AddCoastline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale);
	void AddBoundaryRibbon(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float WidthWorld, float ZOffset);
	void AddPathPolyline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale, float ZOffset);
	void AddBiomePatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset);
	void AddSettlementCluster(const FString& Name, float Lon, float Lat, bool bCapital, bool bPort, const FLinearColor& AccentColor);
	void AddVegetationScatter(float MinLon, float MaxLon, float MinLat, float MaxLat, int32 Columns, int32 Rows, bool bDenseJungle);
	void AddInstance(UInstancedStaticMeshComponent* Component, const FVector& Location, const FRotator& Rotation, const FVector& Scale);
	void ConfigureInstancedComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, const FLinearColor& Color);
	void AddProvinceMarkers();
	void AddProvinceMarker(const FWLProvinceData& Province);
	void AddRoutes();
	void AddRouteSegment(const FWLProvinceData& A, const FWLProvinceData& B);
	void SetupLightingAndCamera();
	void DestroyPresentationActors();
	void SetComponentSetActive(bool bActive);
};
