// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLGovernmentWidget.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Core/WLCharacterTypes.h"
#include "Core/WLFinancialTypes.h"
#include "Core/WLPoliticalTypes.h"
#include "Economy/WLEconomyLibrary.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
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

	FString RankToText(EWLMilitaryRank Rank)
	{
		switch (Rank)
		{
		case EWLMilitaryRank::Colonel:         return TEXT("Coronel");
		case EWLMilitaryRank::BrigadeGeneral:  return TEXT("Gral. de brigada");
		case EWLMilitaryRank::DivisionGeneral: return TEXT("Gral. de division");
		case EWLMilitaryRank::CorpsGeneral:    return TEXT("Gral. de cuerpo");
		case EWLMilitaryRank::FieldMarshal:    return TEXT("Mariscal de campo");
		default:                               return TEXT("Oficial");
		}
	}

	FString DiplomaticStatusToText(EWLDiplomaticStatus Status)
	{
		switch (Status)
		{
		case EWLDiplomaticStatus::War:     return TEXT("GUERRA");
		case EWLDiplomaticStatus::Tension: return TEXT("Tension");
		default:                           return TEXT("Paz");
		}
	}

	FString TreatyToText(EWLTreatyType Treaty)
	{
		switch (Treaty)
		{
		case EWLTreatyType::TradeAgreement: return TEXT("Acuerdo comercial");
		case EWLTreatyType::NonAggression:  return TEXT("No agresion");
		case EWLTreatyType::Alliance:       return TEXT("Alianza");
		case EWLTreatyType::Embargo:        return TEXT("Embargo");
		default:                            return TEXT("Tratado");
		}
	}

	// Boton de accion pequeno con payload, enlazado al dispatcher del widget.
	UWLGovActionButton* MakeActionButton(UWidgetTree* Tree, UWLGovernmentWidget* Owner,
		const FString& ActionId, const FString& Label, const FLinearColor& Bg,
		float MinWidth = 0.f, int32 FontSize = 12)
	{
		UWLGovActionButton* Button = Tree->ConstructWidget<UWLGovActionButton>(UWLGovActionButton::StaticClass());
		Button->SetBackgroundColor(Bg);
		Button->BindAction(Owner, ActionId);
		UBorder* Pad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(9.f, 5.f));
		Pad->SetContent(MakeText(Tree, Label, FontSize, GovText, ETextJustify::Center));
		if (MinWidth > 0.f)
		{
			USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetMinDesiredWidth(MinWidth);
			Box->SetContent(Pad);
			Button->SetContent(Box);
		}
		else
		{
			Button->SetContent(Pad);
		}
		return Button;
	}

	// Fila "Etiqueta ..... valor" para paneles de stats.
	UBorder* MakeStatRow(UWidgetTree* Tree, const FString& Label, const FString& Value,
		const FLinearColor& ValueColor, const FLinearColor& RowColor)
	{
		UBorder* Row = MakeBorder(Tree, RowColor, FMargin(12.f, 7.f));
		UHorizontalBox* HB = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(Tree, Label, 13, GovText)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		HB->AddChildToHorizontalBox(MakeText(Tree, Value, 13, ValueColor, ETextJustify::Right));
		Row->SetContent(HB);
		return Row;
	}
}

void UWLGovActionButton::BindAction(UWLGovernmentWidget* InOwner, const FString& InActionId)
{
	Owner = InOwner;
	ActionId = InActionId;
	OnClicked.AddDynamic(this, &UWLGovActionButton::HandleClicked);
}

void UWLGovActionButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleAction(ActionId);
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
	const TCHAR* Labels[] = { TEXT("RESUMEN"), TEXT("ECONOMIA"), TEXT("ALTO MANDO"), TEXT("POLITICA"), TEXT("DIPLOMACIA"), TEXT("REGISTROS") };

	TabButtons.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(Labels); ++i)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(GovTabIdle);
		switch (i)
		{
		case 0: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabOverview);    break;
		case 1: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabEconomy);     break;
		case 2: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabHighCommand); break;
		case 3: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabPolitics);    break;
		case 4: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabDiplomacy);   break;
		case 5: Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTabRecords);     break;
		default: break;
		}

		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(42.f);
		Box->SetMinDesiredWidth(118.f);
		Box->SetContent(MakeText(WidgetTree, Labels[i], 15, GovText, ETextJustify::Center));
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

		// Gabinete REAL desde el backend de personajes (gestion en ALTO MANDO).
		const UWLCharacterSubsystem* Characters = GetCharacters();
		const TArray<FWLCabinetSeat> Cabinet = Characters
			? Characters->GetCabinet(PlayerIso())
			: TArray<FWLCabinetSeat>();
		if (Cabinet.Num() > 0)
		{
			for (const FWLCabinetSeat& Seat : Cabinet)
			{
				const bool bFilled = Seat.Minister.IsValid();
				AddColumnChild(VB, MakeListCard(WidgetTree, bFilled ? GovGoldDim : GovMuted,
					FString::Printf(TEXT("Min. de %s"), *UWLCharacterSubsystem::MinisterOfficeToString(Seat.Office)),
					bFilled ? Seat.Minister.Name : TEXT("Cargo vacante"),
					bFilled ? FString::Printf(TEXT("Skill %d"), Seat.Minister.Skill) : TEXT("Vacante"),
					bFilled ? GovGood : GovGold), 8.f);
			}
		}
		else
		{
			AddColumnChild(VB, MakeText(WidgetTree, TEXT("Sin datos de gabinete."), 13, GovMuted, ETextJustify::Left, true), 8.f);
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
	// Feedback de la ultima accion (nombramientos, tratados, bonos...): visible en cualquier tab.
	if (!LastActionMessage.IsEmpty())
	{
		UBorder* Strip = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(12.f, 8.f));
		Strip->SetContent(MakeText(WidgetTree, LastActionMessage, 13,
			bLastActionSucceeded ? GovGold : GovBad, ETextJustify::Left, true));
		AddColumnChild(CenterBox, Strip, 4.f);
	}

	switch (ActiveTab)
	{
	case EWLGovernmentTab::Overview:    BuildOverviewTab();    break;
	case EWLGovernmentTab::Economy:     BuildEconomyTab();     break;
	case EWLGovernmentTab::HighCommand: BuildHighCommandTab(); break;
	case EWLGovernmentTab::Politics:    BuildPoliticsTab();    break;
	case EWLGovernmentTab::Diplomacy:   BuildDiplomacyTab();   break;
	case EWLGovernmentTab::Records:     BuildRecordsTab();     break;
	}
}

void UWLGovernmentWidget::BuildOverviewTab()
{
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	const FSummary Sum = BuildSummary();
	const int64 Treasury = Tick ? Tick->GetTreasury(Iso) : 0;
	const int64 Balance = Tick ? Tick->GetMonthlyBalance(Iso) : 0;

	// F5.3/F5.4: si la campania termino, el RESUMEN lo anuncia primero.
	if (const UWLPoliticalSubsystem* Political = GetPolitical())
	{
		const FWLCampaignOutcomeState Outcome = Political->GetCampaignOutcome();
		if (Outcome.bGameOver)
		{
			const bool bPlayerWon = Outcome.WinningNationIso.Equals(Iso, ESearchCase::IgnoreCase);
			UBorder* Banner = MakeBorder(WidgetTree, bPlayerWon ? GovGoldDim : FLinearColor(0.35f, 0.10f, 0.08f, 1.f), FMargin(14.f, 12.f));
			UVerticalBox* BVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			BVB->AddChildToVerticalBox(MakeText(WidgetTree,
				bPlayerWon ? TEXT("VICTORIA") : TEXT("FIN DE LA PARTIDA"), 22, GovText));
			if (UVerticalBoxSlot* S = BVB->AddChildToVerticalBox(MakeText(WidgetTree,
				FString::Printf(TEXT("%s — %s"), *Outcome.OutcomeType, *Outcome.Reason), 13, GovText, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
			Banner->SetContent(BVB);
			AddColumnChild(CenterBox, Banner, 6.f);
		}
	}

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
	// FE1.5: PIB y su crecimiento entre ticks economicos.
	if (Tick)
	{
		const double Growth = Tick->GetNationGDPGrowth(Iso);
		Place(3, 0, MakeMetricCard(WidgetTree, TEXT("PIB / mes"), GovGroupThousands(Tick->GetNationGDP(Iso)), GovText));
		Place(3, 1, MakeMetricCard(WidgetTree, TEXT("Crecimiento"),
			FString::Printf(TEXT("%+.2f%%"), Growth * 100.0),
			Growth > 0.0 ? GovGood : (Growth < 0.0 ? GovBad : GovMuted)));
	}
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
	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Dominacion y golpe/revolucion se chequean cada mes; el resto llega con sus fases."), 13, GovMuted, ETextJustify::Left, true), 8.f);

	// Territorio (antes tab NACION): provincias controladas con poblacion y balance real.
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("TERRITORIO  (%d provincias)"), Sum.ProvinceCount), 17, GovGold), 20.f);
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

// FE1.3: panel ECONOMIA — presupuesto mensual desglosado por categorias + palanca de impuestos (FE1.2).
void UWLGovernmentWidget::BuildEconomyTab()
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	if (!Tick || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos economicos."), 14, GovMuted), 10.f);
		return;
	}

	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);

	// Fila de presupuesto: etiqueta (fill) + importe con signo a la derecha.
	auto AddBudgetRow = [&](const FString& Label, int64 Amount, bool bIsIncome, bool bTotal, int32 Index)
	{
		const FLinearColor RowColor = bTotal ? GovCardAlt : ((Index % 2 == 0) ? GovCard : GovCardAlt);
		UBorder* Row = MakeBorder(WidgetTree, RowColor, FMargin(12.f, bTotal ? 10.f : 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(
			MakeText(WidgetTree, Label, bTotal ? 15 : 14, bTotal ? GovGold : GovText)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		HB->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s%s"), bIsIncome ? TEXT("+") : TEXT("-"), *GovGroupThousands(Amount)),
			bTotal ? 15 : 14, bIsIncome ? GovGood : GovBad, ETextJustify::Right));
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, bTotal ? 8.f : 4.f);
	};

	// FE1.5: PIB y crecimiento arriba del presupuesto.
	{
		const double Growth = Tick->GetNationGDPGrowth(Iso);
		const double Inflation = Tick->GetNationInflationRate(Iso);
		const FWLNationLaborStats Labor = Tick->GetNationLaborStats(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
			TEXT("PIB: %s / mes   ·   Crecimiento: %+.2f%%   ·   Inflacion: %+.2f%%   ·   Ciclo: %s"),
			*GovGroupThousands(Tick->GetNationGDP(Iso)), Growth * 100.0,
			Inflation * 100.0, *Tick->GetNationEconomicCycleLabel(Iso)),
			14, Growth < 0.0 ? GovBad : GovText), 6.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
			TEXT("Empleo: %s / %s   ·   Desempleo: %.1f%%   ·   Productividad: %.0f%%"),
			*GovGroupThousands(Labor.Employed), *GovGroupThousands(Labor.Workforce),
			Labor.UnemploymentRate * 100.0, Labor.Productivity * 100.0),
			12, Labor.UnemploymentRate > 0.15 ? GovBad : GovMuted), 4.f);
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("PRESUPUESTO MENSUAL"), 17, GovGold), 12.f);

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("INGRESOS"), 13, GovMuted), 12.f);
	AddBudgetRow(TEXT("Recursos y produccion"), Budget.ResourceIncome, true, false, 0);
	AddBudgetRow(TEXT("Impuestos"), Budget.TaxIncome, true, false, 1);
	if (Budget.ExportIncome > 0)
	{
		AddBudgetRow(TEXT("Exportaciones"), Budget.ExportIncome, true, false, 2);
	}
	if (Budget.TariffIncome > 0)
	{
		AddBudgetRow(TEXT("Aranceles"), Budget.TariffIncome, true, false, 3);   // FE4.3
	}
	if (Budget.ForeignAidIncome > 0)
	{
		AddBudgetRow(TEXT("Ayuda exterior recibida"), Budget.ForeignAidIncome, true, false, 4);   // FE5.3
	}
	AddBudgetRow(TEXT("TOTAL INGRESOS"), Budget.TotalIncome(), true, true, 0);

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("GASTOS"), 13, GovMuted), 14.f);
	AddBudgetRow(TEXT("Militar"), Budget.MilitaryUpkeep, false, false, 0);
	AddBudgetRow(TEXT("Infraestructura"), Budget.InfrastructureUpkeep, false, false, 1);
	AddBudgetRow(TEXT("Salarios publicos"), Budget.PublicWages, false, false, 2);
	AddBudgetRow(TEXT("Gasto social"), Budget.SocialSpending, false, false, 3);
	if (Budget.DebtInterest > 0)
	{
		AddBudgetRow(TEXT("Intereses de deuda"), Budget.DebtInterest, false, false, 4);   // FE1.4
	}
	if (Budget.DebtService > 0)
	{
		AddBudgetRow(TEXT("Servicio de deuda (bonos/FMI)"), Budget.DebtService, false, false, 5);   // FE5.1
	}
	if (Budget.ImportCost > 0)
	{
		AddBudgetRow(TEXT("Importaciones criticas"), Budget.ImportCost, false, false, 6);
	}
	if (Budget.CorruptionLoss > 0)
	{
		AddBudgetRow(TEXT("Perdida por corrupcion"), Budget.CorruptionLoss, false, false, 7);   // FE6.2
	}
	if (Budget.ForeignAidExpense > 0)
	{
		AddBudgetRow(TEXT("Ayuda exterior concedida"), Budget.ForeignAidExpense, false, false, 8);   // FE5.3
	}
	AddBudgetRow(TEXT("TOTAL GASTOS"), Budget.TotalSpending(), false, true, 0);

	// Balance neto (== GetMonthlyBalance).
	const int64 Net = Budget.Net();
	UBorder* NetCard = MakeBorder(WidgetTree, GovHeaderStrip, FMargin(12.f, 11.f));
	UHorizontalBox* NetHB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = NetHB->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("BALANCE MENSUAL"), 15, GovText)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	NetHB->AddChildToHorizontalBox(MakeText(WidgetTree,
		FString::Printf(TEXT("%s%s"), Net >= 0 ? TEXT("+") : TEXT(""), *GovGroupThousands(Net)),
		18, Net >= 0 ? GovGood : GovBad, ETextJustify::Right));
	NetCard->SetContent(NetHB);
	AddColumnChild(CenterBox, NetCard, 12.f);

	// FE1.4: deuda y linea de credito. Gastar por encima del tesoro endeuda (con interes mensual)
	// hasta el limite de credito; el tesoro negativo ademas penaliza el orden publico cada mes.
	{
		const int64 Treasury = Tick->GetTreasury(Iso);
		const int64 CreditLimit = Tick->GetCreditLimit(Iso);
		if (Treasury < 0)
		{
			UBorder* DebtCard = MakeBorder(WidgetTree, GovCard, FMargin(12.f, 10.f));
			UVerticalBox* DebtVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBox* DebtHB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = DebtHB->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("DEUDA"), 15, GovBad)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			DebtHB->AddChildToHorizontalBox(MakeText(WidgetTree,
				GovGroupThousands(-Treasury), 18, GovBad, ETextJustify::Right));
			DebtVB->AddChildToVerticalBox(DebtHB);
			if (UVerticalBoxSlot* S = DebtVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
				TEXT("Interes %.0f%%/mes (%s/mes)   ·   Credito restante: %s"),
				Rules.DebtMonthlyInterestRate * 100.0,
				*GovGroupThousands(Budget.DebtInterest),
				*GovGroupThousands(FMath::Max<int64>(0, CreditLimit + Treasury))),
				13, GovMuted, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
			DebtCard->SetContent(DebtVB);
			AddColumnChild(CenterBox, DebtCard, 8.f);
		}
		else
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
				TEXT("Sin deuda. Linea de credito disponible: %s (interes %.0f%%/mes si el tesoro cae en negativo)."),
				*GovGroupThousands(CreditLimit), Rules.DebtMonthlyInterestRate * 100.0),
				12, GovMuted, ETextJustify::Left, true), 8.f);
		}
	}

	// FE2.3-FE4.1: produccion nacional por bien con insumos, demanda, precios y comercio.
	{
		const TArray<FWLGoodMarketBalance> Market = Tick->GetNationGoodMarketBalance(Iso);
		if (Market.Num() > 0)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("MERCADO NACIONAL / MES"), 17, GovGold), 20.f);
			const UWLDataRegistry* Registry = GetRegistry();
			int32 Index = 0;
			for (const FWLGoodMarketBalance& Balance : Market)
			{
				FWLGoodData Good;
				const bool bHasGood = Registry && Registry->GetGood(Balance.GoodId, Good);
				const FString GoodName = bHasGood ? Good.Name : Balance.GoodId;
				const bool bManufactured = bHasGood && Good.Category == EWLGoodCategory::Manufactured;

				UBorder* Row = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 7.f));
				UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeText(WidgetTree, GoodName, 14, GovText)))
				{
					S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
					S->SetVerticalAlignment(VAlign_Center);
				}
				USizeBox* KindBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				KindBox->SetWidthOverride(104.f);
				KindBox->SetContent(MakeText(WidgetTree, bManufactured ? TEXT("Manufactura") : TEXT("Extraccion"),
					11, GovMuted, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(KindBox)) { S->SetVerticalAlignment(VAlign_Center); }
				USizeBox* UnitsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				UnitsBox->SetWidthOverride(142.f);
				UnitsBox->SetContent(MakeText(WidgetTree,
					FString::Printf(TEXT("%s / %s"), *GovGroupThousands(Balance.Production), *GovGroupThousands(Balance.Demand)),
					13, GovGold, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(UnitsBox)) { S->SetVerticalAlignment(VAlign_Center); }
				USizeBox* PriceBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				PriceBox->SetWidthOverride(86.f);
				PriceBox->SetContent(MakeText(WidgetTree,
					FString::Printf(TEXT("x%.2f"), Balance.PriceMultiplier),
					13, Balance.PriceMultiplier > 1.05 ? GovBad : GovMuted, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(PriceBox)) { S->SetVerticalAlignment(VAlign_Center); }
				USizeBox* TradeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				TradeBox->SetWidthOverride(136.f);
				TradeBox->SetContent(MakeText(WidgetTree,
					FString::Printf(TEXT("I %s / E %s"),
						*GovGroupThousands(Balance.Imports), *GovGroupThousands(Balance.Exports)),
					12, (Balance.Deficit > 0 || Balance.Imports > 0) ? GovBad : GovMuted, ETextJustify::Right));
				if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(TradeBox)) { S->SetVerticalAlignment(VAlign_Center); }
				Row->SetContent(HB);
				AddColumnChild(CenterBox, Row, 4.f);
				++Index;
			}
			AddColumnChild(CenterBox, MakeText(WidgetTree,
				TEXT("Oferta final tras insumos, precios y comercio regional."),
				12, GovMuted, ETextJustify::Left, true), 6.f);
		}
	}

	// FE3.4: shocks de mercado activos (bien, multiplicador, duracion restante).
	{
		const TArray<FWLMarketShockState> Shocks = Tick->GetActiveMarketShocks();
		if (Shocks.Num() > 0)
		{
			AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("SHOCKS DE MERCADO ACTIVOS"), 17, GovGold), 20.f);
			const UWLDataRegistry* Registry = GetRegistry();
			int32 Index = 0;
			for (const FWLMarketShockState& Shock : Shocks)
			{
				FWLGoodData Good;
				const FString GoodName = (Shock.GoodId == TEXT("*") || Shock.GoodId.Equals(TEXT("all"), ESearchCase::IgnoreCase))
					? TEXT("Todo el mercado")
					: ((Registry && Registry->GetGood(Shock.GoodId, Good)) ? Good.Name : Shock.GoodId);
				AddColumnChild(CenterBox, MakeStatRow(WidgetTree,
					FString::Printf(TEXT("%s — %s"), *Shock.Title, *GoodName),
					FString::Printf(TEXT("x%.2f · %d/%d meses"), Shock.PriceMultiplier, Shock.RemainingMonths, Shock.TotalMonths),
					Shock.PriceMultiplier >= 1.0 ? GovBad : GovGood,
					(Index % 2 == 0) ? GovCard : GovCardAlt), 4.f);
				++Index;
			}
		}
	}

	// FE4.3: arancel nacional (stepper) + resumen de comercio del presupuesto.
	{
		const int32 Tariff = Tick->GetTariffRate(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("COMERCIO EXTERIOR"), 17, GovGold), 20.f);

		UBorder* TariffCard = MakeBorder(WidgetTree, GovCard, FMargin(14.f, 11.f));
		UHorizontalBox* TariffRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* TariffInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TariffInfo->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("ARANCEL A IMPORTACIONES"), 12, GovMuted));
		if (UVerticalBoxSlot* S = TariffInfo->AddChildToVerticalBox(
			MakeText(WidgetTree, FString::Printf(TEXT("%d%%"), Tariff), 27, GovGold)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = TariffInfo->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Ingreso por aranceles: %s/mes   ·   Exporta %s · Importa %s"),
			*GovGroupThousands(Budget.TariffIncome),
			*GovGroupThousands(Budget.ExportIncome), *GovGroupThousands(Budget.ImportCost)),
			13, GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = TariffRow->AddChildToHorizontalBox(TariffInfo))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		auto AddTariffButton = [&](const FString& ActionId, const TCHAR* Label)
		{
			if (UHorizontalBoxSlot* S = TariffRow->AddChildToHorizontalBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, GovTabIdle, 52.f, 20)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			}
		};
		AddTariffButton(TEXT("tariffdown"), TEXT("-"));
		AddTariffButton(TEXT("tariffup"), TEXT("+"));
		TariffCard->SetContent(TariffRow);
		AddColumnChild(CenterBox, TariffCard, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Subir aranceles recauda y protege la industria local, pero encarece importaciones y molesta a los vecinos. Rutas y embargos: en DIPLOMACIA."),
			12, GovMuted, ETextJustify::Left, true), 6.f);
	}

	// FE5.1/FE5.3: finanzas soberanas — rating, deuda, bonos, FMI, default, apoyos externos.
	{
		const FWLFinancialProfile Profile = Tick->GetFinancialProfile(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("FINANZAS SOBERANAS"), 17, GovGold), 20.f);

		const FLinearColor RatingColor = Profile.bInDefault ? GovBad : (Profile.CreditScore >= 60 ? GovGood : GovGold);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Calificacion crediticia"),
			FString::Printf(TEXT("%s (%d/100)%s"), *Profile.CreditRatingLabel, Profile.CreditScore,
				Profile.bInDefault ? TEXT(" · EN DEFAULT") : TEXT("")),
			RatingColor, GovCard), 8.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Deuda viva / servicio mensual"),
			FString::Printf(TEXT("%s / %s"), *GovGroupThousands(Profile.OutstandingDebt), *GovGroupThousands(Profile.MonthlyDebtService)),
			Profile.OutstandingDebt > 0 ? GovBad : GovMuted, GovCardAlt), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Credito disponible"),
			GovGroupThousands(Profile.AvailableCredit), GovText, GovCard), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Riesgo de default"),
			FString::Printf(TEXT("%.0f%%%s"), Profile.DefaultRisk * 100.0, Profile.bIMFEligible ? TEXT(" · elegible para FMI") : TEXT("")),
			Profile.DefaultRisk > 0.4 ? GovBad : GovMuted, GovCardAlt), 4.f);

		UHorizontalBox* FinActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto AddFinAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
		{
			if (UHorizontalBoxSlot* S = FinActions->AddChildToHorizontalBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 130.f)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
		};
		AddFinAction(TEXT("bond"), TEXT("EMITIR BONO"), GovGoldDim);
		if (Profile.bIMFEligible)
		{
			AddFinAction(TEXT("imf"), TEXT("PROGRAMA FMI"), GovTabIdle);
		}
		if (Profile.OutstandingDebt > 0 && !Profile.bInDefault)
		{
			AddFinAction(TEXT("default"), TEXT("DECLARAR DEFAULT"), FLinearColor(0.40f, 0.12f, 0.10f, 1.f));
		}
		AddColumnChild(CenterBox, FinActions, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("El bono emite deuda a 24 meses segun tu rating. El FMI presta barato con condiciones. El default borra deuda pero hunde rating y orden."),
			12, GovMuted, ETextJustify::Left, true), 4.f);

		const TArray<FWLFinancialInstrumentState> Instruments = Tick->GetFinancialInstrumentsForNation(Iso);
		int32 ActiveCount = 0;
		for (const FWLFinancialInstrumentState& Inst : Instruments)
		{
			if (!Inst.IsActive())
			{
				continue;
			}
			if (ActiveCount == 0)
			{
				AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("INSTRUMENTOS ACTIVOS"), 13, GovMuted), 10.f);
			}
			AddColumnChild(CenterBox, MakeStatRow(WidgetTree, Inst.Title,
				FString::Printf(TEXT("%s restante · %s/mes · %d meses"),
					*GovGroupThousands(Inst.PrincipalRemaining), *GovGroupThousands(Inst.MonthlyPayment), Inst.RemainingMonths),
				GovMuted, (ActiveCount % 2 == 0) ? GovCard : GovCardAlt), 4.f);
			++ActiveCount;
		}

		const TArray<FWLForeignSupportState> Supports = Tick->GetForeignSupportForNation(Iso);
		int32 SupportCount = 0;
		for (const FWLForeignSupportState& Support : Supports)
		{
			if (Support.bCompleted)
			{
				continue;
			}
			if (SupportCount == 0)
			{
				AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("APOYOS EXTERNOS ACTIVOS"), 13, GovMuted), 10.f);
			}
			AddColumnChild(CenterBox, MakeStatRow(WidgetTree,
				FString::Printf(TEXT("%s -> %s"), *Support.SponsorIso, *Support.RecipientIso),
				FString::Printf(TEXT("%s/mes · %d meses"), *GovGroupThousands(Support.MonthlyAmount), Support.RemainingMonths),
				GovGood, (SupportCount % 2 == 0) ? GovCard : GovCardAlt), 4.f);
			++SupportCount;
		}
	}

	// FE6: gobernanza economica — ministro, corrupcion y tecnologia mueven la economia real.
	{
		const FWLEconomicGovernanceStats Gov = Tick->GetEconomicGovernanceStats(Iso);
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("GOBERNANZA ECONOMICA"), 17, GovGold), 20.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Ministro de Economia"),
			Gov.EconomyMinisterName.IsEmpty()
				? TEXT("Cargo vacante (nombra en ALTO MANDO)")
				: FString::Printf(TEXT("%s · skill %d"), *Gov.EconomyMinisterName, Gov.EconomyMinisterSkill),
			Gov.EconomyMinisterName.IsEmpty() ? GovGold : GovText, GovCard), 8.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Corrupcion sistemica"),
			FString::Printf(TEXT("%d/100 · skim %.1f%% (%s/mes)"),
				Gov.SystemicCorruption, Gov.CorruptionSkimRate * 100.0, *GovGroupThousands(Budget.CorruptionLoss)),
			Gov.SystemicCorruption >= 50 ? GovBad : GovMuted, GovCardAlt), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Tecnologia / clima de inversion"),
			FString::Printf(TEXT("%d / %d"), Gov.TechnologyLevel, Gov.InvestmentClimate), GovText, GovCard), 4.f);
		AddColumnChild(CenterBox, MakeStatRow(WidgetTree, TEXT("Eficiencia fiscal / productividad"),
			FString::Printf(TEXT("x%.2f / x%.2f"), Gov.TaxCollectionMultiplier, Gov.ProductivityMultiplier),
			GovText, GovCardAlt), 4.f);
	}

	// FE1.2: palanca de impuestos. Mover la tasa cambia la recaudacion (Laffer) y el orden publico mensual.
	{
		const int32 TaxRate = Tick->GetTaxRate(Iso);
		const double TaxMult = UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(TaxRate, Rules);
		const int32 OrderPressure = Tick->GetTaxPublicOrderPressure(Iso);

		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("IMPUESTOS"), 17, GovGold), 20.f);

		UBorder* TaxCard = MakeBorder(WidgetTree, GovCard, FMargin(14.f, 11.f));
		UHorizontalBox* TaxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* TaxInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TaxInfo->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("TASA NACIONAL"), 12, GovMuted));
		if (UVerticalBoxSlot* S = TaxInfo->AddChildToVerticalBox(
			MakeText(WidgetTree, FString::Printf(TEXT("%d%%"), TaxRate), 27, GovGold)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = TaxInfo->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Recaudacion x%.2f   ·   Orden publico %+d/mes"), TaxMult, -OrderPressure),
			13, OrderPressure > 0 ? GovBad : GovMuted)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = TaxRow->AddChildToHorizontalBox(TaxInfo))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}

		auto AddTaxButton = [&](const TCHAR* Label, bool bUp)
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			Button->SetBackgroundColor(GovTabIdle);
			if (bUp)
			{
				Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTaxUp);
			}
			else
			{
				Button->OnClicked.AddDynamic(this, &UWLGovernmentWidget::OnTaxDown);
			}
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(52.f);
			Box->SetHeightOverride(46.f);
			Box->SetContent(MakeText(WidgetTree, Label, 20, GovText, ETextJustify::Center));
			Button->SetContent(Box);
			if (UHorizontalBoxSlot* S = TaxRow->AddChildToHorizontalBox(Button))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			}
		};
		AddTaxButton(TEXT("-"), false);
		AddTaxButton(TEXT("+"), true);

		TaxCard->SetContent(TaxRow);
		AddColumnChild(CenterBox, TaxCard, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
			TEXT("Rango %d%%-%d%%. Subir impuestos recauda mas con rendimiento decreciente y drena orden publico cada mes."),
			Rules.TaxRateMinPercent, Rules.TaxRateMaxPercent), 12, GovMuted, ETextJustify::Left, true), 6.f);
	}
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

	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos politicos."), 14, GovMuted), 10.f);
		return;
	}

	// F2: poder interno — riesgo de golpe y oposicion (datos reales del backend).
	const FWLInternalPowerState Power = Political->GetInternalPower(Iso);

	UBorder* CoupCard = MakeBorder(WidgetTree, GovCard, FMargin(14.f, 12.f));
	UVerticalBox* CVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* CHead = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = CHead->AddChildToHorizontalBox(MakeText(WidgetTree, TEXT("Riesgo de golpe de estado"), 15, GovText)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	const FLinearColor CoupColor = Power.CoupRisk >= 60 ? GovBad : (Power.CoupRisk >= 30 ? GovGold : GovGood);
	CHead->AddChildToHorizontalBox(MakeText(WidgetTree, FString::Printf(TEXT("%d / 100"), Power.CoupRisk), 15, CoupColor, ETextJustify::Right));
	CVB->AddChildToVerticalBox(CHead);

	// Barra de riesgo (mismo patron que la de orden publico).
	{
		UBorder* CoupTrack = MakeBorder(WidgetTree, FLinearColor(0.04f, 0.045f, 0.05f, 1.f), FMargin(0.f));
		USizeBox* CoupTrackBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CoupTrackBox->SetHeightOverride(12.f);
		UHorizontalBox* CoupFillRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const float CoupFrac = FMath::Clamp(Power.CoupRisk / 100.f, 0.f, 1.f);
		UBorder* CoupFill = MakeBorder(WidgetTree, CoupColor, FMargin(0.f));
		if (UHorizontalBoxSlot* S = CoupFillRow->AddChildToHorizontalBox(CoupFill))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(CoupFrac, 0.001f);
			S->SetSize(Size);
		}
		UBorder* CoupRest = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(0.f));
		if (UHorizontalBoxSlot* S = CoupFillRow->AddChildToHorizontalBox(CoupRest))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(1.f - CoupFrac, 0.001f);
			S->SetSize(Size);
		}
		CoupTrackBox->SetContent(CoupFillRow);
		CoupTrack->SetContent(CoupTrackBox);
		if (UVerticalBoxSlot* S = CVB->AddChildToVerticalBox(CoupTrack))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
	}
	if (UVerticalBoxSlot* S = CVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
		TEXT("Oposicion: fuerza %d · popularidad %d   ·   Financiacion externa de golpe: %d"),
		Power.OppositionStrength, Power.OppositionPopularity, Power.ExternalCoupFunding),
		13, Power.OppositionStrength >= 50 ? GovBad : GovMuted, ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}
	CoupCard->SetContent(CVB);
	AddColumnChild(CenterBox, CoupCard, 10.f);

	if (!Power.LastCoupReport.IsEmpty())
	{
		UBorder* Report = MakeBorder(WidgetTree, GovCardAlt, FMargin(12.f, 8.f));
		Report->SetContent(MakeText(WidgetTree, FString::Printf(TEXT("Ultimo golpe: %s"), *Power.LastCoupReport),
			13, Power.bLastCoupSucceeded ? GovBad : GovMuted, ETextJustify::Left, true));
		AddColumnChild(CenterBox, Report, 6.f);
	}

	// F2.4: acciones internas (recompensar/purgar viven en cada general de ALTO MANDO).
	{
		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Actions->AddChildToHorizontalBox(
			MakeActionButton(WidgetTree, this, TEXT("repress"), TEXT("REPRIMIR OPOSICION"), GovTabIdle, 170.f, 13)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		AddColumnChild(CenterBox, Actions, 10.f);
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Reprimir baja la fuerza de la oposicion a costa de orden publico y tesoro. Recompensar o purgar generales: en ALTO MANDO."),
			12, GovMuted, ETextJustify::Left, true), 4.f);
	}

	// F5: eventos politicos pendientes con sus opciones.
	const TArray<FWLPoliticalEventInstance> Events = Political->GetQueuedEvents(Iso);
	int32 PendingCount = 0;
	for (const FWLPoliticalEventInstance& Event : Events)
	{
		if (!Event.bResolved)
		{
			++PendingCount;
		}
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("EVENTOS  (%d pendientes)"), PendingCount), 17, GovGold), 20.f);
	if (PendingCount == 0)
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree,
			TEXT("Sin eventos pendientes. Se disparan al avanzar el mes segun la situacion interna."),
			13, GovMuted, ETextJustify::Left, true), 8.f);
	}
	for (const FWLPoliticalEventInstance& Event : Events)
	{
		if (Event.bResolved)
		{
			continue;
		}
		UBorder* Card = MakeBorder(WidgetTree, GovCard, FMargin(14.f, 12.f));
		UVerticalBox* EVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		EVB->AddChildToVerticalBox(MakeText(WidgetTree, Event.Title, 16, GovGold, ETextJustify::Left, true));
		if (UVerticalBoxSlot* S = EVB->AddChildToVerticalBox(MakeText(WidgetTree, Event.Body, 13, GovText, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		for (const FWLPoliticalEventOption& Option : Event.Options)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("event:%s:%s"), *Event.InstanceId, *Option.OptionId),
				Option.Label, GovGoldDim, 150.f, 13)))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			}
			FString Effects;
			if (Option.PoliticalCapitalDelta != 0) Effects += FString::Printf(TEXT("Capital %+d  "), Option.PoliticalCapitalDelta);
			if (Option.TreasuryDelta != 0)         Effects += FString::Printf(TEXT("Tesoro %+lld  "), static_cast<long long>(Option.TreasuryDelta));
			if (Option.PublicOrderDelta != 0)      Effects += FString::Printf(TEXT("Orden %+d  "), Option.PublicOrderDelta);
			if (Option.OppositionDelta != 0)       Effects += FString::Printf(TEXT("Oposicion %+d  "), Option.OppositionDelta);
			if (Option.RelationDelta != 0)         Effects += FString::Printf(TEXT("Relacion %+d  "), Option.RelationDelta);
			if (!Option.MarketShockGoodId.IsEmpty()) Effects += TEXT("Shock de mercado");
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeText(WidgetTree, Effects, 12, GovMuted, ETextJustify::Left, true)))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* S = EVB->AddChildToVerticalBox(Row))
			{
				S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			}
		}
		Card->SetContent(EVB);
		AddColumnChild(CenterBox, Card, 8.f);
	}
}

// F1.6/F1.7: ALTO MANDO — gabinete gestionable + generales con ascenso/baja/recompensa.
void UWLGovernmentWidget::BuildHighCommandTab()
{
	const FString Iso = PlayerIso();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos de personajes."), 14, GovMuted), 10.f);
		return;
	}

	const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("ALTO MANDO"), 17, GovGold), 6.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree, FString::Printf(
		TEXT("Capital politico: %d   ·   Estabilidad: %d   ·   Corrupcion: %d   ·   Riesgo de golpe: %d"),
		Stats.PoliticalCapital, Stats.Stability, Stats.Corruption, Stats.CoupRisk),
		13, Stats.CoupRisk >= 50 ? GovBad : GovMuted, ETextJustify::Left, true), 4.f);

	// Gabinete: cada cargo con su ministro o vacante + nombrar/destituir.
	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("GABINETE"), 15, GovGold), 14.f);
	const TArray<FWLCabinetSeat> Cabinet = Characters->GetCabinet(Iso);
	int32 Index = 0;
	for (const FWLCabinetSeat& Seat : Cabinet)
	{
		const bool bFilled = Seat.Minister.IsValid();
		UBorder* Row = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 8.f));
		UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("Ministerio de %s"), *UWLCharacterSubsystem::MinisterOfficeToString(Seat.Office)), 14, GovText));
		Info->AddChildToVerticalBox(MakeText(WidgetTree,
			bFilled
				? FString::Printf(TEXT("%s — skill %d · lealtad %d"), *Seat.Minister.Name, Seat.Minister.Skill, Seat.Minister.Loyalty)
				: TEXT("Cargo vacante"),
			12, bFilled ? GovMuted : GovGold, ETextJustify::Left, true));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Info))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (bFilled)
		{
			if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("dismiss:%d"), static_cast<int32>(Seat.Office)), TEXT("DESTITUIR"), FLinearColor(0.34f, 0.13f, 0.11f, 1.f), 96.f)))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
		}
		else
		{
			if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(MakeActionButton(WidgetTree, this,
				FString::Printf(TEXT("appoint:%d"), static_cast<int32>(Seat.Office)), TEXT("NOMBRAR"), GovGoldDim, 96.f)))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
		}
		Row->SetContent(HB);
		AddColumnChild(CenterBox, Row, 4.f);
		++Index;
	}
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("NOMBRAR elige al candidato disponible con mas skill (prefiere su cartera). Nombrar cuesta capital politico."),
		12, GovMuted, ETextJustify::Left, true), 4.f);

	// Generales: tarjeta por general con stats + acciones F1.7/F2.4.
	const TArray<FWLCharacter> Generals = Characters->GetGenerals(Iso);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		FString::Printf(TEXT("GENERALES  (%d)"), Generals.Num()), 15, GovGold), 16.f);
	Index = 0;
	for (const FWLCharacter& General : Generals)
	{
		if (!General.bActive)
		{
			continue;
		}
		UBorder* Card = MakeBorder(WidgetTree, (Index % 2 == 0) ? GovCard : GovCardAlt, FMargin(12.f, 9.f));
		UVerticalBox* GVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s — %s"), *General.Name, *RankToText(General.Rank)), 14, GovText, ETextJustify::Left, true)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			General.AssignedArmyId.IsEmpty() ? TEXT("Sin mando") : FString::Printf(TEXT("Ejercito %s"), *General.AssignedArmyId),
			11, GovMuted, ETextJustify::Right));
		GVB->AddChildToVerticalBox(Head);
		const FLinearColor LoyaltyColor = General.Loyalty < 40 ? GovBad : (General.Loyalty < 60 ? GovGold : GovGood);
		if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("Skill %d · Lealtad %d · Ambicion %d · Popularidad %d · Renombre %d"),
			General.Skill, General.Loyalty, General.Ambition, General.Popularity, General.Renown),
			12, LoyaltyColor, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}
		UWrapBox* Actions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		auto AddGeneralAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
		{
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Actions->AddChildToWrapBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 108.f))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 5.f));
			}
		};
		AddGeneralAction(FString::Printf(TEXT("promote:%s"), *General.Id), TEXT("ASCENDER"), GovGoldDim);
		AddGeneralAction(FString::Printf(TEXT("reward:%s"), *General.Id), TEXT("RECOMPENSAR"), GovTabIdle);
		AddGeneralAction(FString::Printf(TEXT("purge:%s"), *General.Id), TEXT("PURGAR"), FLinearColor(0.34f, 0.13f, 0.11f, 1.f));
		AddGeneralAction(FString::Printf(TEXT("retire:%s"), *General.Id), TEXT("DAR DE BAJA"), GovTabIdle);
		if (UVerticalBoxSlot* S = GVB->AddChildToVerticalBox(Actions))
		{
			S->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
		}
		Card->SetContent(GVB);
		AddColumnChild(CenterBox, Card, 5.f);
		++Index;
	}
	AddColumnChild(CenterBox, MakeActionButton(WidgetTree, this, TEXT("creategeneral"), TEXT("+ CREAR GENERAL"), GovGoldDim, 160.f, 13), 10.f);
	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Recompensar sube lealtad (cuesta tesoro). Purgar elimina al general y asusta al resto (-lealtad). Lealtad baja + ambicion alta = golpe."),
		12, GovMuted, ETextJustify::Left, true), 4.f);
}

// F3.6 + F4: DIPLOMACIA — relaciones, tratados, guerra e intriga por pais.
void UWLGovernmentWidget::BuildDiplomacyTab()
{
	const FString Iso = PlayerIso();
	UWLPoliticalSubsystem* Political = GetPolitical();
	const UWLDataRegistry* Registry = GetRegistry();
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Political || !Registry || !Tick || Iso.IsEmpty())
	{
		AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("Sin datos diplomaticos."), 14, GovMuted), 10.f);
		return;
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree, TEXT("DIPLOMACIA"), 17, GovGold), 6.f);

	const FString SpyId = FindPlayerSpyId();
	TArray<FWLNationData> Nations = Registry->GetAllNations();
	Nations.Sort([](const FWLNationData& A, const FWLNationData& B) { return A.Name < B.Name; });
	for (const FWLNationData& Other : Nations)
	{
		if (Other.Iso.Equals(Iso, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FWLDiplomaticRelationState Relation;
		Political->GetRelation(Iso, Other.Iso, Relation);
		const FWLTradeRouteState Route = Tick->GetTradeRouteBetween(Iso, Other.Iso);
		const FWLIntelligenceNetworkState Network = Political->GetIntelligenceNetwork(Iso, Other.Iso);
		const bool bAtWar = Relation.Status == EWLDiplomaticStatus::War;

		UBorder* Card = MakeBorder(WidgetTree, GovCard, FMargin(12.f, 10.f));
		UVerticalBox* NVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// Cabecera: nombre + estado + opinion.
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* EmblemBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		EmblemBox->SetWidthOverride(26.f);
		EmblemBox->SetHeightOverride(26.f);
		EmblemBox->SetContent(MakeBorder(WidgetTree, Other.MapColor, FMargin(0.f)));
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(EmblemBox)) { S->SetVerticalAlignment(VAlign_Center); }
		UBorder* NamePad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(9.f, 0.f, 0.f, 0.f));
		NamePad->SetContent(MakeText(WidgetTree, Other.Name.ToUpper(), 15, GovText));
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(NamePad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		const FLinearColor StatusColor = bAtWar ? GovBad : (Relation.Status == EWLDiplomaticStatus::Tension ? GovGold : GovGood);
		Head->AddChildToHorizontalBox(MakeText(WidgetTree,
			FString::Printf(TEXT("%s · Opinion %+d"), *DiplomaticStatusToText(Relation.Status), Relation.Opinion),
			13, StatusColor, ETextJustify::Right));
		NVB->AddChildToVerticalBox(Head);

		// Tratados + ruta comercial.
		FString TreatyLine;
		for (const EWLTreatyType Treaty : Relation.Treaties)
		{
			TreatyLine += (TreatyLine.IsEmpty() ? TEXT("") : TEXT(" · ")) + TreatyToText(Treaty);
		}
		if (TreatyLine.IsEmpty())
		{
			TreatyLine = TEXT("Sin tratados");
		}
		if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("%s   ·   Ruta comercial: %s (x%.2f)"),
			*TreatyLine, Route.bOpen ? TEXT("abierta") : *FString::Printf(TEXT("CERRADA — %s"), *Route.Reason), Route.AccessMultiplier),
			12, Route.bOpen ? GovMuted : GovBad, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}

		// Acciones diplomaticas (WrapBox: con 6+ botones la fila se parte en varias lineas, no se desborda).
		UWrapBox* Actions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		auto AddDiploAction = [&](const FString& ActionId, const FString& Label, const FLinearColor& Bg)
		{
			if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(Actions->AddChildToWrapBox(
				MakeActionButton(WidgetTree, this, ActionId, Label, Bg, 0.f, 11))))
			{
				S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
			}
		};
		auto HasTreaty = [&Relation](EWLTreatyType Type) { return Relation.Treaties.Contains(Type); };
		if (bAtWar)
		{
			AddDiploAction(FString::Printf(TEXT("peace:%s"), *Other.Iso), TEXT("NEGOCIAR PAZ"), GovGoldDim);
		}
		else
		{
			AddDiploAction(FString::Printf(TEXT("war:%s"), *Other.Iso), TEXT("DECLARAR GUERRA"), FLinearColor(0.40f, 0.12f, 0.10f, 1.f));
		}
		const struct { EWLTreatyType Type; const TCHAR* Label; } TreatyDefs[] = {
			{ EWLTreatyType::TradeAgreement, TEXT("COMERCIO") },
			{ EWLTreatyType::NonAggression,  TEXT("NO AGRESION") },
			{ EWLTreatyType::Alliance,       TEXT("ALIANZA") },
			{ EWLTreatyType::Embargo,        TEXT("EMBARGO") },
		};
		for (const auto& Def : TreatyDefs)
		{
			if (HasTreaty(Def.Type))
			{
				AddDiploAction(FString::Printf(TEXT("breaktreaty:%d:%s"), static_cast<int32>(Def.Type), *Other.Iso),
					FString::Printf(TEXT("ROMPER %s"), Def.Label), GovTabIdle);
			}
			else
			{
				AddDiploAction(FString::Printf(TEXT("treaty:%d:%s"), static_cast<int32>(Def.Type), *Other.Iso),
					Def.Label, GovGoldDim);
			}
		}
		AddDiploAction(FString::Printf(TEXT("aid:%s"), *Other.Iso), TEXT("AYUDA"), GovTabIdle);
		if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(Actions))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}

		// F4: intriga contra este pais.
		if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(MakeText(WidgetTree, FString::Printf(
			TEXT("INTELIGENCIA — red %d · exposicion %d%s"),
			Network.NetworkStrength, Network.Exposure,
			Network.LastOperationReport.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  ·  %s"), *Network.LastOperationReport)),
			12, Network.Exposure >= 60 ? GovBad : GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
		if (SpyId.IsEmpty())
		{
			if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(MakeText(WidgetTree,
				TEXT("Sin espias activos disponibles."), 12, GovMuted)))
			{
				S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
			}
		}
		else
		{
			UWrapBox* SpyActions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
			auto AddSpyAction = [&](const FString& ActionId, const FString& Label)
			{
				if (UWrapBoxSlot* S = Cast<UWrapBoxSlot>(SpyActions->AddChildToWrapBox(
					MakeActionButton(WidgetTree, this, ActionId, Label, GovFuture, 0.f, 11))))
				{
					S->SetPadding(FMargin(0.f, 0.f, 5.f, 5.f));
				}
			};
			AddSpyAction(FString::Printf(TEXT("spynet:%s"), *Other.Iso), TEXT("RED +"));
			AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::SabotageEconomy), *Other.Iso), TEXT("SABOTEAR ECO"));
			AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::SabotageArmy), *Other.Iso), TEXT("SABOTEAR EJERCITO"));
			AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::FundCoup), *Other.Iso), TEXT("FINANCIAR GOLPE"));
			AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::Propaganda), *Other.Iso), TEXT("PROPAGANDA"));
			AddSpyAction(FString::Printf(TEXT("spy:%d:%s"), static_cast<int32>(EWLSpyOperationType::CounterIntelligence), *Other.Iso), TEXT("CONTRAESP."));
			if (UVerticalBoxSlot* S = NVB->AddChildToVerticalBox(SpyActions))
			{
				S->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
			}
		}

		Card->SetContent(NVB);
		AddColumnChild(CenterBox, Card, 8.f);
	}

	AddColumnChild(CenterBox, MakeText(WidgetTree,
		TEXT("Las operaciones de intriga suben la exposicion; si te descubren, la relacion se hunde. La guerra habilita batallas y corta rutas."),
		12, GovMuted, ETextJustify::Left, true), 8.f);
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
	LastActionMessage.Reset();   // el feedback de acciones es del tab donde ocurrio
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

void UWLGovernmentWidget::OnTabOverview()    { SetActiveTab(EWLGovernmentTab::Overview); }
void UWLGovernmentWidget::OnTabEconomy()     { SetActiveTab(EWLGovernmentTab::Economy); }
void UWLGovernmentWidget::OnTabHighCommand() { SetActiveTab(EWLGovernmentTab::HighCommand); }
void UWLGovernmentWidget::OnTabPolitics()    { SetActiveTab(EWLGovernmentTab::Politics); }
void UWLGovernmentWidget::OnTabDiplomacy()   { SetActiveTab(EWLGovernmentTab::Diplomacy); }
void UWLGovernmentWidget::OnTabRecords()     { SetActiveTab(EWLGovernmentTab::Records); }

// Dispatcher central: todos los UWLGovActionButton llegan aqui con "verbo[:arg1[:arg2]]".
// Cada rama llama al endpoint backend correspondiente y refresca el tab con el mensaje resultante.
void UWLGovernmentWidget::HandleAction(const FString& ActionId)
{
	TArray<FString> Parts;
	ActionId.ParseIntoArray(Parts, TEXT(":"), true);
	if (Parts.Num() == 0)
	{
		return;
	}
	const FString Verb = Parts[0];
	const FString Arg1 = Parts.Num() > 1 ? Parts[1] : FString();
	const FString Arg2 = Parts.Num() > 2 ? Parts[2] : FString();

	const FString Iso = PlayerIso();
	UWLStrategicTickSubsystem* Tick = GetTick();
	UWLCharacterSubsystem* Characters = GetCharacters();
	UWLPoliticalSubsystem* Political = GetPolitical();

	FString Message;
	bool bOk = false;

	if (Verb == TEXT("tariffup") || Verb == TEXT("tariffdown"))
	{
		if (Political && Tick)
		{
			const int32 Delta = Verb == TEXT("tariffup") ? 5 : -5;
			bOk = Political->SetNationTariffRate(Iso, Tick->GetTariffRate(Iso) + Delta, Message);
		}
	}
	else if (Verb == TEXT("creategeneral"))
	{
		FWLCharacter NewGeneral;
		bOk = Characters && Characters->CreateGeneral(Iso, NewGeneral, Message);
	}
	else if (Verb == TEXT("promote"))
	{
		bOk = Characters && Characters->PromoteGeneral(Arg1, Message);
	}
	else if (Verb == TEXT("retire"))
	{
		bOk = Characters && Characters->RetireCharacter(Arg1, Message);
	}
	else if (Verb == TEXT("reward"))
	{
		bOk = Political && Political->RewardGeneral(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("purge"))
	{
		bOk = Political && Political->PurgeCharacter(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("repress"))
	{
		bOk = Political && Political->RepressOpposition(Iso, Message);
	}
	else if (Verb == TEXT("appoint"))
	{
		// Elige al mejor candidato disponible: rol ministro, activo y sin cargo; prefiere su cartera, luego skill.
		if (Characters)
		{
			const EWLMinisterOffice Office = static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1));
			TArray<FWLCharacter> Candidates = Characters->GetCharactersByRole(Iso, EWLCharacterRole::Minister);
			Candidates.RemoveAll([](const FWLCharacter& C)
			{
				return !C.bActive || C.AssignedOffice != EWLMinisterOffice::None;
			});
			Candidates.Sort([Office](const FWLCharacter& A, const FWLCharacter& B)
			{
				const bool bAPref = A.PreferredOffice == Office;
				const bool bBPref = B.PreferredOffice == Office;
				if (bAPref != bBPref)
				{
					return bAPref;
				}
				return A.Skill > B.Skill;
			});
			if (Candidates.Num() > 0)
			{
				bOk = Characters->AppointMinister(Iso, Office, Candidates[0].Id, Message);
			}
			else
			{
				Message = TEXT("No hay candidatos a ministro disponibles.");
			}
		}
	}
	else if (Verb == TEXT("dismiss"))
	{
		bOk = Characters && Characters->DismissMinister(Iso, static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("war"))
	{
		bOk = Political && Political->DeclareWar(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("peace"))
	{
		bOk = Political && Political->MakePeace(Iso, Arg1, Message);
	}
	else if (Verb == TEXT("treaty"))
	{
		bOk = Political && Political->SignTreaty(Iso, Arg2, static_cast<EWLTreatyType>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("breaktreaty"))
	{
		bOk = Political && Political->BreakTreaty(Iso, Arg2, static_cast<EWLTreatyType>(FCString::Atoi(*Arg1)), Message);
	}
	else if (Verb == TEXT("spynet"))
	{
		const FString SpyId = FindPlayerSpyId();
		bOk = Political && !SpyId.IsEmpty() && Political->BuildSpyNetwork(Iso, Arg1, SpyId, Message);
		if (SpyId.IsEmpty())
		{
			Message = TEXT("Sin espias activos disponibles.");
		}
	}
	else if (Verb == TEXT("spy"))
	{
		const FString SpyId = FindPlayerSpyId();
		bOk = Political && !SpyId.IsEmpty() && Political->RunSpyOperation(
			Iso, Arg2, SpyId, static_cast<EWLSpyOperationType>(FCString::Atoi(*Arg1)), Message);
		if (SpyId.IsEmpty())
		{
			Message = TEXT("Sin espias activos disponibles.");
		}
	}
	else if (Verb == TEXT("event"))
	{
		bOk = Political && Political->ResolveEvent(Arg1, Arg2, Message);
	}
	else if (Verb == TEXT("bond"))
	{
		if (Tick)
		{
			const int64 Principal = FMath::Max<int64>(10000, Tick->GetFinancialProfile(Iso).AvailableCredit / 4);
			bOk = Tick->IssueBond(Iso, Principal, 24, Message);
		}
	}
	else if (Verb == TEXT("imf"))
	{
		if (Tick)
		{
			const int64 Principal = FMath::Max<int64>(10000, Tick->GetFinancialProfile(Iso).AvailableCredit / 3);
			bOk = Tick->RequestIMFProgram(Iso, Principal, 36, Message);
		}
	}
	else if (Verb == TEXT("default"))
	{
		bOk = Tick && Tick->MarkDebtDefault(Iso, Message);
	}
	else if (Verb == TEXT("aid"))
	{
		bOk = Tick && Tick->GrantForeignAid(Iso, Arg1, 1000, 12, Message);
	}
	else
	{
		Message = FString::Printf(TEXT("Accion desconocida: %s"), *ActionId);
	}

	LastActionMessage = Message.IsEmpty()
		? FString::Printf(TEXT("%s: %s"), *Verb, bOk ? TEXT("hecho.") : TEXT("no se pudo."))
		: Message;
	bLastActionSucceeded = bOk;
	RebuildCenter();
}

void UWLGovernmentWidget::OnCloseClicked()
{
	if (AWLCampaignPlayerController* PC = GetOwningPlayer<AWLCampaignPlayerController>())
	{
		PC->SetGovernmentWindowOpen(false);
	}
}

void UWLGovernmentWidget::OnTaxDown() { AdjustTaxRate(-5); }
void UWLGovernmentWidget::OnTaxUp()   { AdjustTaxRate(+5); }

void UWLGovernmentWidget::AdjustTaxRate(int32 DeltaPercent)
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	const FString Iso = PlayerIso();
	if (!Tick || Iso.IsEmpty())
	{
		return;
	}
	Tick->SetTaxRate(Iso, Tick->GetTaxRate(Iso) + DeltaPercent);
	RebuildCenter();   // refresca tasa, recaudacion y balance mensual en la misma pestana
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
		S.Controlled.Add(P);
	}
	// FE1.3: ingreso y gasto desde el presupuesto por categorias (incluye militar, salarios y social).
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);
	S.MonthlyIncome = Budget.TotalIncome();
	S.MonthlyUpkeep = Budget.TotalSpending();
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

UWLCharacterSubsystem* UWLGovernmentWidget::GetCharacters() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLCharacterSubsystem>() : nullptr;
}

UWLPoliticalSubsystem* UWLGovernmentWidget::GetPolitical() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLPoliticalSubsystem>() : nullptr;
}

FString UWLGovernmentWidget::FindPlayerSpyId() const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return FString();
	}
	for (const FWLCharacter& Spy : Characters->GetCharactersByRole(PlayerIso(), EWLCharacterRole::Spy))
	{
		if (Spy.bActive)
		{
			return Spy.Id;
		}
	}
	return FString();
}
