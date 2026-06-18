// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Military/WLMilitarySubsystem.h"
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

UWLMilitarySubsystem* UWLCampaignGameInstance::GetMilitary() const
{
	return GetSubsystem<UWLMilitarySubsystem>();
}

void UWLCampaignGameInstance::WLSpawnArmy(const FString& Nation, const FString& ProvinceId, const FString& UnitId, int32 Count)
{
	if (UWLMilitarySubsystem* Mil = GetMilitary())
	{
		const FString Id = Mil->CreateArmy(Nation, ProvinceId, UnitId, Count, TEXT(""));
		if (Id.IsEmpty())
		{
			UE_LOG(LogWorldLeader, Warning, TEXT("WLSpawnArmy: no se pudo crear (revisa nacion/provincia/unidad)."));
		}
	}
}

void UWLCampaignGameInstance::WLArmies()
{
	const UWLMilitarySubsystem* Mil = GetMilitary();
	if (!Mil)
	{
		return;
	}

	const TArray<FWLArmy> Armies = Mil->GetArmies();
	UE_LOG(LogWorldLeader, Log, TEXT("=== Ejercitos (%d) ==="), Armies.Num());
	for (const FWLArmy& Army : Armies)
	{
		UE_LOG(LogWorldLeader, Log, TEXT("  %s [%s] en %s: %d unidades (atk %d / def %d) - Gen. %s"),
			*Army.Id, *Army.OwnerIso, *Army.ProvinceId, Army.Units.Num(),
			Mil->GetArmyAttack(Army.Id), Mil->GetArmyDefense(Army.Id), *Army.General);
	}
}

void UWLCampaignGameInstance::WLMove(const FString& ArmyId, const FString& ProvinceId)
{
	if (UWLMilitarySubsystem* Mil = GetMilitary())
	{
		FString Message;
		Mil->MoveArmy(ArmyId, ProvinceId, Message);
		UE_LOG(LogWorldLeader, Log, TEXT("WLMove: %s"), *Message);
	}
}

void UWLCampaignGameInstance::WLBattle(const FString& AttackerId, const FString& DefenderId)
{
	if (UWLMilitarySubsystem* Mil = GetMilitary())
	{
		FString Report;
		Mil->AutoResolveBattle(AttackerId, DefenderId, Report);
		UE_LOG(LogWorldLeader, Log, TEXT("WLBattle: %s"), *Report);
	}
}
