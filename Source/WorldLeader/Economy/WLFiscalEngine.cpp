// Copyright World Leader project. See ROADMAP.md.

#include "Economy/WLFiscalEngine.h"

void WLFiscalEngine::AccrueDailyBalance(
	int64 MonthlyBalance,
	int32 DaysPerMonth,
	int64& InOutTreasury,
	double& InOutRemainder)
{
	const double Accrued = static_cast<double>(MonthlyBalance)
		/ static_cast<double>(FMath::Max(1, DaysPerMonth)) + InOutRemainder;
	const int64 WholeCredits = static_cast<int64>(Accrued);
	InOutTreasury += WholeCredits;
	InOutRemainder = Accrued - static_cast<double>(WholeCredits);
	if (FMath::Abs(InOutRemainder) < 0.000001)
	{
		InOutRemainder = 0.0;
	}
}
