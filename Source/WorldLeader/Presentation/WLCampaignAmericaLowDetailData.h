// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

struct FWLCampaignAmericaCountrySpec
{
	FString Iso;
	FString GeoAdmin;
	FString DisplayName;
	FString ContinentalRegion;
	FString DetailLevel;
	FString PrimaryBiome;
	FString VisualProfile;
	FString Capital;
	FString Status;
	float LabelLon = 0.f;
	float LabelLat = 0.f;
	bool bSpecialTerritory = false;
	bool bAdministrable = false;
};

struct FWLCampaignAmericaCitySpec
{
	FString Id;
	FString CountryIso;
	FString Name;
	FString Region;
	FString MarkerType;
	FString StrategicRole;
	FString DetailLevel;
	FString VisibleFromZoom;
	FString Description;
	float Lon = 0.f;
	float Lat = 0.f;
	bool bMajor = false;
	bool bPort = false;
	bool bCapital = false;
	bool bAdministrable = false;
};

struct FWLCampaignAmericaLowDetailData
{
	TArray<FWLCampaignAmericaCountrySpec> Countries;
	TArray<FWLCampaignAmericaCitySpec> Cities;
};

class FWLCampaignAmericaLowDetailDataLoader
{
public:
	static bool Load(FWLCampaignAmericaLowDetailData& OutData);
	static bool IsCoreTheaterIso(const FString& Iso);
};
