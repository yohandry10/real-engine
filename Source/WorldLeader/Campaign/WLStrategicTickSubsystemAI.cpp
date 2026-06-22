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
		while (BuiltForNation < Rules.EconomicAIMaxBuildsPerNationPerMonth)
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

			FString BuildMessage;
			if (!BuildBuilding(ProvinceId, BuildingId, BuildMessage))
			{
				OutReports.Add(FString::Printf(TEXT("%s no pudo construir %s en %s: %s"),
					*Nation.Iso, *BuildingId, *ProvinceId, *BuildMessage));
				break;
			}

			OutReports.Add(FString::Printf(TEXT("%s construye %s en %s (+%lld/mes, retorno %lld meses, coste %lld)."),
				*Nation.Iso, *BuildingId, *ProvinceId, MonthlyGain, PaybackMonths, Cost));
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
	if (Treasury <= Rules.EconomicAIMinTreasuryReserve)
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
		if (State.PublicOrder < Rules.EconomicAIMinPublicOrderToBuild)
		{
			continue;
		}

		const TArray<FString> Built = GetProvinceBuildings(Province.Id);
		for (const FWLBuildingData& Building : Buildings)
		{
			if (Building.Cost < 0
				|| Built.Contains(Building.Id)
				|| Treasury - Building.Cost < Rules.EconomicAIMinTreasuryReserve
				|| !ProvinceSupportsBuilding(Province, Building))
			{
				continue;
			}

			const int64 GrossGain = UWLEconomyLibrary::CalculateBuildingIncomeWithRules(Building, Rules);
			const int64 MonthlyGain = UWLEconomyLibrary::ApplyPublicOrderIncomeModifier(GrossGain, State.PublicOrder, Rules);
			if (MonthlyGain <= 0)
			{
				continue;
			}

			const int64 PaybackMonths = Building.Cost <= 0
				? 0
				: (Building.Cost + MonthlyGain - 1) / MonthlyGain;
			if (Rules.EconomicAIMaxPaybackMonths > 0 && PaybackMonths > Rules.EconomicAIMaxPaybackMonths)
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
					&& Province.StrategicValue == BestStrategicValue && Building.Cost < BestCost)
				|| (PaybackMonths == BestPaybackMonths && MonthlyGain == BestMonthlyGain && Fit == BestFit
					&& Province.StrategicValue == BestStrategicValue && Building.Cost == BestCost
					&& (Province.Id < BestProvinceId
						|| (Province.Id == BestProvinceId && Building.Id < BestBuildingId)));

			if (bBetter)
			{
				bFound = true;
				BestPaybackMonths = PaybackMonths;
				BestMonthlyGain = MonthlyGain;
				BestFit = Fit;
				BestStrategicValue = Province.StrategicValue;
				BestCost = Building.Cost;
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
