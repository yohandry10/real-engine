// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Balance/WLBalanceTypes.h"
#include "Core/WLCharacterTypes.h"
#include "Core/WLFinancialTypes.h"
#include "Core/WLGameTypes.h"
#include "Core/WLPoliticalTypes.h"
#include "GameFramework/SaveGame.h"
#include "WLLocalSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FWLNationTreasurySave
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int64 Treasury = 0;

	/** FE1.2: tasa de impuestos de la nacion (%). -1 = nunca ajustada (usar default de balance). */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 TaxRatePercent = -1;

	/** FE4.3: tasa nacional de aranceles (%). -1 = nunca ajustada (usar default de balance). */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 TariffRatePercent = -1;
};

USTRUCT(BlueprintType)
struct FWLProvinceBuildingsSave
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	FString ProvinceId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FString> BuildingIds;

	/** Mismo orden que BuildingIds. Vacio en saves viejos = todos nivel 1. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<int32> BuildingLevels;
};

/**
 * Save local de campania. Guarda el estado runtime que no debe depender del
 * JSON base: nacion seleccionada, fecha, tesoros, provincias y ejercitos.
 */
UCLASS()
class WORLDLEADER_API UWLLocalSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 SaveVersion = 13;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	FString SelectedNationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	EWLAIDifficulty AIDifficulty = EWLAIDifficulty::Medium;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 CurrentYear = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 CurrentMonth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLNationTreasurySave> NationTreasuries;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLProvinceBuildingsSave> ProvinceBuildings;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLProvinceRuntimeState> ProvinceStates;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLMarketShockState> ActiveMarketShocks;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLFinancialInstrumentState> FinancialInstruments;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLForeignSupportState> ForeignSupportStates;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLArmy> Armies;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 NextArmyNumber = 1;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLCharacter> Characters;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLPoliticalCapitalSave> PoliticalCapital;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLInternalPowerState> InternalPowerStates;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLDiplomaticRelationState> DiplomaticRelations;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLIntelligenceNetworkState> IntelligenceNetworks;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLPoliticalEventInstance> PoliticalEvents;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	FWLCampaignOutcomeState CampaignOutcome;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLGovernmentAgendaState> GovernmentAgendas;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLMinistryProgramState> MinistryPrograms;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLCabinetDynamicsState> CabinetDynamics;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLInstitutionalPowerState> InstitutionalPower;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLPublicGroupSupportState> PublicGroupSupport;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLStateCapacityState> StateCapacity;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLPoliticalMemoryRecord> PoliticalMemory;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLPoliticalAIPlanState> GovernmentAIPlans;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLActiveReformState> ActivePolicyReforms;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLEnactedPolicyReformState> EnactedPolicyReforms;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLPartyState> PoliticalParties;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLElectionState> ElectionStates;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLCharacterPoliticalProfile> CharacterPoliticalProfiles;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLPatronageState> PatronageStates;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLMediaPublicOpinionState> MediaStates;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLRegionGovernorState> RegionGovernors;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLCrisisChainState> CrisisChains;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLGovernmentCalibrationState> GovernmentCalibration;
};
