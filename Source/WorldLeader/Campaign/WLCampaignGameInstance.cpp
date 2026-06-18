// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "WorldLeader.h"

UWLDataRegistry* UWLCampaignGameInstance::GetRegistry() const
{
	return GetSubsystem<UWLDataRegistry>();
}

UWLStrategicTickSubsystem* UWLCampaignGameInstance::GetTick() const
{
	return GetSubsystem<UWLStrategicTickSubsystem>();
}

void UWLCampaignGameInstance::WLAdvanceMonth()
{
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->AdvanceMonth();
		WLPrintState();
	}
}

void UWLCampaignGameInstance::WLPrintState()
{
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLPrintState: subsistemas no disponibles."));
		return;
	}

	UE_LOG(LogWorldLeader, Log, TEXT("=== World Leader | %02d/%d ==="),
		Tick->GetCurrentMonth(), Tick->GetCurrentYear());
	UE_LOG(LogWorldLeader, Log, TEXT("Provincias: %d | Naciones: %d"),
		Registry->GetProvinceCount(), Registry->GetNationCount());

	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		UE_LOG(LogWorldLeader, Log, TEXT("  %s (%s): tesoro %lld | balance/mes %lld"),
			*Nation.Name, *Nation.Iso,
			Tick->GetTreasury(Nation.Iso), Tick->GetMonthlyBalance(Nation.Iso));
	}
}

void UWLCampaignGameInstance::WLBuild(const FString& ProvinceId, const FString& BuildingId)
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick)
	{
		return;
	}

	FString Message;
	const bool bOk = Tick->BuildBuilding(ProvinceId, BuildingId, Message);
	UE_LOG(LogWorldLeader, Log, TEXT("WLBuild: %s"), *Message);
	if (bOk)
	{
		WLPrintState();
	}
}
