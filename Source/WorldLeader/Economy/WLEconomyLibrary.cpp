// Copyright World Leader project. See ROADMAP.md.

#include "Economy/WLEconomyLibrary.h"
#include "Core/WLConstants.h"

int64 UWLEconomyLibrary::CalculateProvinceIncome(const FWLProvinceData& Province)
{
	using namespace WLConstants;

	const int64 ResourceIncome =
		  static_cast<int64>(Province.BaseOil)      * PriceOil
		+ static_cast<int64>(Province.BaseGas)      * PriceGas
		+ static_cast<int64>(Province.BaseFood)     * PriceFood
		+ static_cast<int64>(Province.BaseMinerals) * PriceMinerals
		+ static_cast<int64>(Province.BaseIndustry) * IndustryValue;

	const int64 PopulationTax =
		static_cast<int64>(static_cast<double>(Province.Population) * PopulationTaxPerCapita);

	return ResourceIncome + PopulationTax;
}

int64 UWLEconomyLibrary::CalculateProvinceUpkeep(const FWLProvinceData& Province)
{
	return static_cast<int64>(Province.Infrastructure) * WLConstants::InfrastructureUpkeepFactor;
}

int64 UWLEconomyLibrary::CalculateProvinceBalance(const FWLProvinceData& Province)
{
	return CalculateProvinceIncome(Province) - CalculateProvinceUpkeep(Province);
}
