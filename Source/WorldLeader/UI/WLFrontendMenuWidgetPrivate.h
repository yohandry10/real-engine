// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace WLFrontendMenu
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

	inline FSlateFontInfo Font(float Size, bool bBold = false)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::RoundToInt(Size));
		Info.LetterSpacing = 0;
		return Info;
	}

	inline UTextBlock* MakeFrontendText(UWidgetTree* Tree, const FString& Text, float Size, const FLinearColor& Color, bool bBold = false)
	{
		UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetFont(Font(Size, bBold));
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	inline void AddVBoxChild(UVerticalBox* Box, UWidget* Child, float TopPadding = 0.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.f, TopPadding, 0.f, 0.f));
		}
	}

	inline void AddCanvasChild(UCanvasPanel* Root, UWidget* Child, const FAnchors& Anchors, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment)
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

	inline UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(Color);
		Panel->SetPadding(Padding);
		return Panel;
	}

	inline USizeBox* MakeFixedHeight(UWidgetTree* Tree, UWidget* Child, float Height, float MinWidth = 0.f)
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

	inline FSlateBrush TintedBrush(const FSlateBrush& Source, const FLinearColor& Color)
	{
		FSlateBrush Brush = Source;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	inline FButtonStyle MakeButtonStyle(bool bPrimary, bool bEnabled)
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
