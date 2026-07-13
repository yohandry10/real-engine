// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLProvinceEngine.h"

FWLProvinceRuntimeState FWLProvinceEngine::AdvanceMonth(
	const FWLProvinceRuntimeState& Current,
	const FWLProvinceMonthInput& Input,
	const FWLBalanceRules& InRules)
{
	const FWLBalanceRules Rules = InRules.Sanitized();
	FWLProvinceRuntimeState Result = Current;
	const int32 Order = FMath::Clamp(Current.PublicOrder, 0, 100);
	int32 NextOrder = Order;
	if (Order < Rules.PublicOrderNeutral)
	{
		NextOrder = FMath::Min(Rules.PublicOrderNeutral, Order + Rules.PublicOrderDriftPerMonth);
	}
	else if (Order > Rules.PublicOrderNeutral)
	{
		NextOrder = FMath::Max(Rules.PublicOrderNeutral, Order - Rules.PublicOrderDriftPerMonth);
	}
	if (Input.MonthlyBalance < 0)
	{
		NextOrder -= Rules.PublicOrderDeficitPenalty;
	}
	if (Input.bControllerBankrupt)
	{
		NextOrder -= Rules.PublicOrderBankruptcyPenalty;
	}
	NextOrder -= Input.TaxOrderPressure;
	NextOrder += Input.MinisterOrderEffect;
	NextOrder += Input.BuildingOrderEffect;
	Result.PublicOrder = FMath::Clamp(NextOrder, 0, 100);

	double GrowthRate = Rules.MonthlyPopulationGrowthRate;
	if (Result.PublicOrder < 30)
	{
		GrowthRate *= -0.5;
	}
	else
	{
		GrowthRate *= FMath::Clamp(
			static_cast<double>(Result.PublicOrder) / static_cast<double>(Rules.PublicOrderNeutral), 0.0, 1.5);
	}
	const int64 Delta = static_cast<int64>(
		FMath::RoundToDouble(static_cast<double>(FMath::Max<int64>(0, Current.Population)) * GrowthRate));
	Result.Population = FMath::Max<int64>(0, Current.Population + Delta);
	return Result;
}
