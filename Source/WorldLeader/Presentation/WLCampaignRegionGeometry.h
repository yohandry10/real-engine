// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"

struct FWLRegionalCountryGeometry
{
	FString Iso;
	FString Name;
	TArray<TArray<FVector2D>> Rings;
	FLinearColor Color = FLinearColor::White;
	bool bCoreCountry = false;
};

class FWLCampaignRegionGeometry
{
public:
	static bool IsRegionalProvince(const FWLProvinceData& Province);
	static bool IsTheaterIso(const FString& Iso);
	static bool IsContextIso(const FString& Iso);

	static bool LoadCountries(
		float RegionMinLon,
		float RegionMaxLon,
		float RegionMinLat,
		float RegionMaxLat,
		TArray<FWLRegionalCountryGeometry>& OutCountries);

	static bool LonLatBoundsIntersect(
		float AMinLon,
		float AMaxLon,
		float AMinLat,
		float AMaxLat,
		float BMinLon,
		float BMaxLon,
		float BMinLat,
		float BMaxLat);

	static void GetLonLatBounds(
		const TArray<FVector2D>& Ring,
		float& OutMinLon,
		float& OutMaxLon,
		float& OutMinLat,
		float& OutMaxLat);

	static bool PointInLonLatRing(const FVector2D& P, const TArray<FVector2D>& Ring);
	static bool PointInAnyLonLatRing(const FVector2D& P, const TArray<TArray<FVector2D>>& Rings);

private:
	static bool RingIntersectsCampaignRegion(
		const TArray<FVector2D>& Ring,
		float MinLon,
		float MaxLon,
		float MinLat,
		float MaxLat);
};
