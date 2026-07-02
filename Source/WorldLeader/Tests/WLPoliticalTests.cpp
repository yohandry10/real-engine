// Copyright World Leader project. See ROADMAP.md.
//
// Tests F1-F5 backend: personajes/generales, poder interno, diplomacia, intriga y eventos.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Engine/GameInstance.h"
#include "Military/WLMilitarySubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalF1GeneralLifecycleTest,
	"WorldLeader.Politics.F1.GeneralLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalF1GeneralLifecycleTest::RunTest(const FString& Parameters)
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

	const FString ArmyId = Military->CreateArmy(TEXT("CO"), TEXT("CO-DC"), TEXT("infantry"), 1, TEXT(""));
	TestFalse(TEXT("Ejercito creado"), ArmyId.IsEmpty());

	FWLCharacter AutoGeneral;
	TestTrue(TEXT("CreateArmy autogenera general"), Characters->GetAssignedGeneralForArmy(ArmyId, AutoGeneral));

	FWLCharacter General;
	FString Message;
	TestTrue(TEXT("Crear y asignar general"),
		Characters->CreateAndAssignGeneralToArmy(TEXT("CO"), ArmyId, General, Message));
	TestFalse(TEXT("General con id real"), General.Id.IsEmpty());

	FWLArmy Army;
	TestTrue(TEXT("Ejercito consultable"), Military->GetArmy(ArmyId, Army));
	TestTrue(TEXT("El ejercito ya no dice Comandante"), Army.General != TEXT("Comandante"));

	TestTrue(TEXT("Sumar renombre"), Characters->AddRenownToGeneral(General.Id, 30, Message));
	FWLCharacter Updated;
	TestTrue(TEXT("General actualizado"), Characters->GetCharacter(General.Id, Updated));
	TestTrue(TEXT("Renombre subio"), Updated.Renown >= 30);
	TestTrue(TEXT("Veterano agregado"), Updated.Traits.Contains(TEXT("veterano")));

	TestTrue(TEXT("Ascender general"), Characters->PromoteGeneral(General.Id, Message));
	TestTrue(TEXT("Retirar general"), Characters->RetireCharacter(General.Id, Message));
	TestTrue(TEXT("General retirado consultable"), Characters->GetCharacter(General.Id, Updated));
	TestFalse(TEXT("General queda inactivo"), Updated.bActive);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalDiplomacyTreatyWarTest,
	"WorldLeader.Politics.F3.DiplomacyTreatyWar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalDiplomacyTreatyWarTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Political subsystem"), Politics);
	if (!Politics)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	TestTrue(TEXT("Opinion diplomatica ajustable"),
		Politics->SetRelationOpinion(TEXT("CO"), TEXT("VE"), 65, Message));
	TestTrue(TEXT("Firmar alianza"),
		Politics->SignTreaty(TEXT("CO"), TEXT("VE"), EWLTreatyType::Alliance, Message));

	FWLDiplomaticRelationState Relation;
	TestTrue(TEXT("Relacion consultable"), Politics->GetRelation(TEXT("VE"), TEXT("CO"), Relation));
	TestTrue(TEXT("Alianza guardada"), Relation.Treaties.Contains(EWLTreatyType::Alliance));

	TestTrue(TEXT("Declarar guerra"), Politics->DeclareWar(TEXT("CO"), TEXT("VE"), Message));
	TestTrue(TEXT("Relacion guerra consultable"), Politics->GetRelation(TEXT("CO"), TEXT("VE"), Relation));
	TestEqual(TEXT("Estado guerra"), static_cast<int32>(Relation.Status), static_cast<int32>(EWLDiplomaticStatus::War));
	TestFalse(TEXT("Alianza se rompe con guerra"), Relation.Treaties.Contains(EWLTreatyType::Alliance));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalIntrigueEventsSaveTest,
	"WorldLeader.Politics.F2F4F5.IntrigueEventsSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalIntrigueEventsSaveTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Political subsystem"), Politics);
	TestNotNull(TEXT("Tick subsystem"), Tick);
	if (!Politics || !Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	TestTrue(TEXT("Agenda del lider CO expuesta"), Politics->GetLeaderAgendaTraits(TEXT("CO")).Num() > 0);
	TestTrue(TEXT("Crear red de inteligencia"),
		Politics->BuildSpyNetwork(TEXT("CO"), TEXT("VE"), TEXT("CO-SPY-NIEBLA"), Message));
	FWLIntelligenceNetworkState Network = Politics->GetIntelligenceNetwork(TEXT("CO"), TEXT("VE"));
	TestTrue(TEXT("Red con fuerza"), Network.NetworkStrength > 0);

	TestTrue(TEXT("Financiar golpe rival"),
		Politics->RunSpyOperation(TEXT("CO"), TEXT("VE"), TEXT("CO-SPY-NIEBLA"), EWLSpyOperationType::FundCoup, Message));
	FWLInternalPowerState Internal = Politics->GetInternalPower(TEXT("VE"));
	TestTrue(TEXT("Financiacion externa aplicada"), Internal.ExternalCoupFunding > 0);

	Tick->AdjustNationPublicOrder(TEXT("VE"), -40);
	Politics->ProcessPoliticalMonth();
	Politics->ProcessPoliticalMonth();
	Politics->ProcessPoliticalMonth();
	const TArray<FWLPoliticalEventInstance> Events = Politics->GetQueuedEvents(TEXT("VE"));
	TestTrue(TEXT("Eventos politicos encolados"), Events.Num() > 0);

	if (Events.Num() > 0 && Events[0].Options.Num() > 0)
	{
		TestTrue(TEXT("Resolver evento"),
			Politics->ResolveEvent(Events[0].InstanceId, Events[0].Options[0].OptionId, Message));
	}

	TArray<FWLInternalPowerState> SavedInternal;
	TArray<FWLDiplomaticRelationState> SavedRelations;
	TArray<FWLIntelligenceNetworkState> SavedNetworks;
	TArray<FWLPoliticalEventInstance> SavedEvents;
	FWLCampaignOutcomeState SavedOutcome;
	Politics->WriteSaveSnapshot(SavedInternal, SavedRelations, SavedNetworks, SavedEvents, SavedOutcome);
	Politics->ResetPoliticalState();
	FString RestoreMessage;
	TestTrue(TEXT("Restaurar politica"),
		Politics->RestoreSaveSnapshot(SavedInternal, SavedRelations, SavedNetworks, SavedEvents, SavedOutcome, RestoreMessage));
	Network = Politics->GetIntelligenceNetwork(TEXT("CO"), TEXT("VE"));
	TestTrue(TEXT("Red restaurada"), Network.NetworkStrength > 0);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLMakePeaceTest,
	"WorldLeader.Politics.MakePeaceEndsWar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLMakePeaceTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Political subsystem"), Politics);
	if (!Politics)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	TestFalse(TEXT("Sin guerra no hay paz que firmar"), Politics->MakePeace(TEXT("CO"), TEXT("VE"), Message));
	TestTrue(TEXT("Declarar guerra"), Politics->DeclareWar(TEXT("CO"), TEXT("VE"), Message));
	TestTrue(TEXT("Negociar la paz"), Politics->MakePeace(TEXT("CO"), TEXT("VE"), Message));

	FWLDiplomaticRelationState Relation;
	TestTrue(TEXT("Relacion existe"), Politics->GetRelation(TEXT("CO"), TEXT("VE"), Relation));
	TestTrue(TEXT("La guerra termino (queda tension)"), Relation.Status == EWLDiplomaticStatus::Tension);
	TestTrue(TEXT("Opinion posguerra en -30"), Relation.Opinion >= -30);

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
