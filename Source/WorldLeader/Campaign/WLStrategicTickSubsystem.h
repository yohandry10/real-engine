// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WLStrategicTickSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWLOnMonthAdvanced, int32, Year, int32, Month);

/**
 * Motor de campania estrategica. Mantiene la fecha de juego (1 tick = 1 mes,
 * ver roadmap) y aplica la economia mensual a cada nacion. Es deliberadamente
 * minimo para la vertical slice de Phase 0/1: solo tesoro nacional a partir
 * del balance de provincias propias.
 */
UCLASS()
class WORLDLEADER_API UWLStrategicTickSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Avanza un tick (un mes). Recalcula economia y emite OnMonthAdvanced. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	void AdvanceMonth();

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	int32 GetCurrentYear() const { return CurrentYear; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	int32 GetCurrentMonth() const { return CurrentMonth; }

	/** Tesoro actual de una nacion (creditos). 0 si no existe. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetTreasury(const FString& NationIso) const;

	/** Balance mensual neto de una nacion segun sus provincias actuales. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetMonthlyBalance(const FString& NationIso) const;

	UPROPERTY(BlueprintAssignable, Category = "WorldLeader|Campaign")
	FWLOnMonthAdvanced OnMonthAdvanced;

private:
	int32 CurrentYear = 0;
	int32 CurrentMonth = 0;

	/** Tesoro nacional en runtime (ISO -> creditos). */
	TMap<FString, int64> Treasuries;

	void InitTreasuriesFromData();
	void ApplyMonthlyEconomy();

	class UWLDataRegistry* GetDataRegistry() const;
};
