// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLFrontendMenuWidget.h"

#include "UI/WLFrontendMenuWidgetPrivate.h"
#include "Campaign/WLDataRegistry.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Frontend/WLFrontendPlayerController.h"

using namespace WLFrontendMenu;

void UWLFrontendMenuWidget::BuildHomeScreen()
{
	CurrentScreen = EWLFrontendScreen::Home;
	AWLFrontendPlayerController* PC = GetOwningPlayer<AWLFrontendPlayerController>();
	const bool bHasSave = PC && PC->HasLocalCampaignSave();
	BuildShell();
	AddBrandBlock();
	AddBriefingPanel();
	AddBottomStatusBar(bHasSave);
	ApplyDefaultFocus();
}

void UWLFrontendMenuWidget::BuildNewCampaignScreen()
{
	CurrentScreen = EWLFrontendScreen::NewCampaign;
	BuildShell();

	UBorder* Panel = MakePanel(WidgetTree, WLPanel, FMargin(26.f, 24.f));
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NewCampaignContent"));
	Panel->SetContent(ContentBox);

	AddText(ContentBox, TEXT("NUEVA CAMPANIA"), 15.f, WLGold, 0.f);
	AddText(ContentBox, TEXT("Escenario global 2024"), 34.f, WLText, 6.f);
	AddText(ContentBox, TEXT("Selecciona una nacion disponible. El frontend no prioriza ningun pais: la lista sale del registro de datos."), 17.f, WLMute, 10.f);
	AddVBoxChild(ContentBox, MakeDivider(), 18.f);

	NationCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("NationCombo"));
	TArray<FWLNationData> Nations;
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		Nations = Registry->GetAllNations();
	}
	Nations.Sort([](const FWLNationData& A, const FWLNationData& B)
	{
		return A.Name < B.Name;
	});

	for (const FWLNationData& Nation : Nations)
	{
		const FString Option = FString::Printf(TEXT("%s (%s)"), *Nation.Name, *Nation.Iso);
		NationCombo->AddOption(Option);
		NationIsoByOption.Add(Option, Nation.Iso);
	}
	NationCombo->OnSelectionChanged.AddDynamic(this, &UWLFrontendMenuWidget::HandleNationSelectionChanged);
	AddVBoxChild(ContentBox, MakeFixedHeight(WidgetTree, NationCombo, 40.f, 360.f), 24.f);

	NationSummaryText = MakeFrontendText(WidgetTree, TEXT(""), 16.f, WLText);
	AddVBoxChild(ContentBox, NationSummaryText, 18.f);

	if (Nations.Num() > 0)
	{
		const FString FirstOption = FString::Printf(TEXT("%s (%s)"), *Nations[0].Name, *Nations[0].Iso);
		NationCombo->SetSelectedOption(FirstOption);
	}

	StatusText = AddText(ContentBox, TEXT(""), 15.f, WLMute, 18.f);

	StartCampaignButton = AddMenuButton(ContentBox, TEXT("COMENZAR CAMPANIA"), TEXT(">"), Nations.Num() > 0, true);
	StartCampaignButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleStartCampaignClicked);
	BackButton = AddMenuButton(ContentBox, TEXT("VOLVER"), TEXT("<"));
	BackButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleBackClicked);

	AddCanvasChild(RootCanvas, Panel, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(150.f, 0.f), FVector2D(620.f, 560.f), FVector2D(0.5f, 0.5f));
	HandleNationSelectionChanged(NationCombo ? NationCombo->GetSelectedOption() : FString(), ESelectInfo::Direct);
	ApplyDefaultFocus();
}

void UWLFrontendMenuWidget::BuildLoadCampaignScreen()
{
	CurrentScreen = EWLFrontendScreen::LoadCampaign;
	BuildShell();

	UBorder* Panel = MakePanel(WidgetTree, WLPanel, FMargin(26.f, 24.f));
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoadCampaignContent"));
	Panel->SetContent(ContentBox);

	AddText(ContentBox, TEXT("CARGAR PARTIDA"), 15.f, WLGold, 0.f);
	AddText(ContentBox, TEXT("Save local"), 34.f, WLText, 6.f);
	AddText(ContentBox, TEXT("Phase 1 usa un slot local unico. La siguiente iteracion debe convertir esto en lista de saves con metadata."), 17.f, WLMute, 10.f);
	AddVBoxChild(ContentBox, MakeDivider(), 18.f);

	AWLFrontendPlayerController* PC = GetOwningPlayer<AWLFrontendPlayerController>();
	const bool bHasSave = PC && PC->HasLocalCampaignSave();
	AddText(ContentBox, bHasSave ? TEXT("Save encontrado: WorldLeader_LocalCampaign") : TEXT("No hay save local disponible."), 18.f, bHasSave ? WLGood : WLError, 16.f);

	ContinueButton = AddMenuButton(ContentBox, TEXT("CARGAR Y CONTINUAR"), TEXT(">"), bHasSave, bHasSave);
	ContinueButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleContinueClicked);
	BackButton = AddMenuButton(ContentBox, TEXT("VOLVER"), TEXT("<"));
	BackButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleBackClicked);

	StatusText = AddText(ContentBox, TEXT(""), 15.f, WLMute, 18.f);
	AddCanvasChild(RootCanvas, Panel, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(150.f, 0.f), FVector2D(620.f, 460.f), FVector2D(0.5f, 0.5f));
	ApplyDefaultFocus();
}

void UWLFrontendMenuWidget::BuildSettingsScreen()
{
	CurrentScreen = EWLFrontendScreen::Settings;
	BuildShell();

	UBorder* Panel = MakePanel(WidgetTree, WLPanel, FMargin(26.f, 24.f));
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsContent"));
	Panel->SetContent(ContentBox);

	AddText(ContentBox, TEXT("OPCIONES"), 15.f, WLGold, 0.f);
	AddText(ContentBox, TEXT("Ajustes del juego"), 34.f, WLText, 6.f);
	AddText(ContentBox, TEXT("Pendiente para el siguiente slice: video, audio, idioma, escala de UI, teclas y accesibilidad."), 17.f, WLMute, 10.f);
	AddVBoxChild(ContentBox, MakeDivider(), 18.f);

	BackButton = AddMenuButton(ContentBox, TEXT("VOLVER"), TEXT("<"));
	BackButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleBackClicked);
	AddCanvasChild(RootCanvas, Panel, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(150.f, 0.f), FVector2D(620.f, 420.f), FVector2D(0.5f, 0.5f));
	ApplyDefaultFocus();
}
