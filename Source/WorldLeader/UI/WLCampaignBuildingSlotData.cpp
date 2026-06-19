// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLCampaignBuildingSlotData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString ReadCampaignBuildingStringField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, const FString& DefaultValue = TEXT(""))
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
	}

	int32 ReadCampaignBuildingIntField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, int32 DefaultValue = 0)
	{
		int32 Value = DefaultValue;
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(FieldName, Value);
		}
		return Value;
	}

	bool ReadCampaignBuildingBoolField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, bool bDefaultValue = false)
	{
		bool bValue = bDefaultValue;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	TArray<FString> ReadCampaignBuildingStringArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName)
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

	bool CampaignBuildingContextMatches(const FString& ContextText, EWLCampaignBuildingPanelContext Context)
	{
		const FString Normalized = ContextText.ToLower();
		return (Context == EWLCampaignBuildingPanelContext::City && Normalized == TEXT("city"))
			|| (Context == EWLCampaignBuildingPanelContext::Province && Normalized == TEXT("province"));
	}

	void ParseCampaignBuildingSlots(const TSharedPtr<FJsonObject>& Root, FWLCampaignBuildingSlotCatalog& OutCatalog)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("slots"), Values) || !Values)
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

			FWLCampaignBuildingSlotDefinition Slot;
			Slot.Id = ReadCampaignBuildingStringField(*ObjPtr, TEXT("id"));
			Slot.Context = ReadCampaignBuildingStringField(*ObjPtr, TEXT("context"));
			Slot.Label = ReadCampaignBuildingStringField(*ObjPtr, TEXT("label"));
			Slot.DefaultState = ReadCampaignBuildingStringField(*ObjPtr, TEXT("default_state"), TEXT("empty"));
			Slot.InitialBuildingId = ReadCampaignBuildingStringField(*ObjPtr, TEXT("initial_building_id"));
			Slot.Description = ReadCampaignBuildingStringField(*ObjPtr, TEXT("description"));
			Slot.Matches = ReadCampaignBuildingStringArray(*ObjPtr, TEXT("matches"));
			if (!Slot.Id.IsEmpty() && !Slot.Label.IsEmpty() && !Slot.Context.IsEmpty())
			{
				OutCatalog.Slots.Add(MoveTemp(Slot));
			}
		}
	}

	void ParseCampaignBuildings(const TSharedPtr<FJsonObject>& Root, FWLCampaignBuildingSlotCatalog& OutCatalog)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("buildings"), Values) || !Values)
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

			FWLCampaignBuildingDefinition Building;
			Building.Id = ReadCampaignBuildingStringField(*ObjPtr, TEXT("id"));
			Building.Name = ReadCampaignBuildingStringField(*ObjPtr, TEXT("name"));
			Building.Context = ReadCampaignBuildingStringField(*ObjPtr, TEXT("context"));
			Building.TypeLabel = ReadCampaignBuildingStringField(*ObjPtr, TEXT("type"));
			Building.Level = FMath::Max(1, ReadCampaignBuildingIntField(*ObjPtr, TEXT("level"), 1));
			Building.Description = ReadCampaignBuildingStringField(*ObjPtr, TEXT("description"));
			Building.FutureCost = ReadCampaignBuildingStringField(*ObjPtr, TEXT("future_cost"), TEXT("Costo futuro: pendiente"));
			Building.bRequiresPort = ReadCampaignBuildingBoolField(*ObjPtr, TEXT("requires_port"));
			Building.CompatibleSlots = ReadCampaignBuildingStringArray(*ObjPtr, TEXT("compatible_slots"));
			Building.Effects = ReadCampaignBuildingStringArray(*ObjPtr, TEXT("effects"));
			if (Building.IsValid() && !Building.Context.IsEmpty())
			{
				OutCatalog.Buildings.Add(MoveTemp(Building));
			}
		}
	}
}

const FWLCampaignBuildingDefinition* FWLCampaignBuildingSlotCatalog::FindBuilding(const FString& BuildingId) const
{
	for (const FWLCampaignBuildingDefinition& Building : Buildings)
	{
		if (Building.Id.Equals(BuildingId, ESearchCase::IgnoreCase))
		{
			return &Building;
		}
	}
	return nullptr;
}

const FWLCampaignBuildingSlotDefinition* FWLCampaignBuildingSlotCatalog::FindSlotDefinition(
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel) const
{
	const FString NormalizedSlot = FWLCampaignBuildingSlotRules::NormalizeSlotLabel(SlotLabel);
	for (const FWLCampaignBuildingSlotDefinition& Slot : Slots)
	{
		if (!CampaignBuildingContextMatches(Slot.Context, Context))
		{
			continue;
		}
		if (FWLCampaignBuildingSlotRules::NormalizeSlotLabel(Slot.Label) == NormalizedSlot)
		{
			return &Slot;
		}
		for (const FString& Match : Slot.Matches)
		{
			if (FWLCampaignBuildingSlotRules::NormalizeSlotLabel(Match) == NormalizedSlot)
			{
				return &Slot;
			}
		}
	}
	return nullptr;
}

bool FWLCampaignBuildingSlotDataLoader::Load(FWLCampaignBuildingSlotCatalog& OutCatalog)
{
	OutCatalog = FWLCampaignBuildingSlotCatalog();

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("BuildingSlotCatalog.json");
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

	ParseCampaignBuildingSlots(Root, OutCatalog);
	ParseCampaignBuildings(Root, OutCatalog);
	return OutCatalog.Slots.Num() > 0 && OutCatalog.Buildings.Num() > 0;
}

EWLCampaignBuildingPanelContext FWLCampaignBuildingSlotRules::ContextFromCityMode(bool bCityMode)
{
	return bCityMode ? EWLCampaignBuildingPanelContext::City : EWLCampaignBuildingPanelContext::Province;
}

FString FWLCampaignBuildingSlotRules::ContextToText(EWLCampaignBuildingPanelContext Context)
{
	return Context == EWLCampaignBuildingPanelContext::City ? TEXT("city") : TEXT("province");
}

FString FWLCampaignBuildingSlotRules::NormalizeSlotLabel(const FString& SlotLabel)
{
	FString Normalized = SlotLabel.TrimStartAndEnd().ToUpper();
	Normalized.ReplaceInline(TEXT(" "), TEXT("_"));
	Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
	Normalized.ReplaceInline(TEXT("/"), TEXT("_"));
	return Normalized;
}

FString FWLCampaignBuildingSlotRules::MakeSlotKey(
	const FString& ObjectId,
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel,
	int32 SlotIndex)
{
	return FString::Printf(TEXT("%s|%s|%d|%s"),
		*ObjectId,
		*ContextToText(Context),
		SlotIndex,
		*NormalizeSlotLabel(SlotLabel));
}

FString FWLCampaignBuildingSlotRules::GetSlotDisplayLabel(
	const FWLCampaignBuildingSlotCatalog& Catalog,
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel)
{
	if (const FWLCampaignBuildingSlotDefinition* Slot = Catalog.FindSlotDefinition(Context, SlotLabel))
	{
		return Slot->Label;
	}
	return SlotLabel;
}

FString FWLCampaignBuildingSlotRules::GetSlotDescription(
	const FWLCampaignBuildingSlotCatalog& Catalog,
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel)
{
	if (const FWLCampaignBuildingSlotDefinition* Slot = Catalog.FindSlotDefinition(Context, SlotLabel))
	{
		return Slot->Description;
	}
	return TEXT("Slot placeholder preparado para futuras reglas.");
}

bool FWLCampaignBuildingSlotRules::IsSlotLocked(
	const FWLCampaignBuildingSlotCatalog& Catalog,
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel)
{
	if (const FWLCampaignBuildingSlotDefinition* Slot = Catalog.FindSlotDefinition(Context, SlotLabel))
	{
		return Slot->DefaultState.Equals(TEXT("locked"), ESearchCase::IgnoreCase);
	}
	return NormalizeSlotLabel(SlotLabel).Contains(TEXT("AIR"));
}

FString FWLCampaignBuildingSlotRules::GetInitialBuildingId(
	const FWLCampaignBuildingSlotCatalog& Catalog,
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel,
	int32 SlotIndex)
{
	if (IsSlotLocked(Catalog, Context, SlotLabel))
	{
		return FString();
	}
	if (const FWLCampaignBuildingSlotDefinition* Slot = Catalog.FindSlotDefinition(Context, SlotLabel))
	{
		if (!Slot->InitialBuildingId.IsEmpty())
		{
			return Slot->InitialBuildingId;
		}
		if (Slot->DefaultState.Equals(TEXT("occupied"), ESearchCase::IgnoreCase))
		{
			return Context == EWLCampaignBuildingPanelContext::City
				? TEXT("city_civic_hall")
				: TEXT("province_regional_administration");
		}
	}
	if (SlotIndex == 0)
	{
		return Context == EWLCampaignBuildingPanelContext::City
			? TEXT("city_civic_hall")
			: TEXT("province_regional_administration");
	}
	return FString();
}

bool FWLCampaignBuildingSlotRules::IsBuildingCompatible(
	const FWLCampaignBuildingDefinition& Building,
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel)
{
	if (!CampaignBuildingContextMatches(Building.Context, Context))
	{
		return false;
	}

	const FString NormalizedSlot = NormalizeSlotLabel(SlotLabel);
	for (const FString& CompatibleSlot : Building.CompatibleSlots)
	{
		if (NormalizeSlotLabel(CompatibleSlot) == NormalizedSlot)
		{
			return true;
		}
	}
	return false;
}

void FWLCampaignBuildingSlotRules::GetCompatibleBuildings(
	const FWLCampaignBuildingSlotCatalog& Catalog,
	EWLCampaignBuildingPanelContext Context,
	const FString& SlotLabel,
	TArray<FWLCampaignBuildingDefinition>& OutBuildings)
{
	OutBuildings.Reset();
	for (const FWLCampaignBuildingDefinition& Building : Catalog.Buildings)
	{
		if (IsBuildingCompatible(Building, Context, SlotLabel))
		{
			OutBuildings.Add(Building);
		}
	}
	OutBuildings.Sort([](const FWLCampaignBuildingDefinition& A, const FWLCampaignBuildingDefinition& B)
	{
		return A.Name < B.Name;
	});
}
