// Copyright World Leader project. See ROADMAP.md.
//
// Tests F1: backend de personajes/gobierno. No prueban UI; garantizan que el
// gabinete y los generales ya son datos/sistema reales para que la UIX los consuma.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Engine/GameInstance.h"
#include "Military/WLMilitarySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLMinisterEffectsTest,
	"WorldLeader.Government.MinisterEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLMinisterEffectsTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Character subsystem"), Characters);
	TestNotNull(TEXT("Tick subsystem"), Tick);
	if (!Characters || !Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	// El ministro de Defensa seeded de CO es competente (skill > 50) -> abarata el upkeep militar.
	const double DefenseFactor = Characters->GetMinisterEffectFactor(TEXT("CO"), EWLMinisterOffice::Defense);
	TestTrue(TEXT("Ministro de Defensa competente"), DefenseFactor > 0.0);

	const int64 UpkeepWithMinister = Tick->GetNationMilitaryUpkeep(TEXT("CO"));
	FString Message;
	TestTrue(TEXT("Destituir ministro de Defensa"),
		Characters->DismissMinister(TEXT("CO"), EWLMinisterOffice::Defense, Message));
	TestEqual(TEXT("Cargo vacante = factor neutro"),
		Characters->GetMinisterEffectFactor(TEXT("CO"), EWLMinisterOffice::Defense), 0.0);
	TestTrue(TEXT("Sin ministro de Defensa el upkeep militar SUBE"),
		Tick->GetNationMilitaryUpkeep(TEXT("CO")) > UpkeepWithMinister);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCharacterRosterAndCabinetTest,
	"WorldLeader.Government.Characters.RosterAndCabinet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCharacterRosterAndCabinetTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	TestNotNull(TEXT("Character subsystem"), Characters);
	if (!Characters)
	{
		GameInstance->Shutdown();
		return false;
	}

	TestTrue(TEXT("Roster CO cargado"), Characters->GetCharactersByNation(TEXT("CO")).Num() >= 10);
	TestTrue(TEXT("Roster VE cargado"), Characters->GetCharactersByNation(TEXT("VE")).Num() >= 10);
	TestEqual(TEXT("Gabinete CO tiene cinco cargos"), Characters->GetCabinet(TEXT("CO")).Num(), 5);

	FWLCharacter EconomyMinister;
	TestTrue(TEXT("Ministro de economia CO inicial"),
		Characters->GetCabinetMinister(TEXT("co"), EWLMinisterOffice::Economy, EconomyMinister));
	TestEqual(TEXT("Economia inicial canonica"), EconomyMinister.Id, FString(TEXT("CO-MIN-ECO-ROJAS")));

	const FWLGovernmentStats Stats = Characters->GetGovernmentStats(TEXT("CO"));
	TestEqual(TEXT("Cinco ministerios cubiertos"), Stats.FilledOffices, 5);
	TestTrue(TEXT("Estabilidad en rango"), Stats.Stability >= 0 && Stats.Stability <= 100);
	TestTrue(TEXT("Corrupcion en rango"), Stats.Corruption >= 0 && Stats.Corruption <= 100);
	TestTrue(TEXT("Riesgo de golpe en rango"), Stats.CoupRisk >= 0 && Stats.CoupRisk <= 100);
	TestEqual(TEXT("Capital politico inicial"), Stats.PoliticalCapital, 100);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCharacterCabinetAppointmentTest,
	"WorldLeader.Government.Characters.CabinetAppointment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCharacterCabinetAppointmentTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	TestNotNull(TEXT("Character subsystem"), Characters);
	if (!Characters)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	TestTrue(TEXT("Nombrar ministro suplente"),
		Characters->AppointMinister(TEXT("CO"), EWLMinisterOffice::Economy, TEXT("CO-MIN-ECO-ALT-SALCEDO"), Message));
	TestEqual(TEXT("Nombrar cuesta capital politico"), Characters->GetPoliticalCapital(TEXT("CO")), 90);

	FWLCharacter EconomyMinister;
	TestTrue(TEXT("Economia queda ocupada"), Characters->GetCabinetMinister(TEXT("CO"), EWLMinisterOffice::Economy, EconomyMinister));
	TestEqual(TEXT("Nuevo ministro de economia"), EconomyMinister.Id, FString(TEXT("CO-MIN-ECO-ALT-SALCEDO")));

	FWLCharacter OldMinister;
	TestTrue(TEXT("Viejo ministro sigue en roster"), Characters->GetCharacter(TEXT("CO-MIN-ECO-ROJAS"), OldMinister));
	TestEqual(TEXT("Viejo ministro queda sin cargo"), static_cast<int32>(OldMinister.AssignedOffice),
		static_cast<int32>(EWLMinisterOffice::None));

	TestFalse(TEXT("No puede nombrar personaje de otra nacion"),
		Characters->AppointMinister(TEXT("CO"), EWLMinisterOffice::Defense, TEXT("VE-MIN-DEF-CABRERA"), Message));

	TArray<FWLCharacter> SnapshotCharacters;
	TArray<FWLPoliticalCapitalSave> SnapshotPolitical;
	Characters->WriteSaveSnapshot(SnapshotCharacters, SnapshotPolitical);
	Characters->ResetCharacterState();
	FString RestoreMessage;
	TestTrue(TEXT("Restaurar snapshot de personajes"),
		Characters->RestoreSaveSnapshot(SnapshotCharacters, SnapshotPolitical, RestoreMessage));
	TestEqual(TEXT("Capital politico restaurado"), Characters->GetPoliticalCapital(TEXT("CO")), 90);
	TestTrue(TEXT("Gabinete restaurado"), Characters->GetCabinetMinister(TEXT("CO"), EWLMinisterOffice::Economy, EconomyMinister));
	TestEqual(TEXT("Ministro restaurado"), EconomyMinister.Id, FString(TEXT("CO-MIN-ECO-ALT-SALCEDO")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCharacterGeneralAssignmentTest,
	"WorldLeader.Government.Characters.GeneralAssignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCharacterGeneralAssignmentTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	UWLMilitarySubsystem* Military = GameInstance->GetSubsystem<UWLMilitarySubsystem>();
	TestNotNull(TEXT("Character subsystem"), Characters);
	TestNotNull(TEXT("Military subsystem"), Military);
	if (!Characters || !Military)
	{
		GameInstance->Shutdown();
		return false;
	}

	const FString ArmyId = Military->CreateArmy(TEXT("CO"), TEXT("CO-DC"), TEXT("infantry"), 2, TEXT(""));
	TestFalse(TEXT("Ejercito CO creado"), ArmyId.IsEmpty());

	FString Message;
	TestTrue(TEXT("Asignar general CO al ejercito CO"),
		Characters->AssignGeneralToArmy(TEXT("CO-GEN-PADILLA"), ArmyId, Message));

	FWLCharacter General;
	TestTrue(TEXT("General asignado consultable"), Characters->GetAssignedGeneralForArmy(ArmyId, General));
	TestEqual(TEXT("General correcto"), General.Id, FString(TEXT("CO-GEN-PADILLA")));

	TestFalse(TEXT("General VE no puede comandar ejercito CO"),
		Characters->AssignGeneralToArmy(TEXT("VE-GEN-ZAMORA"), ArmyId, Message));

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
