// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLStrategicTickSubsystemPrivate.h"

#include "Campaign/WLDataRegistry.h"
#include "Economy/WLEconomyLibrary.h"
#include "WorldLeader.h"

using WLStrategicTickPrivate::ProvinceSupportsBuilding;

int64 UWLStrategicTickSubsystem::GetProvinceBuildingIncome(const FString& ProvinceId, const FWLBalanceRules& Rules) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return 0;
	}
	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		return 0;
	}

	const TArray<FString>* Built = ProvinceBuildings.Find(Province.Id);
	if (!Built)
	{
		return 0;
	}

	int64 Income = 0;
	for (const FString& BuildingId : *Built)
	{
		FWLBuildingData Building;
		if (Registry->GetBuilding(BuildingId, Building))
		{
			Income += UWLEconomyLibrary::CalculateBuildingIncomeWithRules(Building, Rules);
		}
	}
	return Income;
}

TArray<FString> UWLStrategicTickSubsystem::GetProvinceBuildings(const FString& ProvinceId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	const FString CanonicalProvinceId = (Registry && Registry->GetProvince(ProvinceId, Province))
		? Province.Id
		: ProvinceId.TrimStartAndEnd().ToUpper();

	if (const TArray<FString>* Built = ProvinceBuildings.Find(CanonicalProvinceId))
	{
		return *Built;
	}
	return TArray<FString>();
}

bool UWLStrategicTickSubsystem::IsBuildingSupportedInProvince(const FString& ProvinceId, const FString& BuildingId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	if (!Registry)
	{
		return false;
	}

	FWLProvinceData Province;
	FWLBuildingData Building;
	return Registry->GetProvince(ProvinceId, Province)
		&& Registry->GetBuilding(BuildingId, Building)
		&& ProvinceSupportsBuilding(Province, Building);
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
	if (Building.Cost < 0)
	{
		OutMessage = FString::Printf(TEXT("Edificio con coste invalido: %s (%lld)"), *Building.Id, Building.Cost);
		return false;
	}

	const TArray<FString>* ExistingBuildings = ProvinceBuildings.Find(Province.Id);
	if (ExistingBuildings && ExistingBuildings->Contains(Building.Id))
	{
		OutMessage = FString::Printf(TEXT("%s ya existe en %s (%s)."),
			*Building.Name, *Province.Name, *Province.Id);
		return false;
	}

	if (!ProvinceSupportsBuilding(Province, Building))
	{
		OutMessage = FString::Printf(TEXT("%s no encaja economicamente en %s (%s)."),
			*Building.Name, *Province.Name, *Province.Id);
		return false;
	}

	const FString Nation = GetProvinceControllerIso(Province.Id);
	int64* Treasury = Treasuries.Find(Nation);
	if (!Treasury)
	{
		OutMessage = FString::Printf(TEXT("Nacion sin tesoro: %s"), *Nation);
		return false;
	}

	// FE1.4: se puede gastar en deficit (deuda con interes) hasta el limite de credito de la nacion.
	if (*Treasury - Building.Cost < -GetCreditLimit(Nation))
	{
		OutMessage = FString::Printf(TEXT("Credito agotado en %s: cuesta %lld, tesoro %lld, limite de credito %lld"),
			*Nation, Building.Cost, *Treasury, GetCreditLimit(Nation));
		return false;
	}

	*Treasury -= Building.Cost;
	ProvinceBuildings.FindOrAdd(Province.Id).Add(Building.Id);

	OutMessage = FString::Printf(TEXT("%s construido en %s (%s). Coste %lld. Tesoro %s: %lld"),
		*Building.Name, *Province.Name, *Province.Id, Building.Cost, *Nation, *Treasury);
	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}
