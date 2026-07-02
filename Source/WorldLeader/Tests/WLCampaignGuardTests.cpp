// Copyright World Leader project. See ROADMAP.md.
//
// Tests de guardas de runtime: IDs canonicos, restricciones de construccion
// y validaciones militares basicas sobre subsystems reales.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Engine/GameInstance.h"
#include "Military/WLMilitarySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLConstructionCanonicalIdsTest,
	"WorldLeader.Construction.CanonicalBuildingIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLConstructionCanonicalIdsTest::RunTest(const FString& Parameters)
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
	TestTrue(TEXT("Construir con IDs no canonicos"),
		Tick->BuildBuilding(TEXT("ve-zu"), TEXT("Oil_Well"), Message));
	TestFalse(TEXT("Bloquear duplicado con ID canonico"),
		Tick->BuildBuilding(TEXT("VE-ZU"), TEXT("oil_well"), Message));

	const TArray<FString> Built = Tick->GetProvinceBuildings(TEXT("ve-zu"));
	TestEqual(TEXT("Un solo edificio canonico"), Built.Num(), 1);
	if (Built.Num() == 1)
	{
		TestEqual(TEXT("ID canonico guardado"), Built[0], FString(TEXT("oil_well")));
	}

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLGoodsCatalogTest,
	"WorldLeader.Data.GoodsCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLGoodsCatalogTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLDataRegistry* Registry = GameInstance->GetSubsystem<UWLDataRegistry>();
	TestNotNull(TEXT("Data registry"), Registry);
	if (!Registry)
	{
		GameInstance->Shutdown();
		return false;
	}

	TestTrue(TEXT("Catalogo de bienes cargado"), Registry->GetGoodCount() >= 10);

	FWLGoodData Oil;
	TestTrue(TEXT("Petroleo existe"), Registry->GetGood(TEXT("oil"), Oil));
	TestEqual(TEXT("Petroleo es crudo"), Oil.Category, EWLGoodCategory::Raw);
	TestTrue(TEXT("Petroleo sin insumos"), Oil.Inputs.IsEmpty());

	FWLGoodData Steel;
	TestTrue(TEXT("Acero existe"), Registry->GetGood(TEXT("STEEL"), Steel));   // id no canonico
	TestEqual(TEXT("Acero es manufacturado"), Steel.Category, EWLGoodCategory::Manufactured);
	TestTrue(TEXT("Acero usa minerales"), Steel.Inputs.Contains(TEXT("minerals")));
	TestTrue(TEXT("Acero usa carbon"), Steel.Inputs.Contains(TEXT("coal")));

	for (const FWLGoodData& Good : Registry->GetAllGoods())
	{
		TestTrue(FString::Printf(TEXT("Bien valido: %s"), *Good.Id), Good.IsValid() && Good.BasePrice > 0);
		for (const FString& InputId : Good.Inputs)
		{
			FWLGoodData Input;
			TestTrue(FString::Printf(TEXT("Insumo existe: %s -> %s"), *Good.Id, *InputId),
				Registry->GetGood(InputId, Input));
		}
	}

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLMilitarySubsystemGuardsTest,
	"WorldLeader.Military.SubsystemGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLMilitarySubsystemGuardsTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLMilitarySubsystem* Military = GameInstance->GetSubsystem<UWLMilitarySubsystem>();
	TestNotNull(TEXT("Military subsystem"), Military);
	if (!Military)
	{
		GameInstance->Shutdown();
		return false;
	}

	TestTrue(TEXT("Rechazar nacion inexistente"),
		Military->CreateArmy(TEXT("XX"), TEXT("VE-ZU"), TEXT("infantry"), 1, TEXT("")).IsEmpty());
	TestTrue(TEXT("Rechazar provincia no propia"),
		Military->CreateArmy(TEXT("VE"), TEXT("CO-DC"), TEXT("infantry"), 1, TEXT("")).IsEmpty());
	TestTrue(TEXT("Rechazar count no positivo"),
		Military->CreateArmy(TEXT("VE"), TEXT("VE-ZU"), TEXT("infantry"), 0, TEXT("")).IsEmpty());

	const FString ArmyVe = Military->CreateArmy(TEXT("ve"), TEXT("ve-zu"), TEXT("INFANTRY"), 3, TEXT(""));
	const FString ArmyCo = Military->CreateArmy(TEXT("CO"), TEXT("CO-DC"), TEXT("tank"), 1, TEXT(""));
	TestFalse(TEXT("Ejercito VE valido"), ArmyVe.IsEmpty());
	TestFalse(TEXT("Ejercito CO valido"), ArmyCo.IsEmpty());

	FString Message;
	TestFalse(TEXT("No mover a provincia inexistente"),
		Military->MoveArmy(ArmyVe, TEXT("VE-XX"), Message));
	TestEqual(TEXT("No combatir sin adyacencia"),
		static_cast<int32>(Military->AutoResolveBattle(ArmyVe, ArmyCo, Message)),
		static_cast<int32>(EWLBattleResult::Invalid));

	const FString ArmyCoGua = Military->CreateArmy(TEXT("CO"), TEXT("CO-GUA"), TEXT("infantry"), 1, TEXT(""));
	TestFalse(TEXT("Ejercito CO-GUA valido"), ArmyCoGua.IsEmpty());

	TestEqual(TEXT("Victoria decisiva adyacente"),
		static_cast<int32>(Military->AutoResolveBattle(ArmyVe, ArmyCoGua, Message)),
		static_cast<int32>(EWLBattleResult::AttackerDecisiveVictory));

	UWLStrategicTickSubsystem* Tick = GameInstance->GetSubsystem<UWLStrategicTickSubsystem>();
	TestNotNull(TEXT("Strategic tick subsystem"), Tick);
	if (Tick)
	{
		TestEqual(TEXT("Ocupacion cambia controlador"),
			Tick->GetProvinceControllerIso(TEXT("CO-GUA")), FString(TEXT("VE")));
	}

	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLMilitarySnapshotTest,
	"WorldLeader.Military.SnapshotRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLMilitarySnapshotTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	TestNotNull(TEXT("GameInstance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}
	GameInstance->Init();

	UWLMilitarySubsystem* Military = GameInstance->GetSubsystem<UWLMilitarySubsystem>();
	TestNotNull(TEXT("Military subsystem"), Military);
	if (!Military)
	{
		GameInstance->Shutdown();
		return false;
	}

	const FString ArmyId = Military->CreateArmy(TEXT("VE"), TEXT("VE-ZU"), TEXT("tank"), 2, TEXT("Miranda"));
	TestFalse(TEXT("Ejercito inicial valido"), ArmyId.IsEmpty());

	TArray<FWLArmy> Armies;
	int32 NextArmyNumber = 0;
	Military->WriteSaveSnapshot(Armies, NextArmyNumber);
	TestEqual(TEXT("Snapshot con un ejercito"), Armies.Num(), 1);
	TestTrue(TEXT("Siguiente numero avanza"), NextArmyNumber > 1);

	Military->ResetMilitaryState();
	TestEqual(TEXT("Reset limpia ejercitos"), Military->GetArmies().Num(), 0);

	FString Message;
	TestTrue(TEXT("Restaurar snapshot militar"),
		Military->RestoreSaveSnapshot(Armies, NextArmyNumber, Message));
	TestEqual(TEXT("Ejercito restaurado"), Military->GetArmies().Num(), 1);

	const FString SecondArmyId = Military->CreateArmy(TEXT("VE"), TEXT("VE-BO"), TEXT("infantry"), 1, TEXT(""));
	TestTrue(TEXT("Nuevo ID no colisiona"), SecondArmyId != ArmyId);

	GameInstance->Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
