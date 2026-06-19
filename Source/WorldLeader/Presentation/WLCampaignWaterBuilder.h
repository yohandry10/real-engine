// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

struct FWLCampaignWaterBuildParams
{
	FVector Center = FVector::ZeroVector;
	float HalfSize = 760000.f;
	float SeaZ = -2350.f;
	int32 TileCount = 52;
	UMaterialInterface* WaterMaterial = nullptr;
	UMaterialInterface* DetailMaterial = nullptr;
};

class FWLCampaignWaterBuilder
{
public:
	static void Build(
		UProceduralMeshComponent* SeaMesh,
		UProceduralMeshComponent* DetailMesh,
		const FWLCampaignWaterBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
};
