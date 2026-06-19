// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Balance/WLBalanceTypes.h"
#include "Engine/DataAsset.h"
#include "WLBalanceDataAsset.generated.h"

/**
 * DataAsset editable de balance. Crea una instancia en Content/Data/Balance
 * y asignala en Project Settings > World Leader Balance para tunear economia
 * y calendario sin recompilar C++.
 */
UCLASS(BlueprintType)
class WORLDLEADER_API UWLBalanceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldLeader|Balance")
	FWLBalanceRules Rules;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Balance")
	FWLBalanceRules GetSanitizedRules() const;

	static FWLBalanceRules GetDefaultRules();
};
