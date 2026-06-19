// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLCampaignSelectionPanelData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString ReadStringFieldOrDefault(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, const FString& DefaultValue = TEXT(""))
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
	}

	TArray<FString> ReadStringArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(FieldName, Values) || !Values)
		{
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
			{
				Result.Add(Text);
			}
		}
		return Result;
	}

	bool ParseEntry(const TSharedPtr<FJsonObject>& Obj, FWLCampaignSelectionPanelEntry& OutEntry)
	{
		if (!Obj.IsValid())
		{
			return false;
		}

		OutEntry.Id = ReadStringFieldOrDefault(Obj, TEXT("id"));
		OutEntry.Name = ReadStringFieldOrDefault(Obj, TEXT("name"));
		if (OutEntry.Id.IsEmpty() || OutEntry.Name.IsEmpty())
		{
			return false;
		}

		OutEntry.Country = ReadStringFieldOrDefault(Obj, TEXT("country"));
		OutEntry.CountryIso = ReadStringFieldOrDefault(Obj, TEXT("country_iso"));
		OutEntry.TypeLabel = ReadStringFieldOrDefault(Obj, TEXT("type"));
		OutEntry.TerritoryId = ReadStringFieldOrDefault(Obj, TEXT("territory_id"));
		OutEntry.TerritoryName = ReadStringFieldOrDefault(Obj, TEXT("territory_name"));
		OutEntry.CapitalOrMainCity = ReadStringFieldOrDefault(Obj, TEXT("capital_or_main_city"));
		OutEntry.Owner = ReadStringFieldOrDefault(Obj, TEXT("owner"));
		OutEntry.Controller = ReadStringFieldOrDefault(Obj, TEXT("controller"));
		OutEntry.Population = ReadStringFieldOrDefault(Obj, TEXT("population"));
		OutEntry.PublicOrder = ReadStringFieldOrDefault(Obj, TEXT("public_order"));
		OutEntry.Infrastructure = ReadStringFieldOrDefault(Obj, TEXT("infrastructure"));
		OutEntry.StrategicImportance = ReadStringFieldOrDefault(Obj, TEXT("strategic_importance"));
		OutEntry.DetailLevel = ReadStringFieldOrDefault(Obj, TEXT("detail_level"));
		OutEntry.PortStatus = ReadStringFieldOrDefault(Obj, TEXT("port_status"));
		OutEntry.Resources = ReadStringArray(Obj, TEXT("resources"));
		OutEntry.Ports = ReadStringArray(Obj, TEXT("ports"));
		OutEntry.Cities = ReadStringArray(Obj, TEXT("cities"));
		OutEntry.BuildingSlots = ReadStringArray(Obj, TEXT("building_slots"));
		OutEntry.UrbanSlots = ReadStringArray(Obj, TEXT("urban_slots"));
		OutEntry.DisabledActions = ReadStringArray(Obj, TEXT("disabled_actions"));
		return true;
	}

	void ParseEntryArray(const TSharedPtr<FJsonObject>& Root, const TCHAR* FieldName, TMap<FString, FWLCampaignSelectionPanelEntry>& OutMap)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
			{
				continue;
			}

			FWLCampaignSelectionPanelEntry Entry;
			if (ParseEntry(*ObjPtr, Entry))
			{
				OutMap.Add(Entry.Id, MoveTemp(Entry));
			}
		}
	}
}

bool FWLCampaignSelectionPanelDataLoader::Load(FWLCampaignSelectionPanelData& OutData)
{
	OutData = FWLCampaignSelectionPanelData();

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("SelectionPanelData.json");
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

	ParseEntryArray(Root, TEXT("provinces"), OutData.Provinces);
	ParseEntryArray(Root, TEXT("cities"), OutData.Cities);
	return OutData.Provinces.Num() > 0 || OutData.Cities.Num() > 0;
}
