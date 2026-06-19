// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Balance/WLBalanceDataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "WLBalanceSettings.generated.h"

/**
 * Settings de proyecto para seleccionar el DataAsset de balance activo.
 * Si no hay asset configurado, el juego usa defaults saneados equivalentes
 * a la vertical slice actual.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "World Leader Balance"))
class WORLDLEADER_API UWLBalanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Balance")
	TSoftObjectPtr<UWLBalanceDataAsset> BalanceDataAsset;

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif
};
