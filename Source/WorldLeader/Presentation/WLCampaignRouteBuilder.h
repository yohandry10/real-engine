// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

enum class EWLCampaignRouteType : uint8
{
	Primary,
	Secondary,
	Rural,
	PortAccess,
	BorderCrossing
};

struct FWLCampaignRouteSpec
{
	FString Name;
	EWLCampaignRouteType Type = EWLCampaignRouteType::Secondary;
	TArray<FVector2D> Points;
	int32 Smoothness = 5;
	bool bShowJunctions = true;
};

class FWLCampaignRouteBuilder
{
public:
	static void BuildDefaultTheaterRoutes(
		UProceduralMeshComponent* RoadMesh,
		UMaterialInterface* RoadMaterial,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
};
