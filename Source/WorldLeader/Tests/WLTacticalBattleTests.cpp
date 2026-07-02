// Copyright World Leader project. See ROADMAP.md.
//
// Tests B2: backend tactico sin UI. Garantizan que la simulacion acepta ordenes
// y resuelve dano/moral/victoria de forma determinista.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Battle/WLTacticalBattleSubsystem.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Engine/GameInstance.h"
#include "Military/WLMilitarySubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalBattleBackendTest,
	"WorldLeader.Battle.TacticalBackend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalBattleBackendTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLTacticalBattleSubsystem* Tactical = GameInstance->GetSubsystem<UWLTacticalBattleSubsystem>();
	TestNotNull(TEXT("Tactical battle subsystem"), Tactical);
	if (!Tactical)
	{
		GameInstance->Shutdown();
		return false;
	}

	FWLArmy Attacker;
	Attacker.Id = TEXT("A1");
	Attacker.OwnerIso = TEXT("VE");
	Attacker.ProvinceId = TEXT("CO-CES");
	Attacker.Units = { TEXT("tank"), TEXT("tank"), TEXT("tank") };

	FWLArmy Defender;
	Defender.Id = TEXT("A2");
	Defender.OwnerIso = TEXT("CO");
	Defender.ProvinceId = TEXT("CO-CES");
	Defender.Units = { TEXT("infantry") };

	FWLTacticalBattleState Battle;
	FString Message;
	TestTrue(TEXT("Iniciar batalla tactica"),
		Tactical->StartTacticalBattleFromArmies(Attacker, Defender, TEXT("CO-CES"), Battle, Message));
	TestTrue(TEXT("Batalla activa"), Battle.bActive);
	TestEqual(TEXT("Unidades tacticas creadas"), Battle.Units.Num(), 4);
	TestEqual(TEXT("Objetivo creado"), Battle.Objectives.Num(), 1);

	FString DefenderUnitId;
	TArray<FString> AttackerUnitIds;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.OwnerIso == TEXT("CO"))
		{
			DefenderUnitId = Unit.TacticalUnitId;
		}
		else if (Unit.OwnerIso == TEXT("VE"))
		{
			AttackerUnitIds.Add(Unit.TacticalUnitId);
		}
	}
	TestFalse(TEXT("Defensor tactico encontrado"), DefenderUnitId.IsEmpty());
	TestEqual(TEXT("Tres atacantes tacticos"), AttackerUnitIds.Num(), 3);

	for (const FString& AttackerUnitId : AttackerUnitIds)
	{
		TestTrue(TEXT("Orden de ataque valida"),
			Tactical->IssueAttackOrder(Battle.BattleId, AttackerUnitId, DefenderUnitId, Message));
	}

	TArray<FString> Events;
	TestTrue(TEXT("Avanzar batalla tactica"),
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 30.0, Battle, Events));
	TestEqual(TEXT("Victoria tactica atacante"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));
	TestEqual(TEXT("Ganador VE"), Battle.WinnerIso, FString(TEXT("VE")));
	TestFalse(TEXT("Batalla cerrada"), Battle.bActive);
	TestTrue(TEXT("Eventos de batalla generados"), Events.Num() > 0);

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalBattleCampaignResultTest,
	"WorldLeader.Battle.TacticalCampaignResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalBattleCampaignResultTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLMilitarySubsystem* Military = GameInstance->GetSubsystem<UWLMilitarySubsystem>();
	UWLTacticalBattleSubsystem* Tactical = GameInstance->GetSubsystem<UWLTacticalBattleSubsystem>();
	UWLPoliticalSubsystem* Politics = GameInstance->GetSubsystem<UWLPoliticalSubsystem>();
	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Military subsystem"), Military);
	TestNotNull(TEXT("Tactical battle subsystem"), Tactical);
	TestNotNull(TEXT("Political subsystem"), Politics);
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (!Military || !Tactical || !Politics || !Tick)
	{
		GameInstance->Shutdown();
		return false;
	}

	FString Message;
	const FString AttackerArmyId = Military->CreateArmy(TEXT("VE"), TEXT("VE-ZU"), TEXT("tank"), 3, TEXT("Miranda"));
	const FString DefenderArmyId = Military->CreateArmy(TEXT("CO"), TEXT("CO-CES"), TEXT("infantry"), 1, TEXT("Santander"));
	TestFalse(TEXT("Atacante de campania valido"), AttackerArmyId.IsEmpty());
	TestFalse(TEXT("Defensor de campania valido"), DefenderArmyId.IsEmpty());
	TestTrue(TEXT("VE y CO en guerra"), Politics->DeclareWar(TEXT("VE"), TEXT("CO"), Message));

	FWLTacticalBattleState Battle;
	TestTrue(TEXT("Iniciar tactica desde campania"),
		Military->StartTacticalBattle(AttackerArmyId, DefenderArmyId, Battle, Message));
	TestEqual(TEXT("Enlace atacante tactico"), Battle.AttackerArmyId, AttackerArmyId);
	TestEqual(TEXT("Enlace defensor tactico"), Battle.DefenderArmyId, DefenderArmyId);
	TestEqual(TEXT("Provincia tactica defensora"), Battle.ProvinceId, FString(TEXT("CO-CES")));

	FString DefenderTacticalUnitId;
	TArray<FString> AttackerTacticalUnitIds;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.SourceArmyId == DefenderArmyId)
		{
			DefenderTacticalUnitId = Unit.TacticalUnitId;
		}
		else if (Unit.SourceArmyId == AttackerArmyId)
		{
			AttackerTacticalUnitIds.Add(Unit.TacticalUnitId);
		}
	}
	TestFalse(TEXT("Unidad defensora tactica encontrada"), DefenderTacticalUnitId.IsEmpty());
	TestEqual(TEXT("Tres unidades atacantes enlazadas"), AttackerTacticalUnitIds.Num(), 3);

	for (const FString& TacticalUnitId : AttackerTacticalUnitIds)
	{
		TestTrue(TEXT("Orden tactica desde backend oficial"),
			Tactical->IssueAttackOrder(Battle.BattleId, TacticalUnitId, DefenderTacticalUnitId, Message));
	}

	TArray<FString> Events;
	TestTrue(TEXT("Resolver tactica oficial"),
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 30.0, Battle, Events));
	TestEqual(TEXT("Resultado tactico atacante"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));

	TestTrue(TEXT("Aplicar resultado tactico a campania"),
		Military->ApplyTacticalBattleResult(Battle.BattleId, Message));

	FWLArmy AttackerAfter;
	TestTrue(TEXT("Atacante sobrevive"), Military->GetArmy(AttackerArmyId, AttackerAfter));
	TestEqual(TEXT("Atacante ocupa provincia tactica"), AttackerAfter.ProvinceId, FString(TEXT("CO-CES")));
	TestFalse(TEXT("Defensor eliminado de campania"), Military->GetArmy(DefenderArmyId, AttackerAfter));
	TestEqual(TEXT("Control provincial cambia a VE"), Tick->GetProvinceControllerIso(TEXT("CO-CES")), FString(TEXT("VE")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalBattleAITest,
	"WorldLeader.Battle.TacticalAI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalBattleAITest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLTacticalBattleSubsystem* Tactical = GameInstance->GetSubsystem<UWLTacticalBattleSubsystem>();
	TestNotNull(TEXT("Tactical battle subsystem"), Tactical);
	if (!Tactical)
	{
		GameInstance->Shutdown();
		return false;
	}

	FWLArmy Attacker;
	Attacker.Id = TEXT("A1");
	Attacker.OwnerIso = TEXT("VE");
	Attacker.ProvinceId = TEXT("CO-CES");
	Attacker.Units = { TEXT("tank"), TEXT("tank"), TEXT("tank") };

	FWLArmy Defender;
	Defender.Id = TEXT("A2");
	Defender.OwnerIso = TEXT("CO");
	Defender.ProvinceId = TEXT("CO-CES");
	Defender.Units = { TEXT("infantry") };

	FWLTacticalBattleState Battle;
	FString Message;
	TestTrue(TEXT("Iniciar batalla tactica con IA"),
		Tactical->StartTacticalBattleFromArmies(Attacker, Defender, TEXT("CO-CES"), Battle, Message));
	TestTrue(TEXT("Activar IA atacante"),
		Tactical->SetTacticalAIControl(Battle.BattleId, TEXT("VE"), true, Message));

	TArray<FString> Events;
	TestTrue(TEXT("IA avanza sin ordenes manuales"),
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events));
	TestTrue(TEXT("Eventos de IA generados"),
		Events.ContainsByPredicate([](const FString& Event)
		{
			return Event.Contains(TEXT("IA tactica"));
		}));
	TestTrue(TEXT("VE marcado como controlado por IA"), Battle.IsOwnerAIControlled(TEXT("VE")));

	Events.Reset();
	TestTrue(TEXT("IA resuelve combate"),
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 30.0, Battle, Events));
	TestEqual(TEXT("Victoria atacante por IA"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));
	TestEqual(TEXT("Ganador IA VE"), Battle.WinnerIso, FString(TEXT("VE")));

	TestTrue(TEXT("Desactivar IA atacante"),
		Tactical->SetTacticalAIControl(Battle.BattleId, TEXT("VE"), false, Message));
	FWLTacticalBattleState FinalBattle;
	TestTrue(TEXT("Leer batalla final"), Tactical->GetTacticalBattleState(Battle.BattleId, FinalBattle));
	TestFalse(TEXT("VE ya no esta marcado como IA"), FinalBattle.IsOwnerAIControlled(TEXT("VE")));

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalBattleMoveOrderTest,
	"WorldLeader.Battle.TacticalMoveOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalBattleMoveOrderTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLTacticalBattleSubsystem* Tactical = GameInstance->GetSubsystem<UWLTacticalBattleSubsystem>();
	TestNotNull(TEXT("Tactical battle subsystem"), Tactical);
	if (!Tactical)
	{
		GameInstance->Shutdown();
		return false;
	}

	FWLArmy Attacker;
	Attacker.Id = TEXT("A1");
	Attacker.OwnerIso = TEXT("VE");
	Attacker.ProvinceId = TEXT("CO-CES");
	Attacker.Units = { TEXT("infantry") };

	FWLArmy Defender;
	Defender.Id = TEXT("A2");
	Defender.OwnerIso = TEXT("CO");
	Defender.ProvinceId = TEXT("CO-CES");
	Defender.Units = { TEXT("infantry") };

	FWLTacticalBattleState Battle;
	FString Message;
	TestTrue(TEXT("Iniciar batalla tactica para movimiento"),
		Tactical->StartTacticalBattleFromArmies(Attacker, Defender, TEXT("CO-CES"), Battle, Message));
	const FString MovingUnitId = Battle.Units[0].TacticalUnitId;
	const FVector2D Start = Battle.Units[0].Position;
	TestTrue(TEXT("Orden de movimiento valida"),
		Tactical->IssueMoveOrder(Battle.BattleId, MovingUnitId, FVector2D(0.0, 0.0), Message));

	TArray<FString> Events;
	TestTrue(TEXT("Avanzar movimiento tactico"),
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events));

	FWLTacticalUnitState MovedUnit;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.TacticalUnitId == MovingUnitId)
		{
			MovedUnit = Unit;
			break;
		}
	}
	TestTrue(TEXT("La unidad se acerco al objetivo"),
		FVector2D::Distance(MovedUnit.Position, FVector2D::ZeroVector) < FVector2D::Distance(Start, FVector2D::ZeroVector));

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
