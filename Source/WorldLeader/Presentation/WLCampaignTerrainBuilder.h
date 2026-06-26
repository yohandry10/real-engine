// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

struct FWLCampaignTerrainExclusionZone
{
	float Lon = 0.f;
	float Lat = 0.f;
	float RadiusDeg = 0.f;
	float HalfLonDeg = 0.f;
	float HalfLatDeg = 0.f;
};

struct FWLCampaignTerrainBuildParams
{
	float RegionMinLon = 0.f;
	float RegionMaxLon = 0.f;
	float RegionMinLat = 0.f;
	float RegionMaxLat = 0.f;
	UMaterialInterface* TerrainMaterial = nullptr;
	UMaterialInterface* BoundaryMaterial = nullptr;
	TArray<UMaterialInterface*> TerrainMaterialsByBiome;
	TArray<FWLCampaignTerrainExclusionZone> ExclusionZones;
	FString IncludeOnlyIso;
	bool bIncludeOnlyCoreCountries = false;
	bool bCreateTerrainCollision = true;
};

class FWLCampaignTerrainBuilder
{
public:
	static bool Build(
		UProceduralMeshComponent* TerrainMesh,
		UProceduralMeshComponent* BoundaryMesh,
		const FWLCampaignTerrainBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat,
		FBox2D& Bounds);
};
