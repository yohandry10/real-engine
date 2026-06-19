// Copyright World Leader project. See ROADMAP.md.
//
// Tests del estado mutable de provincias de Phase 1: poblacion, orden publico
// y persistencia por snapshot de campania.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLProvinceStateMonthlyTickTest,
	"WorldLeader.ProvinceState.MonthlyTickUpdatesPopulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLProvinceStateMonthlyTickTest::RunTest(const FString& Parameters)
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

	FWLProvinceRuntimeState Before;
	TestTrue(TEXT("Estado inicial VE-ZU"), Tick->GetProvinceState(TEXT("ve-zu"), Before));
	TestTrue(TEXT("Poblacion inicial positiva"), Before.Population > 0);
	TestTrue(TEXT("Orden publico inicial valido"), Before.PublicOrder >= 0 && Before.PublicOrder <= 100);

	Tick->AdvanceMonth();

	FWLProvinceRuntimeState After;
	TestTrue(TEXT("Estado post tick VE-ZU"), Tick->GetProvinceState(TEXT("VE-ZU"), After));
	TestTrue(TEXT("La poblacion crece con orden publico estable"), After.Population > Before.Population);
	TestTrue(TEXT("Orden publico post tick valido"), After.PublicOrder >= 0 && After.PublicOrder <= 100);
	TestTrue(TEXT("Balance provincial runtime disponible"), Tick->GetProvinceMonthlyBalance(TEXT("VE-ZU")) != 0);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLProvinceStateSnapshotTest,
	"WorldLeader.ProvinceState.SnapshotRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLProvinceStateSnapshotTest::RunTest(const FString& Parameters)
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

	int32 Year = 0;
	int32 Month = 0;
	TArray<FWLNationTreasurySave> Treasuries;
	TArray<FWLProvinceBuildingsSave> Buildings;
	TArray<FWLProvinceRuntimeState> States;
	Tick->WriteSaveSnapshot(Year, Month, Treasuries, Buildings, States);

	bool bMutated = false;
	for (FWLProvinceRuntimeState& State : States)
	{
		if (State.ProvinceId == TEXT("VE-ZU"))
		{
			State.ControllerIso = TEXT("CO");
			State.Population = 123456;
			State.PublicOrder = 34;
			bMutated = true;
			break;
		}
	}
	TestTrue(TEXT("Snapshot contiene VE-ZU"), bMutated);

	FString Message;
	TestTrue(TEXT("Restaurar snapshot con estado de provincia"),
		Tick->RestoreSaveSnapshot(Year, Month, Treasuries, Buildings, States, Message));

	FWLProvinceRuntimeState Restored;
	TestTrue(TEXT("Estado restaurado VE-ZU"), Tick->GetProvinceState(TEXT("VE-ZU"), Restored));
	TestEqual(TEXT("Controlador restaurado"), Restored.ControllerIso, FString(TEXT("CO")));
	TestEqual(TEXT("Poblacion restaurada"), Restored.Population, static_cast<int64>(123456));
	TestEqual(TEXT("Orden publico restaurado"), Restored.PublicOrder, 34);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLProvinceControllerEconomyTest,
	"WorldLeader.ProvinceState.ControllerDrivesEconomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLProvinceControllerEconomyTest::RunTest(const FString& Parameters)
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

	const int64 VeBefore = Tick->GetMonthlyBalance(TEXT("VE"));
	const int64 CoBefore = Tick->GetMonthlyBalance(TEXT("CO"));
	const int64 ZuliaBalance = Tick->GetProvinceMonthlyBalance(TEXT("VE-ZU"));

	FString Message;
	TestTrue(TEXT("Transferir control de VE-ZU a CO"),
		Tick->SetProvinceController(TEXT("VE-ZU"), TEXT("CO"), Message));

	const int64 ZuliaBalanceAfterOccupation = Tick->GetProvinceMonthlyBalance(TEXT("VE-ZU"));
	TestTrue(TEXT("La ocupacion penaliza temporalmente el balance provincial"),
		ZuliaBalanceAfterOccupation < ZuliaBalance);
	TestEqual(TEXT("VE pierde el balance de Zulia"),
		Tick->GetMonthlyBalance(TEXT("VE")), VeBefore - ZuliaBalance);
	TestEqual(TEXT("CO gana el balance de Zulia"),
		Tick->GetMonthlyBalance(TEXT("CO")), CoBefore + ZuliaBalanceAfterOccupation);
	TestEqual(TEXT("Controlador canonico"), Tick->GetProvinceControllerIso(TEXT("ve-zu")), FString(TEXT("CO")));

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
