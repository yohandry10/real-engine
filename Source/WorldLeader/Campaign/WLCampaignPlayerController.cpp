// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"

AWLCampaignPlayerController::AWLCampaignPlayerController()
{
	bShowMouseCursor = true;
}

void AWLCampaignPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AWLCampaignPlayerController::OnAdvanceMonth);
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AWLCampaignPlayerController::OnPrintState);
	}
}

UWLStrategicTickSubsystem* AWLCampaignPlayerController::GetTick() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

void AWLCampaignPlayerController::OnAdvanceMonth()
{
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->AdvanceMonth();
	}
}

void AWLCampaignPlayerController::OnPrintState()
{
	if (UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		GI->WLPrintState();
	}
}
