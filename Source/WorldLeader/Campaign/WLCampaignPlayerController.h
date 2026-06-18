// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WLCampaignPlayerController.generated.h"

class UWLStrategicTickSubsystem;

/**
 * PlayerController de campania. Input del jugador (regla del roadmap).
 * Por ahora, atajos de teclado para la vertical slice:
 *   M -> avanzar un mes (tick estrategico)
 *   P -> imprimir el estado al log
 */
UCLASS()
class WORLDLEADER_API AWLCampaignPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWLCampaignPlayerController();

protected:
	virtual void SetupInputComponent() override;

private:
	void OnAdvanceMonth();
	void OnPrintState();

	UWLStrategicTickSubsystem* GetTick() const;
};
