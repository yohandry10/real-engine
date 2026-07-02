// Copyright World Leader project. See ROADMAP.md.
//
// Paleta y fabrica de widgets de la ventana GOBIERNO, compartidas entre
// WLGovernmentWidget.cpp (shell + tabs clasicos) y
// WLGovernmentWidgetGovernance.cpp (Gobierno P1/P2). Antes vivian en un
// namespace anonimo del cpp principal; al partir el widget en dos TUs se
// movieron aqui como inline.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLCharacterTypes.h"
#include "Core/WLPoliticalTypes.h"
#include "UI/WLGovernmentWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace WLGovUI
{
	// Paleta calida oscura con acentos dorados (mas cercana al panel de faccion de Total War
	// que el teal frio del overlay anterior).
	inline const FLinearColor GovFrame       (0.62f, 0.48f, 0.18f, 1.00f);   // marco dorado
	inline const FLinearColor GovBackdrop     (0.010f, 0.012f, 0.014f, 0.72f);
	inline const FLinearColor GovPanel        (0.070f, 0.066f, 0.056f, 0.99f);
	inline const FLinearColor GovPanelSoft    (0.098f, 0.092f, 0.078f, 1.00f);
	inline const FLinearColor GovHeaderStrip  (0.140f, 0.115f, 0.060f, 1.00f);
	inline const FLinearColor GovCard         (0.125f, 0.118f, 0.100f, 1.00f);
	inline const FLinearColor GovCardAlt      (0.150f, 0.140f, 0.115f, 1.00f);
	inline const FLinearColor GovFuture       (0.085f, 0.082f, 0.075f, 1.00f);
	inline const FLinearColor GovGold         (1.00f, 0.84f, 0.34f, 1.00f);
	inline const FLinearColor GovGoldDim      (0.72f, 0.58f, 0.24f, 1.00f);
	inline const FLinearColor GovText         (0.94f, 0.93f, 0.88f, 1.00f);
	inline const FLinearColor GovMuted        (0.64f, 0.62f, 0.55f, 1.00f);
	inline const FLinearColor GovGood         (0.55f, 0.86f, 0.52f, 1.00f);
	inline const FLinearColor GovBad          (0.94f, 0.52f, 0.42f, 1.00f);
	inline const FLinearColor GovDarkInk      (0.06f, 0.05f, 0.03f, 1.00f);
	inline const FLinearColor GovTabIdle      (0.14f, 0.135f, 0.115f, 1.00f);
	inline const FLinearColor GovDanger       (0.40f, 0.12f, 0.10f, 1.00f);   // fondo de acciones destructivas
	inline const FLinearColor GovConfirm      (0.78f, 0.30f, 0.12f, 1.00f);   // boton en espera de confirmacion
	inline const FLinearColor GovBarTrack     (0.04f, 0.045f, 0.05f, 1.00f);

	inline FString GovGroupThousands(int64 Value)
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

	/** Color de riesgo: valores ALTOS son malos (riesgo, presion, corrupcion...). */
	inline FLinearColor RiskColor(int32 Value, int32 WarnAt = 30, int32 DangerAt = 60)
	{
		return Value >= DangerAt ? GovBad : (Value >= WarnAt ? GovGold : GovGood);
	}

	/** Color de apoyo: valores BAJOS son malos (apoyo, lealtad, legitimidad...). */
	inline FLinearColor SupportColor(int32 Value, int32 WarnAt = 60, int32 DangerAt = 35)
	{
		return Value >= WarnAt ? GovGood : (Value >= DangerAt ? GovGold : GovBad);
	}

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FString& S, int32 Size, const FLinearColor& C,
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

	inline UBorder* MakeBorder(UWidgetTree* Tree, const FLinearColor& C, const FMargin& Pad)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(C);
		B->SetPadding(Pad);
		return B;
	}

	// Tarjeta de metrica: etiqueta pequena + valor grande. Rellena la celda del grid.
	inline UBorder* MakeMetricCard(UWidgetTree* Tree, const FString& Label, const FString& Value, const FLinearColor& ValueColor)
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
	inline UBorder* MakeListCard(UWidgetTree* Tree, const FLinearColor& Accent, const FString& Title, const FString& Sub,
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

	inline UBorder* MakeTag(UWidgetTree* Tree, const FString& S)
	{
		UBorder* Tag = MakeBorder(Tree, GovCardAlt, FMargin(11.f, 5.f));
		Tag->SetContent(MakeText(Tree, S, 13, GovText));
		return Tag;
	}

	/** Insignia compacta de estado (riesgo, rol, area...) con color propio. */
	inline UBorder* MakeBadge(UWidgetTree* Tree, const FString& S, const FLinearColor& Bg,
		const FLinearColor& TextColor = FLinearColor(0.94f, 0.93f, 0.88f, 1.f))
	{
		UBorder* Badge = MakeBorder(Tree, Bg, FMargin(8.f, 3.f));
		Badge->SetContent(MakeText(Tree, S, 10, TextColor, ETextJustify::Center));
		return Badge;
	}

	inline void AddColumnChild(UVerticalBox* Box, UWidget* Child, float TopPad)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Child))
		{
			S->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		}
	}

	/**
	 * Barra horizontal de progreso/riesgo 0..1 (track oscuro + relleno proporcional).
	 * Mismo truco que orden publico: ESlateSizeRule::Fill con el peso en Value.
	 */
	inline UWidget* MakeBar(UWidgetTree* Tree, float Frac, const FLinearColor& FillColor, float Height = 10.f)
	{
		Frac = FMath::Clamp(Frac, 0.f, 1.f);
		UBorder* Track = MakeBorder(Tree, GovBarTrack, FMargin(0.f));
		USizeBox* TrackBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TrackBox->SetHeightOverride(Height);
		UHorizontalBox* FillRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* Fill = MakeBorder(Tree, FillColor, FMargin(0.f));
		if (UHorizontalBoxSlot* S = FillRow->AddChildToHorizontalBox(Fill))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(Frac, 0.001f);
			S->SetSize(Size);
		}
		UBorder* Rest = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(0.f));
		if (UHorizontalBoxSlot* S = FillRow->AddChildToHorizontalBox(Rest))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(1.f - Frac, 0.001f);
			S->SetSize(Size);
		}
		TrackBox->SetContent(FillRow);
		Track->SetContent(TrackBox);
		return Track;
	}

	/** Fila "Etiqueta  V/100" + barra debajo. La lectura rapida de todos los stats 0-100. */
	inline UBorder* MakeGaugeRow(UWidgetTree* Tree, const FString& Label, int32 Value,
		const FLinearColor& ValueColor, const FLinearColor& RowColor, const FString& ToolTip = FString())
	{
		UBorder* Row = MakeBorder(Tree, RowColor, FMargin(12.f, 7.f));
		UVerticalBox* VB = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Head = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* LabelText = MakeText(Tree, Label, 13, GovText);
		if (!ToolTip.IsEmpty())
		{
			LabelText->SetToolTipText(FText::FromString(ToolTip));
		}
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(LabelText))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Head->AddChildToHorizontalBox(MakeText(Tree, FString::Printf(TEXT("%d / 100"), Value), 13, ValueColor, ETextJustify::Right));
		VB->AddChildToVerticalBox(Head);
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeBar(Tree, Value / 100.f, ValueColor, 8.f)))
		{
			S->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
		}
		Row->SetContent(VB);
		return Row;
	}

	/** Franja de alerta (crisis, secesion, ruptura de coalicion...). */
	inline UBorder* MakeAlert(UWidgetTree* Tree, const FString& S, const FLinearColor& Accent)
	{
		UBorder* Strip = MakeBorder(Tree, GovCard, FMargin(0.f));
		UHorizontalBox* HB = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* Bar = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Bar->SetWidthOverride(4.f);
		Bar->SetContent(MakeBorder(Tree, Accent, FMargin(0.f)));
		if (UHorizontalBoxSlot* SlotPtr = HB->AddChildToHorizontalBox(Bar))
		{
			SlotPtr->SetVerticalAlignment(VAlign_Fill);
		}
		UBorder* Pad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(10.f, 7.f));
		Pad->SetContent(MakeText(Tree, S, 12, Accent, ETextJustify::Left, true));
		if (UHorizontalBoxSlot* SlotPtr = HB->AddChildToHorizontalBox(Pad))
		{
			SlotPtr->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SlotPtr->SetVerticalAlignment(VAlign_Center);
		}
		Strip->SetContent(HB);
		return Strip;
	}

	inline FString RankToText(EWLMilitaryRank Rank)
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

	inline FString DiplomaticStatusToText(EWLDiplomaticStatus Status)
	{
		switch (Status)
		{
		case EWLDiplomaticStatus::War:     return TEXT("GUERRA");
		case EWLDiplomaticStatus::Tension: return TEXT("Tension");
		default:                           return TEXT("Paz");
		}
	}

	inline FString TreatyToText(EWLTreatyType Treaty)
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

	// Boton de accion pequeno con payload, enlazado al dispatcher del widget. Si el widget tiene
	// esta accion pendiente de confirmar, el boton se pinta naranja y pide el segundo clic.
	inline UWLGovActionButton* MakeActionButton(UWidgetTree* Tree, UWLGovernmentWidget* Owner,
		const FString& ActionId, const FString& Label, const FLinearColor& Bg,
		float MinWidth = 0.f, int32 FontSize = 12)
	{
		const bool bPending = Owner && Owner->IsPendingConfirm(ActionId);
		UWLGovActionButton* Button = Tree->ConstructWidget<UWLGovActionButton>(UWLGovActionButton::StaticClass());
		Button->SetBackgroundColor(bPending ? GovConfirm : Bg);
		Button->BindAction(Owner, ActionId);
		UBorder* Pad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(9.f, 5.f));
		Pad->SetContent(MakeText(Tree, bPending ? TEXT("CONFIRMAR?") : Label, FontSize, GovText, ETextJustify::Center));
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
	inline UBorder* MakeStatRow(UWidgetTree* Tree, const FString& Label, const FString& Value,
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
