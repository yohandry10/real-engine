// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "WLFinancialTypes.generated.h"

UENUM(BlueprintType)
enum class EWLCreditRating : uint8
{
	AAA,
	AA,
	A,
	BBB,
	BB,
	B,
	CCC,
	Default
};

UENUM(BlueprintType)
enum class EWLFinancialInstrumentType : uint8
{
	Bond,
	BilateralLoan,
	IMFProgram
};

UENUM(BlueprintType)
enum class EWLForeignSupportType : uint8
{
	ForeignAid,
	ForeignDirectInvestment
};

USTRUCT(BlueprintType)
struct FWLFinancialInstrumentState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString InstrumentId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString CreditorIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	EWLFinancialInstrumentType Type = EWLFinancialInstrumentType::Bond;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 OriginalPrincipal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 PrincipalRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 MonthlyPayment = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	double MonthlyInterestRate = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int32 TotalMonths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int32 RemainingMonths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	bool bDefaulted = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString Title;

	bool IsActive() const
	{
		return !bDefaulted && PrincipalRemaining > 0 && RemainingMonths > 0;
	}
};

USTRUCT(BlueprintType)
struct FWLFinancialProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	EWLCreditRating CreditRating = EWLCreditRating::BBB;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString CreditRatingLabel;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int32 CreditScore = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 OutstandingDebt = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 MonthlyDebtService = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 CreditLimit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 AvailableCredit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	double DebtServiceIncomeRatio = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	double DefaultRisk = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	bool bInDefault = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	bool bIMFEligible = false;
};

USTRUCT(BlueprintType)
struct FWLForeignSupportState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString SupportId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString RecipientIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString SponsorIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	EWLForeignSupportType Type = EWLForeignSupportType::ForeignAid;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 TotalAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 MonthlyAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int64 AmountDelivered = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int32 TotalMonths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	int32 RemainingMonths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString TargetProvinceId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString TargetBuildingId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Finance")
	FString Title;

	bool IsActive() const
	{
		return !bCompleted && MonthlyAmount > 0 && RemainingMonths > 0;
	}
};
