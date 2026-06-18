// Copyright World Leader project. See ROADMAP.md.
//
// Test funcional de la economia provincial (Phase 0). Verifica que las
// formulas de WLEconomyLibrary den resultados deterministas conocidos.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/WLGameTypes.h"
#include "Economy/WLEconomyLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLEconomyProvinceBalanceTest,
	"WorldLeader.Economy.ProvinceBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLEconomyProvinceBalanceTest::RunTest(const FString& Parameters)
{
	// Datos identicos a la provincia de prueba VE-ZU (Zulia).
	FWLProvinceData P;
	P.Id            = TEXT("VE-ZU");
	P.BaseOil       = 800;  // 800 * 4 = 3200
	P.BaseGas       = 300;  // 300 * 3 =  900
	P.BaseFood      = 120;  // 120 * 1 =  120
	P.BaseMinerals  = 100;  // 100 * 2 =  200
	P.BaseIndustry  = 180;  // 180 * 5 =  900  -> recursos = 5320
	P.Population     = 5200000; // * 0.0005 = 2600 -> ingreso = 7920
	P.Infrastructure = 55;      // * 8 = 440 (upkeep)

	TestEqual(TEXT("Ingreso provincial"),
		UWLEconomyLibrary::CalculateProvinceIncome(P), static_cast<int64>(7920));
	TestEqual(TEXT("Upkeep provincial"),
		UWLEconomyLibrary::CalculateProvinceUpkeep(P), static_cast<int64>(440));
	TestEqual(TEXT("Balance provincial"),
		UWLEconomyLibrary::CalculateProvinceBalance(P), static_cast<int64>(7480));

	// Una provincia vacia no debe producir ingreso ni romperse.
	FWLProvinceData Empty;
	TestEqual(TEXT("Provincia vacia: ingreso"),
		UWLEconomyLibrary::CalculateProvinceIncome(Empty), static_cast<int64>(0));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
