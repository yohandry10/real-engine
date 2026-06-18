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
		Balance += GetProvinceBuildingIncome(Province.Id);
	}
	return Balance;
}

int64 UWLStrategicTickSubsystem::GetTreasury(const FString& NationIso) const
{
	const int64* Found = Treasuries.Find(NationIso);
	return Found ? *Found : 0;
}

int64 UWLStrategicTickSubsystem::GetProvinceBuildingIncome(const FString& ProvinceId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	const TArray<FString>* Built = ProvinceBuildings.Find(ProvinceId);
	if (!Registry || !Built)
	{
		return 0;
	}

	int64 Income = 0;
	for (const FString& BuildingId : *Built)
	{
		FWLBuildingData Building;
		if (Registry->GetBuilding(BuildingId, Building))
		{
			Income += UWLEconomyLibrary::CalculateBuildingIncome(Building);
		}
	}
	return Income;
}

TArray<FString> UWLStrategicTickSubsystem::GetProvinceBuildings(const FString& ProvinceId) const
{
	if (const TArray<FString>* Built = ProvinceBuildings.Find(ProvinceId))
	{
		return *Built;
	}
	return TArray<FString>();
}

bool UWLStrategicTickSubsystem::BuildBuilding(const FString& ProvinceId, const FString& BuildingId, FString& OutMessage)
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		OutMessage = TEXT("Registro de datos no disponible.");
		return false;
	}

	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		OutMessage = FString::Printf(TEXT("Provincia desconocida: %s"), *ProvinceId);
		return false;
	}

	FWLBuildingData Building;
	if (!Registry->GetBuilding(BuildingId, Building))
	{
		OutMessage = FString::Printf(TEXT("Edificio desconocido: %s"), *BuildingId);
		return false;
	}

	const FString Nation = Province.CountryIso;
	int64* Treasury = Treasuries.Find(Nation);
	if (!Treasury)
	{
		OutMessage = FString::Printf(TEXT("Nacion sin tesoro: %s"), *Nation);
		return false;
	}

	if (*Treasury < Building.Cost)
	{
		OutMessage = FString::Printf(TEXT("Fondos insuficientes en %s: cuesta %lld, tesoro %lld"),
			*Nation, Building.Cost, *Treasury);
		return false;
	}

	*Treasury -= Building.Cost;
	ProvinceBuildings.FindOrAdd(ProvinceId).Add(BuildingId);

	OutMessage = FString::Printf(TEXT("%s construido en %s (%s). Coste %lld. Tesoro %s: %lld"),
		*Building.Name, *Province.Name, *ProvinceId, Building.Cost, *Nation, *Treasury);
	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}
