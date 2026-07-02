// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/WLGameTypes.h"
#include "WLMilitarySubsystem.generated.h"

class UWLDataRegistry;

/**
 * Gestiona los ejercitos en el mapa estrategico (Phase 2): creacion,
 * movimiento por adyacencia y auto-resolucion de batallas con bajas y
 * ocupacion. El estado vive en runtime; el balance no se hardcodea
 * (stats de unidades vienen del JSON via WLDataRegistry).
 */
UCLASS()
class WORLDLEADER_API UWLMilitarySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Crea un ejercito con Count unidades de UnitId. Devuelve su Id (vacio si falla). */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	FString CreateArmy(const FString& OwnerIso, const FString& ProvinceId, const FString& UnitId, int32 Count, const FString& General);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	TArray<FWLArmy> GetArmies() const { return Armies; }

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	bool GetArmy(const FString& ArmyId, FWLArmy& OutArmy) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	bool SetArmyGeneral(const FString& ArmyId, const FString& GeneralName, FString& OutMessage);

	/** Mueve un ejercito a una provincia adyacente. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	bool MoveArmy(const FString& ArmyId, const FString& ToProvinceId, FString& OutMessage);

	// --- Enlace mapa <-> backend (los tokens 3D son la autoridad de posicion) ---

	/** Id del ejercito real producido por una base de reclutamiento. Vacio si aun no existe. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	FString FindArmyIdByBase(const FString& BaseId) const;

	/**
	 * Crea (o re-sincroniza la composicion de) el ejercito REAL de una base de reclutamiento a partir de su
	 * guarnicion visual. Los tipos del catalogo de reclutamiento se mapean a unidades de Units.json.
	 * Al crear, asigna general automaticamente (F1.4). Devuelve el Id del ejercito.
	 */
	FString SyncArmyFromGarrison(
		const FString& BaseId,
		const FString& OwnerIso,
		const FString& ProvinceId,
		const TArray<TPair<FString, int32>>& GarrisonUnits);

	/** Sincroniza la provincia del ejercito con el mapa SIN chequear adyacencia (posicion visual manda). */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	bool SetArmyProvince(const FString& ArmyId, const FString& ProvinceId, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	int32 GetArmyAttack(const FString& ArmyId) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	int32 GetArmyDefense(const FString& ArmyId) const;

	/** Auto-resuelve una batalla atacante vs defensor; aplica bajas y ocupacion. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	EWLBattleResult AutoResolveBattle(const FString& AttackerId, const FString& DefenderId, FString& OutReport);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	void ResetMilitaryState();

	void WriteSaveSnapshot(TArray<FWLArmy>& OutArmies, int32& OutNextArmyNumber) const;
	bool RestoreSaveSnapshot(const TArray<FWLArmy>& SavedArmies, int32 SavedNextArmyNumber, FString& OutMessage);

private:
	UPROPERTY()
	TArray<FWLArmy> Armies;

	int32 NextArmyNumber = 1;

	UWLDataRegistry* GetRegistry() const;
	class UWLStrategicTickSubsystem* GetStrategicTick() const;
	FWLArmy* FindArmy(const FString& ArmyId);
	const FWLArmy* FindArmy(const FString& ArmyId) const;
	int32 SumUnitStat(const FWLArmy& Army, bool bAttack) const;
	void ApplyCasualties(FWLArmy& Army, float LossFraction) const;
};
