// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

enum class EWLCampaignBuildingPanelContext : uint8
{
	Province,
	City
};

enum class EWLCampaignBuildingSlotState : uint8
{
	Empty,
	Occupied,
	Locked
};

struct FWLCampaignBuildingSlotDefinition
{
	FString Id;
	FString Context;
	FString Label;
	FString DefaultState;
	FString InitialBuildingId;
	FString Description;
	TArray<FString> Matches;
};

struct FWLCampaignBuildingDefinition
{
	FString Id;
	FString Name;
	FString Context;
	FString TypeLabel;
	FString Description;
	FString FutureCost;
	int32 Level = 1;
	bool bRequiresPort = false;
	TArray<FString> CompatibleSlots;
	TArray<FString> Effects;

	bool IsValid() const { return !Id.IsEmpty() && !Name.IsEmpty(); }
};

struct FWLCampaignBuildingSlotCatalog
{
	TArray<FWLCampaignBuildingSlotDefinition> Slots;
	TArray<FWLCampaignBuildingDefinition> Buildings;

	const FWLCampaignBuildingDefinition* FindBuilding(const FString& BuildingId) const;
	const FWLCampaignBuildingSlotDefinition* FindSlotDefinition(EWLCampaignBuildingPanelContext Context, const FString& SlotLabel) const;
};

class FWLCampaignBuildingSlotDataLoader
{
public:
	static bool Load(FWLCampaignBuildingSlotCatalog& OutCatalog);
};

class FWLCampaignBuildingSlotRules
{
public:
	static EWLCampaignBuildingPanelContext ContextFromCityMode(bool bCityMode);
	static FString ContextToText(EWLCampaignBuildingPanelContext Context);
	static FString NormalizeSlotLabel(const FString& SlotLabel);
	static FString MakeSlotKey(
		const FString& ObjectId,
		EWLCampaignBuildingPanelContext Context,
		const FString& SlotLabel,
		int32 SlotIndex);
	static FString GetSlotDisplayLabel(
		const FWLCampaignBuildingSlotCatalog& Catalog,
		EWLCampaignBuildingPanelContext Context,
		const FString& SlotLabel);
	static FString GetSlotDescription(
		const FWLCampaignBuildingSlotCatalog& Catalog,
		EWLCampaignBuildingPanelContext Context,
		const FString& SlotLabel);
	static bool IsSlotLocked(
		const FWLCampaignBuildingSlotCatalog& Catalog,
		EWLCampaignBuildingPanelContext Context,
		const FString& SlotLabel);
	static FString GetInitialBuildingId(
		const FWLCampaignBuildingSlotCatalog& Catalog,
		EWLCampaignBuildingPanelContext Context,
		const FString& SlotLabel,
		int32 SlotIndex);
	static bool IsBuildingCompatible(
		const FWLCampaignBuildingDefinition& Building,
		EWLCampaignBuildingPanelContext Context,
		const FString& SlotLabel);
	static void GetCompatibleBuildings(
		const FWLCampaignBuildingSlotCatalog& Catalog,
		EWLCampaignBuildingPanelContext Context,
		const FString& SlotLabel,
		TArray<FWLCampaignBuildingDefinition>& OutBuildings);
};
