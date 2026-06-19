// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Economy/WLEconomyLibrary.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

namespace
{
	FString NormalizeIso(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	int32 ClampPublicOrder(int32 Value)
	{
		return FMath::Clamp(Value, 0, 100);
	}

	int32 StepToward(int32 Value, int32 Target, int32 Step)
	{
		if (Step <= 0 || Value == Target)
		{
			return Value;
		}
		return Value < Target
			? FMath::Min(Value + Step, Target)
			: FMath::Max(Value - Step, Target);
	}

	bool ProvinceSupportsBuilding(const FWLProvinceData& Province, const FWLBuildingData& Building)
	{
		bool bRequiresResourceFit = false;
		bool bHasFit = false;

		auto TestFit = [&bRequiresResourceFit, &bHasFit](int32 ProvinceResource, int32 BuildingBonus)
		{
			if (BuildingBonus > 0)
			{
				bRequiresResourceFit = true;
				bHasFit = bHasFit || ProvinceResource > 0;
			}
		};

		TestFit(Province.BaseOil, Building.BonusOil);
		TestFit(Province.BaseGas, Building.BonusGas);
		TestFit(Province.BaseFood, Building.BonusFood);
		TestFit(Province.BaseMinerals, Building.BonusMinerals);
		TestFit(Province.BaseIndustry, Building.BonusIndustry);

		return !bRequiresResourceFit || bHasFit;
	}

	int64 CalculateProvinceBuildingFit(const FWLProvinceData& Province, const FWLBuildingData& Building)
	{
		return   static_cast<int64>(Province.BaseOil)      * Building.BonusOil
		       + static_cast<int64>(Province.BaseGas)      * Building.BonusGas
		       + static_cast<int64>(Province.BaseFood)     * Building.BonusFood
		       + static_cast<int64>(Province.BaseMinerals) * Building.BonusMinerals
		       + static_cast<int64>(Province.BaseIndustry) * Building.BonusIndustry;
	}
}

void UWLStrategicTickSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Asegura que el registro de datos exista antes de inicializar tesoros.
	Collection.InitializeDependency(UWLDataRegistry::StaticClass());
	Collection.InitializeDependency(UWLBalanceSubsystem::StaticClass());

	const FWLBalanceRules Rules = GetBalanceRules();
	CurrentYear = Rules.StartYear;
	CurrentMonth = Rules.StartMonth;
	InitTreasuriesFromData();
	InitProvinceStatesFromData();
}

UWLDataRegistry* UWLStrategicTickSubsystem::GetDataRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLBalanceSubsystem* UWLStrategicTickSubsystem::GetBalanceSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLBalanceSubsystem>() : nullptr;
}

void UWLStrategicTickSubsystem::InitTreasuriesFromData()
{
	Treasuries.Reset();
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry) return;

	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		Treasuries.Add(Nation.Iso, Nation.StartingTreasury);
	}
}

void UWLStrategicTickSubsystem::InitProvinceStatesFromData()
{
	ProvinceStates.Reset();
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry) return;

	const FWLBalanceRules Rules = GetBalanceRules();
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		FWLProvinceRuntimeState State;
		State.ProvinceId = Province.Id;
		State.ControllerIso = Province.CountryIso;
		State.Population = FMath::Max<int64>(0, Province.Population);
		State.PublicOrder = Rules.InitialPublicOrder;
		ProvinceStates.Add(Province.Id, State);
	}
}

void UWLStrategicTickSubsystem::ResetCampaignState()
{
	const FWLBalanceRules Rules = GetBalanceRules();
	CurrentYear = Rules.StartYear;
	CurrentMonth = Rules.StartMonth;
	ProvinceBuildings.Reset();
	LastEconomicAIReports.Reset();
	InitTreasuriesFromData();
	InitProvinceStatesFromData();
	OnMonthAdvanced.Broadcast(CurrentYear, CurrentMonth);
}

void UWLStrategicTickSubsystem::AdvanceMonth()
{
	const FWLBalanceRules Rules = GetBalanceRules();
	if (++CurrentMonth > Rules.MonthsPerYear)
	{
		CurrentMonth = 1;
		++CurrentYear;
	}

	ApplyMonthlyEconomy();
	ApplyMonthlyProvinceState();

	LastEconomicAIReports.Reset();
	const FString PlayerNationIso = GetActivePlayerNationIsoForAI();
	if (!PlayerNationIso.IsEmpty())
	{
		RunEconomicAIInternal(PlayerNationIso, LastEconomicAIReports);
		for (const FString& Report : LastEconomicAIReports)
		{
			UE_LOG(LogWorldLeader, Log, TEXT("IA economica: %s"), *Report);
		}
	}

	UE_LOG(LogWorldLeader, Log, TEXT("Tick estrategico -> %02d/%d | IA economica: %d construcciones"),
		CurrentMonth, CurrentYear, LastEconomicAIReports.Num());
	OnMonthAdvanced.Broadcast(CurrentYear, CurrentMonth);
}

void UWLStrategicTickSubsystem::ApplyMonthlyEconomy()
{
	for (TPair<FString, int64>& Pair : Treasuries)
	{
		Pair.Value += GetMonthlyBalance(Pair.Key);
	}
}

void UWLStrategicTickSubsystem::ApplyMonthlyProvinceState()
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		FWLProvinceRuntimeState* State = ProvinceStates.Find(Province.Id);
		if (!State)
		{
			FWLProvinceRuntimeState NewState;
			NewState.ProvinceId = Province.Id;
			NewState.ControllerIso = Province.CountryIso;
			NewState.Population = FMath::Max<int64>(0, Province.Population);
			NewState.PublicOrder = Rules.InitialPublicOrder;
			ProvinceStates.Add(Province.Id, NewState);
			State = ProvinceStates.Find(Province.Id);
		}
		if (!State)
		{
			continue;
		}

		const int32 OrderBeforeDrift = ClampPublicOrder(State->PublicOrder);
		int32 NextOrder = StepToward(OrderBeforeDrift, Rules.PublicOrderNeutral, Rules.PublicOrderDriftPerMonth);

		if (GetProvinceMonthlyBalance(Province.Id) < 0)
		{
			NextOrder -= Rules.PublicOrderDeficitPenalty;
		}
		const FString ControllerIso = State->ControllerIso.IsEmpty() ? Province.CountryIso : NormalizeIso(State->ControllerIso);
		State->ControllerIso = ControllerIso;
		if (const int64* NationTreasury = Treasuries.Find(ControllerIso); NationTreasury && *NationTreasury < 0)
		{
			NextOrder -= Rules.PublicOrderBankruptcyPenalty;
		}

		State->PublicOrder = ClampPublicOrder(NextOrder);

		double GrowthRate = Rules.MonthlyPopulationGrowthRate;
		if (State->PublicOrder < 30)
		{
			GrowthRate *= -0.5;
		}
		else
		{
			GrowthRate *= FMath::Clamp(
				static_cast<double>(State->PublicOrder) / static_cast<double>(Rules.PublicOrderNeutral),
				0.0,
				1.5);
		}

		const int64 PopulationDelta = static_cast<int64>(
			FMath::RoundToDouble(static_cast<double>(State->Population) * GrowthRate));
		State->Population = FMath::Max<int64>(0, State->Population + PopulationDelta);
	}
}

int64 UWLStrategicTickSubsystem::GetMonthlyBalance(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry) return 0;

	const FString NormalizedIso = NormalizeIso(NationIso);
	int64 Balance = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) == NormalizedIso)
		{
			Balance += GetProvinceMonthlyBalance(Province.Id);
		}
	}
	return Balance;
}

bool UWLStrategicTickSubsystem::GetProvinceState(const FString& ProvinceId, FWLProvinceRuntimeState& OutState) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	const FString CanonicalProvinceId = (Registry && Registry->GetProvince(ProvinceId, Province))
		? Province.Id
		: ProvinceId.TrimStartAndEnd().ToUpper();

	if (const FWLProvinceRuntimeState* Found = ProvinceStates.Find(CanonicalProvinceId))
	{
		OutState = *Found;
		return true;
	}
	return false;
}

FString UWLStrategicTickSubsystem::GetProvinceControllerIso(const FString& ProvinceId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		return FString();
	}

	FWLProvinceRuntimeState State;
	if (GetProvinceState(Province.Id, State) && !State.ControllerIso.IsEmpty())
	{
		return NormalizeIso(State.ControllerIso);
	}
	return Province.CountryIso;
}

bool UWLStrategicTickSubsystem::SetProvinceController(
	const FString& ProvinceId,
	const FString& NewControllerIso,
	FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}

	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		OutMessage = FString::Printf(TEXT("Provincia desconocida: %s"), *ProvinceId);
		return false;
	}

	FWLNationData Controller;
	if (!Registry->GetNation(NewControllerIso, Controller))
	{
		OutMessage = FString::Printf(TEXT("Controlador desconocido: %s"), *NewControllerIso);
		return false;
	}

	FWLProvinceRuntimeState& State = ProvinceStates.FindOrAdd(Province.Id);
	if (State.ProvinceId.IsEmpty())
	{
		State.ProvinceId = Province.Id;
		State.Population = FMath::Max<int64>(0, Province.Population);
		State.PublicOrder = GetBalanceRules().InitialPublicOrder;
	}

	const FString PreviousController = State.ControllerIso.IsEmpty()
		? Province.CountryIso
		: NormalizeIso(State.ControllerIso);
	State.ControllerIso = Controller.Iso;

	if (PreviousController != Controller.Iso)
	{
		State.PublicOrder = ClampPublicOrder(State.PublicOrder - GetBalanceRules().OccupationPublicOrderPenalty);
	}

	OutMessage = FString::Printf(TEXT("%s ahora esta controlada por %s."), *Province.Id, *Controller.Iso);
	return true;
}

int64 UWLStrategicTickSubsystem::GetProvinceMonthlyIncome(const FString& ProvinceId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}

	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		return 0;
	}

	FWLProvinceRuntimeState State;
	if (!GetProvinceState(Province.Id, State))
	{
		State.ProvinceId = Province.Id;
		State.Population = FMath::Max<int64>(0, Province.Population);
		State.PublicOrder = GetBalanceRules().InitialPublicOrder;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	FWLProvinceData EffectiveProvince = Province;
	EffectiveProvince.Population = State.Population;

	const int64 GrossIncome =
		UWLEconomyLibrary::CalculateProvinceIncomeWithRules(EffectiveProvince, Rules)
		+ GetProvinceBuildingIncome(Province.Id, Rules);

	return UWLEconomyLibrary::ApplyPublicOrderIncomeModifier(GrossIncome, State.PublicOrder, Rules);
}

int64 UWLStrategicTickSubsystem::GetProvinceMonthlyUpkeep(const FString& ProvinceId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}

	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		return 0;
	}

	return UWLEconomyLibrary::CalculateProvinceUpkeepWithRules(Province, GetBalanceRules());
}

int64 UWLStrategicTickSubsystem::GetProvinceMonthlyBalance(const FString& ProvinceId) const
{
	return GetProvinceMonthlyIncome(ProvinceId) - GetProvinceMonthlyUpkeep(ProvinceId);
}

FWLBalanceRules UWLStrategicTickSubsystem::GetBalanceRules() const
{
	if (const UWLBalanceSubsystem* Balance = GetBalanceSubsystem())
	{
		return Balance->GetRules();
	}
	return FWLBalanceRules::Default();
}

int64 UWLStrategicTickSubsystem::GetTreasury(const FString& NationIso) const
{
	const int64* Found = Treasuries.Find(NormalizeIso(NationIso));
	return Found ? *Found : 0;
}

int64 UWLStrategicTickSubsystem::GetProvinceBuildingIncome(const FString& ProvinceId, const FWLBalanceRules& Rules) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}
	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		return 0;
	}

	const TArray<FString>* Built = ProvinceBuildings.Find(Province.Id);
	if (!Built)
	{
		return 0;
	}

	int64 Income = 0;
	for (const FString& BuildingId : *Built)
	{
		FWLBuildingData Building;
		if (Registry->GetBuilding(BuildingId, Building))
		{
			Income += UWLEconomyLibrary::CalculateBuildingIncomeWithRules(Building, Rules);
		}
	}
	return Income;
}

TArray<FString> UWLStrategicTickSubsystem::GetProvinceBuildings(const FString& ProvinceId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	const FString CanonicalProvinceId = (Registry && Registry->GetProvince(ProvinceId, Province))
		? Province.Id
		: ProvinceId.TrimStartAndEnd().ToUpper();

	if (const TArray<FString>* Built = ProvinceBuildings.Find(CanonicalProvinceId))
	{
		return *Built;
	}
	return TArray<FString>();
}

bool UWLStrategicTickSubsystem::IsBuildingSupportedInProvince(const FString& ProvinceId, const FString& BuildingId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return false;
	}

	FWLProvinceData Province;
	FWLBuildingData Building;
	return Registry->GetProvince(ProvinceId, Province)
		&& Registry->GetBuilding(BuildingId, Building)
		&& ProvinceSupportsBuilding(Province, Building);
}

bool UWLStrategicTickSubsystem::BuildBuilding(const FString& ProvinceId, const FString& BuildingId, FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}

	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		OutMessage = FString::Printf(TEXT("Provincia desconocida: %s"), *ProvinceId);
		return false;
	}

	FWLBuildingData Building;
	if (!Registry->GetBuilding(BuildingId, Building))
	{
		OutMessage = FString::Printf(TEXT("Edificio desconocido: %s"), *BuildingId);
		return false;
	}
	if (Building.Cost < 0)
	{
		OutMessage = FString::Printf(TEXT("Edificio con coste invalido: %s (%lld)"), *Building.Id, Building.Cost);
		return false;
	}

	const TArray<FString>* ExistingBuildings = ProvinceBuildings.Find(Province.Id);
	if (ExistingBuildings && ExistingBuildings->Contains(Building.Id))
	{
		OutMessage = FString::Printf(TEXT("%s ya existe en %s (%s)."),
			*Building.Name, *Province.Name, *Province.Id);
		return false;
	}

	if (!ProvinceSupportsBuilding(Province, Building))
	{
		OutMessage = FString::Printf(TEXT("%s no encaja economicamente en %s (%s)."),
			*Building.Name, *Province.Name, *Province.Id);
		return false;
	}

	const FString Nation = GetProvinceControllerIso(Province.Id);
	int64* Treasury = Treasuries.Find(Nation);
	if (!Treasury)
	{
		OutMessage = FString::Printf(TEXT("Nacion sin tesoro: %s"), *Nation);
		return false;
	}

	if (*Treasury < Building.Cost)
	{
		OutMessage = FString::Printf(TEXT("Fondos insuficientes en %s: cuesta %lld, tesoro %lld"),
			*Nation, Building.Cost, *Treasury);
		return false;
	}

	*Treasury -= Building.Cost;
	ProvinceBuildings.FindOrAdd(Province.Id).Add(Building.Id);

	OutMessage = FString::Printf(TEXT("%s construido en %s (%s). Coste %lld. Tesoro %s: %lld"),
		*Building.Name, *Province.Name, *Province.Id, Building.Cost, *Nation, *Treasury);
	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}

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
	InitTreasuriesFromData();
	InitProvinceStatesFromData();

	int32 RestoredTreasuries = 0;
	for (const FWLNationTreasurySave& SavedTreasury : SavedTreasuries)
	{
		FWLNationData Nation;
		if (Registry->GetNation(SavedTreasury.NationIso, Nation))
		{
			Treasuries.FindOrAdd(Nation.Iso) = SavedTreasury.Treasury;
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
