// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLGovernmentWidget.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/TextBlock.h"

namespace
{
	// Paleta calida oscura con acentos dorados (mas cercana al panel de faccion de Total War
	// que el teal frio del overlay anterior).
	const FLinearColor GovFrame       (0.62f, 0.48f, 0.18f, 1.00f);   // marco dorado
	const FLinearColor GovBackdrop     (0.010f, 0.012f, 0.014f, 0.72f);
	const FLinearColor GovPanel        (0.070f, 0.066f, 0.056f, 0.99f);
	const FLinearColor GovPanelSoft    (0.098f, 0.092f, 0.078f, 1.00f);
	const FLinearColor GovHeaderStrip  (0.140f, 0.115f, 0.060f, 1.00f);
	const FLinearColor GovCard         (0.125f, 0.118f, 0.100f, 1.00f);
	const FLinearColor GovCardAlt      (0.150f, 0.140f, 0.115f, 1.00f);
	const FLinearColor GovFuture       (0.085f, 0.082f, 0.075f, 1.00f);
	const FLinearColor GovGold         (1.00f, 0.84f, 0.34f, 1.00f);
	const FLinearColor GovGoldDim      (0.72f, 0.58f, 0.24f, 1.00f);
	const FLinearColor GovText         (0.94f, 0.93f, 0.88f, 1.00f);
	const FLinearColor GovMuted        (0.64f, 0.62f, 0.55f, 1.00f);
	const FLinearColor GovGood         (0.55f, 0.86f, 0.52f, 1.00f);
	const FLinearColor GovBad          (0.94f, 0.52f, 0.42f, 1.00f);
	const FLinearColor GovDarkInk      (0.06f, 0.05f, 0.03f, 1.00f);
	const FLinearColor GovTabIdle      (0.14f, 0.135f, 0.115f, 1.00f);

	FString GovGroupThousands(int64 Value)
	{
		const bool bNeg = Value < 0;
		const uint64 Abs = bNeg ? static_cast<uint64>(-Value) : static_cast<uint64>(Value);
		const FString Digits = FString::Printf(TEXT("%llu"), Abs);
		FString Out;
		int32 Count = 0;
		for (int32 i = Digits.Len() - 1; i >= 0; --i)
		{
			Out = Digits.Mid(i, 1) + Out;
			if (++Count % 3 == 0 && i > 0)
			{
				Out = TEXT(",") + Out;
			}
		}
		return bNeg ? (TEXT("-") + Out) : Out;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& S, int32 Size, const FLinearColor& C,
		ETextJustify::Type Justify = ETextJustify::Left, bool bWrap = false)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(S));
		T->SetColorAndOpacity(FSlateColor(C));
		FSlateFontInfo Font = T->GetFont();
		Font.Size = Size;
		T->SetFont(Font);
		T->SetJustification(Justify);
		T->SetAutoWrapText(bWrap);
		return T;
	}

	UBorder* MakeBorder(UWidgetTree* Tree, const FLinearColor& C, const FMargin& Pad)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(C);
		B->SetPadding(Pad);
		return B;
	}

	// Tarjeta de metrica: etiqueta pequena + valor grande. Rellena la celda del grid.
	UBorder* MakeMetricCard(UWidgetTree* Tree, const FString& Label, const FString& Value, const FLinearColor& ValueColor)
	{
		UBorder* Card = MakeBorder(Tree, GovCard, FMargin(14.f, 11.f));
		UVerticalBox* VB = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(Tree, Label.ToUpper(), 12, GovMuted));
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(Tree, Value, 27, ValueColor)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		Card->SetContent(VB);
		return Card;
	}

	// Tarjeta de lista lateral (ministerio / potencia): titulo + subtitulo + estado a la derecha.
	UBorder* MakeListCard(UWidgetTree* Tree, const FLinearColor& Accent, const FString& Title, const FString& Sub,
		const FString& Status, const FLinearColor& StatusColor)
	{
		UBorder* Card = MakeBorder(Tree, GovCard, FMargin(0.f));
		UHorizontalBox* HB = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// Franja de color a la izquierda.
		USizeBox* Stripe = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Stripe->SetWidthOverride(4.f);
		Stripe->SetContent(MakeBorder(Tree, Accent, FMargin(0.f)));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Stripe))
		{
			S->SetVerticalAlignment(VAlign_Fill);
		}

		UVerticalBox* VB = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(Tree, Title, 15, GovText, ETextJustify::Left, true));
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(Tree, Sub, 12, GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		UBorder* Pad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(11.f, 8.f));
		Pad->SetContent(VB);
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Pad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		if (!Status.IsEmpty())
		{
			// Ancho fijo reservado para el estado: asi el titulo (Fill, con wrap) nunca lo pisa.
			USizeBox* StatusBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			StatusBox->SetWidthOverride(78.f);
			UBorder* StatusPad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(0.f, 0.f, 11.f, 0.f));
			StatusPad->SetContent(MakeText(Tree, Status, 11, StatusColor, ETextJustify::Right));
			StatusBox->SetContent(StatusPad);
			if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(StatusBox))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
		}

		Card->SetContent(HB);
		return Card;
	}

	UBorder* MakeTag(UWidgetTree* Tree, const FString& S)
	{
		UBorder* Tag = MakeBorder(Tree, GovCardAlt, FMargin(11.f, 5.f));
		Tag->SetContent(MakeText(Tree, S, 13, GovText));
		return Tag;
	}

	void AddColumnChild(UVerticalBox* Box, UWidget* Child, float TopPad)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Child))
		{
			S->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		}
	}
}

TSharedRef<SWidget> UWLGovernmentWidget::RebuildWidget()
{
	// Widget 100% C++: hay que poblar el WidgetTree ANTES de que Slate lo tome aqui. Si se
	// construye en NativeConstruct (mas tarde), Slate ya se armo con un arbol vacio y no pinta nada.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildShell();
		SetActiveTab(EWLGovernmentTab::Overview);
	}
	return Super::RebuildWidget();
}

void UWLGovernmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UWLGovernmentWidget::NativeDestruct()
{
	for (UButton* B : TabButtons)
	{
		if (B)
		{
			B->OnClicked.RemoveAll(this);
		}
	}
	Super::NativeDestruct();
}

FReply UWLGovernmentWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::C)
	{
		OnCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UWLGovernmentWidget::BuildShell()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GovRoot"));
	WidgetTree->RootWidget = Root;

	// Fondo modal atenuado (cubre todo el mapa).
	UBorder* Dim = MakeBorder(WidgetTree, GovBackdrop, FMargin(0.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// Marco dorado + panel interior, centrado.
	UBorder* Frame = MakeBorder(WidgetTree, GovFrame, FMargin(2.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Frame))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D(0.f, 0.f));
		S->SetSize(FVector2D(1210.f, 860.f));
	}

	UBorder* Panel = MakeBorder(WidgetTree, GovPanel, FMargin(16.f));
	Frame->SetContent(Panel);

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GovMain"));
	Panel->SetContent(Main);

	BuildHeader(Main);
	BuildTabs(Main);
	BuildBody(Main);
	BuildFooter(Main);
}

void UWLGovernmentWidget::BuildHeader(UVerticalBox* Root)
{
	UWLCampaignGameInstance* GI = GetCampaignGI();
	FWLNationData Nation;
	const bool bHasNation = GI && GI->GetSelectedNation(Nation);
	const UWLStrategicTickSubsystem* Tick = GetTick();

	UBorder* Strip = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(16.f, 12.f));
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Strip->SetContent(HB);

	// Emblema (color de la nacion + ISO).
	USizeBox* EmblemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EmblemBox->SetWidthOverride(56.f);
	EmblemBox->SetHeightOverride(56.f);
	UBorder* Emblem = MakeBorder(WidgetTree, bHasNation ? Nation.MapColor : FLinearColor::Gray, FMargin(0.f));
	Emblem->SetHorizontalAlignment(HAlign_Center);
	Emblem->SetVerticalAlignment(VAlign_Center);
	Emblem->SetContent(MakeText(WidgetTree, bHasNation ? Nation.Iso : TEXT("--"), 20, GovDarkInk, ETextJustify::Center));
	EmblemBox->SetContent(Emblem);
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(EmblemBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	// Titulo + subtitulo.
	const FString LeaderName = (bHasNation && !Nation.Leader.IsEmpty()) ? Nation.Leader : TEXT("Presidente de la Republica");
	const FString GovType = (bHasNation && !Nation.GovernmentType.IsEmpty()) ? Nation.GovernmentType : TEXT("Gobierno");
	UVerticalBox* TitleVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleVB->AddChildToVerticalBox(MakeText(WidgetTree, bHasNation ? Nation.Name.ToUpper() : TEXT("SIN NACION"), 30, GovText));
	if (UVerticalBoxSlot* S = TitleVB->AddChildToVerticalBox(
		MakeText(WidgetTree, FString::Printf(TEXT("Presidente: %s     %s"), *LeaderName, *GovType), 15, GovGold)))
	{
		S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
	}
	UBorder* TitlePad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(16.f, 0.f, 0.f, 0.f));
	TitlePad->SetContent(TitleVB);
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(TitlePad))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}

	// Fecha del turno.
	if (Tick)
	{
		UVerticalBox* DateVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		DateVB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("TURNO"), 11, GovMuted, ETextJustify::Right));
		if (UVerticalBoxSlot* S = DateVB->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%02d/%02d/%d"), Tick->GetCurrentDay(), Tick->GetCurrentMonth(), Tick->GetCurrentYear()),
			16, GovText, ETextJustify::Right)))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(DateVB))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}
	}

	// Boton cerrar (X).
	UButton* Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Close->SetBackgroundColor(FLinearColor(0.42f, 0.14f, 0.12f, 1.f));
	Close->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnCloseClicked);
	USizeBox* CloseBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CloseBox->SetWidthOverride(38.f);
	CloseBox->SetHeightOverride(38.f);
	Close->SetContent(MakeText(WidgetTree, TEXT("X"), 18, GovText, ETextJustify::Center));
	CloseBox->SetContent(Close);
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(CloseBox))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	AddColumnChild(Root, Strip, 0.f);
}

void UWLGovernmentWidget::BuildTabs(UVerticalBox* Root)
{
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// AddDynamic estampa el NOMBRE de la funcion en tiempo de compilacion, asi que el handler NO puede
	// venir de una variable (puntero a miembro): cada pestana se enlaza explicitamente por indice.
	const TCHAR* Labels[] = { TEXT("RESUMEN"), TEXT("POLITICA"), TEXT("NACION"), TEXT("REGISTROS") };

	TabButtons.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(Labels); ++i)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(GovTabIdle);
		switch (i)
		{
		case 0: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabOverview); break;
		case 1: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabPolitics); break;
		case 2: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabNation);   break;
		case 3: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabRecords);  break;
		default: break;
		}

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(42.f);
		Box->SetMinDesiredWidth(150.f);
		Box->SetContent(MakeText(WidgetTree, Labels[i], 16, GovText, ETextJustify::Center));
		Button->SetContent(Box);

		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Button))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		TabButtons.Add(Button);
	}

	AddColumnChild(Root, HB, 12.f);
}

void UWLGovernmentWidget::BuildBody(UVerticalBox* Root)
{
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// --- Columna izquierda: GABINETE ---
	{
		USizeBox* ColBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ColBox->SetWidthOverride(266.f);
		UBorder* Col = MakeBorder(WidgetTree, GovPanelSoft, FMargin(14.f));
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("GABINETE"), 15, GovGold));

		struct FMinistry { const TCHAR* Name; const TCHAR* Role; };
		const FMinistry Ministries[] = {
			{ TEXT("Defensa"),      TEXT("Fuerzas armadas") },
			{ TEXT("Economia"),     TEXT("Tesoro y produccion") },
			{ TEXT("Exterior"),     TEXT("Diplomacia y tratados") },
			{ TEXT("Interior"),     TEXT("Orden y estabilidad") },
			{ TEXT("Inteligencia"), TEXT("Espionaje y contra") },
			{ TEXT("Ciencia"),      TEXT("Tecnologia e I+D") },
		};
		for (const FMinistry& M : Ministries)
		{
			AddColumnChild(VB, MakeListCard(WidgetTree, GovGoldDim,
				FString::Printf(TEXT("Min. de %s"), M.Name), M.Role, TEXT("Por asignar"), GovGold), 8.f);
		}
		Scroll->AddChild(VB);
		Col->SetContent(Scroll);
		ColBox->SetContent(Col);
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(ColBox))
		{
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// --- Centro: contenido por pestana (scroll) ---
	{
		UBorder* Col = MakeBorder(WidgetTree, GovPanelSoft, FMargin(6.f));
		CenterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CenterScroll->AddChild(CenterBox);
		Col->SetContent(CenterScroll);
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(Col))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(12.f, 0.f, 12.f, 0.f));
		}
	}

	// --- Columna derecha: OTRAS POTENCIAS ---
	{
		USizeBox* ColBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ColBox->SetWidthOverride(266.f);
		UBorder* Col = MakeBorder(WidgetTree, GovPanelSoft, FMargin(14.f));
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("OTRAS POTENCIAS"), 15, GovGold));

		const UWLDataRegistry* Registry = GetRegistry();
		const UWLStrategicTickSubsystem* Tick = GetTick();
		const FString Iso = PlayerIso();
		int32 Shown = 0;
		if (Registry)
		{
			TArray<FWLNationData> Nations = Registry->GetAllNations();
			Nations.Sort([](const FWLNationData& A, const FWLNationData& B) { return A.Name < B.Name; });
			for (const FWLNationData& Other : Nations)
			{
				if (Other.Iso.Equals(Iso, ESearchCase::IgnoreCase))
				{
					continue;
				}
				const int64 Tre = Tick ? Tick->GetTreasury(Other.Iso) : 0;
				const int32 Prov = Registry->GetProvincesByNation(Other.Iso).Num();
				AddColumnChild(VB, MakeListCard(WidgetTree, Other.MapColor, Other.Name,
					FString::Printf(TEXT("Tesoro %s"), *GovGroupThousands(Tre)),
					FString::Printf(TEXT("%d prov"), Prov), GovMuted), 8.f);
				++Shown;
			}
		}
		if (Shown == 0)
		{
			AddColumnChild(VB, MakeText(WidgetTree, TEXT("Sin datos de otras naciones."), 13, GovMuted, ETextJustify::Left, true), 8.f);
		}
		Scroll->AddChild(VB);
		Col->SetContent(Scroll);
		ColBox->SetContent(Col);
		if (UHorizontalBoxSlot* S = Body->AddChildToHorizontalBox(ColBox))
		{
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Body))
	{
		S->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UWLGovernmentWidget::BuildFooter(UVerticalBox* Root)
{
	UWLCampaignGameInstance* GI = GetCampaignGI();
	FWLNationData Nation;
	const bool bHasNation = GI && GI->GetSelectedNation(Nation);
	const FString LeaderName = (bHasNation && !Nation.Leader.IsEmpty()) ? Nation.Leader : TEXT("Presidente de la Republica");
	const FString GovType = (bHasNation && !Nation.GovernmentType.IsEmpty()) ? Nation.GovernmentType : TEXT("Gobierno");

	UBorder* Footer = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(16.f, 12.f));
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Footer->SetContent(HB);

	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Left->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("PRESIDENTE"), 12, GovGold));
	if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(MakeText(WidgetTree, LeaderName, 20, GovText)))
	{
		S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* S = Left->AddChildToVerticalBox(MakeText(WidgetTree, GovType, 13, GovMuted)))
	{
		S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Left))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Right->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("PODERES"), 12, GovGold, ETextJustify::Left));
	UHorizontalBox* Tags = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const TCHAR* Powers[] = { TEXT("Ejecutivo"), TEXT("Comandante en jefe"), TEXT("Decretos"), TEXT("Presupuesto") };
	for (const TCHAR* P : Powers)
	{
		if (UHorizontalBoxSlot* S = Tags->AddChildToHorizontalBox(MakeTag(WidgetTree, P)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
	}
	if (UVerticalBoxSlot* S = Right->AddChildToVerticalBox(Tags))
	{
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
	Right->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Agenda y rasgos del lider: fase futura del roadmap."), 12, GovMuted));
	if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Right))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}

	AddColumnChild(Root, Footer, 12.f);
}

void UWLGovernmentWidget::RebuildCenter()
{
	if (!CenterBox)
	{
		return;
	}
	CenterBox->ClearChildren();
	if (CenterScroll)
	{
		CenterScroll->ScrollToStart();
	}
	switch (ActiveTab)
	{
	case EWLGovernmentTab::Overview: BuildOverviewTab(); break;
	case EWLGovernmentTab::Politics: BuildPoliticsTab(); break;
	case EWLGovernmentTab::Nation:   BuildNationTab();   break;
	case EWLGovernmentTab::Records:  BuildRecordsTab();  break;
	}
}

void UWLGovernmentWidget::BuildOverviewTab()
{
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	const FSummary Sum = BuildSummary();
	const int64 Treasury = Tick ? Tick->GetTreasury(Iso) : 0;
	const int64 Balance = Tick ? Tick->GetMonthlyBalance(Iso) : 0;

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("ESTADO DE LA NACION"), 17, GovGold), 6.f);

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	auto Place = [&](int32 R, int32 Col, UBorder* Card)
	{
		if (UUniformGridSlot* S = Grid->AddChildToUniformGrid(Card, R, Col))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	Place(0, 0, MakeMetricCard(WidgetTree, TEXT("Tesoro nacional"), GovGroupThousands(Treasury), GovText));
	Place(0, 1, MakeMetricCard(WidgetTree, TEXT("Balance mensual"),
		FString::Printf(TEXT("%s%s"), Balance >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Balance)), Balance >= 0 ? GovGood : GovBad));
	Place(1, 0, MakeMetricCard(WidgetTree, TEXT("Provincias"), FString::Printf(TEXT("%d"), Sum.ProvinceCount), GovText));
	Place(1, 1, MakeMetricCard(WidgetTree, TEXT("Poblacion"), GovGroupThousands(Sum.Population), GovText));
	Place(2, 0, MakeMetricCard(WidgetTree, TEXT("Ingreso / mes"), GovGroupThousands(Sum.MonthlyIncome), GovGood));
	Place(2, 1, MakeMetricCard(WidgetTree, TEXT("Mantenimiento / mes"), GovGroupThousands(Sum.MonthlyUpkeep), GovMuted));
	AddColumnChild(CenterBox, Grid, 10.f);

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("CONDICIONES DE VICTORIA"), 17, GovGold), 20.f);
	UHorizontalBox* Tags = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const TCHAR* Conditions[] = { TEXT("Dominacion"), TEXT("Economica"), TEXT("Tecnologica"), TEXT("Diplomatica"), TEXT("Militar") };
	for (const TCHAR* C : Conditions)
	{
		if (UHorizontalBoxSlot* S = Tags->AddChildToHorizontalBox(MakeTag(WidgetTree, C)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
	}
	AddColumnChild(CenterBox, Tags, 8.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Seguimiento de objetivos: fase futura del roadmap."), 13, GovMuted), 8.f);
}

void UWLGovernmentWidget::BuildPoliticsTab()
{
	const FSummary Sum = BuildSummary();

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("POLITICA INTERNA"), 17, GovGold), 6.f);

	// Orden publico nacional: dato REAL (media de las provincias controladas).
	UBorder* OrderCard = MakeBorder(WidgetTree, GovCard, FMargin(14.f, 12.f));
	UVerticalBox* OVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* OHead = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = OHead->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("Orden publico nacional"), 15, GovText)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	OHead->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(TEXT("%d / 100"), Sum.AveragePublicOrder), 15, GovGold, ETextJustify::Right));
	OVB->AddChildToVerticalBox(OHead);

	// Barra de progreso (borde de fondo + relleno proporcional).
	UBorder* Track = MakeBorder(WidgetTree, FLinearColor(0.04f, 0.045f, 0.05f, 1.f), FMargin(0.f));
	USizeBox* TrackBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	TrackBox->SetHeightOverride(12.f);
	UHorizontalBox* FillRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const float Frac = FMath::Clamp(Sum.AveragePublicOrder / 100.f, 0.f, 1.f);
	const FLinearColor OrderColor = Sum.AveragePublicOrder >= 60 ? GovGood : (Sum.AveragePublicOrder >= 35 ? GovGold : GovBad);
	UBorder* Fill = MakeBorder(WidgetTree, OrderColor, FMargin(0.f));
	if (UHorizontalBoxSlot* S = FillRow->AddChildToHorizontalBox(Fill))
	{
		// Relleno proporcional: ESlateSizeRule::Fill con el peso en Value (no existe "FillFraction").
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = FMath::Max(Frac, 0.001f);
		S->SetSize(Size);
	}
	UBorder* Rest = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(0.f));
	if (UHorizontalBoxSlot* S = FillRow->AddChildToHorizontalBox(Rest))
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = FMath::Max(1.f - Frac, 0.001f);
		S->SetSize(Size);
	}
	TrackBox->SetContent(FillRow);
	Track->SetContent(TrackBox);
	if (UVerticalBoxSlot* S = OVB->AddChildToVerticalBox(Track))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	OrderCard->SetContent(OVB);
	AddColumnChild(CenterBox, OrderCard, 10.f);

	// Sistemas del roadmap aun no implementados.
	const TCHAR* Systems[] = {
		TEXT("Estabilidad interna"), TEXT("Corrupcion"), TEXT("Apoyo popular"),
		TEXT("Partidos politicos"), TEXT("Elecciones"), TEXT("Leyes y decretos"),
	};
	for (const TCHAR* Sy : Systems)
	{
		UBorder* Row = MakeBorder(WidgetTree, GovFuture, FMargin(12.f, 9.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, Sy, 14, GovMuted)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		HB->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("Fase futura"), 12, GovGoldDim, ETextJustify::Right));
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, 7.f);
	}
}

void UWLGovernmentWidget::BuildNationTab()
{
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FSummary Sum = BuildSummary();

	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("TERRITORIO  (%d provincias)"), Sum.ProvinceCount), 17, GovGold), 6.f);

	if (Sum.Controlled.Num() == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin provincias bajo control directo."), 14, GovMuted), 10.f);
		return;
	}

	auto MakeRow = [&](const FString& Name, const FString& Pop, const FString& Bal, const FLinearColor& BalColor,
		const FLinearColor& RowColor, int32 FontSize, const FLinearColor& NameColor)
	{
		UBorder* Row = MakeBorder(WidgetTree, RowColor, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, Name, FontSize, NameColor)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		USizeBox* PopBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PopBox->SetWidthOverride(160.f);
		PopBox->SetContent(MakeText(WidgetTree, Pop, FontSize, GovMuted, ETextJustify::Right));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(PopBox)) { S->SetVerticalAlignment(VAlign_Center); }
		USizeBox* BalBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BalBox->SetWidthOverride(140.f);
		BalBox->SetContent(MakeText(WidgetTree, Bal, FontSize, BalColor, ETextJustify::Right));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(BalBox)) { S->SetVerticalAlignment(VAlign_Center); }
		Row->SetContent(HB);
		return Row;
	};

	AddColumnChild(CenterBox, MakeRow(TEXT("Provincia"), TEXT("Poblacion"), TEXT("Balance/mes"),
		GovMuted, FLinearColor(0.f, 0.f, 0.f, 0.f), 12, GovMuted), 6.f);

	int32 Index = 0;
	for (const FWLProvinceData& P : Sum.Controlled)
	{
		const int64 Bal = Tick ? Tick->GetProvinceMonthlyBalance(P.Id) : 0;
		AddColumnChild(CenterBox, MakeRow(P.Name, GovGroupThousands(P.Population),
			FString::Printf(TEXT("%s%s"), Bal >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Bal)),
			Bal >= 0 ? GovGood : GovBad, (Index % 2 == 0) ? GovCard : GovCardAlt, 15, GovText), 4.f);
		++Index;
	}
}

void UWLGovernmentWidget::BuildRecordsTab()
{
	const UWLStrategicTickSubsystem* Tick = GetTick();

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("REGISTROS RECIENTES"), 17, GovGold), 6.f);

	const TArray<FString> Reports = Tick ? Tick->GetLastEconomicAIReports() : TArray<FString>();
	if (Reports.Num() > 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Actividad economica de la IA:"), 13, GovMuted), 10.f);
		int32 Index = 0;
		for (const FString& R : Reports)
		{
			UBorder* Row = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
			Row->SetContent(MakeText(WidgetTree, R, 14, GovText, ETextJustify::Left, true));
			AddColumnChild(CenterBox, Row, 5.f);
			++Index;
		}
	}
	else
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Aun no hay eventos. Avanza el tiempo con [M] o el boton AVANZAR DIA."), 14, GovMuted, ETextJustify::Left, true), 10.f);
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Diplomacia, crisis y noticias globales: fase futura del roadmap."), 12, GovMuted, ETextJustify::Left, true), 18.f);
}

void UWLGovernmentWidget::SetActiveTab(EWLGovernmentTab Tab)
{
	ActiveTab = Tab;
	RefreshTabButtonStyles();
	RebuildCenter();
}

void UWLGovernmentWidget::RefreshTabButtonStyles()
{
	for (int32 i = 0; i < TabButtons.Num(); ++i)
	{
		if (TabButtons[i])
		{
			TabButtons[i]->SetBackgroundColor(static_cast<int32>(ActiveTab) == i ? GovGoldDim : GovTabIdle);
		}
	}
}

void UWLGovernmentWidget::OnTabOverview() { SetActiveTab(EWLGovernmentTab::Overview); }
void UWLGovernmentWidget::OnTabPolitics() { SetActiveTab(EWLGovernmentTab::Politics); }
void UWLGovernmentWidget::OnTabNation()   { SetActiveTab(EWLGovernmentTab::Nation); }
void UWLGovernmentWidget::OnTabRecords()  { SetActiveTab(EWLGovernmentTab::Records); }

void UWLGovernmentWidget::OnCloseClicked()
{
	if (AWLCampaignPlayerController* PC = GetOwningPlayer<AWLCampaignPlayerController>())
	{
		PC->SetGovernmentWindowOpen(false);
	}
}

UWLGovernmentWidget::FSummary UWLGovernmentWidget::BuildSummary() const
{
	FSummary S;
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	if (!Registry || !Tick || Iso.IsEmpty())
	{
		return S;
	}

	const int32 InitOrder = Tick->GetBalanceRules().InitialPublicOrder;
	int64 OrderSum = 0;
	for (const FWLProvinceData& P : Registry->GetAllProvinces())
	{
		if (!Tick->GetProvinceControllerIso(P.Id).Equals(Iso, ESearchCase::IgnoreCase))
		{
			continue;
		}
		S.ProvinceCount++;
		FWLProvinceRuntimeState State;
		const bool bHasState = Tick->GetProvinceState(P.Id, State);
		S.Population += bHasState ? State.Population : P.Population;
		OrderSum += bHasState ? State.PublicOrder : InitOrder;
		S.MonthlyIncome += Tick->GetProvinceMonthlyIncome(P.Id);
		S.MonthlyUpkeep += Tick->GetProvinceMonthlyUpkeep(P.Id);
		S.Controlled.Add(P);
	}
	S.MonthlyUpkeep += Tick->GetNationMilitaryUpkeep(Iso);   // FE1.1: mantenimiento militar en el total de gasto
	S.AveragePublicOrder = S.ProvinceCount > 0 ? static_cast<int32>(OrderSum / S.ProvinceCount) : 0;
	S.Controlled.Sort([](const FWLProvinceData& A, const FWLProvinceData& B) { return A.Population > B.Population; });
	return S;
}

FString UWLGovernmentWidget::PlayerIso() const
{
	const UWLCampaignGameInstance* GI = GetCampaignGI();
	return GI ? GI->GetSelectedNationIso() : FString();
}

UWLCampaignGameInstance* UWLGovernmentWidget::GetCampaignGI() const
{
	return Cast<UWLCampaignGameInstance>(GetGameInstance());
}

UWLDataRegistry* UWLGovernmentWidget::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLStrategicTickSubsystem* UWLGovernmentWidget::GetTick() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}
