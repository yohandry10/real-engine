// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLDataRegistry.h"
#include "Economy/WLEconomyLibrary.h"
#include "Core/WLConstants.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

void UWLStrategicTickSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Asegura que el registro de datos exista antes de inicializar tesoros.
	Collection.InitializeDependency(UWLDataRegistry::StaticClass());

	CurrentYear = WLConstants::StartYear;
	CurrentMonth = WLConstants::StartMonth;
	InitTreasuriesFromData();
}

UWLDataRegistry* UWLStrategicTickSubsystem::GetDataRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

void UWLStrategicTickSubsystem::InitTreasuriesFromData()
{
	Treasuries.Reset();
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry) return;

	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		Treasuries.Add(Nation.Iso, Nation.StartingTreasury);
	}
}

void UWLStrategicTickSubsystem::AdvanceMonth()
{
	if (++CurrentMonth > WLConstants::MonthsPerYear)
	{
		CurrentMonth = 1;
		++CurrentYear;
	}

	ApplyMonthlyEconomy();

	UE_LOG(LogWorldLeader, Log, TEXT("Tick estrategico -> %02d/%d"), CurrentMonth, CurrentYear);
	OnMonthAdvanced.Broadcast(CurrentYear, CurrentMonth);
}

void UWLStrategicTickSubsystem::ApplyMonthlyEconomy()
{
	for (TPair<FString, int64>& Pair : Treasuries)
	{
		Pair.Value += GetMonthlyBalance(Pair.Key);
	}
}

int64 UWLStrategicTickSubsystem::GetMonthlyBalance(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry) return 0;

	int64 Balance = 0;
	for (const FWLProvinceData& Province : Registry->GetProvincesByNation(NationIso))
	{
		Balance += UWLEconomyLibrary::CalculateProvinceBalance(Province);
	}
	return Balance;
}

int64 UWLStrategicTickSubsystem::GetTreasury(const FString& NationIso) const
{
	const int64* Found = Treasuries.Find(NationIso);
	return Found ? *Found : 0;
}
