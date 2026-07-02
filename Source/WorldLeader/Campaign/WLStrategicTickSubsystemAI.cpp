// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLStrategicTickSubsystemPrivate.h"

#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Economy/WLEconomyLibrary.h"

using WLStrategicTickPrivate::CalculateProvinceBuildingFit;
using WLStrategicTickPrivate::NormalizeIso;
using WLStrategicTickPrivate::ProvinceSupportsBuilding;

int32 UWLStrategicTickSubsystem::RunEconomicAI(const FString& PlayerNationIso, TArray<FString>& OutReports)
{
	LastEconomicAIReports.Reset();
	const int32 BuiltCount = RunEconomicAIInternal(PlayerNationIso, LastEconomicAIReports);
	OutReports = LastEconomicAIReports;
	return BuiltCount;
}

int32 UWLStrategicTickSubsystem::RunEconomicAIInternal(const FString& PlayerNationIso, TArray<FString>& OutReports)
{
	OutReports.Reset();

	const FWLBalanceRules Rules = GetBalanceRules();
	if (!Rules.bEnableEconomicAI || Rules.EconomicAIMaxBuildsPerNationPerMonth <= 0)
	{
		return 0;
	}
	const int32 MaxBuildsPerNation = Rules.GetEconomicAIMaxBuildsForDifficulty();

	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}

	const FString ExcludedIso = NormalizeIso(PlayerNationIso);
	TArray<FWLNationData> Nations = Registry->GetAllNations();
	Nations.Sort([](const FWLNationData& A, const FWLNationData& B)
	{
		return A.Iso < B.Iso;
	});

	int32 TotalBuilt = 0;
	for (const FWLNationData& Nation : Nations)
	{
		if (!ExcludedIso.IsEmpty() && Nation.Iso == ExcludedIso)
		{
			continue;
		}

		int32 BuiltForNation = 0;
		while (BuiltForNation < MaxBuildsPerNation)
		{
			FString ProvinceId;
			FString BuildingId;
			int64 MonthlyGain = 0;
			int64 Cost = 0;
			int64 PaybackMonths = 0;
			if (!FindBestEconomicAIBuildCandidate(
				Nation.Iso,
				ProvinceId,
				BuildingId,
				MonthlyGain,
				Cost,
				PaybackMonths))
			{
				break;
			}

			const bool bUpgrade = GetProvinceBuildingLevel(ProvinceId, BuildingId) > 0;
			FString BuildMessage;
			const bool bApplied = bUpgrade
				? UpgradeBuilding(ProvinceId, BuildingId, BuildMessage)
				: BuildBuilding(ProvinceId, BuildingId, BuildMessage);
			if (!bApplied)
			{
				OutReports.Add(FString::Printf(TEXT("%s no pudo %s %s en %s: %s"),
					*Nation.Iso, bUpgrade ? TEXT("mejorar") : TEXT("construir"),
					*BuildingId, *ProvinceId, *BuildMessage));
				break;
			}

			OutReports.Add(FString::Printf(TEXT("%s %s %s en %s (+%lld/mes, retorno %lld meses, coste %lld)."),
				*Nation.Iso, bUpgrade ? TEXT("mejora") : TEXT("construye"),
				*BuildingId, *ProvinceId, MonthlyGain, PaybackMonths, Cost));
			++BuiltForNation;
			++TotalBuilt;
		}
	}

	return TotalBuilt;
}

bool UWLStrategicTickSubsystem::FindBestEconomicAIBuildCandidate(
	const FString& NationIso,
	FString& OutProvinceId,
	FString& OutBuildingId,
	int64& OutMonthlyGain,
	int64& OutCost,
	int64& OutPaybackMonths) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return false;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	const FString NormalizedIso = NormalizeIso(NationIso);
	const int64 Treasury = GetTreasury(NormalizedIso);
	const int64 MinTreasuryReserve = Rules.GetEconomicAIMinTreasuryReserveForDifficulty();
	if (Treasury <= MinTreasuryReserve)
	{
		return false;
	}

	TArray<FWLProvinceData> Provinces = Registry->GetAllProvinces();
	Provinces.Sort([](const FWLProvinceData& A, const FWLProvinceData& B)
	{
		return A.Id < B.Id;
	});

	TArray<FWLBuildingData> Buildings = Registry->GetAllBuildings();
	Buildings.Sort([](const FWLBuildingData& A, const FWLBuildingData& B)
	{
		return A.Id < B.Id;
	});

	bool bFound = false;
	int64 BestPaybackMonths = TNumericLimits<int64>::Max();
	int64 BestMonthlyGain = TNumericLimits<int64>::Lowest();
	int64 BestFit = TNumericLimits<int64>::Lowest();
	int32 BestStrategicValue = TNumericLimits<int32>::Lowest();
	int64 BestCost = TNumericLimits<int64>::Max();
	FString BestProvinceId;
	FString BestBuildingId;

	for (const FWLProvinceData& Province : Provinces)
	{
		if (GetProvinceControllerIso(Province.Id) != NormalizedIso)
		{
			continue;
		}

		FWLProvinceRuntimeState State;
		if (!GetProvinceState(Province.Id, State))
		{
			State.ProvinceId = Province.Id;
			State.ControllerIso = Province.CountryIso;
			State.Population = FMath::Max<int64>(0, Province.Population);
			State.PublicOrder = Rules.InitialPublicOrder;
		}
		if (State.PublicOrder < Rules.GetEconomicAIMinPublicOrderToBuildForDifficulty())
		{
			continue;
		}

		const TArray<FString> Built = GetProvinceBuildings(Province.Id);
		TSet<EWLBuildingSlot> OccupiedSlots;
		for (const FString& BuiltId : Built)
		{
			FWLBuildingData BuiltBuilding;
			if (Registry->GetBuilding(BuiltId, BuiltBuilding))
			{
				OccupiedSlots.Add(BuiltBuilding.Slot);
			}
		}
		for (const FWLBuildingData& Building : Buildings)
		{
			if (Building.Cost < 0 || !ProvinceSupportsBuilding(Province, Building))
			{
				continue;
			}

			const int32 CurrentLevel = GetProvinceBuildingLevel(Province.Id, Building.Id);
			const bool bUpgrade = CurrentLevel > 0;
			if (!bUpgrade && OccupiedSlots.Contains(Building.Slot))
			{
				continue;
			}
			if (bUpgrade && CurrentLevel >= FMath::Clamp(Building.MaxLevel, 1, 5))
			{
				continue;
			}

			const int64 Cost = bUpgrade
				? GetProvinceBuildingUpgradeCost(Province.Id, Building.Id)
				: Building.Cost;
			if (Cost <= 0 || Treasury - Cost < MinTreasuryReserve)
			{
				continue;
			}

			const int64 CurrentIncome = bUpgrade
				? UWLEconomyLibrary::CalculateBuildingIncomeForLevel(Building, Rules, CurrentLevel)
				: 0;
			const int64 NextIncome = UWLEconomyLibrary::CalculateBuildingIncomeForLevel(
				Building,
				Rules,
				bUpgrade ? CurrentLevel + 1 : 1);
			const int64 GrossGain = FMath::Max<int64>(0, NextIncome - CurrentIncome);
			const int64 MonthlyGain = UWLEconomyLibrary::ApplyPublicOrderIncomeModifier(GrossGain, State.PublicOrder, Rules);
			if (MonthlyGain <= 0)
			{
				continue;
			}

			const int64 PaybackMonths = Cost <= 0
				? 0
				: (Cost + MonthlyGain - 1) / MonthlyGain;
			const int32 MaxPaybackMonths = Rules.GetEconomicAIMaxPaybackMonthsForDifficulty();
			if (MaxPaybackMonths > 0 && PaybackMonths > MaxPaybackMonths)
			{
				continue;
			}

			const int64 Fit = CalculateProvinceBuildingFit(Province, Building);
			const bool bBetter =
				!bFound
				|| PaybackMonths < BestPaybackMonths
				|| (PaybackMonths == BestPaybackMonths && MonthlyGain > BestMonthlyGain)
				|| (PaybackMonths == BestPaybackMonths && MonthlyGain == BestMonthlyGain && Fit > BestFit)
				|| (PaybackMonths == BestPaybackMonths && MonthlyGain == BestMonthlyGain && Fit == BestFit
					&& Province.StrategicValue > BestStrategicValue)
				|| (PaybackMonths == BestPaybackMonths && MonthlyGain == BestMonthlyGain && Fit == BestFit
					&& Province.StrategicValue == BestStrategicValue && Cost < BestCost)
				|| (PaybackMonths == BestPaybackMonths && MonthlyGain == BestMonthlyGain && Fit == BestFit
					&& Province.StrategicValue == BestStrategicValue && Cost == BestCost
					&& (Province.Id < BestProvinceId
						|| (Province.Id == BestProvinceId && Building.Id < BestBuildingId)));

			if (bBetter)
			{
				bFound = true;
				BestPaybackMonths = PaybackMonths;
				BestMonthlyGain = MonthlyGain;
				BestFit = Fit;
				BestStrategicValue = Province.StrategicValue;
				BestCost = Cost;
				BestProvinceId = Province.Id;
				BestBuildingId = Building.Id;
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	OutProvinceId = BestProvinceId;
	OutBuildingId = BestBuildingId;
	OutMonthlyGain = BestMonthlyGain;
	OutCost = BestCost;
	OutPaybackMonths = BestPaybackMonths;
	return true;
}

FString UWLStrategicTickSubsystem::GetActivePlayerNationIsoForAI() const
{
	const UWLCampaignGameInstance* CampaignGI = Cast<UWLCampaignGameInstance>(GetGameInstance());
	return (CampaignGI && CampaignGI->HasActiveCampaign())
		? NormalizeIso(CampaignGI->GetSelectedNationIso())
		: FString();
}
