// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLEventModalWidget.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLCampaignPlayerController.h"
#include "Core/WLPoliticalTypes.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include "UI/WLGovernmentWidgetShared.h"

using namespace WLGovUI;

namespace
{
	// Una opcion es "sensible" (pide confirmacion) si mueve mucho tesoro/capital, hunde el orden
	// publico o dispara un shock de mercado: decisiones que el jugador no deberia soltar de un clic.
	bool OptionIsSensitive(const FWLPoliticalEventOption& Option)
	{
		return FMath::Abs(Option.PoliticalCapitalDelta) >= 10
			|| FMath::Abs(Option.TreasuryDelta) >= 5000
			|| Option.PublicOrderDelta <= -8
			|| Option.OppositionDelta >= 8
			|| !Option.MarketShockGoodId.IsEmpty();
	}
}

void UWLEventOptionButton::BindOption(UWLEventModalWidget* InOwner, const FString& InInstanceId, const FString& InOptionId)
{
	Owner = InOwner;
	InstanceId = InInstanceId;
	OptionId = InOptionId;
	OnClicked.AddDynamic(this, &UWLEventOptionButton::HandleClicked);
}

void UWLEventOptionButton::HandleClicked()
{
	if (Owner)
	{
		Owner->ChooseOption(InstanceId, OptionId);
	}
}

TSharedRef<SWidget> UWLEventModalWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildShell();
	}
	return Super::RebuildWidget();
}

void UWLEventModalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

FReply UWLEventModalWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape pospone (no resuelve): el evento sigue en la cola, accesible en GOBIERNO > POLITICA.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnPostpone();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UWLEventModalWidget::BuildShell()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EventRoot"));
	WidgetTree->RootWidget = Root;

	// Fondo modal atenuado (bloquea el mapa detras: un evento exige decision).
	UBorder* Dim = MakeBorder(WidgetTree, FLinearColor(0.006f, 0.008f, 0.010f, 0.82f), FMargin(0.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// Marco dorado + panel, centrado. Ancho contenido para leerse como "carta de evento".
	UBorder* Frame = MakeBorder(WidgetTree, GovFrame, FMargin(2.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Frame))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D(0.f, 0.f));
		S->SetSize(FVector2D(720.f, 620.f));
	}
	UBorder* Panel = MakeBorder(WidgetTree, GovPanel, FMargin(18.f));
	Frame->SetContent(Panel);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(ContentBox);
	Panel->SetContent(Scroll);

	Rebuild();
}

void UWLEventModalWidget::Rebuild()
{
	if (!ContentBox)
	{
		return;
	}
	ContentBox->ClearChildren();

	UWLPoliticalSubsystem* Political = GetPolitical();
	const FString Iso = PlayerIso();

	// Primer evento sin resolver de la cola del jugador.
	const FWLPoliticalEventInstance* Current = nullptr;
	TArray<FWLPoliticalEventInstance> Events;
	int32 PendingCount = 0;
	if (Political && !Iso.IsEmpty())
	{
		Events = Political->GetQueuedEvents(Iso);
		for (const FWLPoliticalEventInstance& Event : Events)
		{
			if (!Event.bResolved)
			{
				if (!Current)
				{
					Current = &Event;
				}
				++PendingCount;
			}
		}
	}

	// Cabecera: contador de eventos pendientes + boton posponer.
	{
		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* TitleVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TitleVB->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("DECISION DE GOBIERNO"), 13, GovGold));
		TitleVB->AddChildToVerticalBox(MakeText(WidgetTree,
			PendingCount > 1
				? FString::Printf(TEXT("%d eventos esperan tu decision"), PendingCount)
				: TEXT("Un evento espera tu decision"),
			22, GovText));
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(TitleVB))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UWLEventOptionButton* Postpone = WidgetTree->ConstructWidget<UWLEventOptionButton>(UWLEventOptionButton::StaticClass());
		Postpone->SetBackgroundColor(GovTabIdle);
		Postpone->OnClicked.AddDynamic(this, &UWLEventModalWidget::OnPostpone);
		UBorder* PostponePad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(12.f, 7.f));
		PostponePad->SetContent(MakeText(WidgetTree, TEXT("POSPONER  [Esc]"), 12, GovText, ETextJustify::Center));
		Postpone->SetContent(PostponePad);
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(Postpone))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		AddColumnChild(ContentBox, Head, 0.f);
	}

	if (!LastResolutionMessage.IsEmpty())
	{
		AddColumnChild(ContentBox, MakeAlert(WidgetTree, LastResolutionMessage, GovGoldDim), 10.f);
	}

	if (!Current)
	{
		AddColumnChild(ContentBox, MakeText(WidgetTree,
			TEXT("No quedan eventos pendientes. Puedes cerrar con [Esc]."), 14, GovMuted, ETextJustify::Left, true), 16.f);
		return;
	}

	// Carta del evento actual.
	UBorder* Card = MakeBorder(WidgetTree, GovCard, FMargin(16.f, 14.f));
	UVerticalBox* CVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CVB->AddChildToVerticalBox(MakeText(WidgetTree, Current->Title, 18, GovGold, ETextJustify::Left, true));
	if (UVerticalBoxSlot* S = CVB->AddChildToVerticalBox(MakeText(WidgetTree, Current->Body, 14, GovText, ETextJustify::Left, true)))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	Card->SetContent(CVB);
	AddColumnChild(ContentBox, Card, 14.f);

	AddColumnChild(ContentBox, MakeText(WidgetTree, TEXT("OPCIONES"), 13, GovMuted), 14.f);

	int32 OptionIndex = 0;
	for (const FWLPoliticalEventOption& Option : Current->Options)
	{
		const FString ConfirmKey = FString::Printf(TEXT("%s|%s"), *Current->InstanceId, *Option.OptionId);
		const bool bPending = !PendingConfirmKey.IsEmpty() && PendingConfirmKey == ConfirmKey;
		const bool bSensitive = OptionIsSensitive(Option);

		UBorder* OptCard = MakeBorder(WidgetTree, (OptionIndex % 2 == 0) ? GovCardAlt : GovCard, FMargin(12.f, 10.f));
		UVerticalBox* OVB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// Boton de opcion (ocupa el ancho, con la etiqueta a la izquierda).
		UWLEventOptionButton* Button = WidgetTree->ConstructWidget<UWLEventOptionButton>(UWLEventOptionButton::StaticClass());
		Button->SetBackgroundColor(bPending ? GovConfirm : GovGoldDim);
		Button->BindOption(this, Current->InstanceId, Option.OptionId);
		UBorder* LabelPad = MakeBorder(WidgetTree, FLinearColor(0.f, 0.f, 0.f, 0.f), FMargin(12.f, 8.f));
		LabelPad->SetContent(MakeText(WidgetTree,
			bPending ? FString::Printf(TEXT("CONFIRMAR: %s"), *Option.Label) : Option.Label,
			14, GovDarkInk, ETextJustify::Left));
		Button->SetContent(LabelPad);
		OVB->AddChildToVerticalBox(Button);

		// Desglose de impacto esperado (una linea de "chips" de coste/beneficio).
		FString Impact;
		auto Add = [&Impact](const FString& S)
		{
			if (!S.IsEmpty())
			{
				Impact += (Impact.IsEmpty() ? TEXT("") : TEXT("     ")) + S;
			}
		};
		if (Option.PoliticalCapitalDelta != 0) Add(FString::Printf(TEXT("Capital politico %+d"), Option.PoliticalCapitalDelta));
		if (Option.TreasuryDelta != 0)         Add(FString::Printf(TEXT("Tesoro %+lld"), static_cast<long long>(Option.TreasuryDelta)));
		if (Option.PublicOrderDelta != 0)      Add(FString::Printf(TEXT("Orden publico %+d"), Option.PublicOrderDelta));
		if (Option.OppositionDelta != 0)       Add(FString::Printf(TEXT("Oposicion %+d"), Option.OppositionDelta));
		if (Option.RelationDelta != 0)         Add(FString::Printf(TEXT("Relacion %+d"), Option.RelationDelta));
		if (Impact.IsEmpty())
		{
			Impact = TEXT("Sin impacto directo medible.");
		}
		if (UVerticalBoxSlot* S = OVB->AddChildToVerticalBox(MakeText(WidgetTree, Impact, 12,
			(Option.PublicOrderDelta < 0 || Option.OppositionDelta > 0) ? GovBad : GovMuted, ETextJustify::Left, true)))
		{
			S->SetPadding(FMargin(2.f, 7.f, 0.f, 0.f));
		}

		// Riesgo/consecuencia diferida: shock de mercado.
		if (!Option.MarketShockGoodId.IsEmpty())
		{
			const FString ShockText = FString::Printf(
				TEXT("Riesgo: %s — shock de mercado x%.2f durante %d meses"),
				Option.MarketShockTitle.IsEmpty() ? TEXT("precios") : *Option.MarketShockTitle,
				Option.MarketShockPriceMultiplier, Option.MarketShockDurationMonths);
			if (UVerticalBoxSlot* S = OVB->AddChildToVerticalBox(MakeText(WidgetTree, ShockText, 11, GovBad, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(2.f, 5.f, 0.f, 0.f));
			}
		}
		if (bSensitive && !bPending)
		{
			if (UVerticalBoxSlot* S = OVB->AddChildToVerticalBox(MakeText(WidgetTree,
				TEXT("Decision de peso: pide confirmacion (segundo clic)."), 11, GovGold, ETextJustify::Left, true)))
			{
				S->SetPadding(FMargin(2.f, 5.f, 0.f, 0.f));
			}
		}

		OptCard->SetContent(OVB);
		AddColumnChild(ContentBox, OptCard, 6.f);
		++OptionIndex;
	}

	AddColumnChild(ContentBox, MakeText(WidgetTree,
		TEXT("Resolver aplica el impacto y registra la consecuencia en REGISTROS y memoria politica. POSPONER lo deja en GOBIERNO > POLITICA."),
		11, GovMuted, ETextJustify::Left, true), 12.f);
}

void UWLEventModalWidget::ChooseOption(const FString& InstanceId, const FString& OptionId)
{
	UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political)
	{
		return;
	}

	// Confirmacion en dos clics para opciones sensibles.
	const FString ConfirmKey = FString::Printf(TEXT("%s|%s"), *InstanceId, *OptionId);
	bool bSensitive = false;
	for (const FWLPoliticalEventInstance& Event : Political->GetQueuedEvents(PlayerIso()))
	{
		if (Event.InstanceId == InstanceId)
		{
			for (const FWLPoliticalEventOption& Option : Event.Options)
			{
				if (Option.OptionId == OptionId)
				{
					bSensitive = OptionIsSensitive(Option);
				}
			}
		}
	}
	if (bSensitive && PendingConfirmKey != ConfirmKey)
	{
		PendingConfirmKey = ConfirmKey;
		LastResolutionMessage = TEXT("Decision de peso: pulsa de nuevo el boton naranja para confirmar.");
		Rebuild();
		return;
	}
	PendingConfirmKey.Reset();

	FString Message;
	const bool bOk = Political->ResolveEvent(InstanceId, OptionId, Message);
	LastResolutionMessage = Message.IsEmpty()
		? (bOk ? TEXT("Decision aplicada.") : TEXT("No se pudo resolver el evento."))
		: Message;
	Rebuild();

	// Si ya no quedan eventos, cierra el modal (el PlayerController restaura el input de campania).
	if (!HasPendingEvents())
	{
		if (AWLCampaignPlayerController* PC = GetOwningPlayer<AWLCampaignPlayerController>())
		{
			PC->SetEventModalOpen(false);
		}
	}
}

bool UWLEventModalWidget::HasPendingEvents() const
{
	const UWLPoliticalSubsystem* Political = GetPolitical();
	if (!Political)
	{
		return false;
	}
	for (const FWLPoliticalEventInstance& Event : Political->GetQueuedEvents(PlayerIso()))
	{
		if (!Event.bResolved)
		{
			return true;
		}
	}
	return false;
}

void UWLEventModalWidget::OnPostpone()
{
	if (AWLCampaignPlayerController* PC = GetOwningPlayer<AWLCampaignPlayerController>())
	{
		PC->SetEventModalOpen(false);
	}
}

FString UWLEventModalWidget::PlayerIso() const
{
	const UWLCampaignGameInstance* GI = GetCampaignGI();
	return GI ? GI->GetSelectedNationIso() : FString();
}

UWLCampaignGameInstance* UWLEventModalWidget::GetCampaignGI() const
{
	return Cast<UWLCampaignGameInstance>(GetGameInstance());
}

UWLPoliticalSubsystem* UWLEventModalWidget::GetPolitical() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLPoliticalSubsystem>() : nullptr;
}
