// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignAmericaLowDetailData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadStringField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, FString& OutValue)
	{
		return Obj.IsValid() && Obj->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
	}

	float ReadFloatField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, float DefaultValue = 0.f)
	{
		double Value = DefaultValue;
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(FieldName, Value);
		}
		return static_cast<float>(Value);
	}

	bool ReadBoolField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, bool DefaultValue = false)
	{
		bool Value = DefaultValue;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(FieldName, Value);
		}
		return Value;
	}
}

bool FWLCampaignAmericaLowDetailDataLoader::Load(FWLCampaignAmericaLowDetailData& OutData)
{
	OutData = FWLCampaignAmericaLowDetailData();

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("AmericaLowDetail.json");
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

	const TArray<TSharedPtr<FJsonValue>>* CountryValues = nullptr;
	if (Root->TryGetArrayField(TEXT("countries"), CountryValues) && CountryValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *CountryValues)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
			{
				continue;
			}

			FWLCampaignAmericaCountrySpec Country;
			if (!ReadStringField(*ObjPtr, TEXT("iso"), Country.Iso)
				|| !ReadStringField(*ObjPtr, TEXT("geo_admin"), Country.GeoAdmin))
			{
				continue;
			}

			if (!ReadStringField(*ObjPtr, TEXT("display_name"), Country.DisplayName))
			{
				Country.DisplayName = Country.GeoAdmin;
			}
			Country.LabelLon = ReadFloatField(*ObjPtr, TEXT("label_lon"));
			Country.LabelLat = ReadFloatField(*ObjPtr, TEXT("label_lat"));
			Country.bSpecialTerritory = ReadBoolField(*ObjPtr, TEXT("special_territory"));
			OutData.Countries.Add(Country);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* CityValues = nullptr;
	if (Root->TryGetArrayField(TEXT("cities"), CityValues) && CityValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *CityValues)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
			{
				continue;
			}

			FWLCampaignAmericaCitySpec City;
			if (!ReadStringField(*ObjPtr, TEXT("country_iso"), City.CountryIso)
				|| !ReadStringField(*ObjPtr, TEXT("name"), City.Name))
			{
				continue;
			}
			City.Lon = ReadFloatField(*ObjPtr, TEXT("lon"));
			City.Lat = ReadFloatField(*ObjPtr, TEXT("lat"));
			City.bMajor = ReadBoolField(*ObjPtr, TEXT("major"));
			OutData.Cities.Add(City);
		}
	}

	return !OutData.Countries.IsEmpty();
}

bool FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(const FString& Iso)
{
	return Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
		|| Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase);
}
