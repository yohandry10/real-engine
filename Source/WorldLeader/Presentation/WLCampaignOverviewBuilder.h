// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

struct FWLCampaignOverviewBuildParams
{
	UMaterialInterface* Material = nullptr;
	float LandZ = 90.f;
	float BorderZ = 360.f;
	float BorderWidth = 520.f;
};

class FWLCampaignOverviewBuilder
{
public:
	static void Build(
		UProceduralMeshComponent* OverviewMesh,
		const FWLCampaignOverviewBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
};
