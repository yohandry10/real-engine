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
	float CityZ = 720.f;
	float CityMarkerSize = 1450.f;
};

struct FWLCampaignOverviewLabelSpec
{
	FString Text;
	float Lon = 0.f;
	float Lat = 0.f;
	float ZOffset = 0.f;
	float WorldSize = 1800.f;
	FColor Color = FColor::White;
};

class FWLCampaignOverviewBuilder
{
public:
	static void Build(
		UProceduralMeshComponent* OverviewMesh,
		const FWLCampaignOverviewBuildParams& Params,
		TArray<FWLCampaignOverviewLabelSpec>& OutLabels,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
};
