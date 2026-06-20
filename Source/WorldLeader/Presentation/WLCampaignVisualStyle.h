// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

enum class EWLVisualBiome : uint8
{
	Context = 0,
	Coast,
	Jungle,
	Llanos,
	Mountain,
	UrbanInfluence,
	Count
};

class FWLCampaignVisualStyle
{
public:
	static EWLVisualBiome ClassifyVisualBiome(float Lon, float Lat, bool bCoreCountry);
	static FLinearColor VisualBiomeColor(EWLVisualBiome Biome);
	static FLinearColor ShadeTerrainVertex(const FLinearColor& Base, float Lon, float Lat, float Height);
	static FColor ToVertexFColor(const FLinearColor& Color);
	static float VisualBiomeZOffset(EWLVisualBiome Biome, bool bCoreCountry);
	static FLinearColor CountryTerrainColor(const FString& Iso);
};
