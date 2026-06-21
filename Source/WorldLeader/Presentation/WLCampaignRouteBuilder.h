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

	// Anade una lista arbitraria de rutas a la malla (una seccion mas). Sirve para
	// redes generadas (p.ej. conectar las ciudades de cada pais).
	static void AppendRoutes(
		UProceduralMeshComponent* RoadMesh,
		UMaterialInterface* RoadMaterial,
		const TArray<FWLCampaignRouteSpec>& Routes,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);
};
