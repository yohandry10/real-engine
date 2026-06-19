// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLFrontendMenuWidget.h"

#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Frontend/WLFrontendPlayerController.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"

namespace
{
	const FLinearColor WLInk(0.006f, 0.012f, 0.018f, 1.0f);
	const FLinearColor WLDeepSea(0.012f, 0.04f, 0.055f, 0.96f);
	const FLinearColor WLPanel(0.025f, 0.044f, 0.055f, 0.88f);
	const FLinearColor WLPanelHard(0.018f, 0.029f, 0.038f, 0.94f);
	const FLinearColor WLLine(0.24f, 0.56f, 0.62f, 0.24f);
	const FLinearColor WLGhostLine(0.32f, 0.78f, 0.86f, 0.12f);
	const FLinearColor WLGold(0.93f, 0.76f, 0.34f, 1.0f);
	const FLinearColor WLGoldDim(0.64f, 0.50f, 0.22f, 1.0f);
	const FLinearColor WLText(0.91f, 0.94f, 0.92f, 1.0f);
	const FLinearColor WLMute(0.52f, 0.65f, 0.66f, 1.0f);
	const FLinearColor WLError(0.92f, 0.32f, 0.26f, 1.0f);
	const FLinearColor WLGood(0.42f, 0.88f, 0.62f, 1.0f);

	FSlateFontInfo Font(float Size, bool bBold = false)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::RoundToInt(Size));
		Info.LetterSpacing = 0;
		return Info;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, float Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetFont(Font(Size, bBold));
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	void AddVBoxChild(UVerticalBox* Box, UWidget* Child, float TopPadding = 0.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
		}
	}

	void AddCanvasChild(UCanvasPanel* Root, UWidget* Child, const FAnchors& Anchors, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment)
	{
		if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Child))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(Color);
		Panel->SetPadding(Padding);
		return Panel;
	}

	USizeBox* MakeFixedHeight(UWidgetTree* Tree, UWidget* Child, float Height, float MinWidth = 0.f)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(Height);
		if (MinWidth > 0.f)
		{
			Box->SetMinDesiredWidth(MinWidth);
		}
		Box->SetContent(Child);
		return Box;
	}

}

TSharedRef<SWidget> UWLFrontendMenuWidget::RebuildWidget()
{
	bIsRebuildingWidget = true;
	BuildHomeScreen();
	bIsRebuildingWidget = false;
	return Super::RebuildWidget();
}

void UWLFrontendMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UWLFrontendMenuWidget::NativeDestruct()
{
	ClearTransientBindings();
	Super::NativeDestruct();
}

FReply UWLFrontendMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && CurrentScreen != EWLFrontendScreen::Home)
	{
		BuildHomeScreen();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UWidget* UWLFrontendMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (CurrentScreen == EWLFrontendScreen::Home)
	{
		return ContinueButton && ContinueButton->GetIsEnabled() ? ContinueButton : NewCampaignButton;
	}

	if (CurrentScreen == EWLFrontendScreen::NewCampaign)
	{
		return NationCombo ? Cast<UWidget>(NationCombo) : StartCampaignButton;
	}

	if (CurrentScreen == EWLFrontendScreen::LoadCampaign)
	{
		return ContinueButton && ContinueButton->GetIsEnabled() ? ContinueButton : BackButton;
	}

	if (CurrentScreen == EWLFrontendScreen::Settings)
	{
		return BackButton;
	}

	return Super::NativeGetDesiredFocusTarget();
}

void UWLFrontendMenuWidget::BuildShell()
{
	ClearTransientBindings();
	NationIsoByOption.Reset();

	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FrontendRoot"));
		WidgetTree->RootWidget = RootCanvas;
	}
	else
	{
		RootCanvas->ClearChildren();
	}

	BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HeroBackground"));
	if (!BackgroundTexture)
	{
		BackgroundTexture = LoadTextureFromProjectPng(TEXT("Content/Art/MenuBackgrounds/T_WL_Menu_BG_Hero_4K.png"));
	}
	if (BackgroundTexture)
	{
		BackgroundImage->SetBrushFromTexture(BackgroundTexture, false);
	}
	else
	{
		FSlateBrush FallbackBrush;
		FallbackBrush.TintColor = FSlateColor(WLDeepSea);
		BackgroundImage->SetBrush(FallbackBrush);
	}
	if (UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(BackgroundImage))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));
		BackgroundSlot->SetAlignment(FVector2D::ZeroVector);
		BackgroundSlot->SetZOrder(0);
	}

	AddTopChrome();
	AddLeftNavigation();
}

UTexture2D* UWLFrontendMenuWidget::LoadTextureFromProjectPng(const FString& RelativeProjectPath) const
{
	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeProjectPath);

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *AbsolutePath))
	{
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		return nullptr;
	}

	TArray64<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(
		ImageWrapper->GetWidth(),
		ImageWrapper->GetHeight(),
		PF_B8G8R8A8,
		FName(TEXT("T_WL_Menu_BG_Hero_Runtime")),
		RawData);
	if (Texture)
	{
		Texture->SRGB = true;
		Texture->NeverStream = true;
		Texture->UpdateResource();
	}
	return Texture;
}

void UWLFrontendMenuWidget::AddTopChrome()
{
	UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopChrome"));

	UTextBlock* Left = MakeText(WidgetTree, TEXT("WORLD LEADER / GLOBAL DIPLOMATIC MAP"), 15.f, WLMute, true);
	if (UHorizontalBoxSlot* HorizontalSlot = TopBar->AddChildToHorizontalBox(Left))
	{
		HorizontalSlot->SetPadding(FMargin(0.f));
		HorizontalSlot->SetHorizontalAlignment(HAlign_Left);
		HorizontalSlot->SetVerticalAlignment(VAlign_Center);
	}

	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	if (UHorizontalBoxSlot* HorizontalSlot = TopBar->AddChildToHorizontalBox(Spacer))
	{
		HorizontalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTextBlock* Right = MakeText(WidgetTree, TEXT("PRE-ALPHA 0.0.1  |  UE 5.8"), 14.f, FLinearColor(0.78f, 0.76f, 0.64f, 0.9f), true);
	if (UHorizontalBoxSlot* HorizontalSlot = TopBar->AddChildToHorizontalBox(Right))
	{
		HorizontalSlot->SetHorizontalAlignment(HAlign_Right);
		HorizontalSlot->SetVerticalAlignment(VAlign_Center);
	}

	AddCanvasChild(RootCanvas, TopBar, FAnchors(0.f, 0.f, 1.f, 0.f), FVector2D(28.f, 18.f), FVector2D(-56.f, 32.f), FVector2D(0.f, 0.f));
}

void UWLFrontendMenuWidget::AddLeftNavigation()
{
	UBorder* Panel = MakePanel(WidgetTree, WLPanelHard, FMargin(22.f, 24.f, 22.f, 24.f));
	LeftNavigationBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftNavigation"));
	Panel->SetContent(LeftNavigationBox);

	AddText(LeftNavigationBox, TEXT("COMANDO"), 15.f, WLGold, 0.f);
	AddText(LeftNavigationBox, TEXT("Menu principal"), 26.f, WLText, 6.f);
	AddVBoxChild(LeftNavigationBox, MakeDivider(), 16.f);

	const bool bHasSave = false;
	ContinueButton = AddMenuButton(LeftNavigationBox, TEXT("CONTINUAR CAMPANIA"), bHasSave);
	ContinueButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleContinueClicked);
	NewCampaignButton = AddMenuButton(LeftNavigationBox, TEXT("NUEVA CAMPANIA"));
	NewCampaignButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleNewCampaignClicked);
	LoadButton = AddMenuButton(LeftNavigationBox, TEXT("CARGAR"));
	LoadButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleLoadClicked);
	MultiplayerButton = AddMenuButton(LeftNavigationBox, TEXT("MULTIJUGADOR"), false);
	SettingsButton = AddMenuButton(LeftNavigationBox, TEXT("OPCIONES"));
	SettingsButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleSettingsClicked);
	ModsButton = AddMenuButton(LeftNavigationBox, TEXT("MODS"), false);
	QuitButton = AddMenuButton(LeftNavigationBox, TEXT("SALIR"));
	QuitButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleQuitClicked);

	AddCanvasChild(RootCanvas, Panel, FAnchors(0.f, 0.5f, 0.f, 0.5f), FVector2D(42.f, 0.f), FVector2D(330.f, 548.f), FVector2D(0.f, 0.5f));
}

void UWLFrontendMenuWidget::AddBrandBlock()
{
	UBorder* BrandPanel = MakePanel(WidgetTree, FLinearColor(0.01f, 0.018f, 0.021f, 0.38f), FMargin(24.f, 20.f));
	UVerticalBox* BrandBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BrandBlock"));
	BrandPanel->SetContent(BrandBox);

	UTextBlock* Eyebrow = AddText(BrandBox, TEXT("MODERN GRAND STRATEGY"), 14.f, WLGold, 0.f);
	Eyebrow->SetJustification(ETextJustify::Center);

	UTextBlock* Title = AddText(BrandBox, TEXT("WORLD LEADER"), 54.f, WLText, 4.f);
	Title->SetJustification(ETextJustify::Center);

	UTextBlock* Subtitle = AddText(BrandBox, TEXT("Controla economia, alianzas, crisis y poder militar en un mapa diplomatico global."), 18.f, FLinearColor(0.75f, 0.84f, 0.83f, 1.f), 8.f);
	Subtitle->SetJustification(ETextJustify::Center);

	AddCanvasChild(RootCanvas, BrandPanel, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(80.f, -28.f), FVector2D(520.f, 210.f), FVector2D(0.5f, 0.5f));
}

void UWLFrontendMenuWidget::AddBriefingPanel()
{
	UBorder* Briefing = MakePanel(WidgetTree, WLPanel, FMargin(20.f, 18.f));
	UVerticalBox* BriefBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BriefingPanel"));
	Briefing->SetContent(BriefBox);

	AddText(BriefBox, TEXT("SITUATION BRIEF"), 14.f, WLGold, 0.f);
	AddText(BriefBox, TEXT("Orden mundial fragmentado"), 22.f, WLText, 6.f);
	AddVBoxChild(BriefBox, MakeDivider(), 12.f);
	AddText(BriefBox, TEXT("ENERGIA: rutas comerciales bajo presion."), 15.f, FLinearColor(0.84f, 0.88f, 0.82f, 1.f), 10.f);
	AddText(BriefBox, TEXT("DIPLOMACIA: bloques regionales buscan influencia."), 15.f, FLinearColor(0.84f, 0.88f, 0.82f, 1.f), 8.f);
	AddText(BriefBox, TEXT("SEGURIDAD: zonas de crisis activas en multiples teatros."), 15.f, FLinearColor(0.84f, 0.88f, 0.82f, 1.f), 8.f);
	AddText(BriefBox, TEXT("ECONOMIA: tesoro, industria y estabilidad definen el ritmo."), 15.f, FLinearColor(0.84f, 0.88f, 0.82f, 1.f), 8.f);

	AddCanvasChild(RootCanvas, Briefing, FAnchors(1.f, 0.f, 1.f, 0.f), FVector2D(-44.f, 86.f), FVector2D(340.f, 300.f), FVector2D(1.f, 0.f));
}

void UWLFrontendMenuWidget::AddBottomAction(const FString& Label, UButton* Button)
{
	UBorder* ActionPanel = MakePanel(WidgetTree, FLinearColor(0.035f, 0.035f, 0.03f, 0.82f), FMargin(16.f, 12.f));
	ActionPanel->SetContent(Button);

	UTextBlock* LabelText = MakeText(WidgetTree, Label, 18.f, WLText, true);
	LabelText->SetJustification(ETextJustify::Center);
	Button->SetContent(MakeFixedHeight(WidgetTree, LabelText, 42.f, 330.f));

	AddCanvasChild(RootCanvas, ActionPanel, FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.f, -34.f), FVector2D(392.f, 76.f), FVector2D(0.5f, 1.f));
}

void UWLFrontendMenuWidget::BuildHomeScreen()
{
	CurrentScreen = EWLFrontendScreen::Home;
	BuildShell();
	AddBrandBlock();
	AddBriefingPanel();

	AWLFrontendPlayerController* PC = GetOwningPlayer<AWLFrontendPlayerController>();
	const bool bHasSave = PC && PC->HasLocalCampaignSave();
	if (ContinueButton)
	{
		ContinueButton->SetIsEnabled(bHasSave);
	}

	UButton* PrimaryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PrimaryAction"));
	PrimaryButton->SetBackgroundColor(WLGoldDim);
	if (bHasSave)
	{
		PrimaryButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleContinueClicked);
		AddBottomAction(TEXT("CONTINUAR CAMPANIA"), PrimaryButton);
	}
	else
	{
		PrimaryButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleNewCampaignClicked);
		AddBottomAction(TEXT("NUEVA CAMPANIA"), PrimaryButton);
	}

	StatusText = MakeText(WidgetTree, bHasSave ? TEXT("Save local detectado.") : TEXT("Listo para iniciar una nueva campania."), 15.f, WLMute);
	StatusText->SetJustification(ETextJustify::Center);
	AddCanvasChild(RootCanvas, StatusText, FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.f, -116.f), FVector2D(440.f, 28.f), FVector2D(0.5f, 1.f));
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

	NationSummaryText = MakeText(WidgetTree, TEXT(""), 16.f, WLText);
	AddVBoxChild(ContentBox, NationSummaryText, 18.f);

	if (Nations.Num() > 0)
	{
		const FString FirstOption = FString::Printf(TEXT("%s (%s)"), *Nations[0].Name, *Nations[0].Iso);
		NationCombo->SetSelectedOption(FirstOption);
	}

	StatusText = AddText(ContentBox, TEXT(""), 15.f, WLMute, 18.f);

	StartCampaignButton = AddMenuButton(ContentBox, TEXT("COMENZAR CAMPANIA"), Nations.Num() > 0);
	StartCampaignButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleStartCampaignClicked);
	BackButton = AddMenuButton(ContentBox, TEXT("VOLVER"));
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

	ContinueButton = AddMenuButton(ContentBox, TEXT("CARGAR Y CONTINUAR"), bHasSave);
	ContinueButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleContinueClicked);
	BackButton = AddMenuButton(ContentBox, TEXT("VOLVER"));
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

	BackButton = AddMenuButton(ContentBox, TEXT("VOLVER"));
	BackButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleBackClicked);
	AddCanvasChild(RootCanvas, Panel, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(150.f, 0.f), FVector2D(620.f, 420.f), FVector2D(0.5f, 0.5f));
	ApplyDefaultFocus();
}

UButton* UWLFrontendMenuWidget::AddMenuButton(UVerticalBox* Box, const FString& Label, bool bEnabled)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetClickMethod(EButtonClickMethod::MouseDown);
	Button->SetBackgroundColor(bEnabled ? FLinearColor(0.12f, 0.15f, 0.15f, 0.92f) : FLinearColor(0.08f, 0.08f, 0.08f, 0.75f));
	Button->SetIsEnabled(bEnabled);

	UTextBlock* LabelText = MakeText(WidgetTree, Label, 16.f, bEnabled ? WLText : FLinearColor(0.38f, 0.42f, 0.42f, 1.f), true);
	LabelText->SetJustification(ETextJustify::Center);
	Button->SetContent(MakeFixedHeight(WidgetTree, LabelText, 42.f, 250.f));
	AddVBoxChild(Box, Button, 10.f);
	return Button;
}

UTextBlock* UWLFrontendMenuWidget::AddText(UVerticalBox* Box, const FString& Text, float Size, const FLinearColor& Color, float TopPadding)
{
	UTextBlock* TextBlock = MakeText(WidgetTree, Text, Size, Color, Size >= 22.f);
	AddVBoxChild(Box, TextBlock, TopPadding);
	return TextBlock;
}

UWidget* UWLFrontendMenuWidget::MakeDivider()
{
	USizeBox* DividerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DividerBox->SetHeightOverride(2.f);
	UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Divider->SetBrushColor(FLinearColor(0.86f, 0.68f, 0.30f, 0.55f));
	DividerBox->SetContent(Divider);
	return DividerBox;
}

void UWLFrontendMenuWidget::SetStatusMessage(const FString& Message, bool bSuccess)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(bSuccess ? WLGood : WLError);
	}
}

void UWLFrontendMenuWidget::ApplyDefaultFocus()
{
	if (bIsRebuildingWidget)
	{
		return;
	}

	if (UWidget* FocusTarget = NativeGetDesiredFocusTarget())
	{
		FocusTarget->SetKeyboardFocus();
	}
}

void UWLFrontendMenuWidget::ClearTransientBindings()
{
	if (ContinueButton)
	{
		ContinueButton->OnClicked.RemoveAll(this);
	}
	if (NewCampaignButton)
	{
		NewCampaignButton->OnClicked.RemoveAll(this);
	}
	if (LoadButton)
	{
		LoadButton->OnClicked.RemoveAll(this);
	}
	if (MultiplayerButton)
	{
		MultiplayerButton->OnClicked.RemoveAll(this);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.RemoveAll(this);
	}
	if (ModsButton)
	{
		ModsButton->OnClicked.RemoveAll(this);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.RemoveAll(this);
	}
	if (BackButton)
	{
		BackButton->OnClicked.RemoveAll(this);
	}
	if (StartCampaignButton)
	{
		StartCampaignButton->OnClicked.RemoveAll(this);
	}
	if (NationCombo)
	{
		NationCombo->OnSelectionChanged.RemoveAll(this);
	}

	ContinueButton = nullptr;
	NewCampaignButton = nullptr;
	LoadButton = nullptr;
	MultiplayerButton = nullptr;
	SettingsButton = nullptr;
	ModsButton = nullptr;
	QuitButton = nullptr;
	BackButton = nullptr;
	StartCampaignButton = nullptr;
	NationCombo = nullptr;
	NationSummaryText = nullptr;
	StatusText = nullptr;
}

UWLDataRegistry* UWLFrontendMenuWidget::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FString UWLFrontendMenuWidget::GetSelectedNationIso() const
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

void UWLFrontendMenuWidget::HandleContinueClicked()
{
	FString Message;
	AWLFrontendPlayerController* PC = GetOwningPlayer<AWLFrontendPlayerController>();
	if (!PC || !PC->ContinueCampaign(Message))
	{
		SetStatusMessage(Message.IsEmpty() ? TEXT("No se pudo cargar la campania.") : Message, false);
	}
}

void UWLFrontendMenuWidget::HandleNewCampaignClicked()
{
	BuildNewCampaignScreen();
}

void UWLFrontendMenuWidget::HandleLoadClicked()
{
	BuildLoadCampaignScreen();
}

void UWLFrontendMenuWidget::HandleSettingsClicked()
{
	BuildSettingsScreen();
}

void UWLFrontendMenuWidget::HandleQuitClicked()
{
	if (AWLFrontendPlayerController* PC = GetOwningPlayer<AWLFrontendPlayerController>())
	{
		PC->QuitGame();
	}
}

void UWLFrontendMenuWidget::HandleBackClicked()
{
	BuildHomeScreen();
}

void UWLFrontendMenuWidget::HandleStartCampaignClicked()
{
	const FString NationIso = GetSelectedNationIso();
	if (NationIso.IsEmpty())
	{
		SetStatusMessage(TEXT("Selecciona una nacion disponible."), false);
		return;
	}

	FString Message;
	AWLFrontendPlayerController* PC = GetOwningPlayer<AWLFrontendPlayerController>();
	if (!PC || !PC->StartNewCampaign(NationIso, Message))
	{
		SetStatusMessage(Message.IsEmpty() ? TEXT("No se pudo iniciar la campania.") : Message, false);
	}
}

void UWLFrontendMenuWidget::HandleNationSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!NationSummaryText)
	{
		return;
	}

	const UWLDataRegistry* Registry = GetRegistry();
	FWLNationData Nation;
	if (!Registry || !Registry->GetNation(GetSelectedNationIso(), Nation))
	{
		NationSummaryText->SetText(FText::FromString(TEXT("No hay naciones cargadas en el registro.")));
		NationSummaryText->SetColorAndOpacity(WLError);
		return;
	}

	const TArray<FWLProvinceData> Provinces = Registry->GetProvincesByNation(Nation.Iso);
	FWLProvinceData Capital;
	FString CapitalName = Nation.CapitalProvinceId;
	if (Registry->GetProvince(Nation.CapitalProvinceId, Capital))
	{
		CapitalName = Capital.Name;
	}

	const FString Summary = FString::Printf(
		TEXT("%s (%s)\nGobierno: %s\nCapital: %s\nProvincias iniciales: %d\nTesoro inicial: %lld"),
		*Nation.Name,
		*Nation.Iso,
		*Nation.GovernmentType,
		*CapitalName,
		Provinces.Num(),
		Nation.StartingTreasury);
	NationSummaryText->SetText(FText::FromString(Summary));
	NationSummaryText->SetColorAndOpacity(WLText);
}
