// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

struct FWLCampaignAmericaCountrySpec
{
	FString Iso;
	FString GeoAdmin;
	FString DisplayName;
	float LabelLon = 0.f;
	float LabelLat = 0.f;
	bool bSpecialTerritory = false;
};

struct FWLCampaignAmericaCitySpec
{
	FString CountryIso;
	FString Name;
	float Lon = 0.f;
	float Lat = 0.f;
	bool bMajor = false;
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
