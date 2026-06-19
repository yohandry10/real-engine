// Copyright World Leader project. See ROADMAP.md.

#include "Frontend/WLFrontendPlayerController.h"

#include "Campaign/WLCampaignGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/WLFrontendMenuWidget.h"
#include "WorldLeader.h"

AWLFrontendPlayerController::AWLFrontendPlayerController()
{
	bShowMouseCursor = true;
}

void AWLFrontendPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ShowFrontendMenu();
}

void AWLFrontendPlayerController::ShowFrontendMenu()
{
	if (FrontendMenuWidget)
	{
		FrontendMenuWidget->RemoveFromParent();
		FrontendMenuWidget = nullptr;
	}

	TSubclassOf<UWLFrontendMenuWidget> WidgetClass = FrontendMenuWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UWLFrontendMenuWidget::StaticClass();
	}

	FrontendMenuWidget = CreateWidget<UWLFrontendMenuWidget>(this, WidgetClass);
	if (FrontendMenuWidget)
	{
		FrontendMenuWidget->AddToViewport(100);
		FrontendMenuWidget->ActivateWidget();
	}

	EnterFrontendInputMode();
}

bool AWLFrontendPlayerController::HasLocalCampaignSave() const
{
	const UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	return GI && GI->HasLocalCampaignSave();
}

bool AWLFrontendPlayerController::ContinueCampaign(FString& OutMessage)
{
	UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		OutMessage = TEXT("GameInstance de campania no disponible.");
		return false;
	}

	if (!GI->LoadLocalCampaign(OutMessage))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("Frontend ContinueCampaign: %s"), *OutMessage);
		return false;
	}

	TravelToCampaign();
	return true;
}

bool AWLFrontendPlayerController::StartNewCampaign(const FString& NationIso, FString& OutMessage)
{
	UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI)
	{
		OutMessage = TEXT("GameInstance de campania no disponible.");
		return false;
	}

	if (!GI->StartNewCampaign(NationIso))
	{
		OutMessage = FString::Printf(TEXT("No se pudo iniciar campania con %s."), *NationIso);
		return false;
	}

	OutMessage = FString::Printf(TEXT("Campania iniciada con %s."), *GI->GetSelectedNationIso());
	TravelToCampaign();
	return true;
}

void AWLFrontendPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AWLFrontendPlayerController::EnterFrontendInputMode()
{
	FInputModeUIOnly InputMode;
	if (FrontendMenuWidget)
	{
		InputMode.SetWidgetToFocus(FrontendMenuWidget->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AWLFrontendPlayerController::TravelToCampaign()
{
	if (FrontendMenuWidget)
	{
		FrontendMenuWidget->RemoveFromParent();
		FrontendMenuWidget = nullptr;
	}

	UGameplayStatics::OpenLevel(
		this,
		FName(TEXT("/Engine/Maps/Entry")),
		true,
		TEXT("game=/Script/WorldLeader.WLCampaignGameMode"));
}
