// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignRegionGeometry.h"

#include "Presentation/WLCampaignVisualStyle.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TArray<FVector2D> LonLatRingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring)
	{
		TArray<FVector2D> Result;
		Result.Reserve(Ring.Num());
		for (const TSharedPtr<FJsonValue>& CoordVal : Ring)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
			if (CoordVal.IsValid() && CoordVal->TryGetArray(Pair) && Pair && Pair->Num() >= 2)
			{
				Result.Add(FVector2D(static_cast<float>((*Pair)[0]->AsNumber()), static_cast<float>((*Pair)[1]->AsNumber())));
			}
		}
		return Result;
	}
}

bool FWLCampaignRegionGeometry::IsRegionalProvince(const FWLProvinceData& Province)
{
	return Province.CountryIso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
		|| Province.CountryIso.Equals(TEXT("VE"), ESearchCase::IgnoreCase);
}

bool FWLCampaignRegionGeometry::IsTheaterIso(const FString& Iso)
{
	return Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("EC"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("PE"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("BO"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("BR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("AR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("CL"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("UY"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("PY"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("GY"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("SR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("MX"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("CU"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("JM"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("HT"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("DO"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("PR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("BS"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("TT"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("US"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("CA"), ESearchCase::IgnoreCase);
}

bool FWLCampaignRegionGeometry::IsContextIso(const FString& Iso)
{
	return IsTheaterIso(Iso)
		|| Iso.Equals(TEXT("PA"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("EC"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("PE"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("BR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("GY"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("SR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("FR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("TT"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("AW"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("CW"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("JM"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("HT"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("DO"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("PR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("PY"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("CL"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("AR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("UY"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("GT"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("BZ"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("HN"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("SV"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("NI"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("CR"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("CU"), ESearchCase::IgnoreCase);
}

bool FWLCampaignRegionGeometry::LoadCountries(
	float RegionMinLon,
	float RegionMaxLon,
	float RegionMinLat,
	float RegionMaxLat,
	TArray<FWLRegionalCountryGeometry>& OutCountries)
{
	OutCountries.Reset();

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Geo") / TEXT("Countries.geojson");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
	if (!Root->TryGetArrayField(TEXT("features"), Features))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FeatVal : *Features)
	{
		const TSharedPtr<FJsonObject>* FeatObj = nullptr;
		if (!FeatVal.IsValid() || !FeatVal->TryGetObject(FeatObj))
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* PropsObj = nullptr;
		if (!(*FeatObj)->TryGetObjectField(TEXT("properties"), PropsObj))
		{
			continue;
		}

		FString Iso;
		(*PropsObj)->TryGetStringField(TEXT("ISO_A2"), Iso);
		if (!IsContextIso(Iso))
		{
			continue;
		}

		FString Name;
		(*PropsObj)->TryGetStringField(TEXT("ADMIN"), Name);

		const TSharedPtr<FJsonObject>* GeomObj = nullptr;
		if (!(*FeatObj)->TryGetObjectField(TEXT("geometry"), GeomObj))
		{
			continue;
		}

		FString GeomType;
		(*GeomObj)->TryGetStringField(TEXT("type"), GeomType);
		const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
		if (!(*GeomObj)->TryGetArrayField(TEXT("coordinates"), Coords))
		{
			continue;
		}

		FWLRegionalCountryGeometry Country;
		Country.Iso = Iso;
		Country.Name = Name;
		Country.bCoreCountry = IsTheaterIso(Iso);
		Country.Color = FWLCampaignVisualStyle::CountryTerrainColor(Iso);

		auto ProcessPolygon = [&](const TArray<TSharedPtr<FJsonValue>>& Rings)
		{
			if (Rings.Num() == 0)
			{
				return;
			}
			const TArray<TSharedPtr<FJsonValue>>* Outer = nullptr;
			if (!Rings[0]->TryGetArray(Outer) || !Outer)
			{
				return;
			}

			TArray<FVector2D> Ring = LonLatRingFromJson(*Outer);
			if (RingIntersectsCampaignRegion(Ring, RegionMinLon, RegionMaxLon, RegionMinLat, RegionMaxLat))
			{
				Country.Rings.Add(MoveTemp(Ring));
			}
		};

		if (GeomType == TEXT("Polygon"))
		{
			ProcessPolygon(*Coords);
		}
		else if (GeomType == TEXT("MultiPolygon"))
		{
			for (const TSharedPtr<FJsonValue>& PolyVal : *Coords)
			{
				const TArray<TSharedPtr<FJsonValue>>* PolyRings = nullptr;
				if (PolyVal.IsValid() && PolyVal->TryGetArray(PolyRings) && PolyRings)
				{
					ProcessPolygon(*PolyRings);
				}
			}
		}

		if (!Country.Rings.IsEmpty())
		{
			OutCountries.Add(MoveTemp(Country));
		}
	}

	OutCountries.Sort([](const FWLRegionalCountryGeometry& A, const FWLRegionalCountryGeometry& B)
	{
		if (A.bCoreCountry != B.bCoreCountry)
		{
			return !A.bCoreCountry;
		}
		return A.Iso < B.Iso;
	});

	return !OutCountries.IsEmpty();
}

bool FWLCampaignRegionGeometry::LonLatBoundsIntersect(
	float AMinLon,
	float AMaxLon,
	float AMinLat,
	float AMaxLat,
	float BMinLon,
	float BMaxLon,
	float BMinLat,
	float BMaxLat)
{
	return AMinLon <= BMaxLon && AMaxLon >= BMinLon && AMinLat <= BMaxLat && AMaxLat >= BMinLat;
}

void FWLCampaignRegionGeometry::GetLonLatBounds(
	const TArray<FVector2D>& Ring,
	float& OutMinLon,
	float& OutMaxLon,
	float& OutMinLat,
	float& OutMaxLat)
{
	OutMinLon = TNumericLimits<float>::Max();
	OutMaxLon = TNumericLimits<float>::Lowest();
	OutMinLat = TNumericLimits<float>::Max();
	OutMaxLat = TNumericLimits<float>::Lowest();
	for (const FVector2D& P : Ring)
	{
		OutMinLon = FMath::Min(OutMinLon, P.X);
		OutMaxLon = FMath::Max(OutMaxLon, P.X);
		OutMinLat = FMath::Min(OutMinLat, P.Y);
		OutMaxLat = FMath::Max(OutMaxLat, P.Y);
	}
}

bool FWLCampaignRegionGeometry::PointInLonLatRing(const FVector2D& P, const TArray<FVector2D>& Ring)
{
	bool bInside = false;
	for (int32 Index = 0, Prev = Ring.Num() - 1; Index < Ring.Num(); Prev = Index++)
	{
		const FVector2D& A = Ring[Index];
		const FVector2D& B = Ring[Prev];
		const float Denom = B.Y - A.Y;
		if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const bool bCrosses = ((A.Y > P.Y) != (B.Y > P.Y))
			&& (P.X < (B.X - A.X) * (P.Y - A.Y) / Denom + A.X);
		if (bCrosses)
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

bool FWLCampaignRegionGeometry::PointInAnyLonLatRing(const FVector2D& P, const TArray<TArray<FVector2D>>& Rings)
{
	for (const TArray<FVector2D>& Ring : Rings)
	{
		if (Ring.Num() >= 3 && PointInLonLatRing(P, Ring))
		{
			return true;
		}
	}
	return false;
}

bool FWLCampaignRegionGeometry::RingIntersectsCampaignRegion(
	const TArray<FVector2D>& Ring,
	float MinLon,
	float MaxLon,
	float MinLat,
	float MaxLat)
{
	if (Ring.Num() < 3)
	{
		return false;
	}

	float RingMinLon, RingMaxLon, RingMinLat, RingMaxLat;
	GetLonLatBounds(Ring, RingMinLon, RingMaxLon, RingMinLat, RingMaxLat);
	return LonLatBoundsIntersect(RingMinLon, RingMaxLon, RingMinLat, RingMaxLat, MinLon, MaxLon, MinLat, MaxLat);
}
