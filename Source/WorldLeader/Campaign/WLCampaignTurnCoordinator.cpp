// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignTurnCoordinator.h"

namespace
{
	void ExecutePhase(
		FWLCampaignTurnResult& Result,
		EWLCampaignTurnPhase Phase,
		const TFunction<void()>& Work = TFunction<void()>())
	{
		Result.ExecutedPhases.Add(Phase);
		if (Work)
		{
			Work();
		}
	}
}

FWLCampaignTurnResult FWLCampaignTurnCoordinator::ExecuteDay(const FWLCampaignTurnRequest& Request)
{
	FWLCampaignTurnResult Result;
	Result.Year = FMath::Max(1, Request.Year);
	Result.Month = FMath::Clamp(Request.Month, 1, FMath::Max(1, Request.MonthsPerYear));
	Result.Day = FMath::Clamp(Request.Day, 1, FMath::Max(1, Request.DaysPerMonth));

	ExecutePhase(Result, EWLCampaignTurnPhase::Decisions);
	ExecutePhase(Result, EWLCampaignTurnPhase::DailyEconomy, Request.ApplyDailyEconomy);
	ExecutePhase(Result, EWLCampaignTurnPhase::Recruitment, Request.AdvanceRecruitment);
	ExecutePhase(Result, EWLCampaignTurnPhase::Calendar);

	if (++Result.Day > FMath::Max(1, Request.DaysPerMonth))
	{
		Result.Day = 1;
		Result.bMonthRolled = true;
		if (++Result.Month > FMath::Max(1, Request.MonthsPerYear))
		{
			Result.Month = 1;
			++Result.Year;
		}

		ExecutePhase(Result, EWLCampaignTurnPhase::Fiscal, Request.AdvanceFinancialMonth);
		ExecutePhase(Result, EWLCampaignTurnPhase::Provinces, Request.ApplyMonthlyProvinceState);
		ExecutePhase(Result, EWLCampaignTurnPhase::EconomySnapshot, Request.UpdateGDPHistory);
		ExecutePhase(Result, EWLCampaignTurnPhase::Market, Request.AdvanceMarketShocks);
		ExecutePhase(Result, EWLCampaignTurnPhase::EconomicAI, Request.RunEconomicAI);
		ExecutePhase(Result, EWLCampaignTurnPhase::Politics, Request.ProcessPolitics);
		ExecutePhase(Result, EWLCampaignTurnPhase::Military);
		ExecutePhase(Result, EWLCampaignTurnPhase::Battles);
		ExecutePhase(Result, EWLCampaignTurnPhase::Consequences);
	}

	ExecutePhase(Result, EWLCampaignTurnPhase::Validation);
	if (Request.ValidateState)
	{
		Result.ValidationErrors = Request.ValidateState(Result.Year, Result.Month, Result.Day);
	}
	ExecutePhase(Result, EWLCampaignTurnPhase::Snapshot);
	return Result;
}
