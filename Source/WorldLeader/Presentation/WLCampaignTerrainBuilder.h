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

// Disco bajo una ciudad donde el terreno debe quedar PLANO y al MISMO offset de bioma que el ancla
// de la ciudad. Sin esto, una ciudad que abarca varios biomas (p.ej. puerto Costa +95 junto a Montaña
// +560) tiene celdas de offset mas alto que SOBRESALEN y entierran los edificios -> "ciudad incompleta".
// Tambien fuerza el render dentro del radio para rellenar huecos costeros (celdas con centro en mar).
struct FWLCampaignCityLevelPad
{
	float Lon = 0.f;
	float Lat = 0.f;
	float RadiusDeg = 0.f;   // grados; cubre la huella entera de la ciudad (esquinas incluidas)
	float ZOffset = 0.f;     // = offset de bioma del ancla de la ciudad (mismo valor exacto)
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
	// Circulos (X=lon, Y=lat, Z=radio en grados) donde la linea de contorno NO se dibuja: las
	// huellas de ciudad. Sin esto el ribete blanco trazaba el estuario por ENCIMA del puerto
	// (Guayaquil). Solo afecta al BoundaryMesh, no al terreno (las ExclusionZones si lo perforan).
	TArray<FVector> BoundaryClipCircles;
	// Discos de ciudad: terreno plano + offset de bioma unificado + relleno de huecos (ver struct).
	TArray<FWLCampaignCityLevelPad> CityLevelPads;
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
