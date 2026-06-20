// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignVisualStyle.h"

EWLVisualBiome FWLCampaignVisualStyle::ClassifyVisualBiome(float Lon, float Lat, bool bCoreCountry)
{
	if (!bCoreCountry)
	{
		return EWLVisualBiome::Context;
	}

	const bool bAndes = Lon < -71.4f && Lon > -77.2f && Lat < 11.6f;
	const bool bCaribbeanCoast = Lat > 9.8f;
	const bool bGuajiraDry = Lon < -71.0f && Lon > -74.3f && Lat > 10.1f;
	const bool bAmazonOrGuiana = Lat < 6.4f || (Lon > -67.0f && Lat < 8.4f);
	const bool bLlanos = Lon > -72.6f && Lon < -64.0f && Lat >= 6.1f && Lat < 9.9f;
	const bool bUrbanCorridor = (Lon < -73.2f && Lon > -76.2f && Lat > 4.1f && Lat < 7.0f)
		|| (Lon > -69.0f && Lon < -66.1f && Lat > 9.7f && Lat < 10.8f);

	if (bUrbanCorridor)
	{
		return EWLVisualBiome::UrbanInfluence;
	}
	if (bAndes)
	{
		return EWLVisualBiome::Mountain;
	}
	if (bGuajiraDry || bCaribbeanCoast)
	{
		return EWLVisualBiome::Coast;
	}
	if (bLlanos)
	{
		return EWLVisualBiome::Llanos;
	}
	if (bAmazonOrGuiana)
	{
		return EWLVisualBiome::Jungle;
	}
	return EWLVisualBiome::Jungle;
}

FLinearColor FWLCampaignVisualStyle::VisualBiomeColor(EWLVisualBiome Biome)
{
	switch (Biome)
	{
	case EWLVisualBiome::Coast:
		return FLinearColor(0.50f, 0.40f, 0.20f);
	case EWLVisualBiome::Jungle:
		return FLinearColor(0.025f, 0.230f, 0.070f);
	case EWLVisualBiome::Llanos:
		return FLinearColor(0.285f, 0.300f, 0.105f);
	case EWLVisualBiome::Mountain:
		return FLinearColor(0.385f, 0.330f, 0.215f);
	case EWLVisualBiome::UrbanInfluence:
		return FLinearColor(0.300f, 0.285f, 0.205f);
	default:
		return FLinearColor(0.050f, 0.105f, 0.070f);
	}
}

FLinearColor FWLCampaignVisualStyle::ShadeTerrainVertex(const FLinearColor& Base, float Lon, float Lat, float Height)
{
	const float Relief = FMath::Clamp(Height / 14500.f, 0.f, 1.f);
	const float Noise = 0.07f * FMath::Sin(Lon * 4.7f + Lat * 1.3f) + 0.05f * FMath::Cos(Lon * 2.1f - Lat * 3.4f);
	const float Light = FMath::Clamp(0.82f + Relief * 0.42f + Noise, 0.55f, 1.38f);
	FLinearColor Color(Base.R * Light, Base.G * Light, Base.B * Light, 1.f);
	Color += FLinearColor(Relief * 0.13f, Relief * 0.105f, Relief * 0.065f, 0.f);
	Color.R = FMath::Clamp(Color.R, 0.f, 1.f);
	Color.G = FMath::Clamp(Color.G, 0.f, 1.f);
	Color.B = FMath::Clamp(Color.B, 0.f, 1.f);
	return Color;
}

FColor FWLCampaignVisualStyle::ToVertexFColor(const FLinearColor& Color)
{
	return Color.ToFColor(true);
}

float FWLCampaignVisualStyle::VisualBiomeZOffset(EWLVisualBiome Biome, bool bCoreCountry)
{
	if (!bCoreCountry)
	{
		return -120.f;
	}
	switch (Biome)
	{
	case EWLVisualBiome::Mountain:
		return 560.f;
	case EWLVisualBiome::Coast:
		return 95.f;
	case EWLVisualBiome::Llanos:
		return 135.f;
	case EWLVisualBiome::UrbanInfluence:
		return 210.f;
	default:
		return 165.f;
	}
}

FLinearColor FWLCampaignVisualStyle::CountryTerrainColor(const FString& Iso)
{
	if (Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.235f, 0.345f, 0.185f);
	}
	if (Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.215f, 0.335f, 0.175f);
	}
	return FLinearColor(0.095f, 0.160f, 0.115f);
}
