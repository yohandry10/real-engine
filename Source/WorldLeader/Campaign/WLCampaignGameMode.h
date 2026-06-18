// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WLCampaignGameMode.generated.h"

/**
 * GameMode de la campania estrategica. Reglas del lado servidor/juego
 * (regla del roadmap: GameMode para reglas). Registra el PlayerController y
 * el HUD de campania para que al pulsar Play se vea el estado en pantalla.
 */
UCLASS()
class WORLDLEADER_API AWLCampaignGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWLCampaignGameMode();
};
