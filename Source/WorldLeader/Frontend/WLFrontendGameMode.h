// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WLFrontendGameMode.generated.h"

/**
 * Frontend boot mode. Owns the main menu flow and keeps campaign gameplay
 * classes isolated from title-screen concerns.
 */
UCLASS()
class WORLDLEADER_API AWLFrontendGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWLFrontendGameMode();
};
