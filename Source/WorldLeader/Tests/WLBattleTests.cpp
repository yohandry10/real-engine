// Copyright World Leader project. See ROADMAP.md.
//
// Test funcional de la auto-resolucion de batallas (Phase 2).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/WLGameTypes.h"
#include "Military/WLMilitaryLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLBattleResolveTest,
	"WorldLeader.Military.ResolveBattle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLBattleResolveTest::RunTest(const FString& Parameters)
{
	auto Resolve = [](int32 A, int32 D)
	{
		return static_cast<int32>(UWLMilitaryLibrary::ResolveBattle(A, D));
	};

	TestEqual(TEXT("2.0x -> decisiva atacante"),
		Resolve(200, 100), static_cast<int32>(EWLBattleResult::AttackerDecisiveVictory));
	TestEqual(TEXT("1.3x -> victoria atacante"),
		Resolve(130, 100), static_cast<int32>(EWLBattleResult::AttackerVictory));
	TestEqual(TEXT("1.0x -> empate"),
		Resolve(100, 100), static_cast<int32>(EWLBattleResult::Stalemate));
	TestEqual(TEXT("0.6x -> victoria defensor"),
		Resolve(60, 100), static_cast<int32>(EWLBattleResult::DefenderVictory));
	TestEqual(TEXT("0.3x -> decisiva defensor"),
		Resolve(30, 100), static_cast<int32>(EWLBattleResult::DefenderDecisiveVictory));
	TestEqual(TEXT("Defensa 0 -> decisiva atacante"),
		Resolve(10, 0), static_cast<int32>(EWLBattleResult::AttackerDecisiveVictory));
	TestEqual(TEXT("Texto de resultado invalido"),
		UWLMilitaryLibrary::BattleResultToString(EWLBattleResult::Invalid), FString(TEXT("Resultado invalido")));

	const FWLBalanceRules Rules = FWLBalanceRules::Default();
	TestTrue(TEXT("Skill 100 da bonus de combate"),
		FMath::IsNearlyEqual(UWLMilitaryLibrary::GeneralSkillCombatMultiplier(100, Rules), 1.25f));
	TestTrue(TEXT("Skill 50 es neutro"),
		FMath::IsNearlyEqual(UWLMilitaryLibrary::GeneralSkillCombatMultiplier(50, Rules), 1.0f));
	TestTrue(TEXT("Skill 0 da penalizacion de combate"),
		FMath::IsNearlyEqual(UWLMilitaryLibrary::GeneralSkillCombatMultiplier(0, Rules), 0.75f));

	FWLBalanceRules CustomRules;
	CustomRules.GeneralSkillCombatEffectAtMax = 0.40;
	TestTrue(TEXT("El bonus de skill sale de balance"),
		FMath::IsNearlyEqual(UWLMilitaryLibrary::GeneralSkillCombatMultiplier(100, CustomRules), 1.40f));

	// Bonus de terreno: montaña favorece claramente al defensor.
	TestTrue(TEXT("Montaña x1.5 > llano x1.0"),
		UWLMilitaryLibrary::TerrainDefenseMultiplier(EWLTerrainType::Mountain) >
		UWLMilitaryLibrary::TerrainDefenseMultiplier(EWLTerrainType::Plain));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
