// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

struct FWLCampaignSelectionPanelEntry
{
	FString Id;
	FString Name;
	FString Country;
	FString CountryIso;
	FString TypeLabel;
	FString TerritoryId;
	FString TerritoryName;
	FString CapitalOrMainCity;
	FString Owner;
	FString Controller;
	FString Population;
	FString PublicOrder;
	FString Infrastructure;
	FString StrategicImportance;
	FString DetailLevel;
	FString PortStatus;
	TArray<FString> Resources;
	TArray<FString> Ports;
	TArray<FString> Cities;
	TArray<FString> BuildingSlots;
	TArray<FString> UrbanSlots;
	TArray<FString> DisabledActions;
};

struct FWLCampaignSelectionPanelData
{
	TMap<FString, FWLCampaignSelectionPanelEntry> Provinces;
	TMap<FString, FWLCampaignSelectionPanelEntry> Cities;
};

class FWLCampaignSelectionPanelDataLoader
{
public:
	static bool Load(FWLCampaignSelectionPanelData& OutData);
};
