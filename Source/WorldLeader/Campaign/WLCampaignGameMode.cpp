// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignGameMode.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "UI/WLCampaignHUD.h"
#include "GameFramework/DefaultPawn.h"

AWLCampaignGameMode::AWLCampaignGameMode()
{
	PlayerControllerClass = AWLCampaignPlayerController::StaticClass();
	HUDClass = AWLCampaignHUD::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
}
