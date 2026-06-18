// Copyright World Leader project. See ROADMAP.md.
//
// Test funcional del ingreso que aportan los edificios (Phase 1).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/WLGameTypes.h"
#include "Economy/WLEconomyLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLBuildingIncomeTest,
	"WorldLeader.Construction.BuildingIncome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLBuildingIncomeTest::RunTest(const FString& Parameters)
{
	// oil_well: bonus_oil 200 -> 200 * PriceOil(4) = 800
	FWLBuildingData OilWell;
	OilWell.Id = TEXT("oil_well");
	OilWell.BonusOil = 200;
	TestEqual(TEXT("Ingreso pozo de petroleo"),
		UWLEconomyLibrary::CalculateBuildingIncome(OilWell), static_cast<int64>(800));

	// refinery: bonus_oil 80 (320) + bonus_industry 50 (250) = 570
	FWLBuildingData Refinery;
	Refinery.Id = TEXT("refinery");
	Refinery.BonusOil = 80;
	Refinery.BonusIndustry = 50;
	TestEqual(TEXT("Ingreso refineria"),
		UWLEconomyLibrary::CalculateBuildingIncome(Refinery), static_cast<int64>(570));

	// Un edificio sin bonus no aporta ingreso.
	FWLBuildingData Empty;
	Empty.Id = TEXT("empty");
	TestEqual(TEXT("Edificio sin bonus"),
		UWLEconomyLibrary::CalculateBuildingIncome(Empty), static_cast<int64>(0));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
