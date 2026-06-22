// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "Campaign/WLCampaignPlayerController.h"
#include "UI/WLCampaignBuildingSlotData.h"
#include "UI/WLCampaignSelectionPanelData.h"

namespace WLCampaignPlayerControllerPrivate
{
	inline const FWLCampaignSelectionPanelData& GetCampaignControllerPanelData()
	{
		static FWLCampaignSelectionPanelData Data;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			FWLCampaignSelectionPanelDataLoader::Load(Data);
			bLoaded = true;
		}
		return Data;
	}

	inline const FWLCampaignBuildingSlotCatalog& GetCampaignControllerBuildingCatalog()
	{
		static FWLCampaignBuildingSlotCatalog Catalog;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			FWLCampaignBuildingSlotDataLoader::Load(Catalog);
			bLoaded = true;
		}
		return Catalog;
	}

	inline TArray<FString> GetCampaignControllerPanelSlots(const AWLCampaignPlayerController* PC)
	{
		TArray<FString> Slots;
		if (!PC || !PC->HasCampaignSelectionPanel())
		{
			return Slots;
		}

		const FWLCampaignSelectionPanelData& PanelData = GetCampaignControllerPanelData();
		const FString SelectionId = PC->GetCampaignSelectionId();
		const bool bCityMode = PC->GetCampaignSelectionKind() == EWLCampaignSelectionKind::City;
		if (bCityMode)
		{
			if (const FWLCampaignSelectionPanelEntry* City = PanelData.Cities.Find(SelectionId))
			{
				Slots = City->UrbanSlots;
			}
		}
		else
		{
			if (const FWLCampaignSelectionPanelEntry* Province = PanelData.Provinces.Find(SelectionId))
			{
				Slots = Province->BuildingSlots;
			}
		}

		if (Slots.IsEmpty())
		{
			Slots = bCityMode
				? TArray<FString>{ TEXT("Government"), TEXT("Services"), TEXT("Industry"), TEXT("Security") }
				: TArray<FString>{ TEXT("Infrastructure"), TEXT("Logistics"), TEXT("Security"), TEXT("Industry") };
		}
		return Slots;
	}

	inline float GetCampaignControllerPanelSlotStartY(float PanelY)
	{
		return PanelY + 383.f;
	}

	inline float GetCampaignControllerPanelDetailsY(float PanelY, int32 SlotCount)
	{
		const int32 SlotRows = FMath::Max(1, FMath::DivideAndRoundUp(FMath::Clamp(SlotCount, 1, 6), 2));
		return GetCampaignControllerPanelSlotStartY(PanelY) + static_cast<float>(SlotRows) * 56.f + 16.f;
	}

	inline float GetCampaignControllerForceActionStartY(float PanelY)
	{
		return PanelY + 412.f;
	}

	inline bool IsPointInControllerRect(float MouseX, float MouseY, float X, float Y, float W, float H)
	{
		return MouseX >= X && MouseX <= X + W && MouseY >= Y && MouseY <= Y + H;
	}
}
