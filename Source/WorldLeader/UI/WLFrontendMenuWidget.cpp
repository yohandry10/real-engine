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
	const FLinearColor WLPanelGlass(0.012f, 0.024f, 0.031f, 0.82f);
	const FLinearColor WLLine(0.24f, 0.56f, 0.62f, 0.24f);
	const FLinearColor WLGhostLine(0.32f, 0.78f, 0.86f, 0.12f);
	const FLinearColor WLGold(0.93f, 0.76f, 0.34f, 1.0f);
	const FLinearColor WLGoldDim(0.64f, 0.50f, 0.22f, 1.0f);
	const FLinearColor WLGoldDeep(0.34f, 0.25f, 0.09f, 0.96f);
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

	UTextBlock* MakeFrontendText(UWidgetTree* Tree, const FString& Text, float Size, const FLinearColor& Color, bool bBold = false)
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
			Slot->SetZOrder(10);
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

	FSlateBrush TintedBrush(const FSlateBrush& Source, const FLinearColor& Color)
	{
		FSlateBrush Brush = Source;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	FButtonStyle MakeButtonStyle(bool bPrimary, bool bEnabled)
	{
		FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button"));
		const FLinearColor Normal = bPrimary ? WLGoldDeep : FLinearColor(0.014f, 0.027f, 0.034f, 0.96f);
		const FLinearColor Hovered = bPrimary ? FLinearColor(0.62f, 0.46f, 0.16f, 1.0f) : FLinearColor(0.06f, 0.10f, 0.12f, 1.0f);
		const FLinearColor Pressed = bPrimary ? FLinearColor(0.82f, 0.62f, 0.22f, 1.0f) : FLinearColor(0.09f, 0.13f, 0.14f, 1.0f);
		const FLinearColor Disabled = FLinearColor(0.035f, 0.045f, 0.047f, 0.72f);
		Style.SetNormal(TintedBrush(Style.Normal, bEnabled ? Normal : Disabled));
		Style.SetHovered(TintedBrush(Style.Hovered, bEnabled ? Hovered : Disabled));
		Style.SetPressed(TintedBrush(Style.Pressed, bEnabled ? Pressed : Disabled));
		Style.SetDisabled(TintedBrush(Style.Disabled, Disabled));
		Style.SetNormalPadding(FMargin(1.f));
		Style.SetPressedPadding(FMargin(2.f, 3.f, 0.f, 0.f));
		return Style;
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

	AddBackgroundPolish();
	AddTopChrome();
	AddLeftNavigation();
}

void UWLFrontendMenuWidget::AddBackgroundPolish()
{
	UBorder* GlobalShade = MakePanel(WidgetTree, FLinearColor(0.0f, 0.006f, 0.01f, 0.24f), FMargin(0.f));
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(GlobalShade))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
		CanvasSlot->SetZOrder(1);
	}

	UBorder* LeftReadability = MakePanel(WidgetTree, FLinearColor(0.0f, 0.0f, 0.0f, 0.36f), FMargin(0.f));
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(LeftReadability))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		CanvasSlot->SetPosition(FVector2D(0.f, 0.f));
		CanvasSlot->SetSize(FVector2D(520.f, 0.f));
		CanvasSlot->SetZOrder(2);
	}

	UBorder* RightReadability = MakePanel(WidgetTree, FLinearColor(0.0f, 0.0f, 0.0f, 0.28f), FMargin(0.f));
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(RightReadability))
	{
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		CanvasSlot->SetPosition(FVector2D(0.f, 0.f));
		CanvasSlot->SetSize(FVector2D(520.f, 0.f));
		CanvasSlot->SetZOrder(2);
	}

	UBorder* BottomShade = MakePanel(WidgetTree, FLinearColor(0.0f, 0.0f, 0.0f, 0.54f), FMargin(0.f));
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(BottomShade))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f, -132.f, 0.f, 0.f));
		CanvasSlot->SetZOrder(2);
	}
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

	UTextBlock* Left = MakeFrontendText(WidgetTree, TEXT("WORLD LEADER / MAPA DIPLOMATICO GLOBAL"), 15.f, FLinearColor(0.72f, 0.83f, 0.86f, 0.9f), true);
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

	UTextBlock* Right = MakeFrontendText(WidgetTree, TEXT("ONLINE"), 14.f, FLinearColor(0.78f, 0.76f, 0.64f, 0.9f), true);
	Right->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* HorizontalSlot = TopBar->AddChildToHorizontalBox(Right))
	{
		HorizontalSlot->SetHorizontalAlignment(HAlign_Right);
		HorizontalSlot->SetVerticalAlignment(VAlign_Center);
	}

	AddCanvasChild(RootCanvas, TopBar, FAnchors(0.f, 0.f, 1.f, 0.f), FVector2D(28.f, 18.f), FVector2D(-56.f, 32.f), FVector2D(0.f, 0.f));
}

void UWLFrontendMenuWidget::AddLeftNavigation()
{
	const AWLFrontendPlayerController* PC = GetOwningPlayer<AWLFrontendPlayerController>();
	const bool bHasSave = PC && PC->HasLocalCampaignSave();
	const bool bHome = CurrentScreen == EWLFrontendScreen::Home;

	UBorder* PanelShadow = MakePanel(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.34f), FMargin(0.f));
	AddCanvasChild(RootCanvas, PanelShadow, FAnchors(0.f, 0.5f, 0.f, 0.5f), FVector2D(46.f, 8.f), FVector2D(398.f, 642.f), FVector2D(0.f, 0.5f));

	UBorder* Panel = MakePanel(WidgetTree, WLPanelGlass, FMargin(20.f, 22.f, 20.f, 22.f));
	LeftNavigationBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftNavigation"));
	Panel->SetContent(LeftNavigationBox);

	AddText(LeftNavigationBox, TEXT("COMANDO CENTRAL"), 13.f, WLGold, 0.f);
	AddText(LeftNavigationBox, TEXT("Menu principal"), 25.f, WLText, 5.f);
	AddText(LeftNavigationBox, bHasSave ? TEXT("Sesion de campania disponible") : TEXT("Nueva orden de mando disponible"), 13.f, WLMute, 4.f);
	AddVBoxChild(LeftNavigationBox, MakeDivider(), 12.f);

	ContinueButton = AddMenuButton(LeftNavigationBox, TEXT("CONTINUAR CAMPANIA"), TEXT("*"), bHasSave, bHome && bHasSave);
	ContinueButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleContinueClicked);
	NewCampaignButton = AddMenuButton(LeftNavigationBox, TEXT("NUEVA CAMPANIA"), TEXT("O"), true, (bHome && !bHasSave) || CurrentScreen == EWLFrontendScreen::NewCampaign);
	NewCampaignButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleNewCampaignClicked);
	LoadButton = AddMenuButton(LeftNavigationBox, TEXT("CARGAR"), TEXT("[]"), true, CurrentScreen == EWLFrontendScreen::LoadCampaign);
	LoadButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleLoadClicked);
	MultiplayerButton = AddMenuButton(LeftNavigationBox, TEXT("MULTIJUGADOR"), TEXT("OO"), false);
	SettingsButton = AddMenuButton(LeftNavigationBox, TEXT("OPCIONES"), TEXT("o"), true, CurrentScreen == EWLFrontendScreen::Settings);
	SettingsButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleSettingsClicked);
	ModsButton = AddMenuButton(LeftNavigationBox, TEXT("MODS"), TEXT("#"), false);
	QuitButton = AddMenuButton(LeftNavigationBox, TEXT("SALIR"), TEXT("|"));
	QuitButton->OnClicked.AddDynamic(this, &UWLFrontendMenuWidget::HandleQuitClicked);

	AddCanvasChild(RootCanvas, Panel, FAnchors(0.f, 0.5f, 0.f, 0.5f), FVector2D(38.f, 0.f), FVector2D(398.f, 642.f), FVector2D(0.f, 0.5f));
}

void UWLFrontendMenuWidget::AddBrandBlock()
{
	UBorder* BrandPanel = MakePanel(WidgetTree, FLinearColor(0.0f, 0.006f, 0.01f, 0.28f), FMargin(20.f, 14.f));
	UVerticalBox* BrandBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BrandBlock"));
	BrandPanel->SetContent(BrandBox);

	UTextBlock* Crest = AddText(BrandBox, TEXT("*  O  *"), 22.f, WLGold, 0.f);
	Crest->SetJustification(ETextJustify::Center);

	UTextBlock* Title = AddText(BrandBox, TEXT("WORLD LEADER"), 62.f, WLText, 0.f);
	Title->SetJustification(ETextJustify::Center);
	Title->SetAutoWrapText(false);

	UTextBlock* Eyebrow = AddText(BrandBox, TEXT("GLOBAL COMMAND STRATEGY"), 17.f, WLGold, 0.f);
	Eyebrow->SetJustification(ETextJustify::Center);
	Eyebrow->SetAutoWrapText(false);

	UTextBlock* Rule = AddText(BrandBox, TEXT("------------------------------"), 12.f, FLinearColor(0.83f, 0.63f, 0.22f, 0.82f), 0.f);
	Rule->SetJustification(ETextJustify::Center);

	UTextBlock* Subtitle = AddText(BrandBox, TEXT("Poder, diplomacia e inteligencia en un mapa global."), 15.f, FLinearColor(0.75f, 0.84f, 0.83f, 0.92f), 8.f);
	Subtitle->SetJustification(ETextJustify::Center);
	Subtitle->SetAutoWrapText(false);

	AddCanvasChild(RootCanvas, BrandPanel, FAnchors(0.36f, 0.f, 0.36f, 0.f), FVector2D(0.f, 86.f), FVector2D(860.f, 214.f), FVector2D(0.5f, 0.f));
}

void UWLFrontendMenuWidget::AddBriefingPanel()
{
	UBorder* Shadow = MakePanel(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.36f), FMargin(0.f));
	AddCanvasChild(RootCanvas, Shadow, FAnchors(1.f, 0.5f, 1.f, 0.5f), FVector2D(-50.f, 10.f), FVector2D(430.f, 690.f), FVector2D(1.f, 0.5f));

	UBorder* Briefing = MakePanel(WidgetTree, FLinearColor(0.012f, 0.025f, 0.032f, 0.89f), FMargin(22.f, 20.f));
	UVerticalBox* BriefBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BriefingPanel"));
	Briefing->SetContent(BriefBox);

	AddText(BriefBox, TEXT("BRIEFING GLOBAL"), 14.f, WLGold, 0.f);
	AddText(BriefBox, TEXT("Orden mundial fragmentado"), 24.f, WLText, 5.f);
	AddText(BriefBox, TEXT("Alianzas inestables, mercados sensibles y crisis regionales activas."), 14.f, WLMute, 8.f);
	AddVBoxChild(BriefBox, MakeDivider(), 14.f);

	AddText(BriefBox, TEXT("CRISIS ACTIVAS"), 15.f, WLText, 16.f);

	const auto AddBriefingItem = [this, BriefBox](const FString& Severity, const FString& Title, const FString& Body, const FLinearColor& SeverityColor)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* Mark = MakeFrontendText(WidgetTree, TEXT("!"), 20.f, SeverityColor, true);
		Mark->SetJustification(ETextJustify::Center);
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(MakeFixedHeight(WidgetTree, Mark, 44.f, 34.f)))
		{
			RowSlot->SetVerticalAlignment(VAlign_Top);
			RowSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}

		UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddText(Copy, Title, 14.f, WLText, 0.f);
		AddText(Copy, Body, 12.f, WLMute, 2.f);
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(Copy))
		{
			RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* SeverityText = MakeFrontendText(WidgetTree, Severity, 11.f, SeverityColor, true);
		SeverityText->SetJustification(ETextJustify::Right);
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(SeverityText))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Right);
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}

		AddVBoxChild(BriefBox, Row, 12.f);
	};

	AddBriefingItem(TEXT("ALTA"), TEXT("Mar Negro"), TEXT("Movimientos navales elevan la presion regional."), WLError);
	AddBriefingItem(TEXT("ALTA"), TEXT("Pacifico occidental"), TEXT("Disputa territorial con impacto comercial."), WLError);
	AddBriefingItem(TEXT("MEDIA"), TEXT("Sahel"), TEXT("Inestabilidad interna y riesgo humanitario."), WLGold);

	AddVBoxChild(BriefBox, MakeDivider(), 18.f);
	AddText(BriefBox, TEXT("INTELIGENCIA RECIENTE"), 15.f, WLText, 14.f);
	AddText(BriefBox, TEXT("02h  Operaciones ciberneticas detectadas en Europa."), 12.f, FLinearColor(0.77f, 0.84f, 0.84f, 0.92f), 10.f);
	AddText(BriefBox, TEXT("05h  Nuevo acuerdo comercial propuesto por la UE."), 12.f, FLinearColor(0.77f, 0.84f, 0.84f, 0.92f), 7.f);
	AddText(BriefBox, TEXT("07h  Imagen satelital actualizada: Medio Oriente."), 12.f, FLinearColor(0.77f, 0.84f, 0.84f, 0.92f), 7.f);

	AddCanvasChild(RootCanvas, Briefing, FAnchors(1.f, 0.5f, 1.f, 0.5f), FVector2D(-58.f, 0.f), FVector2D(430.f, 690.f), FVector2D(1.f, 0.5f));
}

void UWLFrontendMenuWidget::AddBottomStatusBar(bool bHasSave)
{
	UBorder* Bar = MakePanel(WidgetTree, FLinearColor(0.008f, 0.017f, 0.022f, 0.92f), FMargin(18.f, 12.f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomStatusBar"));
	Bar->SetContent(Row);

	const auto AddCell = [this, Row](const FString& Header, const FString& Body, float MinWidth, const FLinearColor& Accent)
	{
		UBorder* Cell = MakePanel(WidgetTree, FLinearColor(0.018f, 0.033f, 0.04f, 0.84f), FMargin(14.f, 8.f));
		UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Cell->SetContent(Copy);
		AddText(Copy, Header, 11.f, Accent, 0.f);
		AddText(Copy, Body, 12.f, WLText, 3.f);
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(MakeFixedHeight(WidgetTree, Cell, 58.f, MinWidth)))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	AddCell(TEXT("ONLINE"), TEXT("Global Command Network"), 230.f, WLGood);
	AddCell(TEXT("WORLD NEWS"), TEXT("Cumbre G20 abre nueva ronda comercial."), 420.f, WLGold);

	StatusText = MakeFrontendText(WidgetTree, bHasSave ? TEXT("ENTER Seleccionar   TAB Navegar   ESC Volver") : TEXT("ENTER Nueva campania   TAB Navegar   ESC Volver"), 13.f, WLMute, true);
	StatusText->SetJustification(ETextJustify::Center);
	if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(StatusText))
	{
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	AddCell(TEXT("LEGAL"), TEXT("Creditos  |  Licencias"), 190.f, WLMute);
	AddCell(TEXT("BUILD"), TEXT("v0.1.0  UE 5.8"), 170.f, WLGold);

	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Bar))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(36.f, -92.f, 36.f, 20.f));
		CanvasSlot->SetZOrder(10);
	}
}

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

UButton* UWLFrontendMenuWidget::AddMenuButton(UVerticalBox* Box, const FString& Label, const FString& Icon, bool bEnabled, bool bPrimary)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetClickMethod(EButtonClickMethod::MouseDown);
	Button->SetStyle(MakeButtonStyle(bPrimary, bEnabled));
	Button->SetBackgroundColor(FLinearColor::White);
	Button->SetIsEnabled(bEnabled);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	const FLinearColor TextColor = bEnabled ? WLText : FLinearColor(0.38f, 0.43f, 0.44f, 1.f);
	const FLinearColor AccentColor = bPrimary ? WLGold : (bEnabled ? FLinearColor(0.68f, 0.80f, 0.82f, 0.9f) : FLinearColor(0.28f, 0.32f, 0.33f, 1.f));

	UTextBlock* IconText = MakeFrontendText(WidgetTree, Icon, 19.f, AccentColor, true);
	IconText->SetJustification(ETextJustify::Center);
	if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(MakeFixedHeight(WidgetTree, IconText, 48.f, 42.f)))
	{
		RowSlot->SetPadding(FMargin(8.f, 0.f, 12.f, 0.f));
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* LabelText = MakeFrontendText(WidgetTree, Label, 15.f, TextColor, true);
	LabelText->SetJustification(ETextJustify::Left);
	if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(LabelText))
	{
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* ArrowText = MakeFrontendText(WidgetTree, TEXT(">"), 16.f, AccentColor, true);
	ArrowText->SetJustification(ETextJustify::Right);
	if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(ArrowText))
	{
		RowSlot->SetPadding(FMargin(8.f, 0.f, 12.f, 0.f));
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	Button->SetContent(MakeFixedHeight(WidgetTree, Row, bPrimary ? 56.f : 50.f, 320.f));
	AddVBoxChild(Box, Button, bPrimary ? 12.f : 8.f);
	return Button;
}

UTextBlock* UWLFrontendMenuWidget::AddText(UVerticalBox* Box, const FString& Text, float Size, const FLinearColor& Color, float TopPadding)
{
	UTextBlock* TextBlock = MakeFrontendText(WidgetTree, Text, Size, Color, Size >= 22.f);
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
