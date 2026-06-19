// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLCampaignSelectionPanelData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString ReadSelectionPanelStringFieldOrDefault(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, const FString& DefaultValue = TEXT(""))
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
	}

	TArray<FString> ReadSelectionPanelStringArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName)
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

		OutEntry.Id = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("id"));
		OutEntry.Name = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("name"));
		if (OutEntry.Id.IsEmpty() || OutEntry.Name.IsEmpty())
		{
			return false;
		}

		OutEntry.Country = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("country"));
		OutEntry.CountryIso = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("country_iso"));
		OutEntry.TypeLabel = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("type"));
		OutEntry.TerritoryId = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("territory_id"));
		OutEntry.TerritoryName = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("territory_name"));
		OutEntry.CapitalOrMainCity = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("capital_or_main_city"));
		OutEntry.Owner = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("owner"));
		OutEntry.Controller = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("controller"));
		OutEntry.Population = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("population"));
		OutEntry.PublicOrder = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("public_order"));
		OutEntry.Infrastructure = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("infrastructure"));
		OutEntry.StrategicImportance = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("strategic_importance"));
		OutEntry.DetailLevel = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("detail_level"));
		OutEntry.PortStatus = ReadSelectionPanelStringFieldOrDefault(Obj, TEXT("port_status"));
		OutEntry.Resources = ReadSelectionPanelStringArray(Obj, TEXT("resources"));
		OutEntry.Ports = ReadSelectionPanelStringArray(Obj, TEXT("ports"));
		OutEntry.Cities = ReadSelectionPanelStringArray(Obj, TEXT("cities"));
		OutEntry.BuildingSlots = ReadSelectionPanelStringArray(Obj, TEXT("building_slots"));
		OutEntry.UrbanSlots = ReadSelectionPanelStringArray(Obj, TEXT("urban_slots"));
		OutEntry.DisabledActions = ReadSelectionPanelStringArray(Obj, TEXT("disabled_actions"));
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
