// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

struct FWLEconomyRecurringCosts
{
	int64 PublicWages = 0;
	int64 SocialSpending = 0;
	int64 DebtInterest = 0;
};

/** Pure national economy calculations. No subsystem or world access. */
struct WORLDLEADER_API FWLEconomyEngine
{
	static FWLEconomyRecurringCosts CalculateRecurringCosts(
		int64 Population,
		int64 Treasury,
		double PublicWagesPerCapita,
		double SocialSpendingPerCapita,
		double DebtMonthlyInterestRate);
};
