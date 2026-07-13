// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "Balance/WLBalanceTypes.h"
#include "Core/WLGameTypes.h"

struct FWLProvinceMonthInput
{
	int64 MonthlyBalance = 0;
	bool bControllerBankrupt = false;
	int32 TaxOrderPressure = 0;
	int32 MinisterOrderEffect = 0;
	int32 BuildingOrderEffect = 0;
};

/** Pure province transition. It has no registry, subsystem or world dependency. */
struct WORLDLEADER_API FWLProvinceEngine
{
	static FWLProvinceRuntimeState AdvanceMonth(
		const FWLProvinceRuntimeState& Current,
		const FWLProvinceMonthInput& Input,
		const FWLBalanceRules& Rules);
};
