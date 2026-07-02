// Copyright World Leader project. See ROADMAP.md.
//
// Test funcional del balance editable de Phase 1. Garantiza que economia y
// construccion pueden evaluarse con reglas distintas a los defaults.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Balance/WLBalanceTypes.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Core/WLGameTypes.h"
#include "Economy/WLEconomyLibrary.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLBalanceRulesEconomyTest,
	"WorldLeader.Balance.CustomRulesAffectEconomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLBalanceRulesEconomyTest::RunTest(const FString& Parameters)
{
	FWLBalanceRules Rules;
	Rules.OilPrice = 10;
	Rules.GasPrice = 20;
	Rules.FoodPrice = 30;
	Rules.MineralsPrice = 40;
	Rules.IndustryValue = 50;
	Rules.PopulationTaxPerCapita = 0.1;
	Rules.InfrastructureUpkeepFactor = 3;

	FWLProvinceData Province;
	Province.BaseOil = 2;
	Province.BaseGas = 3;
	Province.BaseFood = 4;
	Province.BaseMinerals = 5;
	Province.BaseIndustry = 6;
	Province.Population = 1000;
	Province.Infrastructure = 7;

	TestEqual(TEXT("Ingreso provincial con reglas custom"),
		UWLEconomyLibrary::CalculateProvinceIncomeWithRules(Province, Rules), static_cast<int64>(800));
	TestEqual(TEXT("Upkeep provincial con reglas custom"),
		UWLEconomyLibrary::CalculateProvinceUpkeepWithRules(Province, Rules), static_cast<int64>(21));
	TestEqual(TEXT("Balance provincial con reglas custom"),
		UWLEconomyLibrary::CalculateProvinceBalanceWithRules(Province, Rules), static_cast<int64>(779));

	FWLBuildingData Building;
	Building.BonusOil = 1;
	Building.BonusGas = 2;
	Building.BonusFood = 3;
	Building.BonusMinerals = 4;
	Building.BonusIndustry = 5;

	TestEqual(TEXT("Ingreso edificio con reglas custom"),
		UWLEconomyLibrary::CalculateBuildingIncomeWithRules(Building, Rules), static_cast<int64>(550));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTaxLeverLafferTest,
	"WorldLeader.Balance.TaxLeverLaffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTaxLeverLafferTest::RunTest(const FString& Parameters)
{
	const FWLBalanceRules Rules = FWLBalanceRules::Default();

	const double AtDefault = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(Rules.TaxRateDefaultPercent, Rules);
	TestEqual(TEXT("Tasa default recauda x1.0"), AtDefault, 1.0, 1e-9);

	const double AtMax = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(Rules.TaxRateMaxPercent, Rules);
	const double AtMin = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(Rules.TaxRateMinPercent, Rules);
	TestTrue(TEXT("Subir impuestos recauda mas"), AtMax > 1.0);
	TestTrue(TEXT("Bajar impuestos recauda menos"), AtMin < 1.0);

	// Rendimiento decreciente (Laffer): doblar la tasa NO dobla la recaudacion.
	const double RateRatio = static_cast<double>(Rules.TaxRateMaxPercent) / static_cast<double>(Rules.TaxRateDefaultPercent);
	TestTrue(TEXT("Curva Laffer: rendimiento decreciente"), AtMax < RateRatio);

	// Fuera de rango se clampa a los limites de la palanca.
	TestEqual(TEXT("Tasa sobre el maximo clampeada"),
		UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(100, Rules), AtMax, 1e-9);
	TestEqual(TEXT("Tasa bajo el minimo clampeada"),
		UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(0, Rules), AtMin, 1e-9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLNationBudgetBreakdownTest,
	"WorldLeader.Balance.NationBudgetBreakdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLNationBudgetBreakdownTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	const FWLNationBudget Budget = Tick->GetNationBudget(TEXT("VE"));
	TestTrue(TEXT("Ingreso por recursos/produccion"), Budget.ResourceIncome > 0);
	TestTrue(TEXT("Ingreso por impuestos"), Budget.TaxIncome > 0);
	TestTrue(TEXT("Gasto militar (fuerzas desplegadas)"), Budget.MilitaryUpkeep > 0);
	TestTrue(TEXT("Gasto en infraestructura"), Budget.InfrastructureUpkeep > 0);
	TestTrue(TEXT("Salarios publicos"), Budget.PublicWages > 0);
	TestTrue(TEXT("Gasto social"), Budget.SocialSpending > 0);
	TestEqual(TEXT("El neto del presupuesto ES el balance mensual"),
		Budget.Net(), Tick->GetMonthlyBalance(TEXT("VE")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLDebtAndCreditLimitTest,
	"WorldLeader.Balance.DebtAndCreditLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLDebtAndCreditLimitTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const int64 NetWithoutDebt = Tick->GetMonthlyBalance(TEXT("VE"));
	TestEqual(TEXT("Sin deuda no hay intereses"), Tick->GetNationBudget(TEXT("VE")).DebtInterest, static_cast<int64>(0));

	// Forzar deuda: restaurar snapshot con el tesoro de VE en -100000.
	auto RestoreTreasury = [&](int64 Treasury)
	{
		TArray<FWLNationTreasurySave> Treasuries;
		FWLNationTreasurySave Saved;
		Saved.NationIso = TEXT("VE");
		Saved.Treasury = Treasury;
		Treasuries.Add(Saved);
		FString Message;
		return Tick->RestoreSaveSnapshot(Tick->GetCurrentYear(), Tick->GetCurrentMonth(),
			Treasuries, TArray<FWLProvinceBuildingsSave>(), TArray<FWLProvinceRuntimeState>(), Message);
	};
	TestTrue(TEXT("Restaurar tesoro en deuda"), RestoreTreasury(-100000));

	const int64 ExpectedInterest = static_cast<int64>(
		FMath::RoundToDouble(100000.0 * Rules.DebtMonthlyInterestRate));
	TestEqual(TEXT("Interes mensual sobre la deuda"),
		Tick->GetNationBudget(TEXT("VE")).DebtInterest, ExpectedInterest);
	TestEqual(TEXT("El interes drena el balance mensual"),
		Tick->GetMonthlyBalance(TEXT("VE")), NetWithoutDebt - ExpectedInterest);

	// Gastar en deficit genera deuda mientras quede credito.
	const int64 CreditLimit = Tick->GetCreditLimit(TEXT("VE"));
	TestTrue(TEXT("Limite de credito positivo"), CreditLimit > 0);
	FString Message;
	TestTrue(TEXT("Construir en deficit (dentro del credito)"),
		Tick->BuildBuilding(TEXT("VE-ZU"), TEXT("oil_well"), Message));
	TestTrue(TEXT("El gasto en deficit aumenta la deuda"), Tick->GetTreasury(TEXT("VE")) < -100000);

	// Con el credito agotado, el gasto se bloquea.
	TestTrue(TEXT("Restaurar tesoro al limite de credito"), RestoreTreasury(-Tick->GetCreditLimit(TEXT("VE"))));
	TestFalse(TEXT("Credito agotado bloquea construir"),
		Tick->BuildBuilding(TEXT("VE-ZU"), TEXT("oil_well"), Message));
	TestTrue(TEXT("Mensaje explica el credito"), Message.Contains(TEXT("Credito agotado")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLBalanceRulesSanitizeTest,
	"WorldLeader.Balance.SanitizeRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLBalanceRulesSanitizeTest::RunTest(const FString& Parameters)
{
	FWLBalanceRules Rules;
	Rules.OilPrice = -10;
	Rules.PopulationTaxPerCapita = -1.0;
	Rules.MonthlyPopulationGrowthRate = -0.25;
	Rules.InitialPublicOrder = 250;
	Rules.PublicOrderNeutral = 0;
	Rules.LowOrderIncomePenaltyAtZero = 2.0;
	Rules.HighOrderIncomeBonusAtMax = -1.0;
	Rules.PublicOrderDriftPerMonth = -5;
	Rules.PublicOrderDeficitPenalty = -3;
	Rules.PublicOrderBankruptcyPenalty = -4;
	Rules.OccupationPublicOrderPenalty = 250;
	Rules.InfrastructureUpkeepFactor = -2;
	Rules.PublicWagesPerCapita = -1.0;
	Rules.SocialSpendingPerCapita = -2.0;
	Rules.DebtMonthlyInterestRate = 5.0;
	Rules.DebtCreditLimitIncomeMonths = -3.0;
	Rules.EconomicAIMinTreasuryReserve = -5000;
	Rules.EconomicAIMaxBuildsPerNationPerMonth = -2;
	Rules.EconomicAIMaxPaybackMonths = -12;
	Rules.EconomicAIMinPublicOrderToBuild = 250;
	Rules.StartYear = -5;
	Rules.StartMonth = 99;
	Rules.MonthsPerYear = 6;

	const FWLBalanceRules Sanitized = Rules.Sanitized();
	TestEqual(TEXT("Precio negativo saneado"), Sanitized.OilPrice, 0);
	TestEqual(TEXT("Impuesto negativo saneado"), Sanitized.PopulationTaxPerCapita, 0.0);
	TestEqual(TEXT("Crecimiento negativo saneado"), Sanitized.MonthlyPopulationGrowthRate, 0.0);
	TestEqual(TEXT("Orden publico inicial clamp"), Sanitized.InitialPublicOrder, 100);
	TestEqual(TEXT("Orden publico neutral clamp"), Sanitized.PublicOrderNeutral, 1);
	TestEqual(TEXT("Penalizacion ingreso clamp"), Sanitized.LowOrderIncomePenaltyAtZero, 1.0);
	TestEqual(TEXT("Bonus ingreso clamp"), Sanitized.HighOrderIncomeBonusAtMax, 0.0);
	TestEqual(TEXT("Deriva orden publico saneada"), Sanitized.PublicOrderDriftPerMonth, 0);
	TestEqual(TEXT("Penalizacion deficit saneada"), Sanitized.PublicOrderDeficitPenalty, 0);
	TestEqual(TEXT("Penalizacion bancarrota saneada"), Sanitized.PublicOrderBankruptcyPenalty, 0);
	TestEqual(TEXT("Penalizacion ocupacion clamp"), Sanitized.OccupationPublicOrderPenalty, 100);
	TestEqual(TEXT("Upkeep negativo saneado"), Sanitized.InfrastructureUpkeepFactor, 0);
	TestEqual(TEXT("Salarios negativos saneados"), Sanitized.PublicWagesPerCapita, 0.0);
	TestEqual(TEXT("Gasto social negativo saneado"), Sanitized.SocialSpendingPerCapita, 0.0);
	TestEqual(TEXT("Interes de deuda clamp"), Sanitized.DebtMonthlyInterestRate, 1.0);
	TestEqual(TEXT("Limite de credito negativo saneado"), Sanitized.DebtCreditLimitIncomeMonths, 0.0);
	TestEqual(TEXT("Reserva IA saneada"), Sanitized.EconomicAIMinTreasuryReserve, static_cast<int64>(0));
	TestEqual(TEXT("Construcciones IA saneadas"), Sanitized.EconomicAIMaxBuildsPerNationPerMonth, 0);
	TestEqual(TEXT("Retorno IA saneado"), Sanitized.EconomicAIMaxPaybackMonths, 0);
	TestEqual(TEXT("Orden publico minimo IA clamp"), Sanitized.EconomicAIMinPublicOrderToBuild, 100);
	TestEqual(TEXT("Anio inicial saneado"), Sanitized.StartYear, 1);
	TestEqual(TEXT("Mes inicial clamp a calendario"), Sanitized.StartMonth, 6);
	TestEqual(TEXT("Meses por anio preservado"), Sanitized.MonthsPerYear, 6);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
