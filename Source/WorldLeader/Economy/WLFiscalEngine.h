// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

struct WLFiscalEngine
{
	static void AccrueDailyBalance(
		int64 MonthlyBalance,
		int32 DaysPerMonth,
		int64& InOutTreasury,
		double& InOutRemainder);
};
