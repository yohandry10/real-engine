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
	// F1 contingentes: 3x tank se agrupan en UN contingente (3 elementos) + 1 de infanteria.
	TestEqual(TEXT("Contingentes tacticos creados"), Battle.Units.Num(), 2);
	TestEqual(TEXT("Objetivo creado"), Battle.Objectives.Num(), 1);

	FString DefenderUnitId;
	TArray<FString> AttackerUnitIds;
	int32 AttackerElements = 0;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.OwnerIso == TEXT("CO"))
		{
			DefenderUnitId = Unit.TacticalUnitId;
		}
		else if (Unit.OwnerIso == TEXT("VE"))
		{
			AttackerUnitIds.Add(Unit.TacticalUnitId);
			AttackerElements += Unit.ElementCount;
		}
	}
	TestFalse(TEXT("Defensor tactico encontrado"), DefenderUnitId.IsEmpty());
	TestEqual(TEXT("Un contingente atacante"), AttackerUnitIds.Num(), 1);
	TestEqual(TEXT("Tres elementos atacantes"), AttackerElements, 3);

	for (const FString& AttackerUnitId : AttackerUnitIds)
	{
		TestTrue(TEXT("Orden de ataque valida"),
			Tactical->IssueAttackOrder(Battle.BattleId, AttackerUnitId, DefenderUnitId, Message));
	}

	// Ticks pequenos (como el juego real): el tanque cierra distancia a su alcance y dispara.
	TArray<FString> Events;
	TArray<FString> AllEvents;
	for (int32 Step = 0; Step < 120 && Battle.bActive; ++Step)
	{
		TestTrue(TEXT("Avanzar batalla tactica"),
			Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events));
		AllEvents.Append(Events);
	}
	TestEqual(TEXT("Victoria tactica atacante"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));
	TestEqual(TEXT("Ganador VE"), Battle.WinnerIso, FString(TEXT("VE")));
	TestFalse(TEXT("Batalla cerrada"), Battle.bActive);
	TestTrue(TEXT("Eventos de batalla generados"), AllEvents.Num() > 0);

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
	int32 AttackerElements = 0;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.SourceArmyId == DefenderArmyId)
		{
			DefenderTacticalUnitId = Unit.TacticalUnitId;
		}
		else if (Unit.SourceArmyId == AttackerArmyId)
		{
			AttackerTacticalUnitIds.Add(Unit.TacticalUnitId);
			AttackerElements += Unit.ElementCount;
		}
	}
	TestFalse(TEXT("Unidad defensora tactica encontrada"), DefenderTacticalUnitId.IsEmpty());
	TestEqual(TEXT("Un contingente atacante enlazado"), AttackerTacticalUnitIds.Num(), 1);
	TestEqual(TEXT("Tres elementos atacantes enlazados"), AttackerElements, 3);

	for (const FString& TacticalUnitId : AttackerTacticalUnitIds)
	{
		TestTrue(TEXT("Orden tactica desde backend oficial"),
			Tactical->IssueAttackOrder(Battle.BattleId, TacticalUnitId, DefenderTacticalUnitId, Message));
	}

	TArray<FString> Events;
	for (int32 Step = 0; Step < 120 && Battle.bActive; ++Step)
	{
		TestTrue(TEXT("Resolver tactica oficial"),
			Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events));
	}
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
	for (int32 Step = 0; Step < 120 && Battle.bActive; ++Step)
	{
		TestTrue(TEXT("IA resuelve combate"),
			Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events));
	}
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

// F1 armas combinadas — CONTRATO de la matriz de contras (Docs/TACTICAL_BATTLE_GAMEPLAY.md):
// en campo abierto, 4 tanques MBT contra 50 de infanteria es una masacre (el equivalente
// moderno de caballeria vs arqueros): el tanque gana conservando >=70% de salud.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalCounterMatrixOpenFieldTest,
	"WorldLeader.Battle.TacticalCounterMatrixOpenField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalCounterMatrixOpenFieldTest::RunTest(const FString& Parameters)
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

	FWLArmy TankSide;
	TankSide.Id = TEXT("A-MBT");
	TankSide.OwnerIso = TEXT("VE");
	TankSide.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 4; ++i) { TankSide.Units.Add(TEXT("mbt")); }

	FWLArmy InfantrySide;
	InfantrySide.Id = TEXT("A-INF");
	InfantrySide.OwnerIso = TEXT("CO");
	InfantrySide.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 50; ++i) { InfantrySide.Units.Add(TEXT("infantry")); }

	FWLTacticalBattleState Battle;
	FString Message;
	TestTrue(TEXT("Iniciar matchup MBT vs infanteria"),
		Tactical->StartTacticalBattleFromArmies(TankSide, InfantrySide, TEXT("CO-CES"), Battle, Message));
	TestEqual(TEXT("Dos contingentes"), Battle.Units.Num(), 2);

	FString TankUnitId, InfantryUnitId;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.UnitId == TEXT("mbt"))      { TankUnitId = Unit.TacticalUnitId; }
		if (Unit.UnitId == TEXT("infantry")) { InfantryUnitId = Unit.TacticalUnitId; }
	}
	TestFalse(TEXT("Contingente MBT creado"), TankUnitId.IsEmpty());
	TestFalse(TEXT("Contingente infanteria creado"), InfantryUnitId.IsEmpty());

	// Ambos con orden de atacar al otro: duelo frontal en campo abierto.
	TestTrue(TEXT("Orden MBT->infanteria"),
		Tactical->IssueAttackOrder(Battle.BattleId, TankUnitId, InfantryUnitId, Message));
	TestTrue(TEXT("Orden infanteria->MBT"),
		Tactical->IssueAttackOrder(Battle.BattleId, InfantryUnitId, TankUnitId, Message));

	TArray<FString> Events;
	for (int32 Step = 0; Step < 300 && Battle.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events);
	}

	TestEqual(TEXT("El bando MBT gana en abierto"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));

	double TankHealth = 0.0;
	int32 TankElements = 0;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.UnitId == TEXT("mbt"))
		{
			TankHealth = Unit.Health;
			TankElements = Unit.ElementCount;
		}
	}
	TestTrue(TEXT("MBT conserva >=70% de salud (masacre, no intercambio)"), TankHealth >= 70.0);
	TestEqual(TEXT("MBT no pierde elementos"), TankElements, 4);

	GameInstance->Shutdown();
	return true;
}

// F2 terreno — CONTRATO del flip urbano (Docs/TACTICAL_BATTLE_GAMEPLAY.md): el MISMO matchup
// que en abierto es masacre del tanque (4 MBT vs 50 de infanteria) se INVIERTE si la
// infanteria defiende una ciudad: el tanque debe entrar a la zona urbana (alcance de
// cobertura) y ahi los equipos ATGM entre edificios lo revientan.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalTerrainUrbanFlipTest,
	"WorldLeader.Battle.TacticalTerrainUrbanFlip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalTerrainUrbanFlipTest::RunTest(const FString& Parameters)
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

	FWLArmy TankSide;
	TankSide.Id = TEXT("A-MBT");
	TankSide.OwnerIso = TEXT("VE");
	TankSide.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 4; ++i) { TankSide.Units.Add(TEXT("mbt")); }

	FWLArmy InfantrySide;
	InfantrySide.Id = TEXT("A-INF");
	InfantrySide.OwnerIso = TEXT("CO");
	InfantrySide.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 50; ++i) { InfantrySide.Units.Add(TEXT("infantry")); }

	FWLTacticalBattleState Battle;
	FString Message;
	TestTrue(TEXT("Iniciar matchup MBT vs infanteria urbana"),
		Tactical->StartTacticalBattleFromArmies(TankSide, InfantrySide, TEXT("CO-CES"), Battle, Message));

	FString TankUnitId, InfantryUnitId;
	FVector2D InfantryPosition = FVector2D::ZeroVector;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.UnitId == TEXT("mbt"))      { TankUnitId = Unit.TacticalUnitId; }
		if (Unit.UnitId == TEXT("infantry")) { InfantryUnitId = Unit.TacticalUnitId; InfantryPosition = Unit.Position; }
	}
	TestFalse(TEXT("Contingente MBT creado"), TankUnitId.IsEmpty());
	TestFalse(TEXT("Contingente infanteria creado"), InfantryUnitId.IsEmpty());

	// La ciudad defendida: parche urbano centrado en la infanteria.
	TestTrue(TEXT("Parche urbano creado"),
		Tactical->AddTacticalTerrainPatch(Battle.BattleId, EWLTacticalTerrain::Urban, InfantryPosition, 520.0, Message));
	TestTrue(TEXT("Leer estado con parche"), Tactical->GetTacticalBattleState(Battle.BattleId, Battle));
	TestEqual(TEXT("Terreno urbano en la posicion defensora"),
		static_cast<int32>(UWLTacticalBattleSubsystem::TerrainAtPosition(Battle, InfantryPosition)),
		static_cast<int32>(EWLTacticalTerrain::Urban));

	// Mismo duelo frontal que el test de campo abierto.
	TestTrue(TEXT("Orden MBT->infanteria"),
		Tactical->IssueAttackOrder(Battle.BattleId, TankUnitId, InfantryUnitId, Message));
	TestTrue(TEXT("Orden infanteria->MBT"),
		Tactical->IssueAttackOrder(Battle.BattleId, InfantryUnitId, TankUnitId, Message));

	TArray<FString> Events;
	for (int32 Step = 0; Step < 400 && Battle.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events);
	}

	TestEqual(TEXT("En ciudad gana la infanteria defensora"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::DefenderVictory));

	int32 InfantryElements = 0;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.UnitId == TEXT("infantry"))
		{
			InfantryElements = Unit.ElementCount;
		}
	}
	TestTrue(TEXT("La infanteria urbana conserva efectivos"), InfantryElements >= 10);

	GameInstance->Shutdown();
	return true;
}

// F3 capa aerea — CONTRATO del paraguas SAM (Docs/TACTICAL_BATTLE_GAMEPLAY.md): al aire solo
// le pegan SAM y cazas. Con el SAM vivo, los helos atacantes caen bajo el paraguas ANTES de
// limpiar los blindados; sin SAM, el helo caza tanques con impunidad (ni lo tocan).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalAirUmbrellaTest,
	"WorldLeader.Battle.TacticalAirUmbrella",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalAirUmbrellaTest::RunTest(const FString& Parameters)
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

	FWLArmy HeliSide;
	HeliSide.Id = TEXT("A-HELI");
	HeliSide.OwnerIso = TEXT("VE");
	HeliSide.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 4; ++i) { HeliSide.Units.Add(TEXT("heli")); }

	FWLArmy ArmorWithSam;
	ArmorWithSam.Id = TEXT("A-DEF");
	ArmorWithSam.OwnerIso = TEXT("CO");
	ArmorWithSam.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 3; ++i) { ArmorWithSam.Units.Add(TEXT("mbt")); }
	for (int32 i = 0; i < 2; ++i) { ArmorWithSam.Units.Add(TEXT("sam")); }

	// --- Batalla A: paraguas SAM activo. El SAM dispara SOLO (sin ordenes) a lo aereo. ---
	FWLTacticalBattleState BattleA;
	FString Message;
	TestTrue(TEXT("Iniciar helos vs blindados con SAM"),
		Tactical->StartTacticalBattleFromArmies(HeliSide, ArmorWithSam, TEXT("CO-CES"), BattleA, Message));

	FString HeliUnitId, TankUnitId;
	for (const FWLTacticalUnitState& Unit : BattleA.Units)
	{
		if (Unit.UnitId == TEXT("heli")) { HeliUnitId = Unit.TacticalUnitId; }
		if (Unit.UnitId == TEXT("mbt"))  { TankUnitId = Unit.TacticalUnitId; }
	}
	TestTrue(TEXT("Orden helo->blindados"),
		Tactical->IssueAttackOrder(BattleA.BattleId, HeliUnitId, TankUnitId, Message));

	TArray<FString> Events;
	for (int32 Step = 0; Step < 300 && BattleA.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(BattleA.BattleId, 1.0, BattleA, Events);
	}
	TestEqual(TEXT("Con SAM vivo el atacante aereo pierde"),
		static_cast<int32>(BattleA.Result), static_cast<int32>(EWLTacticalBattleResult::DefenderVictory));

	// --- Batalla B: mismo ataque sin SAM. Nada toca al helo: limpia blindados al 100%. ---
	FWLArmy ArmorOnly;
	ArmorOnly.Id = TEXT("A-DEF2");
	ArmorOnly.OwnerIso = TEXT("CO");
	ArmorOnly.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 3; ++i) { ArmorOnly.Units.Add(TEXT("mbt")); }

	FWLTacticalBattleState BattleB;
	TestTrue(TEXT("Iniciar helos vs blindados sin SAM"),
		Tactical->StartTacticalBattleFromArmies(HeliSide, ArmorOnly, TEXT("CO-CES"), BattleB, Message));
	FString HeliUnitIdB, TankUnitIdB;
	for (const FWLTacticalUnitState& Unit : BattleB.Units)
	{
		if (Unit.UnitId == TEXT("heli")) { HeliUnitIdB = Unit.TacticalUnitId; }
		if (Unit.UnitId == TEXT("mbt"))  { TankUnitIdB = Unit.TacticalUnitId; }
	}
	TestTrue(TEXT("Orden helo->blindados sin SAM"),
		Tactical->IssueAttackOrder(BattleB.BattleId, HeliUnitIdB, TankUnitIdB, Message));

	for (int32 Step = 0; Step < 300 && BattleB.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(BattleB.BattleId, 1.0, BattleB, Events);
	}
	TestEqual(TEXT("Sin SAM el helo limpia los blindados"),
		static_cast<int32>(BattleB.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));

	double HeliHealth = 0.0;
	for (const FWLTacticalUnitState& Unit : BattleB.Units)
	{
		if (Unit.UnitId == TEXT("heli"))
		{
			HeliHealth = Unit.Health;
		}
	}
	TestTrue(TEXT("Al helo sin SAM enfrente ni lo tocan"), HeliHealth >= 99.0);

	GameInstance->Shutdown();
	return true;
}

// F3 fuego indirecto — la artilleria dispara SALVAS con vuelo contra la posicion fijada al
// disparar: un objetivo ESTATICO es demolido a distancia (2200 vs 300 de alcance).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalIndirectFireTest,
	"WorldLeader.Battle.TacticalIndirectFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalIndirectFireTest::RunTest(const FString& Parameters)
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

	FWLArmy ArtillerySide;
	ArtillerySide.Id = TEXT("A-ART");
	ArtillerySide.OwnerIso = TEXT("VE");
	ArtillerySide.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 2; ++i) { ArtillerySide.Units.Add(TEXT("artillery")); }

	FWLArmy StaticInfantry;
	StaticInfantry.Id = TEXT("A-INF");
	StaticInfantry.OwnerIso = TEXT("CO");
	StaticInfantry.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 10; ++i) { StaticInfantry.Units.Add(TEXT("infantry")); }

	FWLTacticalBattleState Battle;
	FString Message;
	TestTrue(TEXT("Iniciar artilleria vs infanteria estatica"),
		Tactical->StartTacticalBattleFromArmies(ArtillerySide, StaticInfantry, TEXT("CO-CES"), Battle, Message));

	FString ArtilleryUnitId, InfantryUnitId;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.UnitId == TEXT("artillery")) { ArtilleryUnitId = Unit.TacticalUnitId; }
		if (Unit.UnitId == TEXT("infantry"))  { InfantryUnitId = Unit.TacticalUnitId; }
	}
	TestTrue(TEXT("Orden artilleria->infanteria"),
		Tactical->IssueAttackOrder(Battle.BattleId, ArtilleryUnitId, InfantryUnitId, Message));

	TArray<FString> Events;
	bool bVolleyFired = false;
	for (int32 Step = 0; Step < 120 && Battle.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events);
		bVolleyFired = bVolleyFired || Events.ContainsByPredicate([](const FString& Event)
		{
			return Event.Contains(TEXT("salva"));
		});
	}
	TestTrue(TEXT("La artilleria disparo salvas"), bVolleyFired);
	TestEqual(TEXT("El objetivo estatico es demolido a distancia"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));

	GameInstance->Shutdown();
	return true;
}

// F4 flanqueo y moral — CONTRATO (Docs/TACTICAL_BATTLE_GAMEPLAY.md): rodear un contingente
// lo ROMPE (desbandada con la mayoria de efectivos vivos) antes que desgastarlo de frente
// (que lo aniquila sin romperlo). Mismo matchup 20 vs 20 de infanteria, defensor quieto.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalFlankRoutTest,
	"WorldLeader.Battle.TacticalFlankRout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalFlankRoutTest::RunTest(const FString& Parameters)
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

	// Asalto = infanteria (la que maniobra) + un SAM que se queda FIJANDO el objetivo
	// central (contra infanteria es inofensivo): sin el, marchar al rodeo regalaria la
	// victoria por captura al defensor. Fijar y flanquear, como manda el manual.
	FWLArmy Assault;
	Assault.Id = TEXT("A-ASLT");
	Assault.OwnerIso = TEXT("VE");
	Assault.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 10; ++i) { Assault.Units.Add(TEXT("infantry")); }
	Assault.Units.Add(TEXT("sam"));

	FWLArmy Holding;
	Holding.Id = TEXT("A-HOLD");
	Holding.OwnerIso = TEXT("CO");
	Holding.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 20; ++i) { Holding.Units.Add(TEXT("infantry")); }

	auto FindUnitIds = [](const FWLTacticalBattleState& Battle, FString& OutAttacker, FString& OutDefender)
	{
		for (const FWLTacticalUnitState& Unit : Battle.Units)
		{
			if (Unit.OwnerIso == TEXT("VE") && Unit.UnitId == TEXT("infantry")) { OutAttacker = Unit.TacticalUnitId; }
			if (Unit.OwnerIso == TEXT("CO"))                                    { OutDefender = Unit.TacticalUnitId; }
		}
	};
	auto FindDefender = [](const FWLTacticalBattleState& Battle) -> FWLTacticalUnitState
	{
		for (const FWLTacticalUnitState& Unit : Battle.Units)
		{
			if (Unit.OwnerIso == TEXT("CO")) { return Unit; }
		}
		return FWLTacticalUnitState();
	};

	FString Message;
	TArray<FString> Events;

	// --- Batalla A: asalto FRONTAL. El defensor aguanta hasta ser aniquilado, sin romperse. ---
	FWLTacticalBattleState Frontal;
	TestTrue(TEXT("Iniciar asalto frontal"),
		Tactical->StartTacticalBattleFromArmies(Assault, Holding, TEXT("CO-CES"), Frontal, Message));
	FString FrontalAttackerId, FrontalDefenderId;
	FindUnitIds(Frontal, FrontalAttackerId, FrontalDefenderId);
	TestTrue(TEXT("Orden de asalto frontal"),
		Tactical->IssueAttackOrder(Frontal.BattleId, FrontalAttackerId, FrontalDefenderId, Message));
	for (int32 Step = 0; Step < 200 && Frontal.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Frontal.BattleId, 1.0, Frontal, Events);
	}
	const FWLTacticalUnitState FrontalDefender = FindDefender(Frontal);
	TestEqual(TEXT("Asalto frontal gana por desgaste"),
		static_cast<int32>(Frontal.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));
	TestTrue(TEXT("De frente el defensor muere sin romperse"), FrontalDefender.bDestroyed);

	// --- Batalla B: RODEO. Marchar a la retaguardia del defensor y atacar desde atras. ---
	FWLTacticalBattleState Flanked;
	TestTrue(TEXT("Iniciar asalto por retaguardia"),
		Tactical->StartTacticalBattleFromArmies(Assault, Holding, TEXT("CO-CES"), Flanked, Message));
	FString FlankAttackerId, FlankDefenderId;
	FindUnitIds(Flanked, FlankAttackerId, FlankDefenderId);
	TestTrue(TEXT("Orden de marcha envolvente"),
		Tactical->IssueMoveOrder(Flanked.BattleId, FlankAttackerId, FVector2D(1100.0, 260.0), Message));
	for (int32 Step = 0; Step < 80 && Flanked.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Flanked.BattleId, 1.0, Flanked, Events);
		const FWLTacticalUnitState* Marching = Flanked.Units.FindByPredicate(
			[&FlankAttackerId](const FWLTacticalUnitState& U) { return U.TacticalUnitId == FlankAttackerId; });
		if (Marching && Marching->Order == EWLTacticalUnitOrder::Idle)
		{
			break;   // llego a la retaguardia
		}
	}
	TestTrue(TEXT("Orden de ataque por la espalda"),
		Tactical->IssueAttackOrder(Flanked.BattleId, FlankAttackerId, FlankDefenderId, Message));
	for (int32 Step = 0; Step < 200 && Flanked.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Flanked.BattleId, 1.0, Flanked, Events);
	}
	const FWLTacticalUnitState FlankedDefender = FindDefender(Flanked);
	TestEqual(TEXT("El rodeo tambien gana"),
		static_cast<int32>(Flanked.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));
	TestFalse(TEXT("Rodeado se ROMPE, no muere"), FlankedDefender.bDestroyed);
	TestEqual(TEXT("Rodeado termina en desbandada"),
		static_cast<int32>(FlankedDefender.Order), static_cast<int32>(EWLTacticalUnitOrder::Routing));
	TestTrue(TEXT("Roto con la mayoria de la salud intacta"), FlankedDefender.Health > 25.0);

	GameInstance->Shutdown();
	return true;
}

// F5 IA de batalla — CONTRATO: la IA elige objetivo por MATRIZ, no por cercania. Una
// infanteria con un SAM (contra 1.5) mas cerca y una artilleria (contra 1.6) algo mas
// lejos debe atacar la ARTILLERIA. Y el SAM de la IA no maniobra: su trabajo es el paraguas.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalAIMatrixTargetingTest,
	"WorldLeader.Battle.TacticalAIMatrixTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalAIMatrixTargetingTest::RunTest(const FString& Parameters)
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

	FWLArmy Rifles;
	Rifles.Id = TEXT("A-RIF");
	Rifles.OwnerIso = TEXT("VE");
	Rifles.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 10; ++i) { Rifles.Units.Add(TEXT("infantry")); }

	// Defensor: artilleria (primera linea), SAM (segunda) y MBT (tercera). Desde el centro
	// del campo el SAM queda MAS CERCA que la artilleria: elegir por cercania fallaria.
	FWLArmy Mixed;
	Mixed.Id = TEXT("A-MIX");
	Mixed.OwnerIso = TEXT("CO");
	Mixed.ProvinceId = TEXT("CO-CES");
	for (int32 i = 0; i < 2; ++i) { Mixed.Units.Add(TEXT("artillery")); }
	Mixed.Units.Add(TEXT("sam"));
	for (int32 i = 0; i < 4; ++i) { Mixed.Units.Add(TEXT("mbt")); }

	FWLTacticalBattleState Battle;
	FString Message;
	TestTrue(TEXT("Iniciar prueba de eleccion de objetivo"),
		Tactical->StartTacticalBattleFromArmies(Rifles, Mixed, TEXT("CO-CES"), Battle, Message));

	FString RiflesId, ArtilleryId, SamId;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.UnitId == TEXT("infantry"))  { RiflesId = Unit.TacticalUnitId; }
		if (Unit.UnitId == TEXT("artillery")) { ArtilleryId = Unit.TacticalUnitId; }
		if (Unit.UnitId == TEXT("sam"))       { SamId = Unit.TacticalUnitId; }
	}

	// Marchar la infanteria al centro (ambos enemigos a tiro de decision) y soltar la IA.
	TestTrue(TEXT("Marcha al centro del campo"),
		Tactical->IssueMoveOrder(Battle.BattleId, RiflesId, FVector2D(0.0, 0.0), Message));
	TArray<FString> Events;
	for (int32 Step = 0; Step < 40 && Battle.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events);
		const FWLTacticalUnitState* Marching = Battle.Units.FindByPredicate(
			[&RiflesId](const FWLTacticalUnitState& U) { return U.TacticalUnitId == RiflesId; });
		if (Marching && Marching->Order == EWLTacticalUnitOrder::Idle)
		{
			break;
		}
	}
	TestTrue(TEXT("IA para ambos bandos"),
		Tactical->SetTacticalAIControl(Battle.BattleId, TEXT("VE"), true, Message)
		&& Tactical->SetTacticalAIControl(Battle.BattleId, TEXT("CO"), true, Message));
	Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events);

	const FWLTacticalUnitState* Rifle = Battle.Units.FindByPredicate(
		[&RiflesId](const FWLTacticalUnitState& U) { return U.TacticalUnitId == RiflesId; });
	const FWLTacticalUnitState* Sam = Battle.Units.FindByPredicate(
		[&SamId](const FWLTacticalUnitState& U) { return U.TacticalUnitId == SamId; });
	TestNotNull(TEXT("Infanteria IA viva"), Rifle);
	TestNotNull(TEXT("SAM IA vivo"), Sam);
	if (Rifle)
	{
		TestEqual(TEXT("La IA ataca por matriz (artilleria), no al mas cercano (SAM)"),
			Rifle->AttackTargetUnitId, ArtilleryId);
	}
	if (Sam)
	{
		TestEqual(TEXT("El SAM de la IA se queda en su puesto"),
			static_cast<int32>(Sam->Order), static_cast<int32>(EWLTacticalUnitOrder::Idle));
	}

	GameInstance->Shutdown();
	return true;
}

// F5 paridad — CONTRATO: auto-resolver corre la MISMA simulacion tactica que la batalla
// manual (IA en ambos bandos) y aplica bajas y ocupacion a campania en una sola llamada.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalAutoResolveParityTest,
	"WorldLeader.Battle.TacticalAutoResolveParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalAutoResolveParityTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

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
	const FString AttackerArmyId = Military->CreateArmy(TEXT("VE"), TEXT("VE-ZU"), TEXT("tank"), 3, TEXT("Miranda"));
	const FString DefenderArmyId = Military->CreateArmy(TEXT("CO"), TEXT("CO-CES"), TEXT("infantry"), 1, TEXT("Santander"));
	TestTrue(TEXT("VE y CO en guerra para paridad"), Politics->DeclareWar(TEXT("VE"), TEXT("CO"), Message));

	FString Report;
	const EWLBattleResult Result = Military->ResolveTacticalBattleToEnd(AttackerArmyId, DefenderArmyId, Report);
	TestEqual(TEXT("Auto-resolve tactico: gana el atacante blindado"),
		static_cast<int32>(Result), static_cast<int32>(EWLBattleResult::AttackerVictory));
	TestTrue(TEXT("El reporte viene de la simulacion tactica"), Report.Contains(TEXT("Batalla tactica")));

	FWLArmy ArmyAfter;
	TestTrue(TEXT("Atacante sobrevive al auto-resolve"), Military->GetArmy(AttackerArmyId, ArmyAfter));
	TestEqual(TEXT("Atacante ocupa la provincia defendida"), ArmyAfter.ProvinceId, FString(TEXT("CO-CES")));
	TestFalse(TEXT("Defensor aniquilado y retirado de campania"), Military->GetArmy(DefenderArmyId, ArmyAfter));
	TestEqual(TEXT("Control provincial pasa al atacante"), Tick->GetProvinceControllerIso(TEXT("CO-CES")), FString(TEXT("VE")));

	GameInstance->Shutdown();
	return true;
}

// F7 asalto a provincia — CONTRATO: ninguna ciudad se toma gratis. Sin guerra el asalto se
// rechaza; con guerra y sin ejercito defensor la poblacion levanta MILICIA que defiende su
// ciudad; un ejercito blindado la aplasta (pagando algo) y OCUPA la provincia.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLTacticalProvinceAssaultTest,
	"WorldLeader.Battle.TacticalProvinceAssault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLTacticalProvinceAssaultTest::RunTest(const FString& Parameters)
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
	const FString AttackerArmyId = Military->CreateArmy(TEXT("VE"), TEXT("VE-ZU"), TEXT("mbt"), 4, TEXT("Miranda"));
	TestFalse(TEXT("Ejercito blindado creado"), AttackerArmyId.IsEmpty());
	TestTrue(TEXT("Cruza la frontera a la provincia enemiga"),
		Military->MoveArmy(AttackerArmyId, TEXT("CO-CES"), Message));

	// Sin guerra declarada el asalto se RECHAZA.
	FString Reason;
	TestFalse(TEXT("Sin guerra no hay asalto"), Military->CanAssaultProvince(AttackerArmyId, Reason));

	TestTrue(TEXT("VE declara la guerra a CO"), Politics->DeclareWar(TEXT("VE"), TEXT("CO"), Message));
	TestTrue(TEXT("Con guerra el asalto procede"), Military->CanAssaultProvince(AttackerArmyId, Reason));

	FWLTacticalBattleState Battle;
	TestTrue(TEXT("Asalto a la provincia iniciado"),
		Military->StartProvinceAssault(AttackerArmyId, Battle, Message));

	int32 MilitiaElements = 0;
	for (const FWLTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.UnitId == TEXT("militia"))
		{
			MilitiaElements += Unit.ElementCount;
		}
	}
	TestTrue(TEXT("La ciudad levanta milicia (nada se toma gratis)"), MilitiaElements >= 8);

	// IA en ambos bandos y resolver: los blindados toman la ciudad pagando poco.
	Tactical->SetTacticalAIControl(Battle.BattleId, TEXT("VE"), true, Message);
	Tactical->SetTacticalAIControl(Battle.BattleId, TEXT("CO"), true, Message);
	TArray<FString> Events;
	for (int32 Step = 0; Step < 400 && Battle.bActive; ++Step)
	{
		Tactical->AdvanceTacticalBattle(Battle.BattleId, 1.0, Battle, Events);
	}
	TestEqual(TEXT("El blindado somete a la milicia"),
		static_cast<int32>(Battle.Result), static_cast<int32>(EWLTacticalBattleResult::AttackerVictory));

	TestTrue(TEXT("Aplicar el resultado del asalto"),
		Military->ApplyTacticalBattleResult(Battle.BattleId, Message));
	TestEqual(TEXT("La provincia cambia de manos"),
		Tick->GetProvinceControllerIso(TEXT("CO-CES")), FString(TEXT("VE")));

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
