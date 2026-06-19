// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WLCampaignTerritoryLayerComponent.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UTextRenderComponent;

USTRUCT(BlueprintType)
struct FWLCampaignTerritoryRegionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ProvinceType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString OwnerCountry;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ControllerCountry;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString MainCity;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString TerrainType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString BiomeType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ResourcesText;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString DetailLevel;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") int32 StrategicValue = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") int32 PopulationPlaceholder = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") int32 InfrastructurePlaceholder = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") int32 PublicOrderPlaceholder = 70;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bIsCountry = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bIsOccupied = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bIsDisputed = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bIsCapitalRegion = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bIsBorderRegion = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bIsCoastal = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bIsResourceRich = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bSelectable = true;
};

/**
 * Presentation-only territorial layer for Campaign 3D.
 * It owns borders, labels, hover/select highlights and collision proxies.
 */
UCLASS(ClassGroup = (WorldLeader))
class WORLDLEADER_API UWLCampaignTerritoryLayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWLCampaignTerritoryLayerComponent();

	void BuildLayer(
		USceneComponent* Parent,
		UMaterialInterface* InVertexColorMaterial,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);

	void ClearLayer();
	void SetPresentationActive(bool bActive);
	void ApplyVisibility(float CameraHeight);
	void SetLayerToggles(bool bBorders, bool bProvinces, bool bLabels, bool bResources);
	void SetHoveredTerritory(const FString& RegionId);
	void SetSelectedTerritory(const FString& RegionId);

	bool TryGetTerritoryForComponent(const UPrimitiveComponent* Component, FWLCampaignTerritoryRegionView& OutRegion) const;
	bool TryGetTerritoryAtWorldLocation(const FVector& WorldLocation, bool bRequireProvince, FWLCampaignTerritoryRegionView& OutRegion) const;
	bool GetRegionById(const FString& RegionId, FWLCampaignTerritoryRegionView& OutRegion) const;

	bool AreBordersVisible() const { return bShowBorders; }
	bool AreProvincesVisible() const { return bShowProvinces; }
	bool AreLabelsVisible() const { return bShowLabels; }
	bool AreResourcesVisible() const { return bShowResources; }

private:
	struct FRegionRecord
	{
		FWLCampaignTerritoryRegionView View;
		TArray<FVector2D> LonLatPolygon;
		TArray<FVector> WorldPolygon;
		UProceduralMeshComponent* HitComponent = nullptr;
		bool bCountry = false;
	};

	UPROPERTY() TArray<UProceduralMeshComponent*> OwnedMeshes;
	UPROPERTY() TArray<UProceduralMeshComponent*> HitMeshes;
	UPROPERTY() TArray<UTextRenderComponent*> CountryLabels;
	UPROPERTY() TArray<UTextRenderComponent*> ProvinceLabels;
	UPROPERTY() UProceduralMeshComponent* NationalBorderMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* ProvinceBorderMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* ResourceMarkerMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* HoverHighlightMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SelectedHighlightMesh = nullptr;
	UPROPERTY() UMaterialInterface* VertexColorMaterial = nullptr;

	TArray<FRegionRecord> Regions;
	TMap<const UPrimitiveComponent*, int32> HitComponentToRegion;
	FString HoveredRegionId;
	FString SelectedRegionId;
	bool bLayerActive = true;
	bool bShowBorders = true;
	bool bShowProvinces = true;
	bool bShowLabels = true;
	bool bShowResources = true;
	float LastCameraHeight = 0.f;

	UProceduralMeshComponent* NewProceduralMesh(USceneComponent* Parent, const FName& Name, bool bCollision);
	void BuildCountryBordersAndHitProxies(USceneComponent* Parent, TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
	void BuildProvinceRegions(USceneComponent* Parent, TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
	void RebuildHighlightMesh(UProceduralMeshComponent* Mesh, const FString& RegionId, const FLinearColor& Color, float WidthWorld, float ZLift);
	void RebuildResourceMarkers();
	void RegisterHitMesh(FRegionRecord& Region, UProceduralMeshComponent* Mesh);
};
