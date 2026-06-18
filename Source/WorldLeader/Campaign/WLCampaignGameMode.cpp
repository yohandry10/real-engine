// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "UI/WLCampaignHUD.h"
#include "Map/WLWorldMap.h"
#include "GameFramework/DefaultPawn.h"
#include "Engine/World.h"

AWLCampaignGameMode::AWLCampaignGameMode()
{
	PlayerControllerClass = AWLCampaignPlayerController::StaticClass();
	HUDClass = AWLCampaignHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}

void AWLCampaignGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->SpawnActor<AWLWorldMap>(AWLWorldMap::StaticClass());
	}
}
