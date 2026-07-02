// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLStrategicTickSubsystemPrivate.h"

#include "Campaign/WLDataRegistry.h"

using WLStrategicTickPrivate::ClampPublicOrder;
using WLStrategicTickPrivate::NormalizeIso;
using WLStrategicTickPrivate::ProvinceSupportsBuilding;

void UWLStrategicTickSubsystem::WriteSaveSnapshot(
	int32& OutYear,
	int32& OutMonth,
	TArray<FWLNationTreasurySave>& OutTreasuries,
	TArray<FWLProvinceBuildingsSave>& OutProvinceBuildings,
	TArray<FWLProvinceRuntimeState>& OutProvinceStates) const
{
	OutYear = CurrentYear;
	OutMonth = CurrentMonth;

	OutTreasuries.Reset();
	for (const TPair<FString, int64>& Pair : Treasuries)
	{
		FWLNationTreasurySave SavedTreasury;
		SavedTreasury.NationIso = Pair.Key;
		SavedTreasury.Treasury = Pair.Value;
		if (const int32* TaxRate = TaxRates.Find(Pair.Key))
		{
			SavedTreasury.TaxRatePercent = *TaxRate;   // FE1.2: -1 se mantiene si nunca se ajusto
		}
		OutTreasuries.Add(SavedTreasury);
	}
	OutTreasuries.Sort([](const FWLNationTreasurySave& A, const FWLNationTreasurySave& B)
	{
		return A.NationIso < B.NationIso;
	});

	OutProvinceBuildings.Reset();
	for (const TPair<FString, TArray<FString>>& Pair : ProvinceBuildings)
	{
		if (Pair.Value.IsEmpty())
		{
			continue;
		}

		FWLProvinceBuildingsSave SavedBuildings;
		SavedBuildings.ProvinceId = Pair.Key;
		SavedBuildings.BuildingIds = Pair.Value;
		SavedBuildings.BuildingIds.Sort();
		OutProvinceBuildings.Add(SavedBuildings);
	}
	OutProvinceBuildings.Sort([](const FWLProvinceBuildingsSave& A, const FWLProvinceBuildingsSave& B)
	{
		return A.ProvinceId < B.ProvinceId;
	});

	OutProvinceStates.Reset();
	for (const TPair<FString, FWLProvinceRuntimeState>& Pair : ProvinceStates)
	{
		FWLProvinceRuntimeState SavedState = Pair.Value;
		SavedState.ProvinceId = Pair.Key;
		if (SavedState.ControllerIso.IsEmpty())
		{
			FWLProvinceData Province;
			if (const UWLDataRegistry* Registry = GetDataRegistry(); Registry && Registry->GetProvince(Pair.Key, Province))
			{
				SavedState.ControllerIso = Province.CountryIso;
			}
		}
		else
		{
			SavedState.ControllerIso = NormalizeIso(SavedState.ControllerIso);
		}
		SavedState.Population = FMath::Max<int64>(0, SavedState.Population);
		SavedState.PublicOrder = ClampPublicOrder(SavedState.PublicOrder);
		OutProvinceStates.Add(SavedState);
	}
	OutProvinceStates.Sort([](const FWLProvinceRuntimeState& A, const FWLProvinceRuntimeState& B)
	{
		return A.ProvinceId < B.ProvinceId;
	});
}

bool UWLStrategicTickSubsystem::RestoreSaveSnapshot(
	int32 SavedYear,
	int32 SavedMonth,
	const TArray<FWLNationTreasurySave>& SavedTreasuries,
	const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
	const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
	FString& OutMessage)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	if (SavedYear <= 0 || SavedMonth < 1 || SavedMonth > Rules.MonthsPerYear)
	{
		OutMessage = FString::Printf(TEXT("Fecha invalida en save: %02d/%d"), SavedMonth, SavedYear);
		return false;
	}

	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}

	CurrentYear = SavedYear;
	CurrentMonth = SavedMonth;
	ProvinceBuildings.Reset();
	TaxRates.Reset();
	PreviousGDP.Reset();   // FE1.5: el crecimiento se vuelve a medir tras cargar
	GDPGrowth.Reset();
	InitTreasuriesFromData();
	InitProvinceStatesFromData();

	int32 RestoredTreasuries = 0;
	for (const FWLNationTreasurySave& SavedTreasury : SavedTreasuries)
	{
		FWLNationData Nation;
		if (Registry->GetNation(SavedTreasury.NationIso, Nation))
		{
			Treasuries.FindOrAdd(Nation.Iso) = SavedTreasury.Treasury;
			if (SavedTreasury.TaxRatePercent >= 0)
			{
				SetTaxRate(Nation.Iso, SavedTreasury.TaxRatePercent);   // FE1.2
			}
			++RestoredTreasuries;
		}
	}

	int32 RestoredBuildings = 0;
	for (const FWLProvinceBuildingsSave& SavedProvinceEntry : SavedProvinceBuildings)
	{
		FWLProvinceData Province;
		if (!Registry->GetProvince(SavedProvinceEntry.ProvinceId, Province))
		{
			continue;
		}

		TArray<FString> ValidBuildings;
		for (const FString& BuildingId : SavedProvinceEntry.BuildingIds)
		{
			FWLBuildingData Building;
			if (Registry->GetBuilding(BuildingId, Building)
				&& ProvinceSupportsBuilding(Province, Building)
				&& !ValidBuildings.Contains(Building.Id))
			{
				ValidBuildings.Add(Building.Id);
				++RestoredBuildings;
			}
		}
		if (!ValidBuildings.IsEmpty())
		{
			ValidBuildings.Sort();
			ProvinceBuildings.Add(Province.Id, MoveTemp(ValidBuildings));
		}
	}

	int32 RestoredProvinceStates = 0;
	for (const FWLProvinceRuntimeState& SavedState : SavedProvinceStates)
	{
		FWLProvinceData Province;
		if (Registry->GetProvince(SavedState.ProvinceId, Province))
		{
			FWLProvinceRuntimeState RuntimeState;
			RuntimeState.ProvinceId = Province.Id;
			RuntimeState.ControllerIso = SavedState.ControllerIso.IsEmpty()
				? Province.CountryIso
				: NormalizeIso(SavedState.ControllerIso);
			RuntimeState.Population = FMath::Max<int64>(0, SavedState.Population);
			RuntimeState.PublicOrder = ClampPublicOrder(SavedState.PublicOrder);
			FWLNationData Controller;
			if (Registry->GetNation(RuntimeState.ControllerIso, Controller))
			{
				ProvinceStates.FindOrAdd(Province.Id) = RuntimeState;
				++RestoredProvinceStates;
			}
		}
	}

	OnMonthAdvanced.Broadcast(CurrentYear, CurrentMonth);
	OutMessage = FString::Printf(TEXT("Save restaurado: %02d/%d, %d tesoros, %d edificios, %d estados de provincia."),
		CurrentMonth, CurrentYear, RestoredTreasuries, RestoredBuildings, RestoredProvinceStates);
	return true;
}
