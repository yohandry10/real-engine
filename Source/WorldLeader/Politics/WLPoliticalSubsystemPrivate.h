// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "Politics/WLPoliticalSubsystem.h"

class UWLMilitarySubsystem;

namespace WLPoliticsPrivate
{
	inline constexpr int32 CoupAttemptRiskThreshold = 85;
	inline constexpr int64 RewardGeneralCost = 3000;
	inline constexpr int64 RepressOppositionCost = 2000;
	inline constexpr int32 MaxAgendaPriorities = 3;
	inline constexpr int32 ElectionCycleMonths = 48;
	inline constexpr int32 MinPoliticalActionPoints = 3;
	inline constexpr int32 MaxPoliticalActionPoints = 6;
	inline constexpr int32 CampaignPromiseWindowMonths = 6;
	inline constexpr int32 MaxGovernmentLogEntries = 240;
	inline constexpr int32 MaxNewsLogEntries = 40;

	const TArray<EWLPublicGroup>& AllPublicGroups();
	int32 GeneralPoliticalWeight(const FWLCharacter& General, const UWLMilitarySubsystem* Military);
	FString PoliticalActionTypeToString(EWLPoliticalActionType Type);
	EWLGovernmentLogCategory PoliticalActionLogCategory(EWLPoliticalActionType Type);
	FString PoliticalActionLogTitle(EWLPoliticalActionType Type);
	const TArray<FWLMinistryProgramDefinition>& GovernmentProgramDefinitions();
	const TArray<FWLPolicyReformDefinition>& PolicyReformDefinitions();
	TArray<EWLGovernmentPriority> DefaultGovernmentAgenda();
	void AddDefaultEventDefinitions(TArray<FWLPoliticalEventDefinition>& OutDefinitions);
}
