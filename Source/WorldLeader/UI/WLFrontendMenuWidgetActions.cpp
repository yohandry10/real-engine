// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLFrontendMenuWidget.h"

#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Frontend/WLFrontendPlayerController.h"
#include "UI/WLFrontendMenuWidgetPrivate.h"

// Los colores (WLText, WLError, ...) viven en el header WLFrontendMenu; antes este .cpp
// los redefinia en un anonymous-namespace y colisionaban (ambiguo) en unity build.
using namespace WLFrontendMenu;

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
