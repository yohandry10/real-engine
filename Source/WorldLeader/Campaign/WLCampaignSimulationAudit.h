// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UWLLocalSaveGame;
struct FWLTacticalBattleState;

struct FWLCampaignAuditResult
{
	bool bValid = false;
	FString StableHash;
	TArray<FString> Violations;
};

/** Canonical campaign checks used by tests, soak runs and production diagnostics. */
class WORLDLEADER_API FWLCampaignSimulationAudit
{
public:
	static FWLCampaignAuditResult Audit(const UWLLocalSaveGame& Save, int32 MonthsPerYear, int32 DaysPerMonth);
	static TArray<FString> AuditActiveBattles(const TArray<FWLTacticalBattleState>& Battles);
};
