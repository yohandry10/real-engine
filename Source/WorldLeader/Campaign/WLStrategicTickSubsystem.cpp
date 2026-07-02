// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLStrategicTickSubsystemPrivate.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Economy/WLEconomyLibrary.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

using WLStrategicTickPrivate::ClampPublicOrder;
using WLStrategicTickPrivate::NormalizeIso;
using WLStrategicTickPrivate::ProvinceSupportsBuilding;
using WLStrategicTickPrivate::StepToward;

namespace
{
	void AddUnits(TMap<FString, int64>& Map, const FString& GoodId, int64 Units)
	{
		if (!GoodId.IsEmpty() && Units != 0)
		{
			Map.FindOrAdd(GoodId) += Units;
		}
	}

	int64 ReadUnits(const TMap<FString, int64>& Map, const FString& GoodId)
	{
		if (const int64* Found = Map.Find(GoodId))
		{
			return *Found;
		}
		return 0;
	}

	int64 RoundUnits(double Value)
	{
		return static_cast<int64>(FMath::RoundToDouble(FMath::Max(0.0, Value)));
	}

	FString NormalizeGoodId(const FString& GoodId)
	{
		const FString Normalized = GoodId.TrimStartAndEnd().ToLower();
		return (Normalized == TEXT("all") || Normalized == TEXT("*")) ? FString(TEXT("*")) : Normalized;
	}

	bool ShockTargetsGood(const FWLMarketShockState& Shock, const FString& NormalizedGoodId)
	{
		const FString ShockGoodId = NormalizeGoodId(Shock.GoodId);
		return ShockGoodId == TEXT("*") || ShockGoodId == NormalizedGoodId;
	}

	int32 ProvinceBaseForSource(const FWLProvinceData& Province, const FString& Source)
	{
		const FString S = Source.ToLower();
		if (S == TEXT("oil"))      return Province.BaseOil;
		if (S == TEXT("gas"))      return Province.BaseGas;
		if (S == TEXT("food"))     return Province.BaseFood;
		if (S == TEXT("minerals")) return Province.BaseMinerals;
		return 0;
	}

	void ApplyBuildingEffectsToProvince(FWLProvinceData& Province, const FWLProvinceBuildingEffects& Effects)
	{
		Province.BaseOil += FMath::Max(0, Effects.BonusOil);
		Province.BaseGas += FMath::Max(0, Effects.BonusGas);
		Province.BaseFood += FMath::Max(0, Effects.BonusFood);
		Province.BaseMinerals += FMath::Max(0, Effects.BonusMinerals);
		Province.BaseIndustry += FMath::Max(0, Effects.BonusIndustry);
		Province.Infrastructure += FMath::Max(0, Effects.BonusInfrastructure);
	}

	bool HasTrait(const FWLCharacter& Character, const FString& TraitId)
	{
		for (const FString& Trait : Character.Traits)
		{
			if (Trait.TrimStartAndEnd().ToLower() == TraitId)
			{
				return true;
			}
		}
		return false;
	}

	bool IsManufacturedGood(const UWLDataRegistry* Registry, const FString& GoodId)
	{
		FWLGoodData Good;
		return Registry && Registry->GetGood(GoodId, Good) && Good.Category == EWLGoodCategory::Manufactured;
	}

	TArray<FWLGoodOutput> MapToGoodOutputs(const TMap<FString, int64>& Map)
	{
		TArray<FWLGoodOutput> Output;
		for (const TPair<FString, int64>& Pair : Map)
		{
			if (Pair.Value <= 0)
			{
				continue;
			}
			FWLGoodOutput Out;
			Out.GoodId = Pair.Key;
			Out.Units = Pair.Value;
			Output.Add(MoveTemp(Out));
		}
		Output.Sort([](const FWLGoodOutput& A, const FWLGoodOutput& B)
		{
			return A.Units == B.Units ? A.GoodId < B.GoodId : A.Units > B.Units;
		});
		return Output;
	}

	double CalculatePriceMultiplier(int64 Demand, int64 Supply, const FWLBalanceRules& Rules)
	{
		if (Demand <= 0)
		{
			return 1.0;
		}
		if (Supply <= 0)
		{
			return Rules.MaxMarketPriceMultiplier;
		}

		const double Ratio = static_cast<double>(Demand) / static_cast<double>(Supply);
		const double Multiplier = Ratio >= 1.0
			? 1.0 + (Ratio - 1.0) * Rules.PriceShortageSensitivity
			: 1.0 - (1.0 - Ratio) * Rules.PriceSurplusSensitivity;
		return FMath::Clamp(Multiplier, Rules.MinMarketPriceMultiplier, Rules.MaxMarketPriceMultiplier);
	}

	FString CreditRatingToLabel(EWLCreditRating Rating)
	{
		switch (Rating)
		{
		case EWLCreditRating::AAA: return TEXT("AAA");
		case EWLCreditRating::AA: return TEXT("AA");
		case EWLCreditRating::A: return TEXT("A");
		case EWLCreditRating::BBB: return TEXT("BBB");
		case EWLCreditRating::BB: return TEXT("BB");
		case EWLCreditRating::B: return TEXT("B");
		case EWLCreditRating::CCC: return TEXT("CCC");
		case EWLCreditRating::Default: return TEXT("Default");
		default: return TEXT("BBB");
		}
	}

	EWLCreditRating RatingFromScore(int32 Score, bool bInDefault)
	{
		if (bInDefault)
		{
			return EWLCreditRating::Default;
		}
		if (Score >= 90) return EWLCreditRating::AAA;
		if (Score >= 80) return EWLCreditRating::AA;
		if (Score >= 70) return EWLCreditRating::A;
		if (Score >= 60) return EWLCreditRating::BBB;
		if (Score >= 50) return EWLCreditRating::BB;
		if (Score >= 35) return EWLCreditRating::B;
		return EWLCreditRating::CCC;
	}

	FString FinancialInstrumentPrefix(EWLFinancialInstrumentType Type)
	{
		switch (Type)
		{
		case EWLFinancialInstrumentType::Bond: return TEXT("BOND");
		case EWLFinancialInstrumentType::BilateralLoan: return TEXT("LOAN");
		case EWLFinancialInstrumentType::IMFProgram: return TEXT("IMF");
		default: return TEXT("FIN");
		}
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
	ProvinceBuildingLevels.Reset();
	RecruitQueues.Reset();
	GarrisonRecruited.Reset();
	TaxRates.Reset();
	TariffRates.Reset();
	FinancialInstruments.Reset();
	ForeignSupportStates.Reset();
	NextFinancialInstrumentNumber = 1;
	NextForeignSupportNumber = 1;
	PreviousGDP.Reset();
	GDPGrowth.Reset();
	ActiveMarketShocks.Reset();
	NextMarketShockNumber = 1;
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
	AdvanceFinancialMonth();
	ApplyMonthlyProvinceState();
	AdvanceRecruitment();
	UpdateGDPHistory();   // FE1.5: mide el PIB tras aplicar el mes
	AdvanceMarketShocks();

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

	// Coherencia temporal: un dia aplica 1/30 del balance MENSUAL al tesoro (progreso visible sin acelerar
	// la economia 30x). Todo lo demas que es mensual (finanzas, provincias, PIB, shocks, IA) corre SOLO al
	// cerrar el mes. El reclutamiento si avanza por dia (sus "turnos" son dias).
	ApplyDailyEconomy();
	AdvanceRecruitment();

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
		AdvanceFinancialMonth();
		ApplyMonthlyProvinceState();
		UpdateGDPHistory();
		AdvanceMarketShocks();
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

void UWLStrategicTickSubsystem::ApplyDailyEconomy()
{
	for (TPair<FString, int64>& Pair : Treasuries)
	{
		Pair.Value += static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(GetMonthlyBalance(Pair.Key)) / 30.0));
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
		// Fase 3 auditoria: el ministro del Interior mueve el orden publico cada mes (+ si es competente).
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const UWLCharacterSubsystem* Characters = GI->GetSubsystem<UWLCharacterSubsystem>())
			{
				NextOrder += FMath::RoundToInt(
					Characters->GetMinisterEffectFactor(ControllerIso, EWLMinisterOffice::Interior)
					* Rules.InteriorMinisterOrderPerMonth);
			}
		}
		NextOrder += GetProvinceBuildingEffects(Province.Id).BonusPublicOrder;

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
	for (const FWLGoodMarketBalance& GoodBalance : GetNationGoodMarketBalance(NormalizedIso))
	{
		Budget.ExportIncome += GoodBalance.ExportRevenue;   // FE4.1
		Budget.TariffIncome += GoodBalance.ImportTariffIncome;   // FE4.3
		Budget.ImportCost += GoodBalance.ImportCost;
	}
	for (const FWLForeignSupportState& Support : ForeignSupportStates)
	{
		if (!Support.IsActive())
		{
			continue;
		}
		if (Support.RecipientIso == NormalizedIso)
		{
			if (Support.Type == EWLForeignSupportType::ForeignAid)
			{
				Budget.ForeignAidIncome += Support.MonthlyAmount;   // FE5.3
			}
			else if (Support.Type == EWLForeignSupportType::ForeignDirectInvestment)
			{
				Budget.ForeignInvestmentInflow += Support.MonthlyAmount;   // FE5.3: exposicion, no tesoro
			}
		}
		// La ayuda concedida la PAGA el patrocinador (antes era dinero creado de la nada).
		if (Support.SponsorIso == NormalizedIso && Support.Type == EWLForeignSupportType::ForeignAid)
		{
			Budget.ForeignAidExpense += Support.MonthlyAmount;
		}
	}
	// FE1.4: la deuda (tesoro negativo) cobra interes mensual como gasto del presupuesto.
	if (const int64* Treasury = Treasuries.Find(NormalizedIso); Treasury && *Treasury < 0)
	{
		Budget.DebtInterest = static_cast<int64>(
			FMath::RoundToDouble(static_cast<double>(-*Treasury) * Rules.DebtMonthlyInterestRate));
	}
	Budget.DebtService = GetNationDebtService(NormalizedIso);   // FE5.1
	const FWLEconomicGovernanceStats Governance = GetEconomicGovernanceStats(NormalizedIso);
	Budget.CorruptionLoss = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(Budget.TotalIncome()) * Governance.CorruptionSkimRate));
	return Budget;
}

int64 UWLStrategicTickSubsystem::GetCreditLimit(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	return static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(GetNationBudget(NationIso).TotalIncome()) * Rules.DebtCreditLimitIncomeMonths));
}

int64 UWLStrategicTickSubsystem::GetNationDebtService(const FString& NationIso) const
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	int64 DebtService = 0;
	for (const FWLFinancialInstrumentState& Instrument : FinancialInstruments)
	{
		if (Instrument.NationIso == NormalizedIso && Instrument.IsActive())
		{
			DebtService += Instrument.MonthlyPayment;
		}
	}
	return DebtService;
}

int64 UWLStrategicTickSubsystem::GetNationOutstandingDebt(const FString& NationIso) const
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	int64 Outstanding = 0;
	if (const int64* Treasury = Treasuries.Find(NormalizedIso); Treasury && *Treasury < 0)
	{
		Outstanding += -*Treasury;
	}
	for (const FWLFinancialInstrumentState& Instrument : FinancialInstruments)
	{
		if (Instrument.NationIso == NormalizedIso && Instrument.PrincipalRemaining > 0)
		{
			Outstanding += Instrument.PrincipalRemaining;
		}
	}
	return Outstanding;
}

FWLFinancialProfile UWLStrategicTickSubsystem::GetFinancialProfile(const FString& NationIso) const
{
	FWLFinancialProfile Profile;
	const FString NormalizedIso = NormalizeIso(NationIso);
	Profile.NationIso = NormalizedIso;

	const FWLNationBudget Budget = GetNationBudget(NormalizedIso);
	const int64 Income = FMath::Max<int64>(1, Budget.TotalIncome());
	Profile.CreditLimit = GetCreditLimit(NormalizedIso);
	Profile.OutstandingDebt = GetNationOutstandingDebt(NormalizedIso);
	Profile.MonthlyDebtService = GetNationDebtService(NormalizedIso);
	Profile.AvailableCredit = FMath::Max<int64>(0, Profile.CreditLimit - Profile.OutstandingDebt);
	Profile.DebtServiceIncomeRatio = static_cast<double>(Profile.MonthlyDebtService) / static_cast<double>(Income);

	for (const FWLFinancialInstrumentState& Instrument : FinancialInstruments)
	{
		if (Instrument.NationIso == NormalizedIso && Instrument.bDefaulted)
		{
			Profile.bInDefault = true;
			break;
		}
	}
	if (!Profile.bInDefault)
	{
		Profile.bInDefault = Profile.CreditLimit > 0 && Profile.OutstandingDebt > Profile.CreditLimit;
	}

	const double DebtBurden = Profile.CreditLimit > 0
		? static_cast<double>(Profile.OutstandingDebt) / static_cast<double>(Profile.CreditLimit)
		: 1.0;
	const FWLEconomicGovernanceStats Governance = GetEconomicGovernanceStats(NormalizedIso);
	const double Growth = GetNationGDPGrowth(NormalizedIso);
	const double TreasuryStress = GetTreasury(NormalizedIso) < 0
		? static_cast<double>(-GetTreasury(NormalizedIso)) / static_cast<double>(Income)
		: 0.0;
	const int32 Score = FMath::Clamp(FMath::RoundToInt(
		75.0
		+ static_cast<double>(Governance.InvestmentClimate - 50) * 0.35
		+ Growth * 350.0
		- DebtBurden * 25.0
		- Profile.DebtServiceIncomeRatio * 80.0
		- TreasuryStress * 3.0),
		0,
		100);
	Profile.CreditScore = Profile.bInDefault ? 0 : Score;
	Profile.CreditRating = RatingFromScore(Profile.CreditScore, Profile.bInDefault);
	Profile.CreditRatingLabel = CreditRatingToLabel(Profile.CreditRating);

	const FWLBalanceRules Rules = GetBalanceRules();
	Profile.DefaultRisk = FMath::Clamp(
		DebtBurden * 0.35
		+ (Rules.DefaultDebtServiceIncomeRatio > 0.0
			? Profile.DebtServiceIncomeRatio / Rules.DefaultDebtServiceIncomeRatio
			: 1.0) * 0.45
		+ FMath::Max(0.0, 55.0 - static_cast<double>(Profile.CreditScore)) / 100.0,
		0.0,
		1.0);
	Profile.bIMFEligible =
		Profile.bInDefault
		|| Profile.CreditRating == EWLCreditRating::CCC
		|| (GetTreasury(NormalizedIso) < 0 && Profile.CreditScore < 55);
	return Profile;
}

TArray<FWLFinancialInstrumentState> UWLStrategicTickSubsystem::GetFinancialInstrumentsForNation(const FString& NationIso) const
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	TArray<FWLFinancialInstrumentState> Out;
	for (const FWLFinancialInstrumentState& Instrument : FinancialInstruments)
	{
		if (Instrument.NationIso == NormalizedIso)
		{
			Out.Add(Instrument);
		}
	}
	Out.Sort([](const FWLFinancialInstrumentState& A, const FWLFinancialInstrumentState& B)
	{
		return A.InstrumentId < B.InstrumentId;
	});
	return Out;
}

double UWLStrategicTickSubsystem::GetInterestRateForInstrument(const FString& NationIso, EWLFinancialInstrumentType Type) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	if (Type == EWLFinancialInstrumentType::BilateralLoan)
	{
		return Rules.BilateralLoanMonthlyInterestRate;
	}
	if (Type == EWLFinancialInstrumentType::IMFProgram)
	{
		return Rules.IMFMonthlyInterestRate;
	}

	const FWLFinancialProfile Profile = GetFinancialProfile(NationIso);
	const double RiskSpread = FMath::Clamp((100.0 - static_cast<double>(Profile.CreditScore)) / 100.0, 0.0, 1.0) * 0.018;
	return Rules.BondBaseMonthlyInterestRate + RiskSpread;
}

bool UWLStrategicTickSubsystem::CanAddDebtInstrument(
	const FString& NationIso,
	int64 Principal,
	int32 TermMonths,
	double MonthlyInterestRate,
	FString& OutMessage) const
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	const FWLBalanceRules Rules = GetBalanceRules();
	if (Principal <= 0)
	{
		OutMessage = TEXT("Principal invalido.");
		return false;
	}
	if (TermMonths <= 0 || TermMonths > Rules.FinancialInstrumentMaxMonths)
	{
		OutMessage = FString::Printf(TEXT("Plazo invalido: %d meses."), TermMonths);
		return false;
	}

	const FWLFinancialProfile Profile = GetFinancialProfile(NormalizedIso);
	if (Profile.bInDefault)
	{
		OutMessage = FString::Printf(TEXT("%s esta en default y no puede emitir deuda ordinaria."), *NormalizedIso);
		return false;
	}

	const int64 CreditLimit = GetCreditLimit(NormalizedIso);
	const int64 DebtAfter = Profile.OutstandingDebt + Principal;
	if (DebtAfter > CreditLimit)
	{
		OutMessage = FString::Printf(TEXT("Credito insuficiente para %s: deuda %lld + %lld > limite %lld."),
			*NormalizedIso,
			static_cast<long long>(Profile.OutstandingDebt),
			static_cast<long long>(Principal),
			static_cast<long long>(CreditLimit));
		return false;
	}

	const int64 MonthlyPayment = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(Principal) / static_cast<double>(TermMonths)
		+ static_cast<double>(Principal) * MonthlyInterestRate));
	const int64 Income = FMath::Max<int64>(1, GetNationBudget(NormalizedIso).TotalIncome());
	const double ServiceRatioAfter = static_cast<double>(Profile.MonthlyDebtService + MonthlyPayment) / static_cast<double>(Income);
	if (ServiceRatioAfter > Rules.MaxDebtServiceIncomeRatio)
	{
		OutMessage = FString::Printf(TEXT("Servicio de deuda demasiado alto para %s: %.0f%% > %.0f%%."),
			*NormalizedIso,
			ServiceRatioAfter * 100.0,
			Rules.MaxDebtServiceIncomeRatio * 100.0);
		return false;
	}
	return true;
}

FWLFinancialInstrumentState UWLStrategicTickSubsystem::AddDebtInstrument(
	const FString& NationIso,
	const FString& CreditorIso,
	EWLFinancialInstrumentType Type,
	int64 Principal,
	int32 TermMonths,
	double MonthlyInterestRate,
	const FString& Title)
{
	FWLFinancialInstrumentState Instrument;
	Instrument.NationIso = NormalizeIso(NationIso);
	Instrument.CreditorIso = NormalizeIso(CreditorIso);
	Instrument.Type = Type;
	Instrument.OriginalPrincipal = FMath::Max<int64>(0, Principal);
	Instrument.PrincipalRemaining = Instrument.OriginalPrincipal;
	Instrument.MonthlyInterestRate = FMath::Max(0.0, MonthlyInterestRate);
	Instrument.TotalMonths = FMath::Max(1, TermMonths);
	Instrument.RemainingMonths = Instrument.TotalMonths;
	Instrument.MonthlyPayment = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(Instrument.OriginalPrincipal) / static_cast<double>(Instrument.TotalMonths)
		+ static_cast<double>(Instrument.OriginalPrincipal) * Instrument.MonthlyInterestRate));
	Instrument.InstrumentId = FString::Printf(TEXT("%s-%04d"), *FinancialInstrumentPrefix(Type), NextFinancialInstrumentNumber++);
	Instrument.Title = Title.TrimStartAndEnd().IsEmpty()
		? FString::Printf(TEXT("%s %s"), *FinancialInstrumentPrefix(Type), *Instrument.NationIso)
		: Title.TrimStartAndEnd();
	FinancialInstruments.Add(Instrument);
	return Instrument;
}

bool UWLStrategicTickSubsystem::IssueBond(const FString& NationIso, int64 Principal, int32 TermMonths, FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLNationData Nation;
	if (!Registry || !Registry->GetNation(NationIso, Nation))
	{
		OutMessage = TEXT("Nacion invalida para emitir bonos.");
		return false;
	}
	const double Rate = GetInterestRateForInstrument(Nation.Iso, EWLFinancialInstrumentType::Bond);
	if (!CanAddDebtInstrument(Nation.Iso, Principal, TermMonths, Rate, OutMessage))
	{
		return false;
	}
	FWLFinancialInstrumentState Instrument = AddDebtInstrument(
		Nation.Iso,
		TEXT("MARKET"),
		EWLFinancialInstrumentType::Bond,
		Principal,
		TermMonths,
		Rate,
		TEXT("Bono soberano"));
	Treasuries.FindOrAdd(Nation.Iso) += Principal;
	OutMessage = FString::Printf(TEXT("%s emitio bono %s por %lld a %.2f%% mensual."),
		*Nation.Iso,
		*Instrument.InstrumentId,
		static_cast<long long>(Principal),
		Rate * 100.0);
	return true;
}

bool UWLStrategicTickSubsystem::RegisterBilateralLoan(
	const FString& CreditorIso,
	const FString& RecipientIso,
	int64 Principal,
	int32 TermMonths,
	FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLNationData Creditor;
	FWLNationData Recipient;
	if (!Registry || !Registry->GetNation(CreditorIso, Creditor) || !Registry->GetNation(RecipientIso, Recipient))
	{
		OutMessage = TEXT("Nacion invalida para prestamo bilateral.");
		return false;
	}
	if (Creditor.Iso == Recipient.Iso)
	{
		OutMessage = TEXT("Prestamo bilateral requiere dos paises distintos.");
		return false;
	}
	const double Rate = GetInterestRateForInstrument(Recipient.Iso, EWLFinancialInstrumentType::BilateralLoan);
	if (!CanAddDebtInstrument(Recipient.Iso, Principal, TermMonths, Rate, OutMessage))
	{
		return false;
	}
	FWLFinancialInstrumentState Instrument = AddDebtInstrument(
		Recipient.Iso,
		Creditor.Iso,
		EWLFinancialInstrumentType::BilateralLoan,
		Principal,
		TermMonths,
		Rate,
		FString::Printf(TEXT("Prestamo bilateral %s->%s"), *Creditor.Iso, *Recipient.Iso));
	Treasuries.FindOrAdd(Recipient.Iso) += Principal;
	Treasuries.FindOrAdd(Creditor.Iso) -= FMath::Min<int64>(Principal, FMath::Max<int64>(0, Treasuries.FindRef(Creditor.Iso)));
	OutMessage = FString::Printf(TEXT("%s recibe prestamo %s de %s por %lld."),
		*Recipient.Iso,
		*Instrument.InstrumentId,
		*Creditor.Iso,
		static_cast<long long>(Principal));
	return true;
}

bool UWLStrategicTickSubsystem::RequestIMFProgram(const FString& NationIso, int64 Principal, int32 TermMonths, FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLNationData Nation;
	if (!Registry || !Registry->GetNation(NationIso, Nation))
	{
		OutMessage = TEXT("Nacion invalida para programa FMI.");
		return false;
	}
	const FWLFinancialProfile Profile = GetFinancialProfile(Nation.Iso);
	if (!Profile.bIMFEligible)
	{
		OutMessage = FString::Printf(TEXT("%s no es elegible para FMI con rating %s."), *Nation.Iso, *Profile.CreditRatingLabel);
		return false;
	}
	const double Rate = GetInterestRateForInstrument(Nation.Iso, EWLFinancialInstrumentType::IMFProgram);
	FString EligibilityMessage;
	if (!CanAddDebtInstrument(Nation.Iso, Principal, TermMonths, Rate, EligibilityMessage))
	{
		// El FMI puede entrar aun con deuda ordinaria cerrada, pero no por encima del limite absoluto.
		if (Profile.OutstandingDebt + Principal > Profile.CreditLimit)
		{
			OutMessage = EligibilityMessage;
			return false;
		}
	}
	FWLFinancialInstrumentState Instrument = AddDebtInstrument(
		Nation.Iso,
		TEXT("IMF"),
		EWLFinancialInstrumentType::IMFProgram,
		Principal,
		TermMonths,
		Rate,
		TEXT("Programa FMI"));
	Treasuries.FindOrAdd(Nation.Iso) += Principal;
	AdjustNationPublicOrder(Nation.Iso, -2);
	OutMessage = FString::Printf(TEXT("%s firma programa FMI %s por %lld."),
		*Nation.Iso,
		*Instrument.InstrumentId,
		static_cast<long long>(Principal));
	return true;
}

bool UWLStrategicTickSubsystem::MarkDebtDefault(const FString& NationIso, FString& OutMessage)
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	int32 Affected = 0;
	for (FWLFinancialInstrumentState& Instrument : FinancialInstruments)
	{
		if (Instrument.NationIso == NormalizedIso && Instrument.PrincipalRemaining > 0 && !Instrument.bDefaulted)
		{
			Instrument.bDefaulted = true;
			++Affected;
		}
	}
	AdjustNationPublicOrder(NormalizedIso, -GetBalanceRules().DefaultPublicOrderPenalty);
	OutMessage = FString::Printf(TEXT("%s entra en default (%d instrumentos)."), *NormalizedIso, Affected);
	return Affected > 0;
}

TArray<FWLForeignSupportState> UWLStrategicTickSubsystem::GetForeignSupportForNation(const FString& NationIso) const
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	TArray<FWLForeignSupportState> Out;
	for (const FWLForeignSupportState& Support : ForeignSupportStates)
	{
		if (Support.RecipientIso == NormalizedIso || Support.SponsorIso == NormalizedIso)
		{
			Out.Add(Support);
		}
	}
	Out.Sort([](const FWLForeignSupportState& A, const FWLForeignSupportState& B)
	{
		return A.SupportId < B.SupportId;
	});
	return Out;
}

bool UWLStrategicTickSubsystem::GrantForeignAid(
	const FString& SponsorIso,
	const FString& RecipientIso,
	int64 MonthlyAmount,
	int32 DurationMonths,
	FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLNationData Sponsor;
	FWLNationData Recipient;
	if (!Registry || !Registry->GetNation(SponsorIso, Sponsor) || !Registry->GetNation(RecipientIso, Recipient))
	{
		OutMessage = TEXT("Nacion invalida para ayuda exterior.");
		return false;
	}
	if (Sponsor.Iso == Recipient.Iso)
	{
		OutMessage = TEXT("Ayuda exterior requiere dos paises distintos.");
		return false;
	}
	FWLDiplomaticRelationState Relation;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			if (Politics->GetRelation(Sponsor.Iso, Recipient.Iso, Relation)
				&& Relation.Opinion < GetBalanceRules().ForeignAidMinOpinion)
			{
				OutMessage = FString::Printf(TEXT("Opinion insuficiente para ayuda exterior: %d."), Relation.Opinion);
				return false;
			}
		}
	}
	const FWLBalanceRules Rules = GetBalanceRules();
	const int64 Cap = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(FMath::Max<int64>(1, GetNationBudget(Recipient.Iso).TotalIncome()))
		* Rules.ForeignAidMonthlyCapIncomeRatio));
	const int64 EffectiveMonthly = FMath::Clamp<int64>(MonthlyAmount, 1, FMath::Max<int64>(1, Cap));
	FWLForeignSupportState Support;
	Support.SupportId = FString::Printf(TEXT("AID-%04d"), NextForeignSupportNumber++);
	Support.RecipientIso = Recipient.Iso;
	Support.SponsorIso = Sponsor.Iso;
	Support.Type = EWLForeignSupportType::ForeignAid;
	Support.MonthlyAmount = EffectiveMonthly;
	Support.TotalMonths = FMath::Clamp(DurationMonths, 1, Rules.ForeignSupportMaxMonths);
	Support.RemainingMonths = Support.TotalMonths;
	Support.TotalAmount = Support.MonthlyAmount * Support.TotalMonths;
	Support.Title = FString::Printf(TEXT("Ayuda exterior %s->%s"), *Sponsor.Iso, *Recipient.Iso);
	ForeignSupportStates.Add(Support);
	// Dar ayuda compra influencia: mejora la opinion del receptor hacia el patrocinador.
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			FString RelationMessage;
			Politics->AdjustRelationOpinion(Sponsor.Iso, Recipient.Iso, 8, RelationMessage);
		}
	}
	OutMessage = FString::Printf(TEXT("%s concede ayuda %s a %s: %lld/mes por %d meses."),
		*Sponsor.Iso,
		*Support.SupportId,
		*Recipient.Iso,
		static_cast<long long>(Support.MonthlyAmount),
		Support.TotalMonths);
	return true;
}

bool UWLStrategicTickSubsystem::StartForeignInvestment(
	const FString& SponsorIso,
	const FString& RecipientIso,
	const FString& ProvinceId,
	const FString& BuildingId,
	int64 MonthlyAmount,
	int32 DurationMonths,
	FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLNationData Sponsor;
	FWLNationData Recipient;
	FWLProvinceData Province;
	FWLBuildingData Building;
	if (!Registry || !Registry->GetNation(SponsorIso, Sponsor) || !Registry->GetNation(RecipientIso, Recipient)
		|| !Registry->GetProvince(ProvinceId, Province) || !Registry->GetBuilding(BuildingId, Building))
	{
		OutMessage = TEXT("Datos invalidos para FDI.");
		return false;
	}
	if (Sponsor.Iso == Recipient.Iso)
	{
		OutMessage = TEXT("FDI requiere sponsor extranjero.");
		return false;
	}
	if (GetProvinceControllerIso(Province.Id) != Recipient.Iso)
	{
		OutMessage = FString::Printf(TEXT("%s no controla %s."), *Recipient.Iso, *Province.Id);
		return false;
	}
	FWLDiplomaticRelationState Relation;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			if (Politics->GetRelation(Sponsor.Iso, Recipient.Iso, Relation)
				&& Relation.Opinion < GetBalanceRules().ForeignInvestmentMinOpinion)
			{
				OutMessage = FString::Printf(TEXT("Opinion insuficiente para FDI: %d."), Relation.Opinion);
				return false;
			}
		}
	}
	if (!ProvinceSupportsBuilding(Province, Building))
	{
		OutMessage = FString::Printf(TEXT("%s no encaja para FDI en %s."), *Building.Name, *Province.Id);
		return false;
	}
	const TArray<FString>* ExistingBuildings = ProvinceBuildings.Find(Province.Id);
	if (ExistingBuildings)
	{
		for (const FString& ExistingId : *ExistingBuildings)
		{
			FWLBuildingData ExistingBuilding;
			if (Registry->GetBuilding(ExistingId, ExistingBuilding) && ExistingBuilding.Slot == Building.Slot)
			{
				OutMessage = FString::Printf(TEXT("Slot ocupado por %s en %s."), *ExistingBuilding.Name, *Province.Id);
				return false;
			}
		}
	}
	const FWLBalanceRules Rules = GetBalanceRules();
	const int64 Cap = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(FMath::Max<int64>(1, GetNationBudget(Recipient.Iso).TotalIncome()))
		* Rules.ForeignInvestmentMonthlyCapIncomeRatio));
	FWLForeignSupportState Support;
	Support.SupportId = FString::Printf(TEXT("FDI-%04d"), NextForeignSupportNumber++);
	Support.RecipientIso = Recipient.Iso;
	Support.SponsorIso = Sponsor.Iso;
	Support.Type = EWLForeignSupportType::ForeignDirectInvestment;
	Support.MonthlyAmount = FMath::Clamp<int64>(MonthlyAmount, 1, FMath::Max<int64>(1, Cap));
	Support.TotalMonths = FMath::Clamp(DurationMonths, 1, Rules.ForeignSupportMaxMonths);
	Support.RemainingMonths = Support.TotalMonths;
	Support.TotalAmount = Support.MonthlyAmount * Support.TotalMonths;
	Support.TargetProvinceId = Province.Id;
	Support.TargetBuildingId = Building.Id;
	Support.Title = FString::Printf(TEXT("FDI %s en %s"), *Building.Name, *Province.Id);
	ForeignSupportStates.Add(Support);
	OutMessage = FString::Printf(TEXT("%s inicia FDI %s para construir %s en %s."),
		*Sponsor.Iso, *Support.SupportId, *Building.Name, *Province.Id);
	return true;
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
	int64 PopulationActivity = 0;
	int64 OrderSum = 0;
	int32 ProvinceCount = 0;
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

		// Actividad economica de la poblacion (consumo/servicios) sumada al valor final de mercado.
		PopulationActivity += static_cast<int64>(
			FMath::RoundToDouble(static_cast<double>(State.Population) * Rules.GDPPerCapitaActivity));
		OrderSum += ClampPublicOrder(State.PublicOrder);
		++ProvinceCount;
	}

	const int64 MarketProductionValue = GetNationMarketProductionValue(NormalizedIso);
	const int32 AverageOrder = ProvinceCount > 0
		? static_cast<int32>(OrderSum / ProvinceCount)
		: Rules.InitialPublicOrder;
	return UWLEconomyLibrary::ApplyPublicOrderIncomeModifier(
		MarketProductionValue + PopulationActivity,
		AverageOrder,
		Rules);
}

int32 UWLStrategicTickSubsystem::GetNationTechnologyLevel(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 50;
	}

	const FString NormalizedIso = NormalizeIso(NationIso);
	int32 Technology = 50;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) == NormalizedIso)
		{
			Technology += GetProvinceBuildingEffects(Province.Id).BonusTechnology;
		}
	}
	return FMath::Clamp(Technology, 0, 100);
}

FWLEconomicGovernanceStats UWLStrategicTickSubsystem::GetEconomicGovernanceStats(const FString& NationIso) const
{
	FWLEconomicGovernanceStats Stats;
	Stats.NationIso = NormalizeIso(NationIso);
	Stats.TechnologyLevel = GetNationTechnologyLevel(Stats.NationIso);

	const FWLBalanceRules Rules = GetBalanceRules();
	int32 BaseCorruption = 35;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLCharacterSubsystem* Characters = GI->GetSubsystem<UWLCharacterSubsystem>())
		{
			const FWLGovernmentStats Government = Characters->GetGovernmentStats(Stats.NationIso);
			BaseCorruption = Government.Corruption;

			FWLCharacter EconomyMinister;
			if (Characters->GetCabinetMinister(Stats.NationIso, EWLMinisterOffice::Economy, EconomyMinister))
			{
				Stats.EconomyMinisterId = EconomyMinister.Id;
				Stats.EconomyMinisterName = EconomyMinister.Name;
				Stats.EconomyMinisterSkill = FMath::Clamp(EconomyMinister.Skill, 0, 100);

				if (HasTrait(EconomyMinister, TEXT("fiscalista"))
					|| HasTrait(EconomyMinister, TEXT("austera"))
					|| HasTrait(EconomyMinister, TEXT("tecnocrata")))
				{
					BaseCorruption -= 8;
				}
				if (HasTrait(EconomyMinister, TEXT("clientelista"))
					|| HasTrait(EconomyMinister, TEXT("corrupto")))
				{
					BaseCorruption += 16;
				}
			}
			else
			{
				Stats.EconomyMinisterSkill = 40;
				BaseCorruption += 15;
			}
		}
	}

	Stats.SystemicCorruption = FMath::Clamp(BaseCorruption, 0, 100);
	Stats.TaxCollectionMultiplier = FMath::Clamp(
		1.0
			+ static_cast<double>(Stats.EconomyMinisterSkill - 50) * Rules.EconomyMinisterTaxEfficiencyPerSkill
			- static_cast<double>(Stats.SystemicCorruption) * Rules.CorruptionTaxLeakPerPoint,
		0.35,
		1.60);
	Stats.ProductivityMultiplier = FMath::Clamp(
		1.0
			+ static_cast<double>(Stats.TechnologyLevel - 50) * Rules.TechnologyProductivityPerPoint
			- static_cast<double>(Stats.SystemicCorruption) * Rules.CorruptionProductivityPenaltyPerPoint,
		Rules.MinGovernanceProductivityMultiplier,
		Rules.MaxGovernanceProductivityMultiplier);
	Stats.CorruptionSkimRate = FMath::Clamp(
		static_cast<double>(Stats.SystemicCorruption) * Rules.CorruptionBudgetSkimPerPoint,
		0.0,
		0.60);
	Stats.InvestmentClimate = FMath::Clamp(
		50 + (Stats.EconomyMinisterSkill - 50) / 2 + (Stats.TechnologyLevel - 50) / 2 - Stats.SystemicCorruption / 2,
		0,
		100);
	return Stats;
}

int32 UWLStrategicTickSubsystem::GetNationSystemicCorruption(const FString& NationIso) const
{
	return GetEconomicGovernanceStats(NationIso).SystemicCorruption;
}

double UWLStrategicTickSubsystem::GetNationGDPGrowth(const FString& NationIso) const
{
	const double* Found = GDPGrowth.Find(NormalizeIso(NationIso));
	return Found ? *Found : 0.0;
}

// FE2.2/FE2.4: sectores por provincia. Extraccion+manufactura compiten primero por trabajadores;
// los servicios absorben parte del remanente y el resto queda desempleado.
FWLSectorEmployment UWLStrategicTickSubsystem::GetProvinceEmployment(const FString& ProvinceId) const
{
	FWLSectorEmployment Employment;
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		return Employment;
	}
	ApplyBuildingEffectsToProvince(Province, GetProvinceBuildingEffects(Province.Id));

	const FWLBalanceRules Rules = GetBalanceRules();
	FWLProvinceRuntimeState State;
	const int64 Population = GetProvinceState(Province.Id, State)
		? FMath::Max<int64>(0, State.Population)
		: FMath::Max<int64>(0, Province.Population);

	Employment.Workforce = static_cast<int64>(
		FMath::RoundToDouble(static_cast<double>(Population) * Rules.LaborParticipationRate));

	const int64 ExtractionBase = static_cast<int64>(Province.BaseOil) + Province.BaseGas
		+ Province.BaseMinerals + Province.BaseFood;
	const int64 ExtractionWorkersRequired = ExtractionBase * static_cast<int64>(Rules.WorkersPerBasePoint);
	const int64 ManufacturingWorkersRequired =
		static_cast<int64>(Province.BaseIndustry) * static_cast<int64>(Rules.WorkersPerBasePoint);
	const int64 ProductiveWorkersRequired = ExtractionWorkersRequired + ManufacturingWorkersRequired;

	Employment.LaborCoverage = ProductiveWorkersRequired > 0
		? FMath::Clamp(static_cast<double>(Employment.Workforce) / static_cast<double>(ProductiveWorkersRequired), 0.0, 1.0)
		: 1.0;

	Employment.Extraction = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(ExtractionWorkersRequired) * Employment.LaborCoverage));
	Employment.Manufacturing = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(ManufacturingWorkersRequired) * Employment.LaborCoverage));

	const int64 RemainingWorkforce = FMath::Max<int64>(0,
		Employment.Workforce - Employment.Extraction - Employment.Manufacturing);
	const int64 ServicesCapacity = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(Population) * Rules.ServiceEmploymentPerCapita));
	Employment.Services = FMath::Min(RemainingWorkforce, ServicesCapacity);
	Employment.Unemployed = FMath::Max<int64>(0,
		Employment.Workforce - Employment.Extraction - Employment.Manufacturing - Employment.Services);
	Employment.UnemploymentRate = Employment.Workforce > 0
		? FMath::Clamp(static_cast<double>(Employment.Unemployed) / static_cast<double>(Employment.Workforce), 0.0, 1.0)
		: 0.0;
	const FString ControllerIso = State.ControllerIso.IsEmpty() ? Province.CountryIso : NormalizeIso(State.ControllerIso);
	const double GovernanceProductivity = GetEconomicGovernanceStats(ControllerIso).ProductivityMultiplier;
	Employment.Productivity = FMath::Clamp(
		Employment.LaborCoverage
			* (1.0 - Employment.UnemploymentRate * Rules.UnemploymentProductivityPenalty)
			* GovernanceProductivity,
		0.0,
		Rules.MaxGovernanceProductivityMultiplier);
	return Employment;
}

FWLProductionLedger UWLStrategicTickSubsystem::BuildProvinceProductionLedger(const FString& ProvinceId) const
{
	FWLProductionLedger Ledger;
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		return Ledger;
	}
	ApplyBuildingEffectsToProvince(Province, GetProvinceBuildingEffects(Province.Id));

	const FWLBalanceRules Rules = GetBalanceRules();
	FWLProvinceRuntimeState State;
	const FString ControllerIso = GetProvinceState(Province.Id, State) && !State.ControllerIso.IsEmpty()
		? NormalizeIso(State.ControllerIso)
		: Province.CountryIso;
	const double Coverage = GetProvinceEmployment(Province.Id).LaborCoverage;
	const double OutputPerPoint = Rules.SectorOutputPerBasePoint
		* Coverage
		* GetEconomicGovernanceStats(ControllerIso).ProductivityMultiplier;

	TMap<FString, int64> Inventory;
	auto AddProduced = [&Ledger, &Inventory](const FString& GoodId, int64 Units)
	{
		if (Units > 0)
		{
			AddUnits(Ledger.Produced, GoodId, Units);
			AddUnits(Inventory, GoodId, Units);
		}
	};

	const TArray<FWLGoodData> Goods = Registry->GetAllGoods();

	// Extraccion: los crudos salen del catalogo, no de IDs hardcodeados. Esto permite dividir
	// minerals en minerals/coal o food en food/coffee sin tocar codigo.
	for (const FWLGoodData& Good : Goods)
	{
		if (Good.Category != EWLGoodCategory::Raw)
		{
			continue;
		}
		const FString Source = Good.BaseSource.IsEmpty() ? Good.Id : Good.BaseSource;
		const double Share = Good.BaseShare > 0.0 ? Good.BaseShare : (Source == Good.Id ? 1.0 : 0.0);
		const int64 Units = RoundUnits(
			static_cast<double>(ProvinceBaseForSource(Province, Source)) * OutputPerPoint * Share);
		AddProduced(Good.Id, Units);
	}

	// Manufactura: cada bien declara su cuota industrial y sus insumos. El ledger deja produccion
	// bruta para reporting, pero la oferta final descuenta insumos consumidos para no doble-contar.
	const double IndustryUnits = Province.BaseIndustry * OutputPerPoint;
	if (IndustryUnits > 0.0)
	{
		TArray<FWLGoodData> ManufacturedGoods;
		for (const FWLGoodData& Good : Goods)
		{
			if (Good.Category == EWLGoodCategory::Manufactured)
			{
				ManufacturedGoods.Add(Good);
			}
		}
		ManufacturedGoods.Sort([](const FWLGoodData& A, const FWLGoodData& B)
		{
			return A.IndustryShare == B.IndustryShare ? A.Id < B.Id : A.IndustryShare > B.IndustryShare;
		});

		TSet<FString> Completed;
		for (int32 Pass = 0; Pass < ManufacturedGoods.Num(); ++Pass)
		{
			const TMap<FString, int64> InventoryAtPassStart = Inventory;
			bool bProgress = false;
			for (const FWLGoodData& Good : ManufacturedGoods)
			{
				if (Completed.Contains(Good.Id))
				{
					continue;
				}

				const int64 Potential = RoundUnits(IndustryUnits * Good.IndustryShare);
				if (Potential <= 0)
				{
					Completed.Add(Good.Id);
					bProgress = true;
					continue;
				}

				int64 Buildable = Potential;
				bool bWaitForLaterInput = false;
				for (const FString& InputId : Good.Inputs)
				{
					const int64 AvailableAtPassStart = ReadUnits(InventoryAtPassStart, InputId);
					if (AvailableAtPassStart <= 0 && IsManufacturedGood(Registry, InputId))
					{
						bWaitForLaterInput = true;
						break;
					}
					Buildable = FMath::Min(Buildable, ReadUnits(Inventory, InputId));
				}
				if (bWaitForLaterInput)
				{
					continue;
				}
				if (Buildable <= 0)
				{
					Completed.Add(Good.Id);
					bProgress = true;
					continue;
				}

				for (const FString& InputId : Good.Inputs)
				{
					AddUnits(Inventory, InputId, -Buildable);
					AddUnits(Ledger.UsedAsInput, InputId, Buildable);
				}
				AddProduced(Good.Id, Buildable);
				Completed.Add(Good.Id);
				bProgress = true;
			}
			if (!bProgress)
			{
				break;
			}
		}
	}

	for (const TPair<FString, int64>& Pair : Inventory)
	{
		if (Pair.Value > 0)
		{
			Ledger.FinalSupply.Add(Pair.Key, Pair.Value);
		}
	}
	return Ledger;
}

FWLProductionLedger UWLStrategicTickSubsystem::BuildNationProductionLedger(const FString& NationIso) const
{
	FWLProductionLedger Ledger;
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return Ledger;
	}

	const FString NormalizedIso = NormalizeIso(NationIso);
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) != NormalizedIso)
		{
			continue;
		}
		const FWLProductionLedger ProvinceLedger = BuildProvinceProductionLedger(Province.Id);
		for (const TPair<FString, int64>& Pair : ProvinceLedger.Produced)
		{
			AddUnits(Ledger.Produced, Pair.Key, Pair.Value);
		}
		for (const TPair<FString, int64>& Pair : ProvinceLedger.UsedAsInput)
		{
			AddUnits(Ledger.UsedAsInput, Pair.Key, Pair.Value);
		}
		for (const TPair<FString, int64>& Pair : ProvinceLedger.FinalSupply)
		{
			AddUnits(Ledger.FinalSupply, Pair.Key, Pair.Value);
		}
	}
	return Ledger;
}

TArray<FWLGoodOutput> UWLStrategicTickSubsystem::GetProvinceProduction(const FString& ProvinceId) const
{
	return MapToGoodOutputs(BuildProvinceProductionLedger(ProvinceId).Produced);
}

TArray<FWLGoodOutput> UWLStrategicTickSubsystem::GetNationProduction(const FString& NationIso) const
{
	return MapToGoodOutputs(BuildNationProductionLedger(NationIso).Produced);
}

int64 UWLStrategicTickSubsystem::GetNationPopulation(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}

	const FString NormalizedIso = NormalizeIso(NationIso);
	int64 Population = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) != NormalizedIso)
		{
			continue;
		}

		FWLProvinceRuntimeState State;
		Population += GetProvinceState(Province.Id, State)
			? FMath::Max<int64>(0, State.Population)
			: FMath::Max<int64>(0, Province.Population);
	}
	return Population;
}

TMap<FString, int64> UWLStrategicTickSubsystem::BuildNationDemandMap(const FString& NationIso) const
{
	TMap<FString, int64> Demand;
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return Demand;
	}

	const double PopulationMillions = static_cast<double>(GetNationPopulation(NationIso)) / 1000000.0;
	for (const FWLGoodData& Good : Registry->GetAllGoods())
	{
		const int64 Units = RoundUnits(PopulationMillions * Good.DemandPerMillion);
		if (Units > 0)
		{
			Demand.Add(Good.Id, Units);
		}
	}
	return Demand;
}

FWLNationLaborStats UWLStrategicTickSubsystem::GetNationLaborStats(const FString& NationIso) const
{
	FWLNationLaborStats Stats;
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return Stats;
	}

	const FString NormalizedIso = NormalizeIso(NationIso);
	double ProductivityWeighted = 0.0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) != NormalizedIso)
		{
			continue;
		}
		const FWLSectorEmployment Employment = GetProvinceEmployment(Province.Id);
		const int64 Employed = Employment.Extraction + Employment.Manufacturing + Employment.Services;
		Stats.Workforce += Employment.Workforce;
		Stats.Employed += Employed;
		Stats.Unemployed += Employment.Unemployed;
		ProductivityWeighted += Employment.Productivity * static_cast<double>(Employed);
	}
	Stats.UnemploymentRate = Stats.Workforce > 0
		? FMath::Clamp(static_cast<double>(Stats.Unemployed) / static_cast<double>(Stats.Workforce), 0.0, 1.0)
		: 0.0;
	Stats.Productivity = Stats.Employed > 0
		? FMath::Clamp(ProductivityWeighted / static_cast<double>(Stats.Employed), 0.0, 1.0)
		: 1.0;
	return Stats;
}

TArray<FWLGoodMarketBalance> UWLStrategicTickSubsystem::GetNationGoodMarketBalance(const FString& NationIso) const
{
	TArray<FWLGoodMarketBalance> Balances;
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return Balances;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	const FString NormalizedIso = NormalizeIso(NationIso);
	const FWLProductionLedger Domestic = BuildNationProductionLedger(NormalizedIso);
	const TMap<FString, int64> DomesticDemand = BuildNationDemandMap(NormalizedIso);
	const double DomesticTariffImportMultiplier = GetTariffImportVolumeMultiplier(NormalizedIso);
	const int32 DomesticTariffRate = GetTariffRate(NormalizedIso);

	struct FWLPartnerMarketCache
	{
		FString Iso;
		FWLProductionLedger Production;
		TMap<FString, int64> Demand;
		FWLTradeRouteState Route;
		double ImportVolumeMultiplier = 1.0;
	};
	TArray<FWLPartnerMarketCache> Partners;
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		if (Nation.Iso == NormalizedIso)
		{
			continue;
		}

		FWLPartnerMarketCache Partner;
		Partner.Iso = Nation.Iso;
		Partner.Production = BuildNationProductionLedger(Nation.Iso);
		Partner.Demand = BuildNationDemandMap(Nation.Iso);
		Partner.Route = GetTradeRouteBetween(NormalizedIso, Nation.Iso);
		Partner.ImportVolumeMultiplier = GetTariffImportVolumeMultiplier(Nation.Iso);
		Partners.Add(MoveTemp(Partner));
	}

	for (const FWLGoodData& Good : Registry->GetAllGoods())
	{
		FWLGoodMarketBalance Balance;
		Balance.GoodId = Good.Id;
		Balance.Production = ReadUnits(Domestic.Produced, Good.Id);
		Balance.UsedAsInput = ReadUnits(Domestic.UsedAsInput, Good.Id);
		Balance.DomesticSupply = ReadUnits(Domestic.FinalSupply, Good.Id);
		Balance.Demand = ReadUnits(DomesticDemand, Good.Id);

		const int64 DeficitBeforeTrade = FMath::Max<int64>(0, Balance.Demand - Balance.DomesticSupply);
		const int64 SurplusBeforeTrade = FMath::Max<int64>(0, Balance.DomesticSupply - Balance.Demand);
		const int64 ImportableNeed = RoundUnits(static_cast<double>(DeficitBeforeTrade) * DomesticTariffImportMultiplier);
		int64 RegionalImportCapacity = 0;
		int64 RegionalExportDemand = 0;
		double RouteMultiplierWeighted = 0.0;
		double RouteMultiplierWeight = 0.0;
		for (const FWLPartnerMarketCache& Partner : Partners)
		{
			const int64 PartnerSupply = ReadUnits(Partner.Production.FinalSupply, Good.Id);
			const int64 PartnerNeed = ReadUnits(Partner.Demand, Good.Id);
			const int64 PartnerSurplus = FMath::Max<int64>(0, PartnerSupply - PartnerNeed);
			const int64 PartnerDeficit = FMath::Max<int64>(0, PartnerNeed - PartnerSupply);
			const double RouteAccess = FMath::Max(0.0, Partner.Route.AccessMultiplier);

			RegionalImportCapacity += RoundUnits(
				static_cast<double>(PartnerSurplus) * Rules.RegionalTradeAccess * RouteAccess);
			RegionalExportDemand += RoundUnits(
				static_cast<double>(PartnerDeficit) * Rules.RegionalTradeAccess * RouteAccess * Partner.ImportVolumeMultiplier);

			const double RouteWeight = static_cast<double>(PartnerSurplus + PartnerDeficit);
			if (RouteWeight > 0.0)
			{
				RouteMultiplierWeighted += RouteAccess * RouteWeight;
				RouteMultiplierWeight += RouteWeight;
			}
		}
		Balance.RegionalTradeRouteMultiplier = RouteMultiplierWeight > 0.0
			? RouteMultiplierWeighted / RouteMultiplierWeight
			: 1.0;

		const int64 RegionalImports = FMath::Min<int64>(ImportableNeed, RegionalImportCapacity);
		const int64 RemainingImportNeed = FMath::Max<int64>(0, ImportableNeed - RegionalImports);
		Balance.Imports = RegionalImports
			+ RoundUnits(static_cast<double>(RemainingImportNeed) * Rules.GlobalTradeAccess);

		Balance.Exports = FMath::Min<int64>(
			SurplusBeforeTrade,
			RegionalExportDemand
				+ RoundUnits(static_cast<double>(SurplusBeforeTrade - FMath::Min<int64>(SurplusBeforeTrade, RegionalExportDemand))
					* Rules.GlobalTradeAccess));

		Balance.Supply = FMath::Max<int64>(0, Balance.DomesticSupply + Balance.Imports - Balance.Exports);
		Balance.Deficit = FMath::Max<int64>(0, Balance.Demand - Balance.Supply);
		Balance.Surplus = FMath::Max<int64>(0, Balance.Supply - Balance.Demand);
		Balance.TariffRatePercent = DomesticTariffRate;
		Balance.TariffImportVolumeMultiplier = DomesticTariffImportMultiplier;
		Balance.MarketShockMultiplier = GetMarketShockMultiplier(Good.Id);
		Balance.PriceMultiplier = FMath::Clamp(
			CalculatePriceMultiplier(Balance.Demand, Balance.Supply, Rules) * Balance.MarketShockMultiplier,
			Rules.MinMarketPriceMultiplier * Rules.MinMarketShockPriceMultiplier,
			Rules.MaxMarketPriceMultiplier * Rules.MaxMarketShockPriceMultiplier);
		Balance.UnitPrice = static_cast<double>(Good.BasePrice) * Balance.PriceMultiplier;
		Balance.ProductionValue = static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(Balance.DomesticSupply) * Balance.UnitPrice));
		Balance.ImportCost = static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(Balance.Imports) * Balance.UnitPrice * (1.0 + Rules.ImportPriceMarkup)));
		Balance.ImportTariffIncome = static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(Balance.Imports) * Balance.UnitPrice * (static_cast<double>(DomesticTariffRate) / 100.0)));
		Balance.ExportRevenue = static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(Balance.Exports) * Balance.UnitPrice * (1.0 - Rules.ExportPriceDiscount)));
		Balances.Add(MoveTemp(Balance));
	}

	Balances.Sort([](const FWLGoodMarketBalance& A, const FWLGoodMarketBalance& B)
	{
		return A.ProductionValue == B.ProductionValue ? A.GoodId < B.GoodId : A.ProductionValue > B.ProductionValue;
	});
	return Balances;
}

TArray<FWLMarketShockState> UWLStrategicTickSubsystem::GetActiveMarketShocks() const
{
	TArray<FWLMarketShockState> Out;
	for (const FWLMarketShockState& Shock : ActiveMarketShocks)
	{
		if (Shock.IsValid())
		{
			Out.Add(Shock);
		}
	}
	Out.Sort([](const FWLMarketShockState& A, const FWLMarketShockState& B)
	{
		return A.ShockId < B.ShockId;
	});
	return Out;
}

double UWLStrategicTickSubsystem::GetMarketShockMultiplier(const FString& GoodId) const
{
	const FString NormalizedGoodId = NormalizeGoodId(GoodId);
	if (NormalizedGoodId.IsEmpty())
	{
		return 1.0;
	}

	double Multiplier = 1.0;
	for (const FWLMarketShockState& Shock : ActiveMarketShocks)
	{
		if (Shock.IsValid() && ShockTargetsGood(Shock, NormalizedGoodId))
		{
			Multiplier *= Shock.PriceMultiplier;
		}
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	return FMath::Clamp(
		Multiplier,
		Rules.MinMarketShockPriceMultiplier,
		Rules.MaxMarketShockPriceMultiplier);
}

int32 UWLStrategicTickSubsystem::GetTariffRate(const FString& NationIso) const
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	if (const int32* Found = TariffRates.Find(NormalizedIso))
	{
		return *Found;
	}
	return GetBalanceRules().TariffRateDefaultPercent;
}

int32 UWLStrategicTickSubsystem::SetTariffRate(const FString& NationIso, int32 RatePercent)
{
	const FString NormalizedIso = NormalizeIso(NationIso);
	const int32 Clamped = FMath::Clamp(RatePercent, 0, GetBalanceRules().TariffRateMaxPercent);
	TariffRates.Add(NormalizedIso, Clamped);
	return Clamped;
}

double UWLStrategicTickSubsystem::GetTariffImportVolumeMultiplier(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	return FMath::Clamp(
		1.0 - static_cast<double>(GetTariffRate(NationIso)) * Rules.TariffImportPenaltyPerPoint,
		0.0,
		1.0);
}

FWLTradeRouteState UWLStrategicTickSubsystem::GetTradeRouteBetween(const FString& NationA, const FString& NationB) const
{
	FWLTradeRouteState Route;
	Route.NationA = NormalizeIso(NationA);
	Route.NationB = NormalizeIso(NationB);
	Route.Reason = TEXT("Ruta normal");

	if (Route.NationA.IsEmpty() || Route.NationB.IsEmpty() || Route.NationA == Route.NationB)
	{
		Route.bOpen = false;
		Route.AccessMultiplier = 0.0;
		Route.Reason = TEXT("Ruta invalida");
		return Route;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLPoliticalSubsystem* Politics = GI->GetSubsystem<UWLPoliticalSubsystem>())
		{
			FWLDiplomaticRelationState Relation;
			if (Politics->GetRelation(Route.NationA, Route.NationB, Relation))
			{
				Route.bTradeAgreement = Relation.Treaties.Contains(EWLTreatyType::TradeAgreement);
				Route.bEmbargoed = Relation.Treaties.Contains(EWLTreatyType::Embargo);
				Route.bAtWar = Relation.Status == EWLDiplomaticStatus::War;

				if (Route.bAtWar)
				{
					Route.bOpen = Rules.WarTradeRouteAccessMultiplier > 0.0;
					Route.AccessMultiplier = Rules.WarTradeRouteAccessMultiplier;
					Route.Reason = TEXT("Guerra / frontera cerrada");
					return Route;
				}
				if (Route.bEmbargoed)
				{
					Route.bOpen = Rules.EmbargoTradeRouteAccessMultiplier > 0.0;
					Route.AccessMultiplier = Rules.EmbargoTradeRouteAccessMultiplier;
					Route.Reason = TEXT("Embargo/sanciones");
					return Route;
				}

				Route.AccessMultiplier = Relation.Status == EWLDiplomaticStatus::Tension
					? Rules.TensionTradeRouteAccessMultiplier
					: 1.0;
				if (Route.bTradeAgreement)
				{
					Route.AccessMultiplier += Rules.TradeAgreementAccessBonus;
					Route.Reason = TEXT("Acuerdo comercial");
				}
				else if (Relation.Status == EWLDiplomaticStatus::Tension)
				{
					Route.Reason = TEXT("Tension diplomatica");
				}
			}
		}
	}

	Route.AccessMultiplier = FMath::Max(0.0, Route.AccessMultiplier);
	Route.bOpen = Route.AccessMultiplier > 0.0;
	return Route;
}

TArray<FWLTradeRouteState> UWLStrategicTickSubsystem::GetTradeRoutesForNation(const FString& NationIso) const
{
	TArray<FWLTradeRouteState> Routes;
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return Routes;
	}

	const FString NormalizedIso = NormalizeIso(NationIso);
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		if (Nation.Iso != NormalizedIso)
		{
			Routes.Add(GetTradeRouteBetween(NormalizedIso, Nation.Iso));
		}
	}
	Routes.Sort([](const FWLTradeRouteState& A, const FWLTradeRouteState& B)
	{
		return A.NationB < B.NationB;
	});
	return Routes;
}

bool UWLStrategicTickSubsystem::ApplyMarketShock(
	const FString& GoodId,
	double PriceMultiplier,
	int32 DurationMonths,
	const FString& Title,
	const FString& SourceEventId,
	FString& OutMessage)
{
	const FString NormalizedGoodId = NormalizeGoodId(GoodId);
	if (NormalizedGoodId.IsEmpty())
	{
		OutMessage = TEXT("Shock de mercado sin bien objetivo.");
		return false;
	}

	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}
	FWLGoodData Good;
	if (NormalizedGoodId != TEXT("*") && !Registry->GetGood(NormalizedGoodId, Good))
	{
		OutMessage = FString::Printf(TEXT("Bien no disponible para shock: %s"), *GoodId);
		return false;
	}

	const FWLBalanceRules Rules = GetBalanceRules();
	FWLMarketShockState Shock;
	Shock.ShockId = FString::Printf(TEXT("MS-%04d"), NextMarketShockNumber++);
	Shock.GoodId = NormalizedGoodId == TEXT("*") ? FString(TEXT("*")) : Good.Id;
	Shock.Title = Title.TrimStartAndEnd().IsEmpty()
		? FString::Printf(TEXT("Shock de mercado: %s"), Shock.GoodId == TEXT("*") ? TEXT("global") : *Shock.GoodId)
		: Title.TrimStartAndEnd();
	Shock.SourceEventId = SourceEventId.TrimStartAndEnd();
	Shock.PriceMultiplier = FMath::Clamp(
		PriceMultiplier,
		Rules.MinMarketShockPriceMultiplier,
		Rules.MaxMarketShockPriceMultiplier);
	Shock.TotalMonths = FMath::Clamp(DurationMonths, 1, Rules.MaxMarketShockDurationMonths);
	Shock.RemainingMonths = Shock.TotalMonths;
	ActiveMarketShocks.Add(Shock);

	OutMessage = FString::Printf(TEXT("%s aplicado a %s x%.2f por %d meses."),
		*Shock.Title, *Shock.GoodId, Shock.PriceMultiplier, Shock.RemainingMonths);
	return true;
}

bool UWLStrategicTickSubsystem::ClearMarketShock(const FString& ShockId, FString& OutMessage)
{
	const FString NormalizedShockId = ShockId.TrimStartAndEnd();
	const int32 Removed = ActiveMarketShocks.RemoveAll([&NormalizedShockId](const FWLMarketShockState& Shock)
	{
		return Shock.ShockId.Equals(NormalizedShockId, ESearchCase::IgnoreCase);
	});
	if (Removed <= 0)
	{
		OutMessage = FString::Printf(TEXT("Shock de mercado no encontrado: %s"), *ShockId);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Shock de mercado eliminado: %s"), *NormalizedShockId);
	return true;
}

void UWLStrategicTickSubsystem::AdvanceMarketShocks()
{
	for (FWLMarketShockState& Shock : ActiveMarketShocks)
	{
		--Shock.RemainingMonths;
	}
	ActiveMarketShocks.RemoveAll([](const FWLMarketShockState& Shock)
	{
		return !Shock.IsValid();
	});
}

bool UWLStrategicTickSubsystem::CompleteForeignInvestment(FWLForeignSupportState& Support, FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	FWLBuildingData Building;
	if (!Registry
		|| !Registry->GetProvince(Support.TargetProvinceId, Province)
		|| !Registry->GetBuilding(Support.TargetBuildingId, Building))
	{
		OutMessage = FString::Printf(TEXT("FDI %s no pudo completar: datos invalidos."), *Support.SupportId);
		return false;
	}
	if (GetProvinceControllerIso(Province.Id) != Support.RecipientIso)
	{
		OutMessage = FString::Printf(TEXT("FDI %s cancelado: %s ya no controla %s."),
			*Support.SupportId, *Support.RecipientIso, *Province.Id);
		return false;
	}
	if (!ProvinceSupportsBuilding(Province, Building))
	{
		OutMessage = FString::Printf(TEXT("FDI %s no encaja economicamente en %s."), *Support.SupportId, *Province.Id);
		return false;
	}
	TArray<FString>& Built = ProvinceBuildings.FindOrAdd(Province.Id);
	if (Built.Contains(Building.Id))
	{
		OutMessage = FString::Printf(TEXT("FDI %s: %s ya existe en %s."), *Support.SupportId, *Building.Name, *Province.Id);
		return false;
	}
	for (const FString& ExistingId : Built)
	{
		FWLBuildingData ExistingBuilding;
		if (Registry->GetBuilding(ExistingId, ExistingBuilding) && ExistingBuilding.Slot == Building.Slot)
		{
			OutMessage = FString::Printf(TEXT("FDI %s: slot ocupado por %s en %s."),
				*Support.SupportId, *ExistingBuilding.Name, *Province.Id);
			return false;
		}
	}

	Built.Add(Building.Id);
	Built.Sort();
	ProvinceBuildingLevels.FindOrAdd(Province.Id).Add(Building.Id, 1);
	Support.bCompleted = true;
	OutMessage = FString::Printf(TEXT("FDI %s completo: %s construido en %s para %s."),
		*Support.SupportId, *Building.Name, *Province.Id, *Support.RecipientIso);
	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}

void UWLStrategicTickSubsystem::AdvanceFinancialMonth()
{
	TSet<FString> NationsToCheck;

	for (FWLFinancialInstrumentState& Instrument : FinancialInstruments)
	{
		if (!Instrument.IsActive())
		{
			continue;
		}
		NationsToCheck.Add(Instrument.NationIso);
		const int64 InterestDue = static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(Instrument.PrincipalRemaining) * Instrument.MonthlyInterestRate));
		const int64 PrincipalPayment = FMath::Clamp<int64>(
			Instrument.MonthlyPayment - InterestDue,
			1,
			Instrument.PrincipalRemaining);
		Instrument.PrincipalRemaining = FMath::Max<int64>(0, Instrument.PrincipalRemaining - PrincipalPayment);
		Instrument.RemainingMonths = FMath::Max(0, Instrument.RemainingMonths - 1);
		if (Instrument.PrincipalRemaining <= 0 || Instrument.RemainingMonths <= 0)
		{
			Instrument.PrincipalRemaining = 0;
			Instrument.RemainingMonths = 0;
		}
	}

	for (FWLForeignSupportState& Support : ForeignSupportStates)
	{
		if (!Support.IsActive())
		{
			continue;
		}
		Support.AmountDelivered += Support.MonthlyAmount;
		Support.RemainingMonths = FMath::Max(0, Support.RemainingMonths - 1);
		if (Support.RemainingMonths <= 0)
		{
			if (Support.Type == EWLForeignSupportType::ForeignDirectInvestment)
			{
				FString Message;
				CompleteForeignInvestment(Support, Message);
			}
			else
			{
				Support.bCompleted = true;
			}
		}
	}

	FinancialInstruments.RemoveAll([](const FWLFinancialInstrumentState& Instrument)
	{
		return !Instrument.bDefaulted && Instrument.PrincipalRemaining <= 0;
	});
	ForeignSupportStates.RemoveAll([](const FWLForeignSupportState& Support)
	{
		return Support.bCompleted && Support.Type == EWLForeignSupportType::ForeignAid;
	});

	const UWLDataRegistry* Registry = GetDataRegistry();
	if (Registry)
	{
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			NationsToCheck.Add(Nation.Iso);
		}
	}
	const FWLBalanceRules Rules = GetBalanceRules();
	for (const FString& NationIso : NationsToCheck)
	{
		const FWLFinancialProfile Profile = GetFinancialProfile(NationIso);
		if (!Profile.bInDefault && Profile.DebtServiceIncomeRatio > Rules.DefaultDebtServiceIncomeRatio)
		{
			FString Message;
			MarkDebtDefault(NationIso, Message);
			UE_LOG(LogWorldLeader, Warning, TEXT("%s"), *Message);
		}
	}
}

int64 UWLStrategicTickSubsystem::GetNationMarketProductionValue(const FString& NationIso) const
{
	int64 Value = 0;
	for (const FWLGoodMarketBalance& Balance : GetNationGoodMarketBalance(NationIso))
	{
		Value += Balance.ProductionValue;
	}
	return Value;
}

int64 UWLStrategicTickSubsystem::GetNationTradeBalance(const FString& NationIso) const
{
	int64 TradeBalance = 0;
	for (const FWLGoodMarketBalance& Balance : GetNationGoodMarketBalance(NationIso))
	{
		TradeBalance += Balance.ExportRevenue - Balance.ImportCost;
	}
	return TradeBalance;
}

double UWLStrategicTickSubsystem::GetNationInflationRate(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	double Pressure = 0.0;
	double Weight = 0.0;
	for (const FWLGoodMarketBalance& Balance : GetNationGoodMarketBalance(NationIso))
	{
		const double GoodWeight = static_cast<double>(FMath::Max<int64>(Balance.Demand, 1))
			* FMath::Max(0.0, Balance.UnitPrice);
		Pressure += (Balance.PriceMultiplier - 1.0) * GoodWeight;
		Weight += GoodWeight;
	}
	if (Weight <= 0.0)
	{
		return 0.0;
	}
	return FMath::Clamp(
		(Pressure / Weight) * Rules.InflationPressureToMonthlyRate,
		Rules.MinMonthlyInflationRate,
		Rules.MaxMonthlyInflationRate);
}

double UWLStrategicTickSubsystem::GetNationEconomicCycleScore(const FString& NationIso) const
{
	const FWLBalanceRules Rules = GetBalanceRules();
	const int64 GDP = FMath::Max<int64>(1, GetNationGDP(NationIso));
	const double TradeRatio = static_cast<double>(GetNationTradeBalance(NationIso)) / static_cast<double>(GDP);
	const double Inflation = FMath::Max(0.0, GetNationInflationRate(NationIso));
	const FWLNationLaborStats Labor = GetNationLaborStats(NationIso);

	return GetNationGDPGrowth(NationIso)
		+ TradeRatio * Rules.CycleTradeBalanceWeight
		- Labor.UnemploymentRate * Rules.CycleUnemploymentWeight
		- Inflation * Rules.CycleInflationWeight;
}

FString UWLStrategicTickSubsystem::GetNationEconomicCycleLabel(const FString& NationIso) const
{
	const double Score = GetNationEconomicCycleScore(NationIso);
	if (Score > 0.01)
	{
		return TEXT("Expansion");
	}
	if (Score < -0.01)
	{
		return TEXT("Recesion");
	}
	return TEXT("Estable");
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
	double Upkeep = static_cast<double>(GetNationMilitaryStrength(NationIso)) * Rules.MilitaryUpkeepPerStrength;
	// Fase 3 auditoria: un buen ministro de Defensa abarata el mantenimiento; uno inepto lo encarece.
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UWLCharacterSubsystem* Characters = GI->GetSubsystem<UWLCharacterSubsystem>())
		{
			const double Factor = Characters->GetMinisterEffectFactor(NormalizeIso(NationIso), EWLMinisterOffice::Defense);
			Upkeep *= FMath::Max(0.0, 1.0 - Factor * Rules.DefenseMinisterUpkeepEffect);
		}
	}
	return static_cast<int64>(FMath::RoundToDouble(Upkeep));
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

bool UWLStrategicTickSubsystem::AdjustProvincePublicOrder(
	const FString& ProvinceId,
	int32 Delta,
	FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		OutMessage = FString::Printf(TEXT("Provincia desconocida: %s"), *ProvinceId);
		return false;
	}

	FWLProvinceRuntimeState& State = ProvinceStates.FindOrAdd(Province.Id);
	if (State.ProvinceId.IsEmpty())
	{
		State.ProvinceId = Province.Id;
		State.ControllerIso = Province.CountryIso;
		State.Population = FMath::Max<int64>(0, Province.Population);
		State.PublicOrder = GetBalanceRules().InitialPublicOrder;
	}
	const int32 Previous = ClampPublicOrder(State.PublicOrder);
	State.PublicOrder = ClampPublicOrder(Previous + Delta);
	OutMessage = FString::Printf(TEXT("%s orden publico %d -> %d."), *Province.Id, Previous, State.PublicOrder);
	return true;
}

int32 UWLStrategicTickSubsystem::AdjustNationPublicOrder(const FString& NationIso, int32 Delta)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}

	const FString NormalizedIso = NormalizeIso(NationIso);
	int32 Affected = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (GetProvinceControllerIso(Province.Id) != NormalizedIso)
		{
			continue;
		}
		FString Message;
		if (AdjustProvincePublicOrder(Province.Id, Delta, Message))
		{
			++Affected;
		}
	}
	return Affected;
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
	const FString ControllerIso = State.ControllerIso.IsEmpty() ? Province.CountryIso : NormalizeIso(State.ControllerIso);
	const FWLEconomicGovernanceStats Governance = GetEconomicGovernanceStats(ControllerIso);
	const double TaxMultiplier =
		UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(GetTaxRate(ControllerIso), Rules)
		* Governance.TaxCollectionMultiplier;
	const int64 BasePopulationTax = UWLEconomyLibrary::CalculateProvincePopulationTax(EffectiveProvince, Rules);
	const int64 GrossTax = static_cast<int64>(
		FMath::RoundToDouble(static_cast<double>(BasePopulationTax) * TaxMultiplier));

	const int64 GrossResourceIncome = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(
			UWLEconomyLibrary::CalculateProvinceIncomeWithRules(EffectiveProvince, Rules)
			- BasePopulationTax
			+ GetProvinceBuildingIncome(Province.Id, Rules))
		* Governance.ProductivityMultiplier));
	const int64 GrossIncome =
		GrossResourceIncome
		+ GrossTax;

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

	return UWLEconomyLibrary::CalculateProvinceUpkeepWithRules(Province, GetBalanceRules())
		+ GetProvinceBuildingEffects(Province.Id).MonthlyUpkeep;
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

bool UWLStrategicTickSubsystem::AdjustTreasury(const FString& NationIso, int64 Delta, FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLNationData Nation;
	if (!Registry || !Registry->GetNation(NationIso, Nation))
	{
		OutMessage = FString::Printf(TEXT("Nacion desconocida: %s"), *NationIso);
		return false;
	}
	int64& Treasury = Treasuries.FindOrAdd(Nation.Iso);
	Treasury += Delta;
	OutMessage = FString::Printf(TEXT("%s tesoro ajustado %+lld -> %lld."),
		*Nation.Iso,
		static_cast<long long>(Delta),
		static_cast<long long>(Treasury));
	return true;
}
