// Copyright World Leader project. See ROADMAP.md.

#include "Economy/WLEconomyEngine.h"

FWLEconomyRecurringCosts FWLEconomyEngine::CalculateRecurringCosts(
	int64 Population,
	int64 Treasury,
	double PublicWagesPerCapita,
	double SocialSpendingPerCapita,
	double DebtMonthlyInterestRate)
{
	FWLEconomyRecurringCosts Result;
	const int64 SafePopulation = FMath::Max<int64>(0, Population);
	Result.PublicWages = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(SafePopulation) * FMath::Max(0.0, PublicWagesPerCapita)));
	Result.SocialSpending = static_cast<int64>(FMath::RoundToDouble(
		static_cast<double>(SafePopulation) * FMath::Max(0.0, SocialSpendingPerCapita)));
	if (Treasury < 0)
	{
		Result.DebtInterest = static_cast<int64>(FMath::RoundToDouble(
			static_cast<double>(-Treasury) * FMath::Max(0.0, DebtMonthlyInterestRate)));
	}
	return Result;
}
