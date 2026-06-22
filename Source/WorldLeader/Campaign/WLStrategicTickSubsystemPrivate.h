// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "Campaign/WLDataRegistry.h"
#include "CoreMinimal.h"

namespace WLStrategicTickPrivate
{
	inline FString NormalizeIso(const FString& In)
	{
		return In.TrimStartAndEnd().ToUpper();
	}

	inline int32 ClampPublicOrder(int32 Value)
	{
		return FMath::Clamp(Value, 0, 100);
	}

	inline int32 StepToward(int32 Value, int32 Target, int32 Step)
	{
		if (Step <= 0 || Value == Target)
		{
			return Value;
		}
		return Value < Target
			? FMath::Min(Value + Step, Target)
			: FMath::Max(Value - Step, Target);
	}

	inline bool ProvinceSupportsBuilding(const FWLProvinceData& Province, const FWLBuildingData& Building)
	{
		bool bRequiresResourceFit = false;
		bool bHasFit = false;

		auto TestFit = [&bRequiresResourceFit, &bHasFit](int32 ProvinceResource, int32 BuildingBonus)
		{
			if (BuildingBonus > 0)
			{
				bRequiresResourceFit = true;
				bHasFit = bHasFit || ProvinceResource > 0;
			}
		};

		TestFit(Province.BaseOil, Building.BonusOil);
		TestFit(Province.BaseGas, Building.BonusGas);
		TestFit(Province.BaseFood, Building.BonusFood);
		TestFit(Province.BaseMinerals, Building.BonusMinerals);
		TestFit(Province.BaseIndustry, Building.BonusIndustry);

		return !bRequiresResourceFit || bHasFit;
	}

	inline int64 CalculateProvinceBuildingFit(const FWLProvinceData& Province, const FWLBuildingData& Building)
	{
		return   static_cast<int64>(Province.BaseOil)      * Building.BonusOil
		       + static_cast<int64>(Province.BaseGas)      * Building.BonusGas
		       + static_cast<int64>(Province.BaseFood)     * Building.BonusFood
		       + static_cast<int64>(Province.BaseMinerals) * Building.BonusMinerals
		       + static_cast<int64>(Province.BaseIndustry) * Building.BonusIndustry;
	}
}
