// Copyright World Leader project. See ROADMAP.md.

#include "Frontend/WLFrontendGameMode.h"

#include "Frontend/WLFrontendPlayerController.h"

AWLFrontendGameMode::AWLFrontendGameMode()
{
	PlayerControllerClass = AWLFrontendPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}
