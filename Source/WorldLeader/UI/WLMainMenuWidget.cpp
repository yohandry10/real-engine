// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLMainMenuWidget.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"

namespace
{
	const FLinearColor WLMenuBackground(0.015f, 0.018f, 0.015f, 0.98f);
	const FLinearColor WLMenuPanel(0.035f, 0.045f, 0.04f, 0.96f);
	const FLinearColor WLTextPrimary(0.92f, 0.94f, 0.9f, 1.0f);
	const FLinearColor WLTextMuted(0.58f, 0.66f, 0.62f, 1.0f);
	const FLinearColor WLTextAccent(1.0f, 0.86f, 0.25f, 1.0f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, float Size, const FLinearColor& Color)
	{
		UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FMath::RoundToInt(Size);
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	void AddBoxChild(UVerticalBox* Box, UWidget* Child, float TopPadding = 0.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
		}
	}
}

void UWLMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	BuildShell();
	BuildMainMenu();
}

void UWLMainMenuWidget::NativeDestruct()
{
	if (NewCampaignButton)
	{
		NewCampaignButton->OnClicked.RemoveAll(this);
	}
	if (StartCampaignButton)
	{
		StartCampaignButton->OnClicked.RemoveAll(this);
	}
	if (ContinueCampaignButton)
	{
		ContinueCampaignButton->OnClicked.RemoveAll(this);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveAll(this);
	}
	if (NationCombo)
	{
		NationCombo->OnSelectionChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UWLMainMenuWidget::BuildShell()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainMenuRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBackground"));
	Background->SetBrushColor(WLMenuBackground);
	if (UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuPanel"));
	Panel->SetBrushColor(WLMenuPanel);
	Panel->SetPadding(FMargin(34.f, 30.f, 34.f, 30.f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.f, 0.5f));
		PanelSlot->SetPosition(FVector2D(82.f, 0.f));
		PanelSlot->SetSize(FVector2D(620.f, 560.f));
	}

	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuContent"));
	Panel->SetContent(ContentBox);
}

void UWLMainMenuWidget::RebuildContent()
{
	if (ContentBox)
	{
		ContentBox->ClearChildren();
	}
	NationCombo = nullptr;
	NationSummaryText = nullptr;
	StartCampaignButton = nullptr;
	ContinueCampaignButton = nullptr;
	NewCampaignButton = nullptr;
	BackButton = nullptr;
	NationIsoByOption.Reset();
}

void UWLMainMenuWidget::BuildMainMenu()
{
	RebuildContent();
	AddTitle(TEXT("WORLD LEADER"), TEXT("Campana estrategica moderna"));

	AddBoxChild(ContentBox, MakeText(WidgetTree, TEXT("Phase 1"), 18.f, WLTextAccent), 40.f);
	AddBoxChild(ContentBox, MakeText(WidgetTree, TEXT("Mapa real, economia inicial, seleccion de nacion y guardado local."), 19.f, WLTextMuted), 8.f);

	const UWLCampaignGameInstance* CampaignGI = Cast<UWLCampaignGameInstance>(GetGameInstance());
	if (CampaignGI && CampaignGI->HasLocalCampaignSave())
	{
		ContinueCampaignButton = AddMenuButton(TEXT("Cargar campania"));
		ContinueCampaignButton->OnClicked.AddDynamic(this, &UWLMainMenuWidget::HandleContinueCampaignClicked);
		AddBoxChild(ContentBox, ContinueCampaignButton, 42.f);
	}

	NewCampaignButton = AddMenuButton(TEXT("Nueva campania"));
	NewCampaignButton->OnClicked.AddDynamic(this, &UWLMainMenuWidget::HandleNewCampaignClicked);
	AddBoxChild(ContentBox, NewCampaignButton, ContinueCampaignButton ? 12.f : 42.f);
}

void UWLMainMenuWidget::BuildNationSelection()
{
	RebuildContent();
	AddTitle(TEXT("NUEVA CAMPANIA"), TEXT("Seleccion de nacion"));

	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	TArray<FWLNationData> Nations = Registry ? Registry->GetAllNations() : TArray<FWLNationData>();
	Nations.Sort([](const FWLNationData& A, const FWLNationData& B)
	{
		return A.Name < B.Name;
	});

	NationCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("NationCombo"));
	for (const FWLNationData& Nation : Nations)
	{
		const FString Option = FString::Printf(TEXT("%s (%s)"), *Nation.Name, *Nation.Iso);
		NationCombo->AddOption(Option);
		NationIsoByOption.Add(Option, Nation.Iso);
	}
	NationCombo->OnSelectionChanged.AddDynamic(this, &UWLMainMenuWidget::HandleNationSelectionChanged);
	AddBoxChild(ContentBox, NationCombo, 34.f);

	NationSummaryText = MakeText(WidgetTree, TEXT(""), 18.f, WLTextPrimary);
	AddBoxChild(ContentBox, NationSummaryText, 24.f);

	if (Nations.Num() > 0)
	{
		const FString FirstOption = FString::Printf(TEXT("%s (%s)"), *Nations[0].Name, *Nations[0].Iso);
		NationCombo->SetSelectedOption(FirstOption);
	}

	StartCampaignButton = AddMenuButton(TEXT("Comenzar campania"));
	StartCampaignButton->SetIsEnabled(Nations.Num() > 0 && Tick != nullptr);
	StartCampaignButton->OnClicked.AddDynamic(this, &UWLMainMenuWidget::HandleStartCampaignClicked);
	AddBoxChild(ContentBox, StartCampaignButton, 36.f);

	BackButton = AddMenuButton(TEXT("Volver"));
	BackButton->OnClicked.AddDynamic(this, &UWLMainMenuWidget::HandleBackClicked);
	AddBoxChild(ContentBox, BackButton, 12.f);

	UpdateNationSummary();
}

void UWLMainMenuWidget::AddTitle(const FString& Title, const FString& Subtitle)
{
	AddBoxChild(ContentBox, MakeText(WidgetTree, Title, 44.f, WLTextPrimary));
	AddBoxChild(ContentBox, MakeText(WidgetTree, Subtitle, 21.f, WLTextMuted), 8.f);
}

UButton* UWLMainMenuWidget::AddMenuButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetClickMethod(EButtonClickMethod::MouseDown);

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetHeightOverride(52.f);
	SizeBox->SetMinDesiredWidth(420.f);

	UTextBlock* LabelText = MakeText(WidgetTree, Label, 20.f, WLTextPrimary);
	LabelText->SetJustification(ETextJustify::Center);
	SizeBox->SetContent(LabelText);
	Button->SetContent(SizeBox);
	return Button;
}

void UWLMainMenuWidget::HandleNewCampaignClicked()
{
	BuildNationSelection();
}

void UWLMainMenuWidget::HandleContinueCampaignClicked()
{
	if (AWLCampaignPlayerController* CampaignPC = GetOwningPlayer<AWLCampaignPlayerController>())
	{
		CampaignPC->LoadCampaignFromMenu();
	}
}

void UWLMainMenuWidget::HandleStartCampaignClicked()
{
	const FString NationIso = GetSelectedNationIso();
	if (NationIso.IsEmpty())
	{
		return;
	}

	if (AWLCampaignPlayerController* CampaignPC = GetOwningPlayer<AWLCampaignPlayerController>())
	{
		CampaignPC->StartCampaignFromMenu(NationIso);
	}
}

void UWLMainMenuWidget::HandleBackClicked()
{
	BuildMainMenu();
}

void UWLMainMenuWidget::HandleNationSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateNationSummary();
}

void UWLMainMenuWidget::UpdateNationSummary()
{
	if (!NationSummaryText)
	{
		return;
	}

	const FString NationIso = GetSelectedNationIso();
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();

	FWLNationData Nation;
	if (!Registry || !Registry->GetNation(NationIso, Nation))
	{
		NationSummaryText->SetText(FText::FromString(TEXT("No hay naciones jugables cargadas.")));
		return;
	}

	const TArray<FWLProvinceData> Provinces = Registry->GetProvincesByNation(Nation.Iso);
	FString CapitalName = Nation.CapitalProvinceId;
	FWLProvinceData Capital;
	if (Registry->GetProvince(Nation.CapitalProvinceId, Capital))
	{
		CapitalName = Capital.Name;
	}

	const int64 Treasury = Tick ? Tick->GetTreasury(Nation.Iso) : Nation.StartingTreasury;
	const int64 Balance = Tick ? Tick->GetMonthlyBalance(Nation.Iso) : 0;
	const FString Summary = FString::Printf(
		TEXT("%s\nGobierno: %s\nCapital: %s\nProvincias iniciales: %d\nTesoro inicial: %lld\nBalance mensual: %+lld"),
		*Nation.Name,
		*Nation.GovernmentType,
		*CapitalName,
		Provinces.Num(),
		Treasury,
		Balance);
	NationSummaryText->SetText(FText::FromString(Summary));
}

FString UWLMainMenuWidget::GetSelectedNationIso() const
{
	if (!NationCombo)
	{
		return FString();
	}

	const FString Option = NationCombo->GetSelectedOption();
	if (const FString* Iso = NationIsoByOption.Find(Option))
	{
		return *Iso;
	}
	return FString();
}

UWLDataRegistry* UWLMainMenuWidget::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLStrategicTickSubsystem* UWLMainMenuWidget::GetTick() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}
