// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/WLGameTypes.h"
#include "Core/WLTacticalBattleTypes.h"
#include "WLMilitarySubsystem.generated.h"

class UWLDataRegistry;

/**
 * Preview de combate: mismos poderes que AutoResolveBattle pero SIN aplicar bajas.
 * La UI lo lee para mostrar el desglose antes de confirmar, asi las reglas de
 * combate (terreno, edificios, skill del general) no se duplican en el widget.
 */
USTRUCT(BlueprintType)
struct FWLBattlePreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") bool bValid = false;
	/** Si bValid es false, por que no se puede atacar (sin guerra, sin adyacencia...). */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString Reason;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString AttackerArmyId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString DefenderArmyId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString DefenderIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") int32 AttackerUnits = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") int32 DefenderUnits = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") int32 AttackerBaseAttack = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") int32 DefenderBaseDefense = 0;
	/** Poder efectivo tras todos los modificadores (lo que resuelve la batalla). */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") int32 AttackerPower = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") int32 DefenderPower = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString AttackerGeneral;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString DefenderGeneral;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") double AttackerSkillMultiplier = 1.0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") double DefenderSkillMultiplier = 1.0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString TerrainLabel;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") double TerrainMultiplier = 1.0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") double DefenderBuildingMultiplier = 1.0;
	/** Lectura rapida de la ventaja: "Favorable" / "Parejo" / "Desfavorable". */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Battle") FString OddsLabel;
};

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

	/** Desglose de poder previo al combate (sin aplicar nada). Para el flujo de combate de la UI. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	bool PreviewBattle(const FString& AttackerId, const FString& DefenderId, FWLBattlePreview& OutPreview) const;

	/** Ids de ejercitos enemigos que este ejercito puede atacar ahora (misma/adyacente provincia + en guerra). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Military")
	TArray<FString> GetAttackableTargetIds(const FString& AttackerId) const;

	/**
	 * Resuelve una batalla por la via TACTICA: crea la batalla, la juega con IA en ambos bandos hasta
	 * el final determinista y aplica el resultado a campania. Distinto de AutoResolveBattle (calculo
	 * directo de poderes). La vista tactica 3D interactiva es una fase aparte del roadmap.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	EWLBattleResult ResolveTacticalBattleToEnd(const FString& AttackerId, const FString& DefenderId, FString& OutReport);

	/** Inicia una batalla tactica oficial desde ejercitos de campania validados. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	bool StartTacticalBattle(const FString& AttackerId, const FString& DefenderId, FWLTacticalBattleState& OutBattle, FString& OutMessage);

	/** Aplica el resultado tactico cerrado al estado militar de campania: bajas, ocupacion y renombre. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	bool ApplyTacticalBattleResult(const FString& BattleId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Military")
	bool ReorganizeArmy(const FString& ArmyId, int32 MaxUnits, FString& OutMessage);

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
	bool ValidateBattlePair(const FWLArmy& Attacker, const FWLArmy& Defender, FString& OutMessage) const;
	/** Fuente unica del calculo de poderes de combate (terreno + edificios + skill del general). Llena OutPreview. */
	void ComputeBattlePower(const FWLArmy& Attacker, const FWLArmy& Defender, FWLBattlePreview& OutPreview) const;
	void ReconcileArmyFromTacticalBattle(FWLArmy& Army, const FWLTacticalBattleState& Battle, int32& OutDestroyedLosses, int32& OutRoutedUnits) const;
	bool FindRetreatProvinceForArmy(const FWLArmy& Army, const FString& FromProvinceId, FString& OutRetreatProvinceId) const;
	void AwardBattleRenown(const FWLArmy& Attacker, const FWLArmy& Defender, bool bAttackerWins);
};
