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
	CurrentDay = 1;
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
	CurrentDay = 1;
	ProvinceBuildings.Reset();
	RecruitQueues.Reset();
	GarrisonRecruited.Reset();
	TaxRates.Reset();
	PreviousGDP.Reset();
	GDPGrowth.Reset();
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
	UpdateGDPHistory();   // FE1.5: mide el PIB tras aplicar el mes

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

void UWLStrategicTickSubsystem::AdvanceDay()
{
	const FWLBalanceRules Rules = GetBalanceRules();

	// Un "avanzar dia" (accion clara del jugador) corre el tick de economia, provincias y reclutamiento cada
	// dia (progreso visible por dias). La IA economica corre al cerrar el MES (no cada dia -> no sobre-construye).
	ApplyMonthlyEconomy();
	ApplyMonthlyProvinceState();
	AdvanceRecruitment();
	UpdateGDPHistory();   // FE1.5: crecimiento medido entre ticks economicos

	bool bMonthRolled = false;
	if (++CurrentDay > 30)
	{
		CurrentDay = 1;
		bMonthRolled = true;
		if (++CurrentMonth > Rules.MonthsPerYear)
		{
			CurrentMonth = 1;
			++CurrentYear;
		}
	}

	if (bMonthRolled)
	{
		LastEconomicAIReports.Reset();
		const FString PlayerNationIso = GetActivePlayerNationIsoForAI();
		if (!PlayerNationIso.IsEmpty())
		{
			RunEconomicAIInternal(PlayerNationIso, LastEconomicAIReports);
		}
	}

	UE_LOG(LogWorldLeader, Log, TEXT("Avanzar dia -> %02d/%02d/%d"), CurrentDay, CurrentMonth, CurrentYear);
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
		NextOrder -= GetTaxPublicOrderPressure(ControllerIso);   // FE1.2: impuestos altos drenan orden, bajos lo recuperan

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
	// FE1.3: el presupuesto por categorias es la unica fuente de verdad del balance mensual.
	return GetNationBudget(NationIso).Net();
}

FWLNationBudget UWLStrategicTickSubsystem::GetNationBudget(const FString& NationIso) const
{
	FWLNationBudget Budget;
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return Budget;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	const FString NormalizedIso = NormalizeIso(NationIso);
	int64 NationPopulation = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) != NormalizedIso)
		{
			continue;
		}

		int64 TaxIncome = 0;
		const int64 ProvinceIncome = GetProvinceMonthlyIncomeSplit(Province.Id, TaxIncome);
		Budget.TaxIncome += TaxIncome;
		Budget.ResourceIncome += ProvinceIncome - TaxIncome;
		Budget.InfrastructureUpkeep += GetProvinceMonthlyUpkeep(Province.Id);

		FWLProvinceRuntimeState State;
		NationPopulation += GetProvinceState(Province.Id, State)
			? FMath::Max<int64>(0, State.Population)
			: FMath::Max<int64>(0, Province.Population);
	}

	Budget.MilitaryUpkeep = GetNationMilitaryUpkeep(NormalizedIso);   // FE1.1
	// FE1.3: salarios publicos y gasto social escalan con la poblacion administrada.
	Budget.PublicWages = static_cast<int64>(
		FMath::RoundToDouble(static_cast<double>(NationPopulation) * Rules.PublicWagesPerCapita));
	Budget.SocialSpending = static_cast<int64>(
		FMath::RoundToDouble(static_cast<double>(NationPopulation) * Rules.SocialSpendingPerCapita));
	// FE1.4: la deuda (tesoro negativo) cobra interes mensual como gasto del presupuesto.
	if (const int64* Treasury = Treasuries.Find(NormalizedIso); Treasury && *Treasury < 0)
	{
		Budget.DebtInterest = static_cast<int64>(
			FMath::RoundToDouble(static_cast<double>(-*Treasury) * Rules.DebtMonthlyInterestRate));
	}
	return Budget;
}

int64 UWLStrategicTickSubsystem::GetCreditLimit(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	return static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(GetNationBudget(NationIso).TotalIncome()) * Rules.DebtCreditLimitIncomeMonths));
}

int64 UWLStrategicTickSubsystem::GetNationGDP(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	const FString NormalizedIso = NormalizeIso(NationIso);
	int64 GDP = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) != NormalizedIso)
		{
			continue;
		}

		FWLProvinceRuntimeState State;
		if (!GetProvinceState(Province.Id, State))
		{
			State.Population = FMath::Max<int64>(0, Province.Population);
			State.PublicOrder = Rules.InitialPublicOrder;
		}

		FWLProvinceData EffectiveProvince = Province;
		EffectiveProvince.Population = State.Population;

		// Produccion a precios de mercado (recursos/industria/edificios, sin la parte fiscal)...
		const int64 Production =
			UWLEconomyLibrary::CalculateProvinceIncomeWithRules(EffectiveProvince, Rules)
			- UWLEconomyLibrary::CalculateProvincePopulationTax(EffectiveProvince, Rules)
			+ GetProvinceBuildingIncome(Province.Id, Rules);
		// ...mas la actividad economica de la poblacion (consumo/servicios), todo modulado por orden publico.
		const int64 PopulationActivity = static_cast<int64>(
			FMath::RoundToDouble(static_cast<double>(State.Population) * Rules.GDPPerCapitaActivity));

		GDP += UWLEconomyLibrary::ApplyPublicOrderIncomeModifier(
			Production + PopulationActivity, State.PublicOrder, Rules);
	}
	return GDP;
}

double UWLStrategicTickSubsystem::GetNationGDPGrowth(const FString& NationIso) const
{
	const double* Found = GDPGrowth.Find(NormalizeIso(NationIso));
	return Found ? *Found : 0.0;
}

void UWLStrategicTickSubsystem::UpdateGDPHistory()
{
	for (const TPair<FString, int64>& Pair : Treasuries)
	{
		const int64 CurrentGDP = GetNationGDP(Pair.Key);
		if (const int64* Previous = PreviousGDP.Find(Pair.Key); Previous && *Previous > 0)
		{
			GDPGrowth.Add(Pair.Key,
				static_cast<double>(CurrentGDP - *Previous) / static_cast<double>(*Previous));
		}
		PreviousGDP.Add(Pair.Key, CurrentGDP);
	}
}

int32 UWLStrategicTickSubsystem::GetTaxRate(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	const int32* Found = TaxRates.Find(NormalizeIso(NationIso));
	return FMath::Clamp(Found ? *Found : Rules.TaxRateDefaultPercent, Rules.TaxRateMinPercent, Rules.TaxRateMaxPercent);
}

int32 UWLStrategicTickSubsystem::SetTaxRate(const FString& NationIso, int32 RatePercent)
{
	const FWLBalanceRules Rules = GetBalanceRules();
	const int32 Clamped = FMath::Clamp(RatePercent, Rules.TaxRateMinPercent, Rules.TaxRateMaxPercent);
	TaxRates.Add(NormalizeIso(NationIso), Clamped);
	return Clamped;
}

int32 UWLStrategicTickSubsystem::GetTaxPublicOrderPressure(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	const int32 Delta = GetTaxRate(NationIso) - Rules.TaxRateDefaultPercent;
	return FMath::RoundToInt(static_cast<double>(Delta) * Rules.TaxPublicOrderPerPointPerMonth);
}

// FE1.1: nacion a la que se imputa la guarnicion de una base. "CO-FORCE-..." -> CO; "BO-CO-VE-GUAJIRA" -> CO
// (primer token de nacion del teatro). Aproximacion suficiente para el upkeep de tropa reclutada.
static FString MilitaryUpkeepNationFromBaseId(const FString& BaseId)
{
	TArray<FString> Parts;
	BaseId.ParseIntoArray(Parts, TEXT("-"), true);
	for (const FString& Part : Parts)
	{
		const FString Upper = Part.ToUpper();
		if (Upper == TEXT("CO") || Upper == TEXT("VE"))
		{
			return Upper;
		}
	}
	return Parts.Num() > 0 ? Parts[0].ToUpper() : FString();
}

int64 UWLStrategicTickSubsystem::GetNationMilitaryStrength(const FString& NationIso) const
{
	EnsureMilitaryCatalog();
	const FString Iso = NormalizeIso(NationIso);
	int64 Strength = 0;
	if (const int64* Preplaced = PreplacedMilitaryStrength.Find(Iso))
	{
		Strength += *Preplaced;
	}
	// Guarniciones reclutadas: cada unidad producida aporta ~100 de fuerza (proxy hasta que F1.4 enlace
	// ejercito<->nacion con precision).
	for (const TPair<FString, TMap<FString, int32>>& Base : GarrisonRecruited)
	{
		if (MilitaryUpkeepNationFromBaseId(Base.Key) != Iso)
		{
			continue;
		}
		int64 Units = 0;
		for (const TPair<FString, int32>& Unit : Base.Value)
		{
			Units += FMath::Max(0, Unit.Value);
		}
		Strength += Units * 100;
	}
	return Strength;
}

int64 UWLStrategicTickSubsystem::GetNationMilitaryUpkeep(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	return static_cast<int64>(static_cast<double>(GetNationMilitaryStrength(NationIso)) * Rules.MilitaryUpkeepPerStrength);
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
	int64 UnusedTaxIncome = 0;
	return GetProvinceMonthlyIncomeSplit(ProvinceId, UnusedTaxIncome);
}

int64 UWLStrategicTickSubsystem::GetProvinceMonthlyIncomeSplit(const FString& ProvinceId, int64& OutTaxIncome) const
{
	OutTaxIncome = 0;
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

	// FE1.2: la parte de impuestos del ingreso escala con la tasa de la nacion controladora (Laffer).
	const FString ControllerIso = State.ControllerIso.IsEmpty() ? Province.CountryIso : State.ControllerIso;
	const double TaxMultiplier = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(GetTaxRate(ControllerIso), Rules);
	const int64 BasePopulationTax = UWLEconomyLibrary::CalculateProvincePopulationTax(EffectiveProvince, Rules);
	const int64 GrossTax = static_cast<int64>(
		FMath::RoundToDouble(static_cast<double>(BasePopulationTax) * TaxMultiplier));

	const int64 GrossIncome =
		UWLEconomyLibrary::CalculateProvinceIncomeWithRules(EffectiveProvince, Rules)
		- BasePopulationTax
		+ GrossTax
		+ GetProvinceBuildingIncome(Province.Id, Rules);

	const int64 NetIncome = UWLEconomyLibrary::ApplyPublicOrderIncomeModifier(GrossIncome, State.PublicOrder, Rules);

	// FE1.3: desglose exacto — la parte de impuestos escala con el mismo modificador de orden publico
	// y el resto (recursos/produccion) absorbe el redondeo para que las partes sumen el total.
	if (GrossIncome > 0 && GrossTax > 0)
	{
		OutTaxIncome = FMath::Clamp<int64>(static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(NetIncome) * static_cast<double>(GrossTax) / static_cast<double>(GrossIncome))),
			0, NetIncome);
	}
	return NetIncome;
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
