// Copyright World Leader project. See ROADMAP.md.

#include "Economy/WLTradeEngine.h"

double FWLTradeEngine::CalculatePriceMultiplier(
	int64 Demand,
	int64 Supply,
	const FWLBalanceRules& InRules)
{
	const FWLBalanceRules Rules = InRules.Sanitized();
	if (Demand <= 0)
	{
		return 1.0;
	}
	if (Supply <= 0)
	{
		return Rules.MaxMarketPriceMultiplier;
	}
	const double Ratio = static_cast<double>(Demand) / static_cast<double>(Supply);
	const double Multiplier = Ratio >= 1.0
		? 1.0 + (Ratio - 1.0) * Rules.PriceShortageSensitivity
		: 1.0 - (1.0 - Ratio) * Rules.PriceSurplusSensitivity;
	return FMath::Clamp(Multiplier, Rules.MinMarketPriceMultiplier, Rules.MaxMarketPriceMultiplier);
}

FWLTradeValues FWLTradeEngine::CalculateTradeValues(
	int64 Imports,
	int64 Exports,
	double UnitPrice,
	int32 TariffRatePercent,
	const FWLBalanceRules& InRules)
{
	const FWLBalanceRules Rules = InRules.Sanitized();
	const int64 SafeImports = FMath::Max<int64>(0, Imports);
	const int64 SafeExports = FMath::Max<int64>(0, Exports);
	const double SafePrice = FMath::Max(0.0, UnitPrice);
	const int32 SafeTariff = FMath::Clamp(TariffRatePercent, 0, Rules.TariffRateMaxPercent);
	FWLTradeValues Result;
	Result.ImportCost = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(SafeImports) * SafePrice * (1.0 + Rules.ImportPriceMarkup)));
	Result.ImportTariffIncome = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(SafeImports) * SafePrice * (static_cast<double>(SafeTariff) / 100.0)));
	Result.ExportRevenue = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(SafeExports) * SafePrice * (1.0 - Rules.ExportPriceDiscount)));
	return Result;
}
