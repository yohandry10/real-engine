// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

struct FWLCampaignTerrainBuildParams
{
	float RegionMinLon = 0.f;
	float RegionMaxLon = 0.f;
	float RegionMinLat = 0.f;
	float RegionMaxLat = 0.f;
	UMaterialInterface* TerrainMaterial = nullptr;
	UMaterialInterface* BoundaryMaterial = nullptr;
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
