// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Components/ComboBoxString.h"
#include "WLFrontendMenuWidget.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;
class UWLDataRegistry;

UENUM()
enum class EWLFrontendScreen : uint8
{
	Home,
	NewCampaign,
	LoadCampaign,
	Settings
};

/**
 * Premium game frontend for World Leader.
 * Visual direction: global diplomatic map, crisis-room signals, restrained
 * command UI. Built in C++ so the early project stays asset-light and
 * versionable while the layout remains ready for later CommonUI layering.
 */
UCLASS()
class WORLDLEADER_API UWLFrontendMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	void BuildHomeScreen();
	void BuildNewCampaignScreen();
	void BuildLoadCampaignScreen();
	void BuildSettingsScreen();
	void BuildShell();
	void AddBackgroundPolish();
	void AddTopChrome();
	void AddLeftNavigation();
	void AddBrandBlock();
	void AddBriefingPanel();
	void AddBottomStatusBar(bool bHasSave);
	UTexture2D* LoadTextureFromProjectPng(const FString& RelativeProjectPath) const;
	void SetStatusMessage(const FString& Message, bool bSuccess);
	void ClearTransientBindings();

	UButton* AddMenuButton(UVerticalBox* Box, const FString& Label, const FString& Icon, bool bEnabled = true, bool bPrimary = false);
	UTextBlock* AddText(UVerticalBox* Box, const FString& Text, float Size, const FLinearColor& Color, float TopPadding = 0.f);
	UWidget* MakeDivider();
	UWLDataRegistry* GetRegistry() const;
	FString GetSelectedNationIso() const;
	void ApplyDefaultFocus();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleNewCampaignClicked();

	UFUNCTION()
	void HandleLoadClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleStartCampaignClicked();

	UFUNCTION()
	void HandleNationSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UPROPERTY()
	UCanvasPanel* RootCanvas = nullptr;

	UPROPERTY()
	UImage* BackgroundImage = nullptr;

	UPROPERTY()
	UTexture2D* BackgroundTexture = nullptr;

	UPROPERTY()
	UVerticalBox* LeftNavigationBox = nullptr;

	UPROPERTY()
	UVerticalBox* ContentBox = nullptr;

	UPROPERTY()
	UTextBlock* StatusText = nullptr;

	UPROPERTY()
	UTextBlock* NationSummaryText = nullptr;

	UPROPERTY()
	UComboBoxString* NationCombo = nullptr;

	UPROPERTY()
	UButton* ContinueButton = nullptr;

	UPROPERTY()
	UButton* NewCampaignButton = nullptr;

	UPROPERTY()
	UButton* LoadButton = nullptr;

	UPROPERTY()
	UButton* MultiplayerButton = nullptr;

	UPROPERTY()
	UButton* SettingsButton = nullptr;

	UPROPERTY()
	UButton* ModsButton = nullptr;

	UPROPERTY()
	UButton* QuitButton = nullptr;

	UPROPERTY()
	UButton* BackButton = nullptr;

	UPROPERTY()
	UButton* StartCampaignButton = nullptr;

	EWLFrontendScreen CurrentScreen = EWLFrontendScreen::Home;
	TMap<FString, FString> NationIsoByOption;
	bool bIsRebuildingWidget = false;
};
