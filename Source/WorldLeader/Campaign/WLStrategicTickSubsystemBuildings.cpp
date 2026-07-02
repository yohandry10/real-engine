// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLStrategicTickSubsystem.h"
#include "Campaign/WLStrategicTickSubsystemPrivate.h"

#include "Campaign/WLDataRegistry.h"
#include "Economy/WLEconomyLibrary.h"
#include "WorldLeader.h"

using WLStrategicTickPrivate::ProvinceSupportsBuilding;

namespace
{
	int32 GetBuildingMaxLevel(const FWLBuildingData& Building)
	{
		return FMath::Clamp(Building.MaxLevel, 1, 5);
	}

	int32 ClampBuildingLevel(const FWLBuildingData& Building, int32 Level)
	{
		return FMath::Clamp(Level, 1, GetBuildingMaxLevel(Building));
	}

	int64 GetBuildingCostForLevel(const FWLBuildingData& Building, int32 TargetLevel)
	{
		const int32 Level = ClampBuildingLevel(Building, TargetLevel);
		const double Multiplier = FMath::Max(1.0, Building.UpgradeCostMultiplier);
		return static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(FMath::Max<int64>(0, Building.Cost))
			* FMath::Pow(Multiplier, static_cast<double>(Level - 1))));
	}

}

int64 UWLStrategicTickSubsystem::GetProvinceBuildingIncome(const FString& ProvinceId, const FWLBalanceRules& Rules) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		return 0;
	}

	const TArray<FString>* Built = ProvinceBuildings.Find(Province.Id);
	if (!Built)
	{
		return 0;
	}

	int64 Income = 0;
	for (const FString& BuiltBuildingId : *Built)
	{
		FWLBuildingData Building;
		if (Registry->GetBuilding(BuiltBuildingId, Building))
		{
			const int32 Level = GetProvinceBuildingLevel(Province.Id, Building.Id);
			Income += UWLEconomyLibrary::CalculateBuildingIncomeForLevel(Building, Rules, Level);
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

int32 UWLStrategicTickSubsystem::GetProvinceBuildingLevel(const FString& ProvinceId, const FString& BuildingId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		return 0;
	}

	FWLBuildingData Building;
	if (!Registry->GetBuilding(BuildingId, Building))
	{
		return 0;
	}

	const TArray<FString>* Built = ProvinceBuildings.Find(Province.Id);
	if (!Built || !Built->Contains(Building.Id))
	{
		return 0;
	}

	if (const TMap<FString, int32>* Levels = ProvinceBuildingLevels.Find(Province.Id))
	{
		if (const int32* Level = Levels->Find(Building.Id))
		{
			return ClampBuildingLevel(Building, *Level);
		}
	}
	return 1;
}

int64 UWLStrategicTickSubsystem::GetProvinceBuildingUpgradeCost(
	const FString& ProvinceId,
	const FString& BuildingId) const
{
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLBuildingData Building;
	if (!Registry || !Registry->GetBuilding(BuildingId, Building))
	{
		return 0;
	}

	const int32 CurrentLevel = GetProvinceBuildingLevel(ProvinceId, Building.Id);
	if (CurrentLevel <= 0 || CurrentLevel >= GetBuildingMaxLevel(Building))
	{
		return 0;
	}
	return GetBuildingCostForLevel(Building, CurrentLevel + 1);
}

FWLProvinceBuildingEffects UWLStrategicTickSubsystem::GetProvinceBuildingEffects(const FString& ProvinceId) const
{
	FWLProvinceBuildingEffects Effects;
	const UWLDataRegistry* Registry = GetDataRegistry();
	FWLProvinceData Province;
	if (!Registry || !Registry->GetProvince(ProvinceId, Province))
	{
		return Effects;
	}

	const TArray<FString>* Built = ProvinceBuildings.Find(Province.Id);
	if (!Built)
	{
		return Effects;
	}

	const TMap<FString, int32>* Levels = ProvinceBuildingLevels.Find(Province.Id);
	for (const FString& BuiltBuildingId : *Built)
	{
		FWLBuildingData Building;
		if (!Registry->GetBuilding(BuiltBuildingId, Building))
		{
			continue;
		}

		int32 RawLevel = 1;
		if (Levels)
		{
			if (const int32* SavedLevel = Levels->Find(Building.Id))
			{
				RawLevel = *SavedLevel;
			}
		}
		const int32 Level = ClampBuildingLevel(Building, RawLevel);
		Effects.TotalLevels += Level;
		Effects.MonthlyIncome += UWLEconomyLibrary::CalculateBuildingIncomeForLevel(Building, GetBalanceRules(), Level);
		Effects.MonthlyUpkeep += FMath::Max<int64>(0, Building.MonthlyUpkeep) * Level;
		Effects.BonusOil += Building.BonusOil * Level;
		Effects.BonusGas += Building.BonusGas * Level;
		Effects.BonusFood += Building.BonusFood * Level;
		Effects.BonusMinerals += Building.BonusMinerals * Level;
		Effects.BonusIndustry += Building.BonusIndustry * Level;
		Effects.BonusFinancialIncome += Building.BonusFinancialIncome * Level;
		Effects.BonusInfrastructure += Building.BonusInfrastructure * Level;
		Effects.BonusPublicOrder += Building.BonusPublicOrder * Level;
		Effects.BonusRecruitmentCapacity += Building.BonusRecruitmentCapacity * Level;
		Effects.BonusMilitaryPower += Building.BonusMilitaryPower * Level;
		Effects.BonusDefense += Building.BonusDefense * Level;
		Effects.BonusAirCapacity += Building.BonusAirCapacity * Level;
		Effects.BonusNavalCapacity += Building.BonusNavalCapacity * Level;
		Effects.BonusTechnology += Building.BonusTechnology * Level;
	}
	return Effects;
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
		const int32 CurrentLevel = GetProvinceBuildingLevel(Province.Id, Building.Id);
		OutMessage = FString::Printf(TEXT("%s ya existe en %s (%s), nivel %d. Usa UpgradeBuilding para subirlo."),
			*Building.Name, *Province.Name, *Province.Id, CurrentLevel);
		return false;
	}
	if (ExistingBuildings)
	{
		for (const FString& ExistingId : *ExistingBuildings)
		{
			FWLBuildingData ExistingBuilding;
			if (Registry->GetBuilding(ExistingId, ExistingBuilding) && ExistingBuilding.Slot == Building.Slot)
			{
				OutMessage = FString::Printf(TEXT("%s ya ocupa el slot de %s en %s (%s)."),
					*ExistingBuilding.Name, *Building.Name, *Province.Name, *Province.Id);
				return false;
			}
		}
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
	ProvinceBuildingLevels.FindOrAdd(Province.Id).Add(Building.Id, 1);

	OutMessage = FString::Printf(TEXT("%s construido en %s (%s). Coste %lld. Tesoro %s: %lld"),
		*Building.Name, *Province.Name, *Province.Id, Building.Cost, *Nation, *Treasury);
	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}

bool UWLStrategicTickSubsystem::UpgradeBuilding(
	const FString& ProvinceId,
	const FString& BuildingId,
	FString& OutMessage)
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

	TArray<FString>* Built = ProvinceBuildings.Find(Province.Id);
	if (!Built || !Built->Contains(Building.Id))
	{
		OutMessage = FString::Printf(TEXT("%s no existe en %s (%s)."),
			*Building.Name, *Province.Name, *Province.Id);
		return false;
	}

	int32 CurrentLevel = GetProvinceBuildingLevel(Province.Id, Building.Id);
	if (CurrentLevel <= 0)
	{
		CurrentLevel = 1;
	}
	const int32 MaxLevel = GetBuildingMaxLevel(Building);
	if (CurrentLevel >= MaxLevel)
	{
		OutMessage = FString::Printf(TEXT("%s ya esta en nivel maximo %d en %s (%s)."),
			*Building.Name, MaxLevel, *Province.Name, *Province.Id);
		return false;
	}

	const int32 NextLevel = CurrentLevel + 1;
	const int64 UpgradeCost = GetBuildingCostForLevel(Building, NextLevel);
	const FString Nation = GetProvinceControllerIso(Province.Id);
	int64* Treasury = Treasuries.Find(Nation);
	if (!Treasury)
	{
		OutMessage = FString::Printf(TEXT("Nacion sin tesoro: %s"), *Nation);
		return false;
	}
	if (*Treasury - UpgradeCost < -GetCreditLimit(Nation))
	{
		OutMessage = FString::Printf(TEXT("Credito agotado en %s: mejora cuesta %lld, tesoro %lld, limite de credito %lld"),
			*Nation, UpgradeCost, *Treasury, GetCreditLimit(Nation));
		return false;
	}

	*Treasury -= UpgradeCost;
	ProvinceBuildingLevels.FindOrAdd(Province.Id).Add(Building.Id, NextLevel);

	OutMessage = FString::Printf(TEXT("%s mejorado a nivel %d en %s (%s). Coste %lld. Tesoro %s: %lld"),
		*Building.Name, NextLevel, *Province.Name, *Province.Id, UpgradeCost, *Nation, *Treasury);
	UE_LOG(LogWorldLeader, Log, TEXT("%s"), *OutMessage);
	return true;
}
