// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLStrategicTickSubsystemPrivate.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Economy/WLEconomyLibrary.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

using WLStrategicTickPrivate::ClampPublicOrder;
using WLStrategicTickPrivate::NormalizeIso;
using WLStrategicTickPrivate::StepToward;

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
	RecruitQueues.Reset();
	GarrisonRecruited.Reset();
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
	AdvanceRecruitment();

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
