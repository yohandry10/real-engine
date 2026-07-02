// Copyright World Leader project. See ROADMAP.md.
//
// Test funcional del ingreso que aportan los edificios (Phase 1).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Core/WLGameTypes.h"
#include "Engine/GameInstance.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLProvincialBuildingLevelsTest,
	"WorldLeader.Construction.ProvincialBuildingLevels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLProvincialBuildingLevelsTest::RunTest(const FString& Parameters)
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

	FString Message;
	const int64 TreasuryBefore = Tick->GetTreasury(TEXT("VE"));
	TestTrue(TEXT("Construye edificio nivel 1"),
		Tick->BuildBuilding(TEXT("VE-ZU"), TEXT("oil_well"), Message));
	TestEqual(TEXT("Nivel inicial"), Tick->GetProvinceBuildingLevel(TEXT("VE-ZU"), TEXT("oil_well")), 1);

	const FWLProvinceBuildingEffects Level1Effects = Tick->GetProvinceBuildingEffects(TEXT("VE-ZU"));
	TestEqual(TEXT("Ingreso edificio nivel 1"), Level1Effects.MonthlyIncome, static_cast<int64>(800));
	TestEqual(TEXT("Upkeep edificio nivel 1"), Level1Effects.MonthlyUpkeep, static_cast<int64>(90));
	TestEqual(TEXT("Bonus oil nivel 1"), Level1Effects.BonusOil, 200);

	TestFalse(TEXT("No permite segundo edificio del mismo slot"),
		Tick->BuildBuilding(TEXT("VE-ZU"), TEXT("gas_plant"), Message));

	const int64 UpgradeCost = Tick->GetProvinceBuildingUpgradeCost(TEXT("VE-ZU"), TEXT("oil_well"));
	TestTrue(TEXT("Coste de mejora positivo"), UpgradeCost > 0);
	TestTrue(TEXT("Mejora a nivel 2"),
		Tick->UpgradeBuilding(TEXT("VE-ZU"), TEXT("oil_well"), Message));
	TestEqual(TEXT("Nivel tras mejora"), Tick->GetProvinceBuildingLevel(TEXT("VE-ZU"), TEXT("oil_well")), 2);

	const FWLProvinceBuildingEffects Level2Effects = Tick->GetProvinceBuildingEffects(TEXT("VE-ZU"));
	TestEqual(TEXT("Ingreso edificio nivel 2"), Level2Effects.MonthlyIncome, static_cast<int64>(1600));
	TestEqual(TEXT("Upkeep edificio nivel 2"), Level2Effects.MonthlyUpkeep, static_cast<int64>(180));
	TestEqual(TEXT("Bonus oil nivel 2"), Level2Effects.BonusOil, 400);
	TestTrue(TEXT("Tesoro descuenta construir+mejorar"),
		Tick->GetTreasury(TEXT("VE")) < TreasuryBefore - UpgradeCost);

	int32 Year = 0;
	int32 Month = 0;
	TArray<FWLNationTreasurySave> Treasuries;
	TArray<FWLProvinceBuildingsSave> Buildings;
	TArray<FWLProvinceRuntimeState> ProvinceStates;
	Tick->WriteSaveSnapshot(Year, Month, Treasuries, Buildings, ProvinceStates);

	UGameInstance* RestoredGameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("Restored GameInstance"), RestoredGameInstance);
	if (!RestoredGameInstance)
	{
		GameInstance->Shutdown();
		return false;
	}
	RestoredGameInstance->Init();

	UWLStrategicTickSubsystem* RestoredTick = RestoredGameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Restored strategic tick"), RestoredTick);
	if (!RestoredTick)
	{
		RestoredGameInstance->Shutdown();
		GameInstance->Shutdown();
		return false;
	}

	TestTrue(TEXT("Restaura snapshot con niveles"),
		RestoredTick->RestoreSaveSnapshot(Year, Month, Treasuries, Buildings, ProvinceStates, Message));
	TestEqual(TEXT("Nivel restaurado"), RestoredTick->GetProvinceBuildingLevel(TEXT("VE-ZU"), TEXT("oil_well")), 2);
	TestEqual(TEXT("Efectos restaurados"),
		RestoredTick->GetProvinceBuildingEffects(TEXT("VE-ZU")).BonusOil, 400);

	RestoredGameInstance->Shutdown();
	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
