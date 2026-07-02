// Copyright World Leader project. See ROADMAP.md.

#include "Military/WLMilitaryLibrary.h"

float UWLMilitaryLibrary::TerrainDefenseMultiplier(EWLTerrainType Terrain)
{
	switch (Terrain)
	{
	case EWLTerrainType::Mountain: return 1.5f;
	case EWLTerrainType::Urban:    return 1.4f;
	case EWLTerrainType::Jungle:   return 1.3f;
	case EWLTerrainType::Arctic:   return 1.2f;
	case EWLTerrainType::Coastal:  return 1.1f;
	default:                       return 1.0f;
	}
}

EWLBattleResult UWLMilitaryLibrary::ResolveBattle(int32 AttackPower, int32 DefensePower)
{
	if (DefensePower <= 0) return EWLBattleResult::AttackerDecisiveVictory;
	if (AttackPower <= 0)  return EWLBattleResult::DefenderDecisiveVictory;

	const float Ratio = static_cast<float>(AttackPower) / static_cast<float>(DefensePower);
	if (Ratio >= 2.0f) return EWLBattleResult::AttackerDecisiveVictory;
	if (Ratio >= 1.2f) return EWLBattleResult::AttackerVictory;
	if (Ratio >= 0.8f) return EWLBattleResult::Stalemate;
	if (Ratio >= 0.5f) return EWLBattleResult::DefenderVictory;
	return EWLBattleResult::DefenderDecisiveVictory;
}

float UWLMilitaryLibrary::GeneralSkillCombatMultiplier(int32 Skill, const FWLBalanceRules& Rules)
{
	const FWLBalanceRules Balance = Rules.Sanitized();
	const double SkillFactor = static_cast<double>(FMath::Clamp(Skill, 0, 100) - 50) / 50.0;
	return static_cast<float>(1.0 + SkillFactor * Balance.GeneralSkillCombatEffectAtMax);
}

FString UWLMilitaryLibrary::BattleResultToString(EWLBattleResult Result)
{
	switch (Result)
	{
	case EWLBattleResult::Invalid:                 return TEXT("Resultado invalido");
	case EWLBattleResult::AttackerDecisiveVictory: return TEXT("Victoria decisiva del atacante");
	case EWLBattleResult::AttackerVictory:         return TEXT("Victoria del atacante");
	case EWLBattleResult::Stalemate:               return TEXT("Empate tactico");
	case EWLBattleResult::DefenderVictory:         return TEXT("Victoria del defensor");
	case EWLBattleResult::DefenderDecisiveVictory: return TEXT("Victoria decisiva del defensor");
	default:                                       return TEXT("Desconocido");
	}
}
