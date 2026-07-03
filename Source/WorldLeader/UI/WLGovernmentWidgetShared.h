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
#include "UI/WLGovIcons.h"
#include "UI/WLGovAssets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace WLGovUI
{
	// Paleta de ALTO CONTRASTE: base carbon-pizarra casi negra, acento dorado nitido y texto blanco.
	// Antes era un marron monocromo lavado (bajo contraste) que hacia que todo pareciese "de debug".
	inline const FLinearColor GovFrame       (0.68f, 0.54f, 0.22f, 1.00f);   // marco dorado
	inline const FLinearColor GovBackdrop     (0.006f, 0.008f, 0.012f, 0.82f);
	inline const FLinearColor GovPanel        (0.030f, 0.036f, 0.048f, 0.995f); // pizarra casi negra
	inline const FLinearColor GovPanelSoft    (0.050f, 0.058f, 0.074f, 1.00f);
	inline const FLinearColor GovHeaderStrip  (0.058f, 0.068f, 0.090f, 1.00f);  // franja pizarra limpia (antes gold-brown muddy)
	inline const FLinearColor GovCard         (0.082f, 0.092f, 0.116f, 1.00f);  // tarjeta pizarra
	inline const FLinearColor GovCardAlt      (0.102f, 0.114f, 0.142f, 1.00f);
	inline const FLinearColor GovCardEdge     (0.22f, 0.25f, 0.31f, 1.00f);     // borde de tarjeta (contraste)
	inline const FLinearColor GovFuture       (0.070f, 0.078f, 0.098f, 1.00f);
	inline const FLinearColor GovGold         (1.00f, 0.82f, 0.32f, 1.00f);
	inline const FLinearColor GovGoldDim      (0.74f, 0.58f, 0.22f, 1.00f);
	inline const FLinearColor GovText         (0.94f, 0.96f, 0.99f, 1.00f);     // casi blanco
	inline const FLinearColor GovMuted        (0.58f, 0.63f, 0.72f, 1.00f);     // gris frio
	inline const FLinearColor GovGood         (0.42f, 0.87f, 0.55f, 1.00f);
	inline const FLinearColor GovBad          (0.98f, 0.46f, 0.42f, 1.00f);
	inline const FLinearColor GovDarkInk      (0.04f, 0.045f, 0.06f, 1.00f);
	inline const FLinearColor GovTabIdle      (0.095f, 0.106f, 0.132f, 1.00f);
	inline const FLinearColor GovDanger       (0.44f, 0.13f, 0.12f, 1.00f);   // fondo de acciones destructivas
	inline const FLinearColor GovConfirm      (0.82f, 0.32f, 0.12f, 1.00f);   // boton en espera de confirmacion
	inline const FLinearColor GovBarTrack     (0.035f, 0.040f, 0.052f, 1.00f);

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

	/**
	 * Tarjeta con ESQUINAS REDONDEADAS y BORDE (brush redondeado de Slate). Da profundidad y
	 * definicion que un rectangulo relleno plano no tiene — es lo que separa "UI de programador"
	 * de una tarjeta de juego. Firma compatible con MakeBorder para poder intercambiarlas.
	 */
	inline UBorder* MakeCard(UWidgetTree* Tree, const FLinearColor& Fill, const FMargin& Pad,
		float Radius = 7.f, const FLinearColor& Outline = GovCardEdge, float OutlineWidth = 1.2f)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth));
		B->SetPadding(Pad);
		return B;
	}

	/** Panel/superficie redondeada SIN borde visible (para fondos suaves). */
	inline UBorder* MakeRoundedSurface(UWidgetTree* Tree, const FLinearColor& Fill, const FMargin& Pad, float Radius = 10.f)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrush(FSlateRoundedBoxBrush(Fill, Radius));
		B->SetPadding(Pad);
		return B;
	}

	/**
	 * Da al boton un estilo de PILDORA REDONDEADA con borde y feedback de hover/pressed. Los brushes
	 * base son blancos: SetBackgroundColor(Bg) los tinta, asi que el boton sale del color deseado pero
	 * redondeado. Sin esto los botones son rectangulos planos (sello de "UI de programador").
	 */
	inline void StyleRoundedButton(UButton* Button, float Radius = 5.f)
	{
		FButtonStyle Style;
		const FLinearColor Edge(0.30f, 0.34f, 0.42f, 0.85f);
		Style.Normal   = FSlateRoundedBoxBrush(FLinearColor::White, Radius, Edge, 1.0f);
		Style.Hovered  = FSlateRoundedBoxBrush(FLinearColor(1.18f, 1.18f, 1.18f, 1.f), Radius, GovGold, 1.4f);
		Style.Pressed  = FSlateRoundedBoxBrush(FLinearColor(0.78f, 0.78f, 0.78f, 1.f), Radius, Edge, 1.0f);
		Style.Disabled = FSlateRoundedBoxBrush(FLinearColor(0.55f, 0.55f, 0.55f, 1.f), Radius);
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		Button->SetStyle(Style);
	}

	/**
	 * Emblema de nacion: BANDERA real (UI/Flags/<ISO>.png cargada en runtime) si el usuario la dejo;
	 * si no, un chip de color redondeado con el ISO. Asi el juego usa arte real en cuanto exista.
	 */
	inline UWidget* MakeFlag(UWidgetTree* Tree, const FString& Iso, const FLinearColor& FallbackColor,
		float W, float H, const FString& FallbackLabel = FString())
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(W);
		Box->SetHeightOverride(H);
		if (UTexture2D* Flag = WLGovAssetsNS::GetFlag(Iso))
		{
			UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
			Img->SetBrushFromTexture(Flag, false);
			Box->SetContent(Img);
		}
		else
		{
			UBorder* Chip = MakeRoundedSurface(Tree, FallbackColor, FMargin(0.f), 4.f);
			Chip->SetHorizontalAlignment(HAlign_Center);
			Chip->SetVerticalAlignment(VAlign_Center);
			if (!FallbackLabel.IsEmpty())
			{
				Chip->SetContent(MakeText(Tree, FallbackLabel, FMath::Max(9, static_cast<int32>(H * 0.35f)), GovDarkInk, ETextJustify::Center));
			}
			Box->SetContent(Chip);
		}
		return Box;
	}

	/** Fondo de panel TEXTURIZADO (gradiente+viñeta runtime, o UI/gov_panel_bg.png). Profundidad vs relleno plano. */
	inline UBorder* MakeTexturedPanel(UWidgetTree* Tree, const FMargin& Pad)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		if (UTexture2D* Bg = WLGovAssetsNS::GetPanelBackground())
		{
			B->SetBrushFromTexture(Bg);
		}
		else
		{
			B->SetBrushColor(GovPanel);
		}
		B->SetPadding(Pad);
		return B;
	}

	inline uint32 PortraitSeedHash(const FString& Seed)
	{
		uint32 H = 2166136261u;
		for (const TCHAR C : Seed)
		{
			H ^= static_cast<uint32>(C);
			H *= 16777619u;
		}
		return H;
	}

	inline UTexture2D* LoadPortraitFromPool(const FString& Seed, const TCHAR* Prefix, int32 PoolSize)
	{
		if (PoolSize <= 0)
		{
			return nullptr;
		}
		const int32 PoolIndex = static_cast<int32>(PortraitSeedHash(Seed) % PoolSize) + 1;
		return WLGovAssetsNS::LoadExternalTexture(FString::Printf(TEXT("UI/Portraits/%s_%02d.png"), Prefix, PoolIndex));
	}

	inline UTexture2D* LoadPortraitForSeed(const FString& Seed)
	{
		if (UTexture2D* Exact = WLGovAssetsNS::LoadExternalTexture(FString::Printf(TEXT("UI/Portraits/%s.png"), *Seed)))
		{
			return Exact;
		}

		// Personajes dinamicos tienen IDs por rol (US-LEADER-GEN01, CO-MIN-ECO-GEN02, etc.).
		// Si no existe retrato exacto, rotan por pools genericos estables.
		if (Seed.Contains(TEXT("-LEADER-"), ESearchCase::IgnoreCase))
		{
			return LoadPortraitFromPool(Seed, TEXT("leader"), 20);
		}
		if (Seed.Contains(TEXT("-MIN-"), ESearchCase::IgnoreCase))
		{
			return LoadPortraitFromPool(Seed, TEXT("minister"), 150);
		}
		if (Seed.Contains(TEXT("-GEN-"), ESearchCase::IgnoreCase))
		{
			return LoadPortraitFromPool(Seed, TEXT("general"), 19);
		}
		if (Seed.Contains(TEXT("-OPP-"), ESearchCase::IgnoreCase))
		{
			return LoadPortraitFromPool(Seed, TEXT("opposition"), 15);
		}
		if (Seed.Contains(TEXT("-SPY-"), ESearchCase::IgnoreCase))
		{
			return LoadPortraitFromPool(Seed, TEXT("spy"), 5);
		}

		return nullptr;
	}

	/**
	 * Retrato de personaje ENMARCADO: UI/Portraits/<Seed>.png si existe; si no, pools por rol
	 * (leader_01..20, minister_01..150, general_01..19, opposition_01..15, spy_01..05);
	 * si no, un busto estilizado
	 * generado en runtime (cara+pelo+hombros con el color de la cartera). Marco redondeado con acento.
	 */
	inline UWidget* MakePortrait(UWidgetTree* Tree, const FString& Seed, const FLinearColor& Accent, float W, float H)
	{
		UBorder* Frame = MakeCard(Tree, GovDarkInk, FMargin(2.f), 6.f, Accent * 0.7f + FLinearColor(0.10f, 0.11f, 0.13f), 1.4f);
		UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		UTexture2D* Tex = LoadPortraitForSeed(Seed);
		if (!Tex)
		{
			Tex = WLGovIconsNS::GetPortraitTexture(Seed, Accent, static_cast<int32>(W), static_cast<int32>(H));
		}
		if (Tex)
		{
			Img->SetBrushFromTexture(Tex, false);
		}
		Frame->SetContent(Img);
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(W);
		Box->SetHeightOverride(H);
		Box->SetContent(Frame);
		return Box;
	}

	/** Icono vectorial generado en runtime (moneda, poblacion, escudo...) mostrado a SizePx y tintado. */
	inline UImage* MakeIcon(UWidgetTree* Tree, EWLGovIcon Icon, int32 SizePx, const FLinearColor& Color)
	{
		UImage* Img = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UTexture2D* Tex = WLGovIconsNS::GetIconTexture(Icon, SizePx, Color))
		{
			Img->SetBrushFromTexture(Tex, false);
		}
		Img->SetDesiredSizeOverride(FVector2D(SizePx, SizePx));
		return Img;
	}

	// Tarjeta de metrica: etiqueta pequena + valor grande. Rellena la celda del grid.
	inline UBorder* MakeMetricCard(UWidgetTree* Tree, const FString& Label, const FString& Value, const FLinearColor& ValueColor)
	{
		UBorder* Card = MakeCard(Tree, GovCard, FMargin(14.f, 11.f));
		UVerticalBox* VB = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(Tree, Label.ToUpper(), 12, GovMuted));
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(Tree, Value, 27, ValueColor)))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		Card->SetContent(VB);
		return Card;
	}

	// Tarjeta de metrica con icono: barra de acento de color + icono en badge + etiqueta/valor.
	// Reemplaza a las tarjetas planas de solo texto (etiqueta minuscula + numero flotando en vacio).
	inline UBorder* MakeMetricCardIcon(UWidgetTree* Tree, EWLGovIcon Icon, const FLinearColor& Accent,
		const FString& Label, const FString& Value, const FLinearColor& ValueColor)
	{
		UBorder* Card = MakeCard(Tree, GovCard, FMargin(0.f));
		UHorizontalBox* HB = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// Barra de acento a la izquierda (color de categoria), redondeada como el resto de cantos.
		USizeBox* Bar = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Bar->SetWidthOverride(4.f);
		Bar->SetContent(MakeRoundedSurface(Tree, Accent, FMargin(0.f), 2.f));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Bar))
		{
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(4.f, 5.f, 0.f, 5.f));
		}

		// Icono en chip translucido de su color de acento (profundidad sin ruido).
		UBorder* IconChip = MakeRoundedSurface(Tree,
			FLinearColor(Accent.R, Accent.G, Accent.B, 0.14f), FMargin(8.f), 9.f);
		IconChip->SetVerticalAlignment(VAlign_Center);
		IconChip->SetHorizontalAlignment(HAlign_Center);
		IconChip->SetContent(MakeIcon(Tree, Icon, 26, Accent));
		UBorder* IconPad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(10.f, 9.f, 6.f, 9.f));
		IconPad->SetVerticalAlignment(VAlign_Center);
		IconPad->SetContent(IconChip);
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(IconPad)) { S->SetVerticalAlignment(VAlign_Center); }

		// Etiqueta + valor.
		UVerticalBox* VB = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		VB->AddChildToVerticalBox(MakeText(Tree, Label.ToUpper(), 11, GovMuted));
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeText(Tree, Value, 23, ValueColor)))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		UBorder* TextPad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(2.f, 11.f, 12.f, 11.f));
		TextPad->SetContent(VB);
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(TextPad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Card->SetContent(HB);
		return Card;
	}

	// Tarjeta de lista lateral (ministerio / potencia): titulo + subtitulo + estado a la derecha.
	inline UBorder* MakeListCard(UWidgetTree* Tree, const FLinearColor& Accent, const FString& Title, const FString& Sub,
		const FString& Status, const FLinearColor& StatusColor)
	{
		UBorder* Card = MakeCard(Tree, GovCard, FMargin(0.f));
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
		UBorder* Tag = MakeCard(Tree, GovCardAlt, FMargin(11.f, 5.f));
		Tag->SetContent(MakeText(Tree, S, 13, GovText));
		return Tag;
	}

	/** Icono de seccion derivado del titulo por palabra clave (ancla visual, no otro muro de texto). */
	inline EWLGovIcon SectionIconFor(const FString& Title)
	{
		const FString T = Title.ToUpper();
		auto Has = [&T](const TCHAR* K) { return T.Contains(K); };
		if (Has(TEXT("PRESUPUESTO")) || Has(TEXT("MERCADO")) || Has(TEXT("COMERCIO")) || Has(TEXT("INGRES")) || Has(TEXT("GASTO"))) return EWLGovIcon::Balance;
		if (Has(TEXT("FINANZAS")) || Has(TEXT("IMPUEST")) || Has(TEXT("DEUDA"))) return EWLGovIcon::Treasury;
		if (Has(TEXT("GRUPOS")) || Has(TEXT("POBLAC")) || Has(TEXT("SOCIAL"))) return EWLGovIcon::Population;
		if (Has(TEXT("TERRITORIO")) || Has(TEXT("PROVINCIA")) || Has(TEXT("REGION"))) return EWLGovIcon::Provinces;
		if (Has(TEXT("PODER")) || Has(TEXT("ORDEN")) || Has(TEXT("CAPACIDAD")) || Has(TEXT("GOBERNANZA"))) return EWLGovIcon::Order;
		if (Has(TEXT("DIPLOMAC")) || Has(TEXT("PANORAMA")) || Has(TEXT("EXTERIOR"))) return EWLGovIcon::Diplomacy;
		if (Has(TEXT("ALTO MANDO")) || Has(TEXT("GABINETE")) || Has(TEXT("GENERAL")) || Has(TEXT("EJERCITO")) || Has(TEXT("MILITAR")) || Has(TEXT("BATALLA"))) return EWLGovIcon::Military;
		if (Has(TEXT("ELECCION")) || Has(TEXT("APROBAC")) || Has(TEXT("PERFILES"))) return EWLGovIcon::Approval;
		if (Has(TEXT("CRISIS")) || Has(TEXT("MEMORIA")) || Has(TEXT("EVENTO")) || Has(TEXT("NOTICIAS"))) return EWLGovIcon::Crisis;
		if (Has(TEXT("CONGRESO")) || Has(TEXT("LEYES")) || Has(TEXT("REFORMA")) || Has(TEXT("AGENDA")) || Has(TEXT("PATRONAZGO")) || Has(TEXT("MEDIOS"))) return EWLGovIcon::Politics;
		if (Has(TEXT("CRECIMIENTO")) || Has(TEXT("PROGRAMA"))) return EWLGovIcon::Growth;
		return EWLGovIcon::Capital;
	}

	/**
	 * Cabecera de seccion: icono + titulo en mayusculas sobre una franja oscura limpia y una linea
	 * dorada debajo. El icono da un ancla visual que separa cada bloque de un vistazo (mas intuitivo
	 * que una linea de texto suelta).
	 */
	inline UWidget* MakeSectionTitle(UWidgetTree* Tree, const FString& Title)
	{
		UVerticalBox* VB = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// Icono en chip dorado translucido: ancla visual clara de cada bloque.
		UBorder* IconChip = MakeRoundedSurface(Tree, FLinearColor(1.f, 0.82f, 0.32f, 0.13f), FMargin(7.f), 8.f);
		IconChip->SetVerticalAlignment(VAlign_Center);
		IconChip->SetHorizontalAlignment(HAlign_Center);
		IconChip->SetContent(MakeIcon(Tree, SectionIconFor(Title), 19, GovGold));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconChip))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}

		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeText(Tree, Title.ToUpper(), 16, GovGold)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		VB->AddChildToVerticalBox(Row);

		// Divisor asimetrico: tramo dorado corto + resto en linea tenue (menos "tabla", mas diseno).
		UHorizontalBox* Divider = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* GoldSeg = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		GoldSeg->SetWidthOverride(86.f);
		GoldSeg->SetHeightOverride(3.f);
		GoldSeg->SetContent(MakeRoundedSurface(Tree, GovGold, FMargin(0.f), 1.5f));
		if (UHorizontalBoxSlot* S = Divider->AddChildToHorizontalBox(GoldSeg)) { S->SetVerticalAlignment(VAlign_Center); }
		USizeBox* RestSeg = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RestSeg->SetHeightOverride(1.f);
		RestSeg->SetContent(MakeRoundedSurface(Tree, FLinearColor(0.22f, 0.25f, 0.31f, 0.55f), FMargin(0.f), 0.5f));
		if (UHorizontalBoxSlot* S = Divider->AddChildToHorizontalBox(RestSeg))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(Divider))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		return VB;
	}

	/** Insignia compacta de estado (riesgo, rol, area...) con color propio. Pildora redondeada. */
	inline UBorder* MakeBadge(UWidgetTree* Tree, const FString& S, const FLinearColor& Bg,
		const FLinearColor& TextColor = FLinearColor(0.94f, 0.93f, 0.88f, 1.f))
	{
		UBorder* Badge = MakeRoundedSurface(Tree, Bg, FMargin(9.f, 3.f), 9.f);
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
		// Pildora redondeada: track oscuro con borde sutil y relleno redondeado del color del valor.
		UBorder* Track = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Track->SetBrush(FSlateRoundedBoxBrush(GovBarTrack, Height * 0.5f,
			FLinearColor(0.16f, 0.18f, 0.23f, 1.f), 1.f));
		Track->SetPadding(FMargin(1.5f));
		USizeBox* TrackBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TrackBox->SetHeightOverride(Height);
		UHorizontalBox* FillRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* Fill = MakeRoundedSurface(Tree, FillColor, FMargin(0.f), (Height - 3.f) * 0.5f);
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
		// Canto de color a la izquierda (el estado de la metrica se ve antes de leerla).
		UBorder* Row = MakeCard(Tree, RowColor, FMargin(0.f));
		UHorizontalBox* Outer = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* Edge = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Edge->SetWidthOverride(4.f);
		Edge->SetContent(MakeRoundedSurface(Tree, ValueColor, FMargin(0.f), 2.f));
		if (UHorizontalBoxSlot* S = Outer->AddChildToHorizontalBox(Edge))
		{
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(4.f, 5.f, 0.f, 5.f));
		}

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
		// Valor grande + "/100" pequeno y apagado: jerarquia tipografica, no una fraccion plana.
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(
			MakeText(Tree, FString::Printf(TEXT("%d"), Value), 16, ValueColor, ETextJustify::Right)))
		{
			S->SetVerticalAlignment(VAlign_Bottom);
		}
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(MakeText(Tree, TEXT("/100"), 10, GovMuted)))
		{
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(2.f, 0.f, 0.f, 2.f));
		}
		VB->AddChildToVerticalBox(Head);
		if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(MakeBar(Tree, Value / 100.f, ValueColor, 9.f)))
		{
			S->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
		}
		UBorder* Pad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(9.f, 7.f, 12.f, 7.f));
		Pad->SetContent(VB);
		if (UHorizontalBoxSlot* S = Outer->AddChildToHorizontalBox(Pad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		Row->SetContent(Outer);
		return Row;
	}

	/** Franja de alerta (crisis, secesion, ruptura de coalicion...). */
	inline UBorder* MakeAlert(UWidgetTree* Tree, const FString& S, const FLinearColor& Accent)
	{
		UBorder* Strip = MakeCard(Tree, GovCard, FMargin(0.f));
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
		float MinWidth = 0.f, int32 FontSize = 12, bool bEnabled = true)
	{
		const bool bPending = Owner && Owner->IsPendingConfirm(ActionId);
		UWLGovActionButton* Button = Tree->ConstructWidget<UWLGovActionButton>(UWLGovActionButton::StaticClass());
		StyleRoundedButton(Button, 5.f);
		Button->SetBackgroundColor(bEnabled ? (bPending ? GovConfirm : Bg) : GovMuted);
		Button->SetIsEnabled(bEnabled);
		if (bEnabled)
		{
			Button->BindAction(Owner, ActionId);
		}
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
		UBorder* Row = MakeCard(Tree, RowColor, FMargin(0.f));
		UHorizontalBox* HB = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		// Canto de color a la izquierda, como en los medidores: una sola familia visual.
		USizeBox* Edge = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Edge->SetWidthOverride(4.f);
		Edge->SetContent(MakeRoundedSurface(Tree, ValueColor, FMargin(0.f), 2.f));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(Edge))
		{
			S->SetVerticalAlignment(VAlign_Fill);
			S->SetPadding(FMargin(4.f, 5.f, 0.f, 5.f));
		}
		UBorder* LabelPad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(9.f, 8.f, 0.f, 8.f));
		LabelPad->SetContent(MakeText(Tree, Label, 13, GovText));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(LabelPad))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UBorder* ValuePad = MakeBorder(Tree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(0.f, 8.f, 12.f, 8.f));
		ValuePad->SetContent(MakeText(Tree, Value, 14, ValueColor, ETextJustify::Right));
		if (UHorizontalBoxSlot* S = HB->AddChildToHorizontalBox(ValuePad))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Row->SetContent(HB);
		return Row;
	}
}
