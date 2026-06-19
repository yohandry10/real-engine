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

	FString ReadStringFieldOrDefault(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, const FString& DefaultValue)
	{
		FString Value;
		return ReadStringField(Obj, FieldName, Value) ? Value : DefaultValue;
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
			Country.ContinentalRegion = ReadStringFieldOrDefault(*ObjPtr, TEXT("continental_region"), TEXT("America"));
			Country.DetailLevel = ReadStringFieldOrDefault(*ObjPtr, TEXT("detail_level"),
				IsCoreTheaterIso(Country.Iso) ? TEXT("high") : TEXT("low"));
			Country.PrimaryBiome = ReadStringFieldOrDefault(*ObjPtr, TEXT("primary_biome"), TEXT("continental"));
			Country.VisualProfile = ReadStringFieldOrDefault(*ObjPtr, TEXT("visual_profile"), Country.PrimaryBiome);
			Country.Capital = ReadStringFieldOrDefault(*ObjPtr, TEXT("capital"), TEXT(""));
			Country.Status = ReadStringFieldOrDefault(*ObjPtr, TEXT("status"), TEXT("visual context"));
			Country.bAdministrable = ReadBoolField(*ObjPtr, TEXT("administrable"), IsCoreTheaterIso(Country.Iso));
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
			City.Id = ReadStringFieldOrDefault(*ObjPtr, TEXT("id"), FString::Printf(TEXT("%s-%s"), *City.CountryIso, *City.Name));
			City.Region = ReadStringFieldOrDefault(*ObjPtr, TEXT("region"), TEXT("America"));
			City.MarkerType = ReadStringFieldOrDefault(*ObjPtr, TEXT("marker_type"), TEXT("low_detail"));
			City.StrategicRole = ReadStringFieldOrDefault(*ObjPtr, TEXT("strategic_role"), TEXT("ciudad estrategica"));
			City.DetailLevel = ReadStringFieldOrDefault(*ObjPtr, TEXT("detail_level"),
				IsCoreTheaterIso(City.CountryIso) ? TEXT("high") : TEXT("low"));
			City.VisibleFromZoom = ReadStringFieldOrDefault(*ObjPtr, TEXT("visible_from_zoom"), TEXT("regional"));
			City.Description = ReadStringFieldOrDefault(*ObjPtr, TEXT("description"), TEXT("Lectura estrategica placeholder."));
			City.Lon = ReadFloatField(*ObjPtr, TEXT("lon"));
			City.Lat = ReadFloatField(*ObjPtr, TEXT("lat"));
			City.bMajor = ReadBoolField(*ObjPtr, TEXT("major"));
			City.bPort = ReadBoolField(*ObjPtr, TEXT("port"));
			City.bCapital = ReadBoolField(*ObjPtr, TEXT("capital"));
			City.bAdministrable = ReadBoolField(*ObjPtr, TEXT("administrable"), IsCoreTheaterIso(City.CountryIso));
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
