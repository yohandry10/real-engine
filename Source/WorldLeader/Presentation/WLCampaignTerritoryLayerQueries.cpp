// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignTerritoryLayerComponent.h"

#include "Presentation/WLCampaignAmericaLowDetailData.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Presentation/WLCampaignTerritoryLayerPrivate.h"

using namespace WLCampaignTerritoryLayerPrivate;

bool UWLCampaignTerritoryLayerComponent::TryGetTerritoryForComponent(
	const UPrimitiveComponent* Component,
	FWLCampaignTerritoryRegionView& OutRegion) const
{
	if (!Component)
	{
		return false;
	}

	const int32* RegionIndex = HitComponentToRegion.Find(Component);
	if (!RegionIndex || !Regions.IsValidIndex(*RegionIndex))
	{
		return false;
	}

	OutRegion = Regions[*RegionIndex].View;
	return OutRegion.bSelectable;
}

bool UWLCampaignTerritoryLayerComponent::TryGetTerritoryAtWorldLocation(
	const FVector& WorldLocation,
	bool bRequireProvince,
	FWLCampaignTerritoryRegionView& OutRegion) const
{
	for (const FRegionRecord& Region : Regions)
	{
		if (Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		if (PointInWorldPolygonXY(Region.WorldPolygon, WorldLocation))
		{
			OutRegion = Region.View;
			return true;
		}
	}

	const FRegionRecord* BoundsMatchedProvince = nullptr;
	float BoundsMatchedArea = TNumericLimits<float>::Max();
	for (const FRegionRecord& Region : Regions)
	{
		if (Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		if (!WorldPolygonBoundsContainsXY(Region.WorldPolygon, WorldLocation, 1500.f))
		{
			continue;
		}
		const float Area = WorldPolygonBoundsAreaXY(Region.WorldPolygon);
		if (Area < BoundsMatchedArea)
		{
			BoundsMatchedArea = Area;
			BoundsMatchedProvince = &Region;
		}
	}
	if (BoundsMatchedProvince)
	{
		OutRegion = BoundsMatchedProvince->View;
		return true;
	}

	const FRegionRecord* NearestProvince = nullptr;
	float NearestDistanceSq = FMath::Square(260000.f);
	for (const FRegionRecord& Region : Regions)
	{
		if (Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared2D(Region.View.WorldLocation, WorldLocation);
		if (DistanceSq < NearestDistanceSq)
		{
			NearestDistanceSq = DistanceSq;
			NearestProvince = &Region;
		}
	}
	if (NearestProvince)
	{
		OutRegion = NearestProvince->View;
		return true;
	}

	if (bRequireProvince)
	{
		return false;
	}

	for (const FRegionRecord& Region : Regions)
	{
		if (!Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		if (PointInWorldPolygonXY(Region.WorldPolygon, WorldLocation))
		{
			OutRegion = Region.View;
			return true;
		}
	}
	return false;
}

bool UWLCampaignTerritoryLayerComponent::GetRegionById(const FString& RegionId, FWLCampaignTerritoryRegionView& OutRegion) const
{
	const FRegionRecord* Region = Regions.FindByPredicate([&](const FRegionRecord& Candidate)
	{
		return Candidate.View.Id == RegionId;
	});
	if (!Region)
	{
		return false;
	}
	OutRegion = Region->View;
	return true;
}
