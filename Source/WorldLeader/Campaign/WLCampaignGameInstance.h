// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WLCampaignGameInstance.generated.h"

class UWLDataRegistry;
class UWLStrategicTickSubsystem;

/**
 * GameInstance de la campania. Punto de entrada del estado global del cliente
 * (regla del roadmap: GameInstance para estado global). Expone comandos de
 * consola para probar la vertical slice antes de tener UI:
 *   WLAdvanceMonth  -> avanza un tick (un mes)
 *   WLPrintState    -> imprime fecha, provincias, naciones y tesoros
 */
UCLASS()
class WORLDLEADER_API UWLCampaignGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** Consola: avanza la campania un mes y registra el nuevo estado. */
	UFUNCTION(Exec)
	void WLAdvanceMonth();

	/** Consola: imprime el estado actual de la campania al log. */
	UFUNCTION(Exec)
	void WLPrintState();

private:
	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;
};
