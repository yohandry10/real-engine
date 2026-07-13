// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

enum class EWLCampaignTurnPhase : uint8
{
	Decisions,
	DailyEconomy,
	Recruitment,
	Calendar,
	Fiscal,
	Provinces,
	EconomySnapshot,
	Market,
	EconomicAI,
	Politics,
	Military,
	Battles,
	Consequences,
	Validation,
	Snapshot
};

struct FWLCampaignTurnRequest
{
	int32 Year = 1;
	int32 Month = 1;
	int32 Day = 1;
	int32 MonthsPerYear = 12;
	int32 DaysPerMonth = 30;

	TFunction<void()> ApplyDailyEconomy;
	TFunction<void()> AdvanceRecruitment;
	TFunction<void()> AdvanceFinancialMonth;
	TFunction<void()> ApplyMonthlyProvinceState;
	TFunction<void()> UpdateGDPHistory;
	TFunction<void()> AdvanceMarketShocks;
	TFunction<void()> RunEconomicAI;
	TFunction<void()> ProcessPolitics;
	TFunction<TArray<FString>(int32 Year, int32 Month, int32 Day)> ValidateState;
};

struct FWLCampaignTurnResult
{
	int32 Year = 1;
	int32 Month = 1;
	int32 Day = 1;
	bool bMonthRolled = false;
	TArray<EWLCampaignTurnPhase> ExecutedPhases;
	TArray<FString> ValidationErrors;
};

/** Owns the authoritative phase order. Domain callbacks cannot reorder one another. */
class WORLDLEADER_API FWLCampaignTurnCoordinator
{
public:
	static FWLCampaignTurnResult ExecuteDay(const FWLCampaignTurnRequest& Request);
};
