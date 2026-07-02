// Copyright World Leader project. See ROADMAP.md.
//
// Tests de la IA economica simple de Phase 1.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Engine/GameInstance.h"

namespace
{
	int32 CountBuildingsForControlledNation(const UWLDataRegistry* Registry, const UWLStrategicTickSubsystem* Tick, const FString& NationIso)
	{
		if (!Registry || !Tick)
		{
			return 0;
		}

		int32 Count = 0;
		for (const FWLProvinceData& Province : Registry->GetAllProvinces())
		{
			if (Tick->GetProvinceControllerIso(Province.Id) == NationIso)
			{
				Count += Tick->GetProvinceBuildings(Province.Id).Num();
			}
		}
		return Count;
	}

	int32 CountNationsExcept(const UWLDataRegistry* Registry, const FString& ExcludedNationIso)
	{
		if (!Registry)
		{
			return 0;
		}

		const FString ExcludedIso = ExcludedNationIso.TrimStartAndEnd().ToUpper();
		int32 Count = 0;
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			if (Nation.Iso != ExcludedIso)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 CountNationsWithBuildingsExcept(
		const UWLDataRegistry* Registry,
		const UWLStrategicTickSubsystem* Tick,
		const FString& ExcludedNationIso)
	{
		if (!Registry || !Tick)
		{
			return 0;
		}

		const FString ExcludedIso = ExcludedNationIso.TrimStartAndEnd().ToUpper();
		int32 Count = 0;
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			if (Nation.Iso != ExcludedIso && CountBuildingsForControlledNation(Registry, Tick, Nation.Iso) > 0)
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLEconomicAIBuildsForNonPlayerTest,
	"WorldLeader.EconomyAI.BuildsForNonPlayerOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLEconomicAIBuildsForNonPlayerTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	const UWLDataRegistry* Registry = GameInstance->GetSubsystem<UWLDataRegistry>();
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Data registry"), Registry);
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Registry || !Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	const int64 ColombiaTreasuryBefore = Tick->GetTreasury(TEXT("CO"));
	const int32 ExpectedAINations = CountNationsExcept(Registry, TEXT("VE"));
	TArray<FString> Reports;
	const int32 Built = Tick->RunEconomicAI(TEXT("VE"), Reports);

	TestTrue(TEXT("America completa participa como IA no jugadora"), ExpectedAINations >= 30);
	TestEqual(TEXT("Cada nacion IA construye una vez"), Built, ExpectedAINations);
	TestEqual(TEXT("Reportes de IA"), Reports.Num(), ExpectedAINations);
	TestEqual(TEXT("Jugador sin construcciones automaticas"),
		CountBuildingsForControlledNation(Registry, Tick, TEXT("VE")), 0);
	TestEqual(TEXT("Colombia IA con una construccion"),
		CountBuildingsForControlledNation(Registry, Tick, TEXT("CO")), 1);
	TestEqual(TEXT("Todas las naciones IA tienen construccion"),
		CountNationsWithBuildingsExcept(Registry, Tick, TEXT("VE")), ExpectedAINations);
	TestTrue(TEXT("IA gasta tesoro"), Tick->GetTreasury(TEXT("CO")) < ColombiaTreasuryBefore);
	TestEqual(TEXT("Ultimo conteo de IA"), Tick->GetLastEconomicAIBuildCount(), ExpectedAINations);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLEconomicAIRequiresCampaignForMonthlyTickTest,
	"WorldLeader.EconomyAI.MonthlyTickRequiresActiveCampaign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLEconomicAIRequiresCampaignForMonthlyTickTest::RunTest(const FString& Parameters)
{
	UWLCampaignGameInstance* CampaignGameInstance = NewObject<UWLCampaignGameInstance>();
	TestNotNull(TEXT("Campaign GameInstance"), CampaignGameInstance);
	if (!CampaignGameInstance)
	{
		return false;
	}
	CampaignGameInstance->Init();
	TestTrue(TEXT("Iniciar campania VE"), CampaignGameInstance->StartNewCampaign(TEXT("VE")));

	const UWLDataRegistry* Registry = CampaignGameInstance->GetSubsystem<UWLDataRegistry>();
	UWLStrategicTickSubsystem* Tick = CampaignGameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Data registry"), Registry);
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Registry || !Tick)
	{
		CampaignGameInstance->Shutdown();
		return false;
	}

	Tick->AdvanceMonth();
	const int32 ExpectedAINations = CountNationsExcept(Registry, TEXT("VE"));

	TestTrue(TEXT("America completa participa en tick mensual"), ExpectedAINations >= 30);
	TestEqual(TEXT("Tick mensual ejecuta IA no jugadora"), Tick->GetLastEconomicAIBuildCount(), ExpectedAINations);
	TestEqual(TEXT("Jugador sin auto-construccion"),
		CountBuildingsForControlledNation(Registry, Tick, TEXT("VE")), 0);
	TestEqual(TEXT("Colombia construye como IA"),
		CountBuildingsForControlledNation(Registry, Tick, TEXT("CO")), 1);
	TestEqual(TEXT("Todas las naciones IA construyen en tick mensual"),
		CountNationsWithBuildingsExcept(Registry, Tick, TEXT("VE")), ExpectedAINations);

	CampaignGameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLEconomicAIDifficultyCadenceTest,
	"WorldLeader.EconomyAI.DifficultyCadence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLEconomicAIDifficultyCadenceTest::RunTest(const FString& Parameters)
{
	auto RunCase = [this](EWLAIDifficulty Difficulty) -> int32
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		TestNotNull(TEXT("GameInstance"), GameInstance);
		if (!GameInstance)
		{
			return -1;
		}
		GameInstance->Init();

		UWLBalanceSubsystem* Balance = GameInstance->GetSubsystem<UWLBalanceSubsystem>();
		UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
		TestNotNull(TEXT("Balance subsystem"), Balance);
		TestNotNull(TEXT("Strategic tick subsystem"), Tick);
		if (!Balance || !Tick)
		{
			GameInstance->Shutdown();
			return -1;
		}

		FWLBalanceRules Rules = Balance->GetRules();
		Rules.AIDifficulty = Difficulty;
		Rules.EconomicAIMaxBuildsPerNationPerMonth = 1;
		Rules.EconomicAIMinTreasuryReserve = 0;
		Rules.EconomicAIMaxPaybackMonths = 0;
		Rules.EconomicAIMinPublicOrderToBuild = 0;
		Balance->SetRuntimeRules(Rules);

		TArray<FString> Reports;
		const int32 Built = Tick->RunEconomicAI(TEXT("VE"), Reports);
		GameInstance->Shutdown();
		return Built;
	};

	const int32 EasyBuilt = RunCase(EWLAIDifficulty::Easy);
	const int32 HardBuilt = RunCase(EWLAIDifficulty::Hard);
	TestTrue(TEXT("Facil sigue actuando economicamente"), EasyBuilt >= 1);
	TestTrue(TEXT("Dificil aumenta cadencia economica"), HardBuilt > EasyBuilt);
	return EasyBuilt >= 1 && HardBuilt > EasyBuilt;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLBuildingResourceFitTest,
	"WorldLeader.Construction.RequiresResourceFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLBuildingResourceFitTest::RunTest(const FString& Parameters)
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
	TestFalse(TEXT("Bogota no soporta pozo petrolero"),
		Tick->BuildBuilding(TEXT("CO-DC"), TEXT("oil_well"), Message));
	TestEqual(TEXT("No registra edificio invalido"),
		Tick->GetProvinceBuildings(TEXT("CO-DC")).Num(), 0);

	TestTrue(TEXT("Bogota soporta fabrica industrial"),
		Tick->BuildBuilding(TEXT("CO-DC"), TEXT("factory"), Message));
	TestEqual(TEXT("Registra fabrica valida"),
		Tick->GetProvinceBuildings(TEXT("CO-DC")).Num(), 1);

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
