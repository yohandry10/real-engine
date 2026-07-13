// Copyright World Leader project. See ROADMAP.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Campaign/WLProvinceEngine.h"
#include "Economy/WLEconomyEngine.h"
#include "Economy/WLFiscalEngine.h"
#include "Economy/WLTradeEngine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	WLFiscalEngineAccrualTest,
	"WorldLeader.Campaign.Domain.FiscalAccrual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool WLFiscalEngineAccrualTest::RunTest(const FString& Parameters)
{
	int64 Treasury = 100;
	double Remainder = 0.0;
	for (int32 Day = 0; Day < 30; ++Day)
	{
		WLFiscalEngine::AccrueDailyBalance(100, 30, Treasury, Remainder);
	}
	TestEqual(TEXT("Devenga exactamente el balance mensual"), Treasury, static_cast<int64>(200));
	TestTrue(TEXT("No pierde fraccion"), FMath::IsNearlyZero(Remainder));

	Treasury = 0;
	Remainder = 0.0;
	for (int32 Day = 0; Day < 30; ++Day)
	{
		WLFiscalEngine::AccrueDailyBalance(-100, 30, Treasury, Remainder);
	}
	TestEqual(TEXT("Deuda simetrica"), Treasury, static_cast<int64>(-100));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	WLProvinceEngineTransitionTest,
	"WorldLeader.Campaign.Domain.ProvinceTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool WLProvinceEngineTransitionTest::RunTest(const FString& Parameters)
{
	FWLBalanceRules Rules = FWLBalanceRules::Default();
	FWLProvinceRuntimeState Current;
	Current.ProvinceId = TEXT("co-bogota");
	Current.ControllerIso = TEXT("CO");
	Current.Population = 100000;
	Current.PublicOrder = 40;
	FWLProvinceMonthInput Input;
	Input.MonthlyBalance = -1;
	Input.bControllerBankrupt = true;
	Input.TaxOrderPressure = 2;
	const FWLProvinceRuntimeState Result = FWLProvinceEngine::AdvanceMonth(Current, Input, Rules);
	TestTrue(TEXT("Penalizaciones reducen orden"), Result.PublicOrder < Current.PublicOrder);
	TestTrue(TEXT("Poblacion nunca negativa"), Result.Population >= 0);

	Current.Population = 0;
	Current.PublicOrder = -50;
	const FWLProvinceRuntimeState Clamped = FWLProvinceEngine::AdvanceMonth(Current, {}, Rules);
	TestTrue(TEXT("Orden saneado"), Clamped.PublicOrder >= 0 && Clamped.PublicOrder <= 100);
	TestEqual(TEXT("Poblacion cero estable"), Clamped.Population, static_cast<int64>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	WLEconomyAndTradeEngineTest,
	"WorldLeader.Campaign.Domain.EconomyAndTrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool WLEconomyAndTradeEngineTest::RunTest(const FString& Parameters)
{
	const FWLBalanceRules Rules = FWLBalanceRules::Default();
	const FWLEconomyRecurringCosts Costs = FWLEconomyEngine::CalculateRecurringCosts(
		100000, -10000, Rules.PublicWagesPerCapita,
		Rules.SocialSpendingPerCapita, Rules.DebtMonthlyInterestRate);
	TestTrue(TEXT("Salarios publicos positivos"), Costs.PublicWages > 0);
	TestTrue(TEXT("Gasto social positivo"), Costs.SocialSpending > 0);
	TestEqual(TEXT("Interes de deuda determinista"), Costs.DebtInterest,
		static_cast<int64>(FMath::RoundToDouble(10000.0 * Rules.DebtMonthlyInterestRate)));

	TestEqual(TEXT("Sin demanda usa precio base"),
		FWLTradeEngine::CalculatePriceMultiplier(0, 100, Rules), 1.0);
	TestEqual(TEXT("Sin oferta alcanza maximo"),
		FWLTradeEngine::CalculatePriceMultiplier(100, 0, Rules), Rules.MaxMarketPriceMultiplier);
	const FWLTradeValues Trade = FWLTradeEngine::CalculateTradeValues(100, 40, 5.0, 20, Rules);
	TestTrue(TEXT("Importaciones cuestan"), Trade.ImportCost > 0);
	TestEqual(TEXT("Arancel conserva formula"), Trade.ImportTariffIncome, static_cast<int64>(100));
	TestTrue(TEXT("Exportaciones generan ingreso"), Trade.ExportRevenue > 0);
	return true;
}

#endif
