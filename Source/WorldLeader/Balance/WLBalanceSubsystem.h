// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Balance/WLBalanceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WLBalanceSubsystem.generated.h"

class UWLBalanceDataAsset;

/**
 * Fuente unica de reglas numericas de campania. Carga el DataAsset configurado
 * en Project Settings y expone siempre reglas saneadas; si el asset falta, el
 * juego conserva defaults deterministas para no bloquear prototipos/tests.
 */
UCLASS()
class WORLDLEADER_API UWLBalanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Balance")
	bool ReloadBalance();

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Balance")
	FWLBalanceRules GetRules() const { return ActiveRules; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Balance")
	int32 GetStartYear() const { return ActiveRules.StartYear; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Balance")
	int32 GetStartMonth() const { return ActiveRules.StartMonth; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Balance")
	int32 GetMonthsPerYear() const { return ActiveRules.MonthsPerYear; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Balance")
	bool IsUsingConfiguredAsset() const { return LoadedBalanceAsset != nullptr; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UWLBalanceDataAsset> LoadedBalanceAsset;

	FWLBalanceRules ActiveRules = FWLBalanceRules::Default();
};
