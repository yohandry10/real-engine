// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Balance/WLBalanceTypes.h"
#include "Core/WLGameTypes.h"
#include "Core/WLTacticalBattleTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WLTacticalBattleSubsystem.generated.h"

class UWLDataRegistry;

/**
 * Backend B2: simulacion tactica determinista para la vertical slice.
 *
 * No renderiza y no usa input directo. Campaign/3D/UI deben iniciar batallas,
 * emitir ordenes y leer FWLTacticalBattleState.
 */
UCLASS()
class WORLDLEADER_API UWLTacticalBattleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	void ResetTacticalBattles();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool StartTacticalBattleFromArmies(
		const FWLArmy& Attacker,
		const FWLArmy& Defender,
		const FString& ProvinceId,
		FWLTacticalBattleState& OutBattle,
		FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Battle")
	bool GetTacticalBattleState(const FString& BattleId, FWLTacticalBattleState& OutBattle) const;

	/** Snapshot ordenado para auditoria; no expone el mapa mutable interno. */
	TArray<FWLTacticalBattleState> GetTacticalBattleStates() const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool IssueMoveOrder(const FString& BattleId, const FString& TacticalUnitId, FVector2D Target, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool IssueAttackOrder(const FString& BattleId, const FString& TacticalUnitId, const FString& TargetUnitId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool SetTacticalAIControl(const FString& BattleId, const FString& OwnerIso, bool bEnabled, FString& OutMessage);

	/** F2: anade un parche circular de terreno (urbano/bosque) al campo de batalla. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool AddTacticalTerrainPatch(const FString& BattleId, EWLTacticalTerrain Terrain, FVector2D Position, double Radius, FString& OutMessage);

	/** Terreno en un punto del campo: primer parche que lo contiene; abierto si ninguno. */
	static EWLTacticalTerrain TerrainAtPosition(const FWLTacticalBattleState& Battle, const FVector2D& Position);

	/** Alcance maximo de fuego DIRECTO contra un objetivo en ese terreno (la cobertura obliga a acercarse). */
	static double GetCoverEngageRange(EWLTacticalTerrain TerrainAtTarget);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool AdvanceTacticalBattle(const FString& BattleId, double DeltaSeconds, FWLTacticalBattleState& OutBattle, TArray<FString>& OutEvents);

private:
	UPROPERTY()
	TMap<FString, FWLTacticalBattleState> Battles;

	int32 NextBattleNumber = 1;

	UWLDataRegistry* GetRegistry() const;
	FWLBalanceRules GetBalanceRules() const;
	FWLTacticalBattleState* FindBattle(const FString& BattleId);
	const FWLTacticalBattleState* FindBattle(const FString& BattleId) const;
	FWLTacticalUnitState* FindUnit(FWLTacticalBattleState& Battle, const FString& TacticalUnitId);
	const FWLTacticalUnitState* FindUnit(const FWLTacticalBattleState& Battle, const FString& TacticalUnitId) const;
	void AddArmyUnits(FWLTacticalBattleState& Battle, const FWLArmy& Army, const FVector2D& Origin, double DirectionSign);
	bool IsValidAttackTarget(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit, const FString& TargetUnitId, int32 RoutMoraleThreshold) const;
	const FWLTacticalUnitState* FindNearestEffectiveEnemy(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit, int32 RoutMoraleThreshold) const;
	/** F5: mejor objetivo para la IA segun la MATRIZ (con terreno) ponderada por distancia; null si no puede danar nada. */
	const FWLTacticalUnitState* FindBestAITarget(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit, int32 RoutMoraleThreshold) const;
	const FWLTacticalObjectiveState* FindBestObjectiveForUnit(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit) const;
	void IssueTacticalAIOrders(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents);
	void AdvanceUnitOrders(FWLTacticalBattleState& Battle, double DeltaSeconds, TArray<FString>& OutEvents);
	/** F3: resuelve impactos de salvas indirectas en vuelo (area sobre la posicion fijada al disparar). */
	void AdvanceShells(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents);
	/** F3: paraguas antiaereo — el SAM dispara SOLO a aviacion enemiga en alcance, sin orden. */
	void AdvanceAutoAirDefense(FWLTacticalBattleState& Battle, double DeltaSeconds, TArray<FString>& OutEvents);
	void AdvanceObjectives(FWLTacticalBattleState& Battle, double DeltaSeconds, const TArray<double>& HealthBeforeTick, TArray<FString>& OutEvents);
	void UpdateBattleResult(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents);
};
