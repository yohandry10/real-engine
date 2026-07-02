// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Balance/WLBalanceTypes.h"
#include "Core/WLGameTypes.h"
#include "WLMilitaryLibrary.generated.h"

/**
 * Reglas puras de combate por auto-resolucion (Phase 2). Deterministas y
 * testeables: sin estado ni aleatoriedad.
 */
UCLASS()
class WORLDLEADER_API UWLMilitaryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Multiplicador de defensa por terreno (montaña/urbano favorecen al defensor). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	static float TerrainDefenseMultiplier(EWLTerrainType Terrain);

	/** Resuelve una batalla segun la relacion poder de ataque / defensa. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	static EWLBattleResult ResolveBattle(int32 AttackPower, int32 DefensePower);

	/** Multiplicador de poder de combate derivado del skill del general y parametrizado por balance. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	static float GeneralSkillCombatMultiplier(int32 Skill, const FWLBalanceRules& Rules);

	/** Texto legible de un resultado de batalla. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	static FString BattleResultToString(EWLBattleResult Result);
};
