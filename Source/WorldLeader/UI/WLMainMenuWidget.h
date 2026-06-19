// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "WLMainMenuWidget.generated.h"

class UButton;
class UComboBoxString;
class UTextBlock;
class UVerticalBox;
class UWLDataRegistry;
class UWLStrategicTickSubsystem;

/**
 * Menu principal UMG construido en C++ para la vertical slice Phase 1.
 * Evita assets binarios mientras deja un flujo formal:
 * Main Menu -> Nueva campania -> elegir nacion -> campania.
 */
UCLASS()
class WORLDLEADER_API UWLMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY() UVerticalBox* ContentBox = nullptr;
	UPROPERTY() UComboBoxString* NationCombo = nullptr;
	UPROPERTY() UTextBlock* NationSummaryText = nullptr;
	UPROPERTY() UButton* StartCampaignButton = nullptr;
	UPROPERTY() UButton* ContinueCampaignButton = nullptr;
	UPROPERTY() UButton* NewCampaignButton = nullptr;
	UPROPERTY() UButton* BackButton = nullptr;

	TMap<FString, FString> NationIsoByOption;

	UFUNCTION()
	void HandleNewCampaignClicked();

	UFUNCTION()
	void HandleContinueCampaignClicked();

	UFUNCTION()
	void HandleStartCampaignClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleNationSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void BuildShell();
	void BuildMainMenu();
	void BuildNationSelection();
	void RebuildContent();
	void AddTitle(const FString& Title, const FString& Subtitle);
	UButton* AddMenuButton(const FString& Label);
	void UpdateNationSummary();
	FString GetSelectedNationIso() const;

	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;
};
