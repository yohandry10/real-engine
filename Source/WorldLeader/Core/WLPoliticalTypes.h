// Copyright World Leader project. See ROADMAP.md.
//
// Tipos backend F2-F5. La UI debe leer/escribir estos datos via UWLPoliticalSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "WLPoliticalTypes.generated.h"

UENUM(BlueprintType)
enum class EWLDiplomaticStatus : uint8
{
	Peace   UMETA(DisplayName = "Paz"),
	Tension UMETA(DisplayName = "Tension"),
	War     UMETA(DisplayName = "Guerra")
};

UENUM(BlueprintType)
enum class EWLTreatyType : uint8
{
	TradeAgreement   UMETA(DisplayName = "Acuerdo comercial"),
	NonAggression    UMETA(DisplayName = "No agresion"),
	Alliance         UMETA(DisplayName = "Alianza"),
	Embargo          UMETA(DisplayName = "Embargo")
};

UENUM(BlueprintType)
enum class EWLSpyOperationType : uint8
{
	SabotageEconomy     UMETA(DisplayName = "Sabotear economia"),
	SabotageArmy        UMETA(DisplayName = "Sabotear ejercito"),
	FundCoup            UMETA(DisplayName = "Financiar golpe"),
	Propaganda          UMETA(DisplayName = "Propaganda"),
	CounterIntelligence UMETA(DisplayName = "Contraespionaje")
};

USTRUCT(BlueprintType)
struct FWLInternalPowerState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	int32 AveragePublicOrder = 70;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	int32 OppositionStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	int32 OppositionPopularity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	int32 ExternalCoupFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	int32 CoupRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	int32 LastCoupRoll = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	bool bLastCoupSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Politics")
	FString LastCoupReport;
};

USTRUCT(BlueprintType)
struct FWLDiplomaticRelationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Diplomacy")
	FString NationA;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Diplomacy")
	FString NationB;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Diplomacy")
	int32 Opinion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Diplomacy")
	EWLDiplomaticStatus Status = EWLDiplomaticStatus::Peace;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Diplomacy")
	TArray<EWLTreatyType> Treaties;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Diplomacy")
	FString CasusBelli;
};

/** FE4.2-FE4.5: ruta comercial derivada de diplomacia, tratados, embargos y guerra. */
USTRUCT(BlueprintType)
struct FWLTradeRouteState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	FString NationA;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	FString NationB;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	bool bOpen = true;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	bool bTradeAgreement = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	bool bEmbargoed = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	bool bAtWar = false;

	/** 0 = ruta cortada, 1 = normal, >1 = acuerdo comercial aumenta volumen. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	double AccessMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Trade")
	FString Reason;
};

USTRUCT(BlueprintType)
struct FWLIntelligenceNetworkState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Intrigue")
	FString OwnerIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Intrigue")
	FString TargetIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Intrigue")
	int32 NetworkStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Intrigue")
	int32 Exposure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Intrigue")
	FString LastOperationReport;
};

USTRUCT(BlueprintType)
struct FWLPoliticalEventOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString OptionId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 PoliticalCapitalDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int64 TreasuryDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 OppositionDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 PublicOrderDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 RelationDelta = 0;

	/** FE3.4/F5: bien afectado por shock de mercado al resolver esta opcion. "*" o "all" = todo el mercado. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString MarketShockGoodId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString MarketShockTitle;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	double MarketShockPriceMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 MarketShockDurationMonths = 0;
};

USTRUCT(BlueprintType)
struct FWLPoliticalEventDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString EventId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString Body;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString Trigger;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 MinCoupRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 MinOppositionStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	int32 MaxPublicOrder = 100;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	TArray<FWLPoliticalEventOption> Options;
};

USTRUCT(BlueprintType)
struct FWLPoliticalEventInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString EventId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString TargetIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	FString Body;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	TArray<FWLPoliticalEventOption> Options;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Events")
	bool bResolved = false;
};

USTRUCT(BlueprintType)
struct FWLCampaignOutcomeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign")
	bool bGameOver = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign")
	FString OutcomeType;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign")
	FString WinningNationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign")
	FString LosingNationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign")
	FString Reason;
};
