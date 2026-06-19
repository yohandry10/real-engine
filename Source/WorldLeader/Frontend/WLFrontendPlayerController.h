// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WLFrontendPlayerController.generated.h"

class UWLFrontendMenuWidget;

/**
 * Player controller for the title/frontend layer.
 */
UCLASS()
class WORLDLEADER_API AWLFrontendPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWLFrontendPlayerController();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Frontend")
	void ShowFrontendMenu();

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Frontend")
	bool HasLocalCampaignSave() const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Frontend")
	bool ContinueCampaign(FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Frontend")
	bool StartNewCampaign(const FString& NationIso, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Frontend")
	void QuitGame();

protected:
	virtual void BeginPlay() override;

private:
	void EnterFrontendInputMode();
	void TravelToCampaign();

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Frontend")
	TSubclassOf<UWLFrontendMenuWidget> FrontendMenuWidgetClass;

	UPROPERTY()
	UWLFrontendMenuWidget* FrontendMenuWidget = nullptr;
};
