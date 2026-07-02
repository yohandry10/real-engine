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

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool IssueMoveOrder(const FString& BattleId, const FString& TacticalUnitId, FVector2D Target, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool IssueAttackOrder(const FString& BattleId, const FString& TacticalUnitId, const FString& TargetUnitId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Battle")
	bool SetTacticalAIControl(const FString& BattleId, const FString& OwnerIso, bool bEnabled, FString& OutMessage);

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
	const FWLTacticalObjectiveState* FindBestObjectiveForUnit(const FWLTacticalBattleState& Battle, const FWLTacticalUnitState& Unit) const;
	void IssueTacticalAIOrders(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents);
	void AdvanceUnitOrders(FWLTacticalBattleState& Battle, double DeltaSeconds, TArray<FString>& OutEvents);
	void AdvanceObjectives(FWLTacticalBattleState& Battle, double DeltaSeconds, TArray<FString>& OutEvents);
	void UpdateBattleResult(FWLTacticalBattleState& Battle, TArray<FString>& OutEvents);
};
