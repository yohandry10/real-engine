// Copyright World Leader project. See ROADMAP.md.

#include "Balance/WLBalanceSubsystem.h"
#include "Balance/WLBalanceDataAsset.h"
#include "Balance/WLBalanceSettings.h"
#include "WorldLeader.h"

void UWLBalanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadBalance();
}

bool UWLBalanceSubsystem::ReloadBalance()
{
	LoadedBalanceAsset = nullptr;
	ActiveRules = UWLBalanceDataAsset::GetDefaultRules();

	const UWLBalanceSettings* Settings = GetDefault<UWLBalanceSettings>();
	if (!Settings || Settings->BalanceDataAsset.IsNull())
	{
		UE_LOG(LogWorldLeader, Log, TEXT("WLBalanceSubsystem: usando balance por defecto."));
		return false;
	}

	UWLBalanceDataAsset* LoadedAsset = Settings->BalanceDataAsset.LoadSynchronous();
	if (!LoadedAsset)
	{
		UE_LOG(LogWorldLeader, Warning,
			TEXT("WLBalanceSubsystem: no se pudo cargar BalanceDataAsset %s. Usando defaults."),
			*Settings->BalanceDataAsset.ToSoftObjectPath().ToString());
		return false;
	}

	LoadedBalanceAsset = LoadedAsset;
	ActiveRules = LoadedAsset->GetSanitizedRules();

	UE_LOG(LogWorldLeader, Log,
		TEXT("WLBalanceSubsystem: balance cargado desde %s (inicio %02d/%d)."),
		*LoadedAsset->GetPathName(), ActiveRules.StartMonth, ActiveRules.StartYear);
	return true;
}
