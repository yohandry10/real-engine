// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "Balance/WLBalanceTypes.h"

struct FWLTradeValues
{
	int64 ImportCost = 0;
	int64 ImportTariffIncome = 0;
	int64 ExportRevenue = 0;
};

/** Pure market/trade calculations with deterministic rounding. */
struct WORLDLEADER_API FWLTradeEngine
{
	static double CalculatePriceMultiplier(int64 Demand, int64 Supply, const FWLBalanceRules& Rules);
	static FWLTradeValues CalculateTradeValues(
		int64 Imports,
		int64 Exports,
		double UnitPrice,
		int32 TariffRatePercent,
		const FWLBalanceRules& Rules);
};
