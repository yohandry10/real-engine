// Copyright World Leader project. See ROADMAP.md.
//
// Tests F1-F5 backend: personajes/generales, poder interno, diplomacia, intriga y eventos.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
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
	// El general autogenerado por CreateArmy ocupa un cupo del limite nacional de mandos activos
	// (max(2, ejercitos+2), con 2 generales sembrados por nacion). Nombrar a mano un general para
	// ESTE ejercito es una sustitucion de mando, no un mando extra: retirar el autogenerado primero.
	TestTrue(TEXT("Retirar general autogenerado antes de sustituir"),
		Characters->RetireCharacter(AutoGeneral.Id, Message));
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
	FWLPoliticalCoupRiskIgnoresFreshIdleGeneralsTest,
	"WorldLeader.Politics.F2.CoupRiskIgnoresFreshIdleGenerals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalCoupRiskIgnoresFreshIdleGeneralsTest::RunTest(const FString& Parameters)
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
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Character subsystem"), Characters);
	TestNotNull(TEXT("Military subsystem"), Military);
	TestNotNull(TEXT("Political subsystem"), Politics);
	if (!Characters || !Military || !Politics)
	{
		GameInstance->Shutdown();
		return false;
	}

	FWLArmy CommandArmy;
	CommandArmy.Id = TEXT("A-CO-DANGER");
	CommandArmy.OwnerIso = TEXT("CO");
	CommandArmy.ProvinceId = TEXT("CO-DC");
	CommandArmy.General = TEXT("General Riesgo");
	CommandArmy.Units = { TEXT("infantry"), TEXT("infantry"), TEXT("tank") };

	FString Message;
	TestTrue(TEXT("Restaurar ejercito con mando politico"),
		Military->RestoreSaveSnapshot({ CommandArmy }, 2, Message));

	FWLCharacter Dangerous;
	Dangerous.Id = TEXT("CO-GEN-DANGER");
	Dangerous.Name = TEXT("General Riesgo");
	Dangerous.CountryIso = TEXT("CO");
	Dangerous.Role = EWLCharacterRole::General;
	Dangerous.Rank = EWLMilitaryRank::FieldMarshal;
	Dangerous.Skill = 75;
	Dangerous.Loyalty = 10;
	Dangerous.Ambition = 90;
	Dangerous.Popularity = 80;
	Dangerous.Renown = 120;
	Dangerous.AssignedArmyId = CommandArmy.Id;
	Dangerous.bActive = true;

	TArray<FWLCharacter> BaselineCharacters = { Dangerous };
	TestTrue(TEXT("Restaurar general peligroso"),
		Characters->RestoreSaveSnapshot(BaselineCharacters, {}, Message));
	FString Report;
	Politics->AttemptCoup(TEXT("CO"), Report);
	const int32 BaselineRisk = Politics->GetInternalPower(TEXT("CO")).CoupRisk;
	TestTrue(TEXT("El general peligroso genera riesgo real"), BaselineRisk > 0);

	Politics->ResetPoliticalState();
	TArray<FWLCharacter> WithIdleGenerals = { Dangerous };
	for (int32 Index = 0; Index < 6; ++Index)
	{
		FWLCharacter Idle = Dangerous;
		Idle.Id = FString::Printf(TEXT("CO-GEN-IDLE-%02d"), Index);
		Idle.Name = FString::Printf(TEXT("Coronel Nuevo %02d"), Index);
		Idle.Rank = EWLMilitaryRank::Colonel;
		Idle.Skill = 50;
		Idle.Loyalty = 90;
		Idle.Ambition = 10;
		Idle.Popularity = 30;
		Idle.Renown = 0;
		Idle.AssignedArmyId.Reset();
		WithIdleGenerals.Add(Idle);
	}
	TestTrue(TEXT("Restaurar general peligroso con coroneles sin mando"),
		Characters->RestoreSaveSnapshot(WithIdleGenerals, {}, Message));
	Politics->AttemptCoup(TEXT("CO"), Report);
	const int32 RiskWithIdleGenerals = Politics->GetInternalPower(TEXT("CO")).CoupRisk;
	TestEqual(TEXT("Coroneles nuevos sin mando no diluyen el riesgo de golpe"),
		RiskWithIdleGenerals, BaselineRisk);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalRewardInactiveGeneralDoesNotChargeTest,
	"WorldLeader.Politics.F2.RewardInactiveGeneralDoesNotCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalRewardInactiveGeneralDoesNotChargeTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Character subsystem"), Characters);
	TestNotNull(TEXT("Political subsystem"), Politics);
	TestNotNull(TEXT("Tick subsystem"), Tick);
	if (!Characters || !Politics || !Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	FWLCharacter InactiveGeneral;
	InactiveGeneral.Id = TEXT("CO-GEN-INACTIVE-TEST");
	InactiveGeneral.Name = TEXT("General Retirado");
	InactiveGeneral.CountryIso = TEXT("CO");
	InactiveGeneral.Role = EWLCharacterRole::General;
	InactiveGeneral.Rank = EWLMilitaryRank::BrigadeGeneral;
	InactiveGeneral.Skill = 65;
	InactiveGeneral.Loyalty = 40;
	InactiveGeneral.Ambition = 80;
	InactiveGeneral.bActive = false;

	FString Message;
	TestTrue(TEXT("Restaurar general inactivo"),
		Characters->RestoreSaveSnapshot({ InactiveGeneral }, {}, Message));
	TestTrue(TEXT("Dar tesoro para detectar cobro indebido"),
		Tick->AdjustTreasury(TEXT("CO"), 10000, Message));
	const int64 TreasuryBefore = Tick->GetTreasury(TEXT("CO"));
	TestFalse(TEXT("No se recompensa general inactivo"),
		Politics->RewardGeneral(TEXT("CO"), InactiveGeneral.Id, Message));
	TestEqual(TEXT("RewardGeneral no cobra si falla validacion"),
		Tick->GetTreasury(TEXT("CO")), TreasuryBefore);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalPurgeGeneralScaresMilitaryTest,
	"WorldLeader.Politics.F2.PurgeGeneralScaresMilitary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalPurgeGeneralScaresMilitaryTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Character subsystem"), Characters);
	TestNotNull(TEXT("Political subsystem"), Politics);
	if (!Characters || !Politics)
	{
		GameInstance->Shutdown();
		return false;
	}

	FWLCharacter Purged;
	Purged.Id = TEXT("CO-GEN-PURGED-TEST");
	Purged.Name = TEXT("General Purgado");
	Purged.CountryIso = TEXT("CO");
	Purged.Role = EWLCharacterRole::General;
	Purged.Rank = EWLMilitaryRank::BrigadeGeneral;
	Purged.Skill = 60;
	Purged.Loyalty = 50;
	Purged.Ambition = 70;
	Purged.bActive = true;

	FWLCharacter Survivor = Purged;
	Survivor.Id = TEXT("CO-GEN-SURVIVOR-TEST");
	Survivor.Name = TEXT("General Superviviente");
	Survivor.Loyalty = 64;

	FString Message;
	TestTrue(TEXT("Restaurar generales para purga"),
		Characters->RestoreSaveSnapshot({ Purged, Survivor }, {}, Message));

	int32 MilitarySupportBefore = INDEX_NONE;
	for (const FWLPublicGroupSupportState& Group : Politics->GetPublicGroups(TEXT("CO")))
	{
		if (Group.Group == EWLPublicGroup::Military)
		{
			MilitarySupportBefore = Group.Support;
			break;
		}
	}
	TestTrue(TEXT("Grupo militar existe"), MilitarySupportBefore != INDEX_NONE);

	TestTrue(TEXT("Purgar general activo"),
		Politics->PurgeCharacter(TEXT("CO"), Purged.Id, Message));
	FWLCharacter SurvivorAfter;
	TestTrue(TEXT("General superviviente consultable"),
		Characters->GetCharacter(Survivor.Id, SurvivorAfter));
	TestEqual(TEXT("La purga baja lealtad del resto de generales"),
		SurvivorAfter.Loyalty, Survivor.Loyalty - 4);

	int32 MilitarySupportAfter = INDEX_NONE;
	for (const FWLPublicGroupSupportState& Group : Politics->GetPublicGroups(TEXT("CO")))
	{
		if (Group.Group == EWLPublicGroup::Military)
		{
			MilitarySupportAfter = Group.Support;
			break;
		}
	}
	TestEqual(TEXT("La purga reduce apoyo militar"),
		MilitarySupportAfter, MilitarySupportBefore - 2);

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
	FWLPoliticalAmericaDiplomacyCoverageTest,
	"WorldLeader.Politics.F3.AmericaDiplomacyCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalAmericaDiplomacyCoverageTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	const UWLDataRegistry* Registry = GameInstance->GetSubsystem<UWLDataRegistry>();
	TestNotNull(TEXT("Political subsystem"), Politics);
	TestNotNull(TEXT("Data registry"), Registry);
	if (!Politics || !Registry)
	{
		GameInstance->Shutdown();
		return false;
	}

	const TArray<FWLNationData> Nations = Registry->GetAllNations();
	TestTrue(TEXT("America cargada para diplomacia"), Nations.Num() >= 30);
	const TArray<FWLDiplomaticRelationState> Relations = Politics->GetRelationsForNation(TEXT("CO"));
	TestEqual(TEXT("Colombia tiene diplomacia con todas las demas naciones"), Relations.Num(), Nations.Num() - 1);

	for (const FWLNationData& Nation : Nations)
	{
		const TArray<FWLDiplomaticRelationState> NationRelations = Politics->GetRelationsForNation(Nation.Iso);
		TestEqual(FString::Printf(TEXT("%s tiene relaciones bilaterales completas"), *Nation.Iso),
			NationRelations.Num(), Nations.Num() - 1);
		if (Nation.Iso != TEXT("CO"))
		{
			FWLDiplomaticRelationState Relation;
			TestTrue(FString::Printf(TEXT("Relacion CO-%s existe"), *Nation.Iso),
				Politics->GetRelation(TEXT("CO"), Nation.Iso, Relation));
		}
	}

	const FString ExpectedIsos[] = { TEXT("US"), TEXT("MX"), TEXT("BR"), TEXT("AR"), TEXT("PE"), TEXT("CL") };
	FString Message;
	for (const FString& OtherIso : ExpectedIsos)
	{
		FWLDiplomaticRelationState Relation;
		TestTrue(FString::Printf(TEXT("Relacion CO-%s existe"), *OtherIso),
			Politics->GetRelation(TEXT("CO"), OtherIso, Relation));
		TestTrue(FString::Printf(TEXT("Opinion CO-%s ajustable"), *OtherIso),
			Politics->SetRelationOpinion(TEXT("CO"), OtherIso, 35, Message));
		TestTrue(FString::Printf(TEXT("Tratado comercial CO-%s firmable"), *OtherIso),
			Politics->SignTreaty(TEXT("CO"), OtherIso, EWLTreatyType::TradeAgreement, Message));
	}

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalDiplomacyValidationGuardsTest,
	"WorldLeader.Politics.DiplomacyValidationGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalDiplomacyValidationGuardsTest::RunTest(const FString& Parameters)
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
	FWLDiplomaticRelationState Relation;
	TestFalse(TEXT("No ajusta opinion con nacion invalida"),
		Politics->AdjustRelationOpinion(TEXT("CO"), TEXT("XX"), 10, Message));
	TestFalse(TEXT("No crea relacion para nacion invalida"),
		Politics->GetRelation(TEXT("CO"), TEXT("XX"), Relation));
	TestFalse(TEXT("No rompe tratado consigo mismo"),
		Politics->BreakTreaty(TEXT("CO"), TEXT("CO"), EWLTreatyType::TradeAgreement, Message));
	TestFalse(TEXT("No crea relacion consigo mismo"),
		Politics->GetRelation(TEXT("CO"), TEXT("CO"), Relation));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalStrategicAIDifficultyWarPostureTest,
	"WorldLeader.Politics.StrategicAIDifficultyWarPosture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalStrategicAIDifficultyWarPostureTest::RunTest(const FString& Parameters)
{
	auto RunCase = [this](EWLAIDifficulty Difficulty, bool bExpectWar) -> bool
	{
		UWLCampaignGameInstance* GameInstance = NewObject<UWLCampaignGameInstance>();
		TestNotNull(TEXT("Campaign GameInstance"), GameInstance);
		if (!GameInstance)
		{
			return false;
		}
		GameInstance->Init();

		UWLBalanceSubsystem* Balance = GameInstance->GetSubsystem<UWLBalanceSubsystem>();
		UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
		UWLMilitarySubsystem* Military = GameInstance->GetSubsystem<UWLMilitarySubsystem>();
		TestNotNull(TEXT("Balance subsystem"), Balance);
		TestNotNull(TEXT("Political subsystem"), Politics);
		TestNotNull(TEXT("Military subsystem"), Military);
		if (!Balance || !Politics || !Military)
		{
			GameInstance->Shutdown();
			return false;
		}

		FWLBalanceRules Rules = Balance->GetRules();
		Rules.AIDifficulty = Difficulty;
		Balance->SetRuntimeRules(Rules);
		TestTrue(TEXT("Iniciar campania VE"), GameInstance->StartNewCampaign(TEXT("VE")));

		// VE recibe una fuerza simbolica; el ejercito de CO se DIMENSIONA para una ventaja MODERADA
		// (~1.5x la fuerza de VE), robusta al baseline de datos de cada nacion: por encima del umbral
		// de guerra en Dificil (1.35x) pero por debajo del de Facil (2.0x). Asi el test valida el
		// ESCALADO por dificultad, no numeros magicos que dependen de los datos.
		UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
		TestNotNull(TEXT("Strategic tick subsystem"), Tick);
		if (!Tick)
		{
			GameInstance->Shutdown();
			return false;
		}
		const FString VeArmy = Military->CreateArmy(TEXT("VE"), TEXT("VE-ZU"), TEXT("infantry"), 1, TEXT(""));
		const int64 VeStrength = Tick->GetNationMilitaryStrength(TEXT("VE"));
		const int64 CoBaseline = Tick->GetNationMilitaryStrength(TEXT("CO"));
		const int64 TargetCoStrength = static_cast<int64>(static_cast<double>(VeStrength) * 1.5);
		const int32 CoUnits = FMath::Clamp(static_cast<int32>((TargetCoStrength - CoBaseline) / 100), 5, 800);
		const FString CoArmy = Military->CreateArmy(TEXT("CO"), TEXT("CO-DC"), TEXT("infantry"), CoUnits, TEXT(""));
		TestFalse(TEXT("Ejercito CO creado"), CoArmy.IsEmpty());
		TestFalse(TEXT("Ejercito VE creado"), VeArmy.IsEmpty());

		FString Message;
		TestTrue(TEXT("Opinion tensa controlada"),
			Politics->SetRelationOpinion(TEXT("CO"), TEXT("VE"), -55, Message));
		Politics->ProcessPoliticalMonth();

		FWLDiplomaticRelationState Relation;
		TestTrue(TEXT("Relacion consultable"), Politics->GetRelation(TEXT("CO"), TEXT("VE"), Relation));
		const bool bAtWar = Relation.Status == EWLDiplomaticStatus::War;
		TestEqual(
			bExpectWar ? TEXT("Dificil declara guerra con ventaja moderada") : TEXT("Facil evita guerra con ventaja moderada"),
			bAtWar,
			bExpectWar);

		GameInstance->Shutdown();
		return true;
	};

	const bool bEasyOk = RunCase(EWLAIDifficulty::Easy, false);
	const bool bHardOk = RunCase(EWLAIDifficulty::Hard, true);
	return bEasyOk && bHardOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalEventRelationDeltaTest,
	"WorldLeader.Politics.EventRelationDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalEventRelationDeltaTest::RunTest(const FString& Parameters)
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

	FWLPoliticalEventOption Option;
	Option.OptionId = TEXT("support");
	Option.Label = TEXT("Apoyar al vecino");
	Option.RelationDelta = 12;

	FWLPoliticalEventInstance Event;
	Event.InstanceId = TEXT("EV-0900");
	Event.EventId = TEXT("diplomatic_support");
	Event.NationIso = TEXT("CO");
	Event.TargetIso = TEXT("VE");
	Event.Title = TEXT("Apoyo diplomatico");
	Event.Body = TEXT("El gobierno toma posicion.");
	Event.Options.Add(Option);

	FString Message;
	FWLCampaignOutcomeState Outcome;
	TArray<FWLPoliticalEventInstance> Events;
	Events.Add(Event);
	TestTrue(TEXT("Restaurar evento diplomatico"),
		Politics->RestoreSaveSnapshot(
			TArray<FWLInternalPowerState>(),
			TArray<FWLDiplomaticRelationState>(),
			TArray<FWLIntelligenceNetworkState>(),
			Events,
			Outcome,
			Message));

	FWLDiplomaticRelationState Before;
	TestTrue(TEXT("Relacion previa existe"), Politics->GetRelation(TEXT("CO"), TEXT("VE"), Before));
	TestTrue(TEXT("Resolver aplica delta diplomatico"),
		Politics->ResolveEvent(TEXT("EV-0900"), TEXT("support"), Message));

	FWLDiplomaticRelationState After;
	TestTrue(TEXT("Relacion posterior existe"), Politics->GetRelation(TEXT("CO"), TEXT("VE"), After));
	TestEqual(TEXT("Delta diplomatico aplicado"), After.Opinion, Before.Opinion + 12);

	Event.InstanceId = TEXT("EV-0901");
	Event.TargetIso.Reset();
	Events.Reset();
	Events.Add(Event);
	TestTrue(TEXT("Restaurar evento sin objetivo"),
		Politics->RestoreSaveSnapshot(
			TArray<FWLInternalPowerState>(),
			TArray<FWLDiplomaticRelationState>(),
			TArray<FWLIntelligenceNetworkState>(),
			Events,
			Outcome,
			Message));
	TestFalse(TEXT("RelationDelta sin objetivo no toca estado"),
		Politics->ResolveEvent(TEXT("EV-0901"), TEXT("support"), Message));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLGovernmentAgendaProgramsAndStateTest,
	"WorldLeader.Government.RealGovernment.AgendaProgramsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLGovernmentAgendaProgramsAndStateTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	TestNotNull(TEXT("Political subsystem"), Politics);
	TestNotNull(TEXT("Character subsystem"), Characters);
	if (!Politics || !Characters)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	TestTrue(TEXT("Set agenda nacional"),
		Politics->SetGovernmentAgenda(TEXT("CO"),
			{ EWLGovernmentPriority::Austerity, EWLGovernmentPriority::Industrialization, EWLGovernmentPriority::Control },
			Message));
	const FWLGovernmentAgendaState Agenda = Politics->GetGovernmentAgenda(TEXT("CO"));
	TestEqual(TEXT("Agenda tiene tres prioridades"), Agenda.Priorities.Num(), 3);
	TestTrue(TEXT("Agenda conserva austeridad"), Agenda.Priorities.Contains(EWLGovernmentPriority::Austerity));

	const TArray<FWLMinistryProgramDefinition> Programs = Politics->GetAvailableMinistryPrograms(TEXT("CO"));
	TestTrue(TEXT("Catalogo ministerial amplio"), Programs.Num() >= 50);
	TSet<EWLMinisterOffice> Offices;
	for (const FWLMinistryProgramDefinition& Program : Programs)
	{
		Offices.Add(Program.Office);
	}
	TestTrue(TEXT("Economia tiene programas"), Offices.Contains(EWLMinisterOffice::Economy));
	TestTrue(TEXT("Defensa tiene programas"), Offices.Contains(EWLMinisterOffice::Defense));
	TestTrue(TEXT("Interior tiene programas"), Offices.Contains(EWLMinisterOffice::Interior));
	TestTrue(TEXT("Exterior tiene programas"), Offices.Contains(EWLMinisterOffice::Foreign));
	TestTrue(TEXT("Inteligencia tiene programas"), Offices.Contains(EWLMinisterOffice::Intelligence));

	const int32 PoliticalCapitalBefore = Characters->GetPoliticalCapital(TEXT("CO"));
	TestTrue(TEXT("Iniciar programa de inversion publica"),
		Politics->StartMinistryProgram(TEXT("CO"), TEXT("econ_public_investment"), Message));
	TestTrue(TEXT("Programa queda activo"), Politics->GetActiveMinistryPrograms(TEXT("CO")).Num() > 0);
	TestTrue(TEXT("Programa cuesta capital politico"),
		Characters->GetPoliticalCapital(TEXT("CO")) < PoliticalCapitalBefore);

	Politics->ProcessPoliticalMonth();
	const TArray<FWLPublicGroupSupportState> Groups = Politics->GetPublicGroups(TEXT("CO"));
	TestEqual(TEXT("Seis grupos sociales"), Groups.Num(), 6);
	bool bWorkersShifted = false;
	for (const FWLPublicGroupSupportState& Group : Groups)
	{
		if (Group.Group == EWLPublicGroup::Workers && Group.Support > 50)
		{
			bWorkersShifted = true;
		}
	}
	TestTrue(TEXT("Programa modifica apoyo de trabajadores"), bWorkersShifted);

	const FWLStateCapacityState Capacity = Politics->GetStateCapacity(TEXT("CO"));
	TestTrue(TEXT("Capacidad estatal calculada"), Capacity.AdministrativeEfficiency >= 0 && Capacity.PolicyFailureRisk >= 0);

	const FWLCabinetDynamicsState Dynamics = Politics->GetCabinetDynamics(TEXT("CO"));
	TestTrue(TEXT("Dinamica de gabinete calculada"),
		Dynamics.ScandalRisk >= 0 && Dynamics.SabotageRisk >= 0 && Dynamics.ResignationRisk >= 0);

	const FWLInstitutionalPowerState InstitutionsBefore = Politics->GetInstitutionalPower(TEXT("CO"));
	TestTrue(TEXT("Aprobar reforma consume base institucional"),
		Politics->PassGovernmentReform(TEXT("CO"), TEXT("test_reform"), 4, Message));
	const FWLInstitutionalPowerState InstitutionsAfter = Politics->GetInstitutionalPower(TEXT("CO"));
	TestFalse(TEXT("Congreso registra votacion"), InstitutionsAfter.LastVoteReport.IsEmpty());
	TestTrue(TEXT("Coalicion no sube gratis con reforma"),
		InstitutionsAfter.RulingCoalitionSupport <= InstitutionsBefore.RulingCoalitionSupport);

	TArray<FWLGovernmentAgendaState> SavedAgendas;
	TArray<FWLMinistryProgramState> SavedPrograms;
	TArray<FWLCabinetDynamicsState> SavedDynamics;
	TArray<FWLInstitutionalPowerState> SavedInstitutions;
	TArray<FWLPublicGroupSupportState> SavedGroups;
	TArray<FWLStateCapacityState> SavedCapacity;
	TArray<FWLPoliticalMemoryRecord> SavedMemory;
	TArray<FWLPoliticalAIPlanState> SavedPlans;
	Politics->WriteGovernmentSaveSnapshot(
		SavedAgendas,
		SavedPrograms,
		SavedDynamics,
		SavedInstitutions,
		SavedGroups,
		SavedCapacity,
		SavedMemory,
		SavedPlans);
	TestTrue(TEXT("Snapshot guarda agenda"), SavedAgendas.Num() > 0);
	TestTrue(TEXT("Snapshot guarda grupos sociales"), SavedGroups.Num() >= 6);
	TestTrue(TEXT("Restaurar snapshot de gobierno"),
		Politics->RestoreGovernmentSaveSnapshot(
			SavedAgendas,
			SavedPrograms,
			SavedDynamics,
			SavedInstitutions,
			SavedGroups,
			SavedCapacity,
			SavedMemory,
			SavedPlans,
			Message));
	TestEqual(TEXT("Agenda restaurada"), Politics->GetGovernmentAgenda(TEXT("CO")).Priorities.Num(), 3);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLGovernmentP2RealPoliticsSystemsTest,
	"WorldLeader.Government.P2.RealPoliticsSystems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLGovernmentP2RealPoliticsSystemsTest::RunTest(const FString& Parameters)
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

	const TArray<FWLPolicyReformDefinition> Reforms = Politics->GetAvailablePolicyReforms(TEXT("CO"));
	TestTrue(TEXT("Catalogo de reformas P2 fuerte"), Reforms.Num() >= 24);
	TSet<EWLPolicyReformArea> Areas;
	for (const FWLPolicyReformDefinition& Reform : Reforms)
	{
		Areas.Add(Reform.Area);
	}
	TestEqual(TEXT("Todas las areas de reforma cubiertas"), Areas.Num(), 12);

	FString Message;
	TestTrue(TEXT("Aprobar reforma tributaria P2"),
		Politics->EnactPolicyReform(TEXT("CO"), TEXT("tax_broad_base"), Message));
	TestTrue(TEXT("Reforma queda activa"), Politics->GetActivePolicyReforms(TEXT("CO")).Num() > 0);

	const TArray<FWLPartyState> PartiesBefore = Politics->GetPoliticalParties(TEXT("CO"));
	TestTrue(TEXT("Partidos politicos sembrados"), PartiesBefore.Num() >= 5);
	FString NegotiatedPartyId;
	for (const FWLPartyState& Party : PartiesBefore)
	{
		if (Party.Role == EWLPartyRole::SoftOpposition || Party.Role == EWLPartyRole::Ally)
		{
			NegotiatedPartyId = Party.PartyId;
			break;
		}
	}
	TestFalse(TEXT("Hay partido negociable"), NegotiatedPartyId.IsEmpty());
	TestTrue(TEXT("Negociar soporte partidista"),
		Politics->NegotiatePartySupport(TEXT("CO"), NegotiatedPartyId, Message));

	TestFalse(TEXT("Promesa electoral bloqueada fuera de ventana"),
		Politics->MakeCampaignPromise(TEXT("CO"), TEXT("edu_public_schools"), Message));

	const TArray<FWLCharacterPoliticalProfile> Profiles = Politics->GetCharacterPoliticalProfiles(TEXT("CO"));
	TestTrue(TEXT("Personajes tienen perfil politico"), Profiles.Num() > 0);
	TestFalse(TEXT("Perfil tiene biografia"), Profiles[0].Biography.IsEmpty());

	Politics->ProcessPoliticalMonth();
	// Las acciones de gobierno (patronazgo por contrato 2200, inversion regional 1800, etc.) cuestan
	// tesoro real; este test encadena varias en un mes, asi que se financia la caja para probar el
	// EFECTO de las acciones y no el bloqueo por presupuesto (cubierto en otros tests).
	if (UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>())
	{
		FString FundMessage;
		Tick->AdjustTreasury(TEXT("CO"), 100000, FundMessage);
	}
	TestTrue(TEXT("Usar patronazgo"),
		Politics->UsePatronage(TEXT("CO"), EWLPatronageActionType::AwardContract, Message));
	TestTrue(TEXT("Patronazgo eleva corrupcion de contratos"),
		Politics->GetPatronageState(TEXT("CO")).ContractCorruption > 0);

	TestTrue(TEXT("Accion de medios"),
		Politics->RunMediaAction(TEXT("CO"), EWLMediaActionType::Censorship, Message));
	TestTrue(TEXT("Censura deja backlash"),
		Politics->GetMediaPublicOpinion(TEXT("CO")).CensorshipBacklash > 0);

	// Cada accion de gobierno consume PA del presupuesto MENSUAL (3/mes). Este test ya gasto PA
	// en patronazgo y medios este mes; avanzar un mes real refresca el presupuesto para probar el
	// EFECTO de la politica regional (la economia de PA se valida en otros tests).
	if (UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>())
	{
		const int32 StartMonth = Tick->GetCurrentYear() * 12 + Tick->GetCurrentMonth();
		for (int32 Guard = 0; Guard < 40 && (Tick->GetCurrentYear() * 12 + Tick->GetCurrentMonth()) == StartMonth; ++Guard)
		{
			Tick->AdvanceDay();
		}
		FString FundMessage;
		Tick->AdjustTreasury(TEXT("CO"), 100000, FundMessage);
	}

	const TArray<FWLRegionGovernorState> Regions = Politics->GetRegionGovernors(TEXT("CO"));
	TestTrue(TEXT("Gobernadores/regiones sembrados"), Regions.Num() > 0);
	const bool bRegionPolicyRan = Regions.Num() > 0
		&& Politics->RunRegionPolicy(TEXT("CO"), Regions[0].RegionId, EWLRegionPolicyActionType::RegionalInvestment, Message);
	if (!bRegionPolicyRan)
	{
		AddError(FString::Printf(TEXT("RunRegionPolicy fallo: %s"), *Message));
	}
	TestTrue(TEXT("Politica regional ejecuta"), bRegionPolicyRan);

	FWLCrisisChainState Crisis;
	Crisis.NationIso = TEXT("CO");
	Crisis.CrisisId = TEXT("CO|TEST_CRISIS");
	Crisis.Type = EWLCrisisChainType::Impeachment;
	Crisis.Stage = 2;
	Crisis.Intensity = 85;
	Crisis.MonthsActive = 1;
	Crisis.LastReport = TEXT("crisis test");
	TestTrue(TEXT("Restaurar crisis P2"),
		Politics->RestoreGovernmentP2SaveSnapshot(
			Politics->GetActivePolicyReforms(TEXT("CO")),
			Politics->GetEnactedPolicyReforms(TEXT("CO")),
			Politics->GetPoliticalParties(TEXT("CO")),
			{ Politics->GetElectionState(TEXT("CO")) },
			Politics->GetCharacterPoliticalProfiles(TEXT("CO")),
			{ Politics->GetPatronageState(TEXT("CO")) },
			{ Politics->GetMediaPublicOpinion(TEXT("CO")) },
			Politics->GetRegionGovernors(TEXT("CO")),
			{ Crisis },
			{ Politics->GetGovernmentCalibration(TEXT("CO")) },
			TArray<FWLPoliticalActionRecord>(),
			Message));
	TestTrue(TEXT("Crisis P2 activa restaurada"), Politics->GetActiveCrisisChains(TEXT("CO")).Num() > 0);

	Politics->ProcessPoliticalMonth();
	TestTrue(TEXT("Calibracion observa mes"),
		Politics->GetGovernmentCalibration(TEXT("CO")).MonthsObserved > 0);

	TArray<FWLActiveReformState> SavedReforms;
	TArray<FWLEnactedPolicyReformState> SavedEnactedReforms;
	TArray<FWLPartyState> SavedParties;
	TArray<FWLElectionState> SavedElections;
	TArray<FWLCharacterPoliticalProfile> SavedProfiles;
	TArray<FWLPatronageState> SavedPatronage;
	TArray<FWLMediaPublicOpinionState> SavedMedia;
	TArray<FWLRegionGovernorState> SavedRegions;
	TArray<FWLCrisisChainState> SavedCrises;
	TArray<FWLGovernmentCalibrationState> SavedCalibration;
	TArray<FWLPoliticalActionRecord> SavedActionRecords;
	Politics->WriteGovernmentP2SaveSnapshot(
		SavedReforms,
		SavedEnactedReforms,
		SavedParties,
		SavedElections,
		SavedProfiles,
		SavedPatronage,
		SavedMedia,
		SavedRegions,
		SavedCrises,
		SavedCalibration,
		SavedActionRecords);
	TestTrue(TEXT("Snapshot P2 guarda reformas"), SavedReforms.Num() > 0);
	TestTrue(TEXT("Snapshot P2 expone reformas consolidadas"), SavedEnactedReforms.Num() >= 0);
	TestTrue(TEXT("Snapshot P2 guarda partidos"), SavedParties.Num() >= 5);
	TestTrue(TEXT("Snapshot P2 guarda elecciones"), SavedElections.Num() > 0);
	TestTrue(TEXT("Snapshot P2 guarda perfiles"), SavedProfiles.Num() > 0);
	TestTrue(TEXT("Snapshot P2 guarda regiones"), SavedRegions.Num() > 0);
	TestTrue(TEXT("Snapshot P2 guarda calibracion"), SavedCalibration.Num() > 0);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLGovernmentP2InvariantsAndPersistenceTest,
	"WorldLeader.Government.P2.InvariantsAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLGovernmentP2InvariantsAndPersistenceTest::RunTest(const FString& Parameters)
{
	UWLCampaignGameInstance* GameInstance = NewObject<UWLCampaignGameInstance>();
	TestNotNull(TEXT("Campaign GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();
	TestTrue(TEXT("Campania CO"), GameInstance->StartNewCampaign(TEXT("CO")));

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
	const int64 InitialTreasury = Tick->GetTreasury(TEXT("CO"));
	Tick->AdjustTreasury(TEXT("CO"), -InitialTreasury, Message);
	TestEqual(TEXT("Tesoro CO queda en cero"), Tick->GetTreasury(TEXT("CO")), static_cast<int64>(0));
	TestFalse(TEXT("Programa no inicia sin tesoro"),
		Politics->StartMinistryProgram(TEXT("CO"), TEXT("econ_public_investment"), Message));
	TestFalse(TEXT("Patronazgo no gasta sin tesoro"),
		Politics->UsePatronage(TEXT("CO"), EWLPatronageActionType::AwardContract, Message));
	TestEqual(TEXT("Contratos no mutan sin tesoro"),
		Politics->GetPatronageState(TEXT("CO")).ContractCorruption, 0);

	const TArray<FWLRegionGovernorState> Regions = Politics->GetRegionGovernors(TEXT("CO"));
	TestTrue(TEXT("Hay regiones para politica territorial"), Regions.Num() > 0);
	if (Regions.Num() > 0)
	{
		TestFalse(TEXT("Inversion regional no corre sin tesoro"),
			Politics->RunRegionPolicy(TEXT("CO"), Regions[0].RegionId, EWLRegionPolicyActionType::RegionalInvestment, Message));
	}

	Tick->AdjustTreasury(TEXT("CO"), 100000, Message);
	FWLElectionState CampaignWindow;
	CampaignWindow.NationIso = TEXT("CO");
	CampaignWindow.MonthsToElection = 6;
	CampaignWindow.CampaignIntensity = 20;
	CampaignWindow.Legitimacy = 70;
	TestTrue(TEXT("Restaurar ventana electoral para promesa"),
		Politics->RestoreGovernmentP2SaveSnapshot(
			TArray<FWLActiveReformState>(),
			TArray<FWLEnactedPolicyReformState>(),
			TArray<FWLPartyState>(),
			{ CampaignWindow },
			TArray<FWLCharacterPoliticalProfile>(),
			TArray<FWLPatronageState>(),
			TArray<FWLMediaPublicOpinionState>(),
			TArray<FWLRegionGovernorState>(),
			TArray<FWLCrisisChainState>(),
			TArray<FWLGovernmentCalibrationState>(),
			TArray<FWLPoliticalActionRecord>(),
			Message));
	TestTrue(TEXT("Promesa electoral tax_broad_base"),
		Politics->MakeCampaignPromise(TEXT("CO"), TEXT("tax_broad_base"), Message));
	TestTrue(TEXT("Aprobar reforma prometida"),
		Politics->EnactPolicyReform(TEXT("CO"), TEXT("tax_broad_base"), Message));
	TestTrue(TEXT("Promesa queda marcada como cumplida"),
		Politics->GetElectionState(TEXT("CO")).bCampaignPromiseFulfilled);
	for (int32 Month = 0; Month < 25; ++Month)
	{
		Politics->ProcessPoliticalMonth();
	}
	const TArray<FWLEnactedPolicyReformState> Enacted = Politics->GetEnactedPolicyReforms(TEXT("CO"));
	const FWLEnactedPolicyReformState* EnactedTaxReform = Enacted.FindByPredicate([](const FWLEnactedPolicyReformState& Reform)
	{
		return Reform.ReformId == TEXT("tax_broad_base");
	});
	TestTrue(TEXT("Reforma queda consolidada permanente"),
		EnactedTaxReform != nullptr);
	if (EnactedTaxReform)
	{
		TestTrue(TEXT("Reforma consolidada conserva promesa cumplida"),
			EnactedTaxReform->bFulfilledCampaignPromise);
	}
	TestFalse(TEXT("Reforma consolidada no se duplica"),
		Politics->EnactPolicyReform(TEXT("CO"), TEXT("tax_broad_base"), Message));

	FWLPartyState PartialParty;
	PartialParty.NationIso = TEXT("VE");
	PartialParty.PartyId = TEXT("ve_gov");
	PartialParty.Name = TEXT("Partido de Gobierno");
	PartialParty.Role = EWLPartyRole::Ruling;
	PartialParty.Seats = 36;
	FWLActiveReformState DuplicateActiveReform;
	DuplicateActiveReform.NationIso = TEXT("CO");
	DuplicateActiveReform.ReformId = TEXT("tax_broad_base");
	DuplicateActiveReform.MonthsRemaining = 12;
	FWLEnactedPolicyReformState DuplicateEnactedReform;
	DuplicateEnactedReform.NationIso = TEXT("CO");
	DuplicateEnactedReform.ReformId = TEXT("tax_broad_base");
	DuplicateEnactedReform.MonthsSinceEnacted = 1;
	FWLCharacterPoliticalProfile PartialProfile;
	PartialProfile.NationIso = TEXT("VE");
	PartialProfile.CharacterId = TEXT("VE-MIN-ECO-SERRANO");
	TestTrue(TEXT("Restore P2 parcial no deja Congreso incompleto"),
		Politics->RestoreGovernmentP2SaveSnapshot(
			{ DuplicateActiveReform, DuplicateActiveReform },
			{ DuplicateEnactedReform },
			{ PartialParty, PartialParty },
			TArray<FWLElectionState>(),
			{ PartialProfile },
			TArray<FWLPatronageState>(),
			TArray<FWLMediaPublicOpinionState>(),
			TArray<FWLRegionGovernorState>(),
			TArray<FWLCrisisChainState>(),
			TArray<FWLGovernmentCalibrationState>(),
			TArray<FWLPoliticalActionRecord>(),
			Message));
	TestTrue(TEXT("Partidos VE faltantes se resembran"),
		Politics->GetPoliticalParties(TEXT("VE")).Num() >= 5);
	int32 GovPartyCount = 0;
	for (const FWLPartyState& Party : Politics->GetPoliticalParties(TEXT("VE")))
	{
		if (Party.PartyId == TEXT("ve_gov"))
		{
			++GovPartyCount;
		}
	}
	TestEqual(TEXT("Restore P2 deduplica partidos guardados"), GovPartyCount, 1);
	TestFalse(TEXT("Reforma activa duplicada se elimina si ya esta consolidada"),
		Politics->GetActivePolicyReforms(TEXT("CO")).ContainsByPredicate([](const FWLActiveReformState& Reform)
		{
			return Reform.ReformId == TEXT("tax_broad_base");
		}));
	TestEqual(TEXT("Restore P2 mantiene una sola reforma consolidada"),
		Politics->GetEnactedPolicyReforms(TEXT("CO")).FilterByPredicate([](const FWLEnactedPolicyReformState& Reform)
		{
			return Reform.ReformId == TEXT("tax_broad_base");
		}).Num(), 1);
	TestTrue(TEXT("Restore P2 parcial resembrar perfiles faltantes"),
		Politics->GetCharacterPoliticalProfiles(TEXT("VE")).Num() > 1);

	FWLMediaPublicOpinionState StrongMedia;
	StrongMedia.NationIso = TEXT("CO");
	StrongMedia.PresidentialApproval = 95;
	FWLElectionState Election;
	Election.NationIso = TEXT("CO");
	Election.MonthsToElection = 0;
	Election.CampaignIntensity = 50;
	Election.Legitimacy = 85;
	Election.ConsecutiveTermsWon = 1;
	TestTrue(TEXT("Restore eleccion para limite de mandato"),
		Politics->RestoreGovernmentP2SaveSnapshot(
			TArray<FWLActiveReformState>(),
			TArray<FWLEnactedPolicyReformState>(),
			TArray<FWLPartyState>(),
			{ Election },
			TArray<FWLCharacterPoliticalProfile>(),
			TArray<FWLPatronageState>(),
			{ StrongMedia },
			TArray<FWLRegionGovernorState>(),
			TArray<FWLCrisisChainState>(),
			TArray<FWLGovernmentCalibrationState>(),
			TArray<FWLPoliticalActionRecord>(),
			Message));
	Politics->ProcessPoliticalMonth();
	const FWLElectionState TermElection = Politics->GetElectionState(TEXT("CO"));
	TestTrue(TEXT("Victoria consecutiva activa limite de mandato"), TermElection.bTermLimited);
	TestTrue(TEXT("Terminos consecutivos avanzan"), TermElection.ConsecutiveTermsWon >= 2);

	FWLElectionState BrokenPromiseElection;
	BrokenPromiseElection.NationIso = TEXT("CO");
	BrokenPromiseElection.MonthsToElection = 0;
	BrokenPromiseElection.CampaignPromiseReformId = TEXT("edu_public_schools");
	BrokenPromiseElection.bCampaignPromiseFulfilled = false;
	BrokenPromiseElection.CampaignIntensity = 50;
	BrokenPromiseElection.Legitimacy = 85;
	TestTrue(TEXT("Restore eleccion con promesa incumplida"),
		Politics->RestoreGovernmentP2SaveSnapshot(
			TArray<FWLActiveReformState>(),
			TArray<FWLEnactedPolicyReformState>(),
			TArray<FWLPartyState>(),
			{ BrokenPromiseElection },
			TArray<FWLCharacterPoliticalProfile>(),
			TArray<FWLPatronageState>(),
			{ StrongMedia },
			TArray<FWLRegionGovernorState>(),
			TArray<FWLCrisisChainState>(),
			TArray<FWLGovernmentCalibrationState>(),
			TArray<FWLPoliticalActionRecord>(),
			Message));
	Politics->ProcessPoliticalMonth();
	TestTrue(TEXT("Promesa incumplida deja reporte electoral"),
		Politics->GetElectionState(TEXT("CO")).LastElectionReport.Contains(TEXT("Promesa incumplida")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLPoliticalActionExecutorInvariantsTest,
	"WorldLeader.Government.PoliticalAction.ExecutorInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLPoliticalActionExecutorInvariantsTest::RunTest(const FString& Parameters)
{
	UWLCampaignGameInstance* GameInstance = NewObject<UWLCampaignGameInstance>();
	TestNotNull(TEXT("Campaign GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();
	TestTrue(TEXT("Campania CO"), GameInstance->StartNewCampaign(TEXT("CO")));

	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	UWLCharacterSubsystem* Characters = GameInstance->GetSubsystem<UWLCharacterSubsystem>();
	TestNotNull(TEXT("Political subsystem"), Politics);
	TestNotNull(TEXT("Tick subsystem"), Tick);
	TestNotNull(TEXT("Character subsystem"), Characters);
	if (!Politics || !Tick || !Characters)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	Tick->AdjustTreasury(TEXT("CO"), -Tick->GetTreasury(TEXT("CO")), Message);
	const int32 CapitalBeforeEvent = Characters->GetPoliticalCapital(TEXT("CO"));

	FWLPoliticalEventOption CostlyOption;
	CostlyOption.OptionId = TEXT("pay");
	CostlyOption.Label = TEXT("Pagar salida politica");
	CostlyOption.PoliticalCapitalDelta = -10;
	CostlyOption.TreasuryDelta = -5000;
	CostlyOption.OppositionDelta = -10;
	CostlyOption.PublicOrderDelta = 2;

	FWLPoliticalEventInstance CostlyEvent;
	CostlyEvent.InstanceId = TEXT("EV-7777");
	CostlyEvent.EventId = TEXT("costly_event_test");
	CostlyEvent.NationIso = TEXT("CO");
	CostlyEvent.Title = TEXT("Evento caro");
	CostlyEvent.Body = TEXT("Opcion cara para probar no mutation on failure.");
	CostlyEvent.Options.Add(CostlyOption);
	FWLCampaignOutcomeState Outcome;
	TestTrue(TEXT("Restaurar evento caro"),
		Politics->RestoreSaveSnapshot(
			TArray<FWLInternalPowerState>(),
			TArray<FWLDiplomaticRelationState>(),
			TArray<FWLIntelligenceNetworkState>(),
			{ CostlyEvent },
			Outcome,
			Message));
	TestFalse(TEXT("Evento caro falla sin recursos"),
		Politics->ResolveEvent(TEXT("EV-7777"), TEXT("pay"), Message));
	TestEqual(TEXT("Evento caro no cobra capital al fallar"),
		Characters->GetPoliticalCapital(TEXT("CO")),
		CapitalBeforeEvent);
	TestEqual(TEXT("Evento caro no baja tesoro al fallar"),
		Tick->GetTreasury(TEXT("CO")),
		static_cast<int64>(0));

	const int32 CapitalBeforeReform = Characters->GetPoliticalCapital(TEXT("CO"));
	TestFalse(TEXT("Reforma con coste de tesoro falla sin cobrar capital"),
		Politics->EnactPolicyReform(TEXT("CO"), TEXT("tax_broad_base"), Message));
	TestEqual(TEXT("Capital intacto tras reforma sin tesoro"),
		Characters->GetPoliticalCapital(TEXT("CO")),
		CapitalBeforeReform);

	Tick->AdjustTreasury(TEXT("CO"), 100000, Message);
	TestTrue(TEXT("Censura ejecuta una vez"),
		Politics->RunMediaAction(TEXT("CO"), EWLMediaActionType::Censorship, Message));
	TestFalse(TEXT("Censura bloqueada por cooldown"),
		Politics->RunMediaAction(TEXT("CO"), EWLMediaActionType::Censorship, Message));

	const TArray<FWLRegionGovernorState> Regions = Politics->GetRegionGovernors(TEXT("CO"));
	TestTrue(TEXT("Hay region para cooldown"), Regions.Num() > 0);
	if (Regions.Num() > 0)
	{
		TestTrue(TEXT("Inversion regional ejecuta una vez"),
			Politics->RunRegionPolicy(TEXT("CO"), Regions[0].RegionId, EWLRegionPolicyActionType::RegionalInvestment, Message));
		TestFalse(TEXT("Inversion regional bloqueada por cooldown"),
			Politics->RunRegionPolicy(TEXT("CO"), Regions[0].RegionId, EWLRegionPolicyActionType::RegionalInvestment, Message));
	}

	TArray<FWLActiveReformState> SavedReforms;
	TArray<FWLEnactedPolicyReformState> SavedEnactedReforms;
	TArray<FWLPartyState> SavedParties;
	TArray<FWLElectionState> SavedElections;
	TArray<FWLCharacterPoliticalProfile> SavedProfiles;
	TArray<FWLPatronageState> SavedPatronage;
	TArray<FWLMediaPublicOpinionState> SavedMedia;
	TArray<FWLRegionGovernorState> SavedRegions;
	TArray<FWLCrisisChainState> SavedCrises;
	TArray<FWLGovernmentCalibrationState> SavedCalibration;
	TArray<FWLPoliticalActionRecord> SavedActionRecords;
	Politics->WriteGovernmentP2SaveSnapshot(
		SavedReforms,
		SavedEnactedReforms,
		SavedParties,
		SavedElections,
		SavedProfiles,
		SavedPatronage,
		SavedMedia,
		SavedRegions,
		SavedCrises,
		SavedCalibration,
		SavedActionRecords);
	TestTrue(TEXT("Ledger guarda acciones politicas"), SavedActionRecords.Num() > 0);
	TestTrue(TEXT("Restaurar ledger de acciones politicas"),
		Politics->RestoreGovernmentP2SaveSnapshot(
			SavedReforms,
			SavedEnactedReforms,
			SavedParties,
			SavedElections,
			SavedProfiles,
			SavedPatronage,
			SavedMedia,
			SavedRegions,
			SavedCrises,
			SavedCalibration,
			SavedActionRecords,
			Message));
	FWLPoliticalActionRequest MediaPreviewRequest;
	MediaPreviewRequest.NationIso = TEXT("CO");
	MediaPreviewRequest.ActionType = EWLPoliticalActionType::RunMediaAction;
	MediaPreviewRequest.NumericValue = static_cast<int32>(EWLMediaActionType::Censorship);
	const FWLPoliticalActionPreview RestoredPreview = Politics->GetPoliticalActionPreview(MediaPreviewRequest);
	TestFalse(TEXT("Cooldown restaurado bloquea accion"), RestoredPreview.bCanExecute);
	TestTrue(TEXT("Cooldown restaurado informa meses"), RestoredPreview.CooldownRemainingMonths > 0);

	FWLCrisisChainState StageFourCrisis;
	StageFourCrisis.NationIso = TEXT("CO");
	StageFourCrisis.CrisisId = TEXT("CO|STAGE5");
	StageFourCrisis.Type = EWLCrisisChainType::SoftCoup;
	StageFourCrisis.Stage = 4;
	StageFourCrisis.Intensity = 90;
	StageFourCrisis.MonthsActive = 2;
	TestTrue(TEXT("Restaurar crisis etapa cuatro"),
		Politics->RestoreGovernmentP2SaveSnapshot(
			TArray<FWLActiveReformState>(),
			TArray<FWLEnactedPolicyReformState>(),
			TArray<FWLPartyState>(),
			TArray<FWLElectionState>(),
			TArray<FWLCharacterPoliticalProfile>(),
			TArray<FWLPatronageState>(),
			TArray<FWLMediaPublicOpinionState>(),
			TArray<FWLRegionGovernorState>(),
			{ StageFourCrisis },
			TArray<FWLGovernmentCalibrationState>(),
			TArray<FWLPoliticalActionRecord>(),
			Message));
	Politics->ProcessPoliticalMonth();
	const TArray<FWLCrisisChainState> Crises = Politics->GetActiveCrisisChains(TEXT("CO"));
	TestTrue(TEXT("Crisis puede alcanzar etapa cinco"),
		Crises.ContainsByPredicate([](const FWLCrisisChainState& Crisis)
		{
			return Crisis.CrisisId == TEXT("CO|STAGE5") && Crisis.Stage == 5;
		}));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLGovernmentMemoryChainsAndAITest,
	"WorldLeader.Government.RealGovernment.MemoryChainsAndAI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLGovernmentMemoryChainsAndAITest::RunTest(const FString& Parameters)
{
	UWLCampaignGameInstance* GameInstance = NewObject<UWLCampaignGameInstance>();
	TestNotNull(TEXT("Campaign GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();
	TestTrue(TEXT("Campania CO"), GameInstance->StartNewCampaign(TEXT("CO")));

	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	TestNotNull(TEXT("Political subsystem"), Politics);
	if (!Politics)
	{
		GameInstance->Shutdown();
		return false;
	}

	FWLPoliticalEventOption Repress;
	Repress.OptionId = TEXT("repress");
	Repress.Label = TEXT("Reprimir protestas");
	Repress.OppositionDelta = -8;
	Repress.PublicOrderDelta = -4;

	FWLPoliticalEventInstance Event;
	Event.InstanceId = TEXT("EV-0888");
	Event.EventId = TEXT("memory_seed");
	Event.NationIso = TEXT("CO");
	Event.Title = TEXT("Protesta local");
	Event.Body = TEXT("La protesta exige respuesta.");
	Event.Options.Add(Repress);

	FWLCampaignOutcomeState Outcome;
	FString Message;
	TestTrue(TEXT("Restaurar evento para memoria"),
		Politics->RestoreSaveSnapshot(
			TArray<FWLInternalPowerState>(),
			TArray<FWLDiplomaticRelationState>(),
			TArray<FWLIntelligenceNetworkState>(),
			{ Event },
			Outcome,
			Message));
	// Resolver un evento es una accion de gobierno: cuesta PA y, segun la opcion, tesoro/capital.
	// Se financia la caja para aislar el efecto (memoria/registro) del bloqueo por presupuesto.
	if (UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>())
	{
		FString FundMessage;
		Tick->AdjustTreasury(TEXT("CO"), 100000, FundMessage);
	}
	const bool bResolvedMemoryEvent = Politics->ResolveEvent(TEXT("EV-0888"), TEXT("repress"), Message);
	if (!bResolvedMemoryEvent)
	{
		AddError(FString::Printf(TEXT("ResolveEvent(EV-0888/repress) fallo: %s"), *Message));
	}
	TestTrue(TEXT("Resolver evento crea memoria"), bResolvedMemoryEvent);
	TestTrue(TEXT("Memoria politica registrada"),
		Politics->GetPoliticalMemory(TEXT("CO")).Num() > 0);
	TestTrue(TEXT("Resolver evento crea registro visible"),
		Politics->GetGovernmentLog(TEXT("CO")).ContainsByPredicate([](const FWLGovernmentLogEntry& Entry)
		{
			return Entry.Category == EWLGovernmentLogCategory::Event
				&& Entry.Title.Contains(TEXT("Evento resuelto"));
		}));

	FWLPoliticalMemoryRecord Unrest;
	Unrest.NationIso = TEXT("CO");
	Unrest.MemoryKey = TEXT("crisis_unrest");
	Unrest.Value = 2;
	Unrest.MonthsRemaining = 6;
	Unrest.LastReason = TEXT("test crisis");
	TestTrue(TEXT("Restaurar memoria de crisis"),
		Politics->RestoreGovernmentSaveSnapshot(
			TArray<FWLGovernmentAgendaState>(),
			TArray<FWLMinistryProgramState>(),
			TArray<FWLCabinetDynamicsState>(),
			TArray<FWLInstitutionalPowerState>(),
			TArray<FWLPublicGroupSupportState>(),
			TArray<FWLStateCapacityState>(),
			{ Unrest },
			TArray<FWLPoliticalAIPlanState>(),
			Message));
	Politics->ProcessPoliticalMonth();
	const TArray<FWLPoliticalEventInstance> CoEvents = Politics->GetQueuedEvents(TEXT("CO"));
	const bool bStrikeQueued = CoEvents.ContainsByPredicate([](const FWLPoliticalEventInstance& Queued)
	{
		return Queued.EventId == TEXT("crisis_strike");
	});
	TestTrue(TEXT("Memoria escala a cadena de crisis"), bStrikeQueued);

	TestTrue(TEXT("Reiniciar campania VE para probar IA CO"), GameInstance->StartNewCampaign(TEXT("VE")));
	FWLInternalPowerState CoInternal;
	CoInternal.NationIso = TEXT("CO");
	CoInternal.AveragePublicOrder = 35;
	CoInternal.OppositionStrength = 75;
	CoInternal.CoupRisk = 70;
	TestTrue(TEXT("Restaurar presion politica CO"),
		Politics->RestoreSaveSnapshot(
			{ CoInternal },
			TArray<FWLDiplomaticRelationState>(),
			TArray<FWLIntelligenceNetworkState>(),
			TArray<FWLPoliticalEventInstance>(),
			Outcome,
			Message));
	Politics->ProcessPoliticalMonth();
	const FWLPoliticalAIPlanState Plan = Politics->GetGovernmentAIPlan(TEXT("CO"));
	TestEqual(TEXT("IA politica elige estabilizar"),
		static_cast<int32>(Plan.Objective), static_cast<int32>(EWLGovernmentAIObjective::Stabilize));
	TestTrue(TEXT("IA politica define agenda"),
		Politics->GetGovernmentAgenda(TEXT("CO")).Priorities.Contains(EWLGovernmentPriority::Control));
	TestTrue(TEXT("IA politica inicia programa"), !Plan.CurrentProgramId.IsEmpty()
		|| Politics->GetActiveMinistryPrograms(TEXT("CO")).Num() > 0);
	// La IA conduce el backend de gobierno P2: arriba se comprueba que arranca un PROGRAMA ministerial
	// (ese backend). Las REFORMAS P2 exigen condiciones institucionales reales (coalicion>=50,
	// capacidad estatal, capital y tesoro) que una nacion en CRISIS no tiene — que la IA no fuerce una
	// reforma bajo crisis es correcto; aqui se verifica que el catalogo de reformas P2 esta cableado y
	// disponible para la nacion IA (su enactamiento con condiciones sanas se cubre en RealPoliticsSystems).
	TestTrue(TEXT("Backend P2 de reformas disponible para la IA"),
		Politics->GetAvailablePolicyReforms(TEXT("CO")).Num() > 0);
	TestTrue(TEXT("IA politica conserva partidos para coalicion"),
		Politics->GetPoliticalParties(TEXT("CO")).Num() >= 5);

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

	TArray<FWLInternalPowerState> SavedInternal;
	TArray<FWLDiplomaticRelationState> SavedRelations;
	TArray<FWLIntelligenceNetworkState> SavedNetworks;
	TArray<FWLPoliticalEventInstance> SavedEvents;
	FWLCampaignOutcomeState SavedOutcome;
	Tick->AdjustNationPublicOrder(TEXT("VE"), -40);
	Politics->WriteSaveSnapshot(SavedInternal, SavedRelations, SavedNetworks, SavedEvents, SavedOutcome);
	for (FWLInternalPowerState& SavedState : SavedInternal)
	{
		if (SavedState.NationIso == TEXT("VE"))
		{
			SavedState.AveragePublicOrder = 35;
			SavedState.OppositionStrength = 55;
			SavedState.CoupRisk = 65;
		}
	}
	TestTrue(TEXT("Restaurar presion interna para evento"),
		Politics->RestoreSaveSnapshot(SavedInternal, SavedRelations, SavedNetworks, SavedEvents, SavedOutcome, Message));
	Politics->ProcessPoliticalMonth();
	const TArray<FWLPoliticalEventInstance> Events = Politics->GetQueuedEvents(TEXT("VE"));
	TestTrue(TEXT("Eventos politicos encolados"), Events.Num() > 0);

	if (Events.Num() > 0 && Events[0].Options.Num() > 0)
	{
		// Financiar VE para que el bloqueo (si lo hay) no sea por presupuesto, y exponer el motivo real.
		Tick->AdjustTreasury(TEXT("VE"), 100000, Message);
		const bool bResolvedQueuedEvent =
			Politics->ResolveEvent(Events[0].InstanceId, Events[0].Options[0].OptionId, Message);
		if (!bResolvedQueuedEvent)
		{
			AddError(FString::Printf(TEXT("ResolveEvent(%s/%s) fallo: %s"),
				*Events[0].InstanceId, *Events[0].Options[0].OptionId, *Message));
		}
		TestTrue(TEXT("Resolver evento"), bResolvedQueuedEvent);
	}

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

// CONTRATO: la IA no se queda en casa. Antes declaraba guerras pero NUNCA atacaba (su cerebro no
// tenia seccion ofensiva) — las guerras no tenian dientes del lado enemigo. Ahora, en guerra, la IA
// marcha sus ejercitos hacia el enemigo, ataca y asalta. VE (bajo IA, jugador=CO) con un ejercito en
// su frontera debe, al procesar el mes, AVANZAR / combatir / conquistar territorio CO.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLStrategicAIWagesWarTest,
	"WorldLeader.Politics.StrategicAIWagesWar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLStrategicAIWagesWarTest::RunTest(const FString& Parameters)
{
	UWLCampaignGameInstance* GameInstance = NewObject<UWLCampaignGameInstance>();
	TestNotNull(TEXT("Campaign GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();
	// El jugador es CO -> VE queda bajo control de la IA estrategica.
	TestTrue(TEXT("Campania CO"), GameInstance->StartNewCampaign(TEXT("CO")));

	UWLMilitarySubsystem* Military = GameInstance->GetSubsystem<UWLMilitarySubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Military subsystem"), Military);
	TestNotNull(TEXT("Political subsystem"), Politics);
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Military || !Politics || !Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	// Ejercito blindado de la IA (VE) en su frontera, adyacente a territorio CO.
	const FString VeArmyId = Military->CreateArmy(TEXT("VE"), TEXT("VE-ZU"), TEXT("mbt"), 4, TEXT("Frontera"));
	TestFalse(TEXT("Ejercito IA creado"), VeArmyId.IsEmpty());
	FWLArmy Before;
	TestTrue(TEXT("Ejercito IA consultable"), Military->GetArmy(VeArmyId, Before));
	TestEqual(TEXT("Ejercito IA parte de su frontera"), Before.ProvinceId, FString(TEXT("VE-ZU")));

	TestTrue(TEXT("VE (IA) declara la guerra a CO"), Politics->DeclareWar(TEXT("VE"), TEXT("CO"), Message));

	// Procesar meses: la IA debe AVANZAR / combatir / conquistar. Antes del arreglo, nada de esto
	// ocurriria jamas (el ejercito se quedaba quieto en VE-ZU para siempre).
	bool bWagedWar = false;
	for (int32 Month = 0; Month < 6 && !bWagedWar; ++Month)
	{
		Politics->ProcessPoliticalMonth();
		FWLArmy After;
		const bool bArmyAlive = Military->GetArmy(VeArmyId, After);
		if (!bArmyAlive)
		{
			bWagedWar = true;   // el ejercito se consumio combatiendo (la IA ataco)
		}
		else if (After.ProvinceId != TEXT("VE-ZU"))
		{
			bWagedWar = true;   // marcho fuera de su frontera hacia el enemigo
		}
		if (Tick->GetProvinceControllerIso(TEXT("CO-CES")) == TEXT("VE"))
		{
			bWagedWar = true;   // conquisto territorio enemigo
		}
	}
	TestTrue(TEXT("La IA HACE la guerra (marcha/ataca/conquista), no se queda en casa"), bWagedWar);

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
