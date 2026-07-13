// Copyright World Leader project. See ROADMAP.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Campaign/WLCampaignSimulationAudit.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignTurnCoordinator.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "Military/WLMilitarySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Save/WLLocalSaveGame.h"
#include "Save/WLSaveMigration.h"

namespace
{
	FWLCampaignTurnResult RunDays(
		int32 DayCount,
		int64& Treasury,
		int32& MonthlyClosures,
		TArray<FString>* OutMonthlyHashes = nullptr)
	{
		FWLCampaignTurnResult State;
		State.Year = 2024;
		State.Month = 1;
		State.Day = 1;
		for (int32 Index = 0; Index < DayCount; ++Index)
		{
			FWLCampaignTurnRequest Request;
			Request.Year = State.Year;
			Request.Month = State.Month;
			Request.Day = State.Day;
			Request.MonthsPerYear = 12;
			Request.DaysPerMonth = 30;
			Request.ApplyDailyEconomy = [&Treasury]() { Treasury += 7; };
			Request.AdvanceFinancialMonth = [&Treasury, &MonthlyClosures]()
			{
				Treasury -= 11;
				++MonthlyClosures;
			};
			State = FWLCampaignTurnCoordinator::ExecuteDay(Request);
			if (State.bMonthRolled && OutMonthlyHashes)
			{
				UWLLocalSaveGame* Snapshot = NewObject<UWLLocalSaveGame>();
				Snapshot->SelectedNationIso = TEXT("CO");
				Snapshot->CurrentYear = State.Year;
				Snapshot->CurrentMonth = State.Month;
				Snapshot->CurrentDay = State.Day;
				FWLNationTreasurySave TreasuryRow;
				TreasuryRow.NationIso = TEXT("CO");
				TreasuryRow.Treasury = Treasury;
				Snapshot->NationTreasuries.Add(TreasuryRow);
				const FWLCampaignAuditResult Audit = FWLCampaignSimulationAudit::Audit(*Snapshot, 12, 30);
				OutMonthlyHashes->Add(Audit.bValid ? Audit.StableHash : TEXT("INVALID"));
			}
		}
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCampaignTurnPhaseOrderTest,
	"WorldLeader.Campaign.Simulation.TurnPhaseOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCampaignTurnPhaseOrderTest::RunTest(const FString& Parameters)
{
	FWLCampaignTurnRequest Request;
	Request.Year = 2024;
	Request.Month = 12;
	Request.Day = 30;
	const FWLCampaignTurnResult Result = FWLCampaignTurnCoordinator::ExecuteDay(Request);
	TestTrue(TEXT("Cierra el mes"), Result.bMonthRolled);
	TestEqual(TEXT("Avanza el anio"), Result.Year, 2025);
	TestEqual(TEXT("Reinicia el mes"), Result.Month, 1);
	TestEqual(TEXT("Reinicia el dia"), Result.Day, 1);

	const TArray<EWLCampaignTurnPhase> Expected = {
		EWLCampaignTurnPhase::Decisions,
		EWLCampaignTurnPhase::DailyEconomy,
		EWLCampaignTurnPhase::Recruitment,
		EWLCampaignTurnPhase::Calendar,
		EWLCampaignTurnPhase::Fiscal,
		EWLCampaignTurnPhase::Provinces,
		EWLCampaignTurnPhase::EconomySnapshot,
		EWLCampaignTurnPhase::Market,
		EWLCampaignTurnPhase::EconomicAI,
		EWLCampaignTurnPhase::Politics,
		EWLCampaignTurnPhase::Military,
		EWLCampaignTurnPhase::Battles,
		EWLCampaignTurnPhase::Consequences,
		EWLCampaignTurnPhase::Validation,
		EWLCampaignTurnPhase::Snapshot
	};
	TestEqual(TEXT("Cantidad de fases"), Result.ExecutedPhases.Num(), Expected.Num());
	for (int32 Index = 0; Index < Expected.Num() && Index < Result.ExecutedPhases.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Fase %d"), Index), Result.ExecutedPhases[Index], Expected[Index]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCampaignDeterministicSoakTest,
	"WorldLeader.Campaign.Simulation.DeterministicSoak",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCampaignDeterministicSoakTest::RunTest(const FString& Parameters)
{
	for (const int32 Months : { 12, 60, 120 })
	{
		int64 TreasuryA = 100000;
		int64 TreasuryB = 100000;
		int32 ClosuresA = 0;
		int32 ClosuresB = 0;
		TArray<FString> HashesA;
		TArray<FString> HashesB;
		const FWLCampaignTurnResult A = RunDays(Months * 30, TreasuryA, ClosuresA, &HashesA);
		const FWLCampaignTurnResult B = RunDays(Months * 30, TreasuryB, ClosuresB, &HashesB);
		TestEqual(FString::Printf(TEXT("Tesoro determinista a %d meses"), Months), TreasuryA, TreasuryB);
		TestEqual(FString::Printf(TEXT("Cierres a %d meses"), Months), ClosuresA, Months);
		TestEqual(FString::Printf(TEXT("Replay de cierres a %d meses"), ClosuresA), ClosuresA, ClosuresB);
		TestEqual(FString::Printf(TEXT("Anio determinista a %d meses"), Months), A.Year, B.Year);
		TestEqual(FString::Printf(TEXT("Mes determinista a %d meses"), Months), A.Month, B.Month);
		TestEqual(FString::Printf(TEXT("Hash por cada cierre a %d meses"), Months), HashesA.Num(), Months);
		TestEqual(FString::Printf(TEXT("Replay mensual identico a %d meses"), Months), HashesA, HashesB);
		TestFalse(FString::Printf(TEXT("Ningun snapshot invalido a %d meses"), Months), HashesA.Contains(TEXT("INVALID")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCampaignStableHashAndInvariantTest,
	"WorldLeader.Campaign.Simulation.StableHashAndInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCampaignStableHashAndInvariantTest::RunTest(const FString& Parameters)
{
	UWLLocalSaveGame* A = NewObject<UWLLocalSaveGame>();
	UWLLocalSaveGame* B = NewObject<UWLLocalSaveGame>();
	A->SelectedNationIso = B->SelectedNationIso = TEXT("CO");
	A->CurrentYear = B->CurrentYear = 2026;
	A->CurrentMonth = B->CurrentMonth = 3;
	A->CurrentDay = B->CurrentDay = 14;

	FWLNationTreasurySave Colombia;
	Colombia.NationIso = TEXT("CO");
	Colombia.Treasury = 12345;
	FWLNationTreasurySave Venezuela;
	Venezuela.NationIso = TEXT("VE");
	Venezuela.Treasury = -500;
	A->NationTreasuries = { Colombia, Venezuela };
	B->NationTreasuries = { Venezuela, Colombia };

	FWLProvinceRuntimeState Bogota;
	Bogota.ProvinceId = TEXT("co-bogota");
	Bogota.ControllerIso = TEXT("CO");
	Bogota.Population = 1000;
	Bogota.PublicOrder = 70;
	A->ProvinceStates = B->ProvinceStates = { Bogota };

	const FWLCampaignAuditResult AuditA = FWLCampaignSimulationAudit::Audit(*A, 12, 30);
	const FWLCampaignAuditResult AuditB = FWLCampaignSimulationAudit::Audit(*B, 12, 30);
	TestTrue(TEXT("Snapshot valido"), AuditA.bValid);
	TestEqual(TEXT("Hash independiente del orden"), AuditA.StableHash, AuditB.StableHash);
	TestEqual(TEXT("Hash canonico multiplataforma"), AuditA.StableHash, FString(TEXT("6197fe44be138e7b")));
	FWLGarrisonUnitSave Garrison;
	Garrison.BaseId = TEXT("QA-BASE");
	Garrison.UnitType = TEXT("infantry");
	Garrison.Count = 10;
	B->GarrisonUnits.Add(Garrison);
	const FWLCampaignAuditResult WithGarrison = FWLCampaignSimulationAudit::Audit(*B, 12, 30);
	TestNotEqual(TEXT("Guarnicion afecta el hash"), WithGarrison.StableHash, AuditA.StableHash);

	B->ProvinceStates[0].Population = -1;
	const FWLCampaignAuditResult Invalid = FWLCampaignSimulationAudit::Audit(*B, 12, 30);
	TestFalse(TEXT("Poblacion negativa rechazada"), Invalid.bValid);
	TestTrue(TEXT("Informa la violacion"), Invalid.Violations.Num() > 0);

	FWLTacticalBattleState BattleA;
	BattleA.BattleId = TEXT("B1");
	BattleA.AttackerArmyId = TEXT("A1");
	BattleA.DefenderArmyId = TEXT("A2");
	BattleA.bActive = true;
	FWLTacticalBattleState BattleB = BattleA;
	BattleB.BattleId = TEXT("B2");
	BattleB.DefenderArmyId = TEXT("A3");
	TestTrue(TEXT("Detecta ejercito en dos batallas"),
		FWLCampaignSimulationAudit::AuditActiveBattles({ BattleA, BattleB }).Num() > 0);
	BattleB.AttackerArmyId = TEXT("A4");
	TestTrue(TEXT("Batallas disjuntas validas"),
		FWLCampaignSimulationAudit::AuditActiveBattles({ BattleA, BattleB }).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLSaveMigrationChainTest,
	"WorldLeader.Save.Migration.SupportedVersionMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLSaveMigrationChainTest::RunTest(const FString& Parameters)
{
	for (int32 Version = WLSaveVersion::OldestMigratable; Version <= WLSaveVersion::Current; ++Version)
	{
		UWLLocalSaveGame* Save = NewObject<UWLLocalSaveGame>();
		Save->SaveVersion = Version;
		Save->CurrentDay = Version == 16 ? 0 : 1;
		FWLProvinceBuildingsSave Buildings;
		Buildings.ProvinceId = TEXT("co-bogota");
		Buildings.BuildingIds = { TEXT("factory"), TEXT("road") };
		Save->ProvinceBuildings.Add(Buildings);
		FString Message;
		TestTrue(FString::Printf(TEXT("Migra v%d"), Version), FWLSaveMigration::MigrateToCurrent(*Save, Message));
		TestEqual(FString::Printf(TEXT("v%d llega a current"), Version), Save->SaveVersion, WLSaveVersion::Current);
		if (Version == 16)
		{
			TestEqual(TEXT("Dia reconstruido"), Save->CurrentDay, 1);
			TestEqual(TEXT("Niveles reconstruidos"), Save->ProvinceBuildings[0].BuildingLevels.Num(), 2);
		}
		TestTrue(FString::Printf(TEXT("v%d queda idempotente"), Version),
			FWLSaveMigration::MigrateToCurrent(*Save, Message));
	}

	UWLLocalSaveGame* Unsupported = NewObject<UWLLocalSaveGame>();
	FString Message;
	Unsupported->SaveVersion = WLSaveVersion::OldestMigratable - 1;
	TestFalse(TEXT("Version demasiado antigua rechazada"), FWLSaveMigration::MigrateToCurrent(*Unsupported, Message));
	Unsupported->SaveVersion = WLSaveVersion::Current + 1;
	TestFalse(TEXT("Version futura rechazada"), FWLSaveMigration::MigrateToCurrent(*Unsupported, Message));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCampaignCoordinatorIntegrationTest,
	"WorldLeader.Campaign.Integration.CoordinatorDrivesPolitics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCampaignCoordinatorIntegrationTest::RunTest(const FString& Parameters)
{
	UWLCampaignGameInstance* GameInstance = NewObject<UWLCampaignGameInstance>();
	TestNotNull(TEXT("Campaign GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();
	TestTrue(TEXT("Inicia campania CO"), GameInstance->StartNewCampaign(TEXT("CO")));
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Tick"), Tick);
	TestNotNull(TEXT("Politics"), Politics);
	if (Tick && Politics)
	{
		const int32 AgendaMonthsBefore = Politics->GetGovernmentAgenda(TEXT("CO")).MonthsActive;
		for (int32 Day = 0; Day < Tick->GetBalanceRules().DaysPerMonth; ++Day)
		{
			GameInstance->WLAdvanceDay();
		}
		TestEqual(TEXT("Cierra exactamente un mes"), Tick->GetCurrentMonth(), 2);
		TestEqual(TEXT("Politica corre una vez"),
			Politics->GetGovernmentAgenda(TEXT("CO")).MonthsActive, AgendaMonthsBefore + 1);
	}
	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCampaignProductionVerticalSliceTest,
	"WorldLeader.Campaign.Integration.ProductionVerticalSlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCampaignProductionVerticalSliceTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("WorldLeader_Automation_ProductionVerticalSlice");
	constexpr int32 UserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(Slot, UserIndex);

	UWLCampaignGameInstance* GameInstance = NewObject<UWLCampaignGameInstance>();
	TestNotNull(TEXT("Campaign GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();
	TestTrue(TEXT("Nueva campania"), GameInstance->StartNewCampaign(TEXT("CO")));
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	UWLMilitarySubsystem* Military = GameInstance->GetSubsystem<UWLMilitarySubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Tick"), Tick);
	TestNotNull(TEXT("Military"), Military);
	TestNotNull(TEXT("Politics"), Politics);

	FString Message;
	const FString BaseId = TEXT("QA-CO-BASE");
	if (Tick && Military && Politics)
	{
		TestTrue(TEXT("Encola reclutamiento"), Tick->QueueRecruit(BaseId, TEXT("CO"), TEXT("infantry"), Message));
		GameInstance->WLAdvanceDay();
		const TArray<FWLGarrisonGroup> Garrison = Tick->GetGarrisonRecruited(BaseId);
		TestTrue(TEXT("Reclutamiento completado"), Garrison.ContainsByPredicate([](const FWLGarrisonGroup& Group)
		{
			return Group.UnitType == TEXT("infantry") && Group.Count > 0;
		}));

		TArray<TPair<FString, int32>> Composition;
		for (const FWLGarrisonGroup& Group : Garrison)
		{
			Composition.Emplace(Group.UnitType, Group.Count);
		}
		const FString AttackerId = Military->SyncArmyFromGarrison(
			BaseId, TEXT("CO"), TEXT("CO-AMA"), Composition);
		TestFalse(TEXT("Ejercito materializado"), AttackerId.IsEmpty());
		TestTrue(TEXT("Movimiento adyacente"), Military->MoveArmy(AttackerId, TEXT("CO-CAQ"), Message));

		TArray<FWLArmy> Armies;
		int32 NextArmyNumber = 1;
		Military->WriteSaveSnapshot(Armies, NextArmyNumber);
		FWLArmy Defender;
		Defender.Id = TEXT("QA-VE-DEFENDER");
		Defender.OwnerIso = TEXT("VE");
		Defender.ProvinceId = TEXT("CO-CAQ");
		Defender.General = TEXT("QA Defender");
		Defender.Units = { TEXT("infantry"), TEXT("infantry"), TEXT("infantry") };
		Armies.Add(Defender);
		TestTrue(TEXT("Fixture defensor valido"), Military->RestoreSaveSnapshot(Armies, NextArmyNumber, Message));
		TestTrue(TEXT("Declaracion de guerra"), Politics->DeclareWar(TEXT("CO"), TEXT("VE"), Message));

		const EWLBattleResult BattleResult = Military->ResolveTacticalBattleToEnd(
			AttackerId, Defender.Id, Message);
		TestTrue(TEXT("Batalla tactica resuelta"), BattleResult != EWLBattleResult::Invalid);

		const int32 SavedYear = Tick->GetCurrentYear();
		const int32 SavedMonth = Tick->GetCurrentMonth();
		const int32 SavedDay = Tick->GetCurrentDay();
		TArray<FWLArmy> ArmiesAfterBattle;
		Military->WriteSaveSnapshot(ArmiesAfterBattle, NextArmyNumber);
		TestTrue(TEXT("Guarda slot QA"), GameInstance->SaveCampaignToSlot(Slot, UserIndex, Message));

		GameInstance->WLAdvanceDay();
		TestTrue(TEXT("Carga slot QA"), GameInstance->LoadCampaignFromSlot(Slot, UserIndex, Message));
		TestEqual(TEXT("Restaura anio"), Tick->GetCurrentYear(), SavedYear);
		TestEqual(TEXT("Restaura mes"), Tick->GetCurrentMonth(), SavedMonth);
		TestEqual(TEXT("Restaura dia"), Tick->GetCurrentDay(), SavedDay);
		TestEqual(TEXT("Restaura ejercitos post batalla"), Military->GetArmies().Num(), ArmiesAfterBattle.Num());
		GameInstance->WLAdvanceDay();
		TestEqual(TEXT("Campania continua despues de cargar"), Tick->GetCurrentDay(), SavedDay + 1);
	}

	GameInstance->Shutdown();
	TestTrue(TEXT("Limpia slot QA"), UGameplayStatics::DeleteGameInSlot(Slot, UserIndex));
	return true;
}

#endif
