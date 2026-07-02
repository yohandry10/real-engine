// Copyright World Leader project. See ROADMAP.md.

#include "Economy/WLEconomyLibrary.h"

int64 UWLEconomyLibrary::CalculateProvinceIncome(const FWLProvinceData& Province)
{
	return CalculateProvinceIncomeWithRules(Province, FWLBalanceRules::Default());
}

int64 UWLEconomyLibrary::CalculateProvinceIncomeWithRules(const FWLProvinceData& Province, const FWLBalanceRules& Rules)
{
	const FWLBalanceRules Balance = Rules.Sanitized();

	const int64 ResourceIncome =
		  static_cast<int64>(Province.BaseOil)      * Balance.OilPrice
		+ static_cast<int64>(Province.BaseGas)      * Balance.GasPrice
		+ static_cast<int64>(Province.BaseFood)     * Balance.FoodPrice
		+ static_cast<int64>(Province.BaseMinerals) * Balance.MineralsPrice
		+ static_cast<int64>(Province.BaseIndustry) * Balance.IndustryValue;

	return ResourceIncome + CalculateProvincePopulationTax(Province, Balance);
}

int64 UWLEconomyLibrary::CalculateProvincePopulationTax(const FWLProvinceData& Province, const FWLBalanceRules& Rules)
{
	const FWLBalanceRules Balance = Rules.Sanitized();
	return static_cast<int64>(static_cast<double>(Province.Population) * Balance.PopulationTaxPerCapita);
}

double UWLEconomyLibrary::CalculateTaxRateIncomeMultiplier(int32 TaxRatePercent, const FWLBalanceRules& Rules)
{
	const FWLBalanceRules Balance = Rules.Sanitized();

	// Recaudacion relativa tipo Laffer: R(r) = r * (1 - evasion * r), con r en 0..1. Crece con la
	// tasa pero con rendimiento decreciente; pico en 1/(2*evasion) y despues cae.
	const auto Revenue = [&Balance](double Rate01)
	{
		return Rate01 * (1.0 - Balance.TaxLafferEvasionAtFullRate * Rate01);
	};

	const double DefaultRevenue = Revenue(static_cast<double>(Balance.TaxRateDefaultPercent) / 100.0);
	if (DefaultRevenue <= 0.0)
	{
		return 1.0;   // sin tasa base de referencia la palanca queda neutra
	}

	const int32 ClampedRate = FMath::Clamp(TaxRatePercent, Balance.TaxRateMinPercent, Balance.TaxRateMaxPercent);
	return Revenue(static_cast<double>(ClampedRate) / 100.0) / DefaultRevenue;
}

int64 UWLEconomyLibrary::CalculateProvinceIncomeWithState(
	const FWLProvinceData& Province,
	const FWLBalanceRules& Rules,
	int64 RuntimePopulation,
	int32 PublicOrder)
{
	FWLProvinceData EffectiveProvince = Province;
	EffectiveProvince.Population = FMath::Max<int64>(0, RuntimePopulation);
	return ApplyPublicOrderIncomeModifier(
		CalculateProvinceIncomeWithRules(EffectiveProvince, Rules),
		PublicOrder,
		Rules);
}

int64 UWLEconomyLibrary::CalculateProvinceUpkeep(const FWLProvinceData& Province)
{
	return CalculateProvinceUpkeepWithRules(Province, FWLBalanceRules::Default());
}

int64 UWLEconomyLibrary::CalculateProvinceUpkeepWithRules(const FWLProvinceData& Province, const FWLBalanceRules& Rules)
{
	const FWLBalanceRules Balance = Rules.Sanitized();
	return static_cast<int64>(Province.Infrastructure) * Balance.InfrastructureUpkeepFactor;
}

int64 UWLEconomyLibrary::CalculateProvinceBalance(const FWLProvinceData& Province)
{
	return CalculateProvinceBalanceWithRules(Province, FWLBalanceRules::Default());
}

int64 UWLEconomyLibrary::CalculateProvinceBalanceWithRules(const FWLProvinceData& Province, const FWLBalanceRules& Rules)
{
	return CalculateProvinceIncomeWithRules(Province, Rules) - CalculateProvinceUpkeepWithRules(Province, Rules);
}

int64 UWLEconomyLibrary::CalculateProvinceBalanceWithState(
	const FWLProvinceData& Province,
	const FWLBalanceRules& Rules,
	int64 RuntimePopulation,
	int32 PublicOrder)
{
	return CalculateProvinceIncomeWithState(Province, Rules, RuntimePopulation, PublicOrder)
		- CalculateProvinceUpkeepWithRules(Province, Rules);
}

int64 UWLEconomyLibrary::CalculateBuildingIncome(const FWLBuildingData& Building)
{
	return CalculateBuildingIncomeWithRules(Building, FWLBalanceRules::Default());
}

int64 UWLEconomyLibrary::CalculateBuildingIncomeWithRules(const FWLBuildingData& Building, const FWLBalanceRules& Rules)
{
	const FWLBalanceRules Balance = Rules.Sanitized();

	return   static_cast<int64>(Building.BonusOil)      * Balance.OilPrice
	       + static_cast<int64>(Building.BonusGas)      * Balance.GasPrice
	       + static_cast<int64>(Building.BonusFood)     * Balance.FoodPrice
	       + static_cast<int64>(Building.BonusMinerals) * Balance.MineralsPrice
	       + static_cast<int64>(Building.BonusIndustry) * Balance.IndustryValue;
}

double UWLEconomyLibrary::CalculatePublicOrderIncomeMultiplier(int32 PublicOrder, const FWLBalanceRules& Rules)
{
	const FWLBalanceRules Balance = Rules.Sanitized();
	const int32 ClampedOrder = FMath::Clamp(PublicOrder, 0, 100);

	if (ClampedOrder >= Balance.PublicOrderNeutral)
	{
		const double Denominator = static_cast<double>(100 - Balance.PublicOrderNeutral);
		const double Alpha = Denominator > 0.0
			? static_cast<double>(ClampedOrder - Balance.PublicOrderNeutral) / Denominator
			: 1.0;
		return 1.0 + Alpha * Balance.HighOrderIncomeBonusAtMax;
	}

	const double Alpha = static_cast<double>(Balance.PublicOrderNeutral - ClampedOrder)
		/ static_cast<double>(Balance.PublicOrderNeutral);
	return 1.0 - Alpha * Balance.LowOrderIncomePenaltyAtZero;
}

int64 UWLEconomyLibrary::ApplyPublicOrderIncomeModifier(
	int64 GrossIncome,
	int32 PublicOrder,
	const FWLBalanceRules& Rules)
{
	if (GrossIncome <= 0)
	{
		return GrossIncome;
	}

	return static_cast<int64>(
		FMath::RoundToDouble(static_cast<double>(GrossIncome)
			* CalculatePublicOrderIncomeMultiplier(PublicOrder, Rules)));
}
