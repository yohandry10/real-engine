// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLPoliticalTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WLPoliticalSubsystem.generated.h"

class UWLCharacterSubsystem;
class UWLDataRegistry;
class UWLStrategicTickSubsystem;

/**
 * Backend F2-F5: poder interno, golpes, diplomacia, intriga exterior y eventos.
 *
 * La UIX debe consumir estas UFUNCTIONs como endpoints Blueprint, sin duplicar
 * calculos de riesgo/opinion/espionaje en widgets.
 */
UCLASS()
class WORLDLEADER_API UWLPoliticalSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Politics")
	void ResetPoliticalState();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Politics")
	void ProcessPoliticalMonth();

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Politics")
	FWLInternalPowerState GetInternalPower(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Politics")
	bool AttemptCoup(const FString& NationIso, FString& OutReport);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Politics")
	bool RewardGeneral(const FString& NationIso, const FString& CharacterId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Politics")
	bool PurgeCharacter(const FString& NationIso, const FString& CharacterId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Politics")
	bool RepressOpposition(const FString& NationIso, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Diplomacy")
	TArray<FWLDiplomaticRelationState> GetRelationsForNation(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Diplomacy")
	bool GetRelation(const FString& NationA, const FString& NationB, FWLDiplomaticRelationState& OutRelation) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Diplomacy")
	bool AdjustRelationOpinion(const FString& NationA, const FString& NationB, int32 Delta, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Diplomacy")
	bool SetRelationOpinion(const FString& NationA, const FString& NationB, int32 Opinion, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Diplomacy")
	bool DeclareWar(const FString& AggressorIso, const FString& TargetIso, FString& OutMessage);

	/** Termina una guerra: vuelve a tension (opinion -30) y reabre rutas. Sin esto la guerra era eterna. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Diplomacy")
	bool MakePeace(const FString& NationA, const FString& NationB, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Diplomacy")
	bool SignTreaty(const FString& NationA, const FString& NationB, EWLTreatyType Treaty, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Diplomacy")
	bool BreakTreaty(const FString& NationA, const FString& NationB, EWLTreatyType Treaty, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Diplomacy")
	bool SetNationTariffRate(const FString& NationIso, int32 RatePercent, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Intrigue")
	FWLIntelligenceNetworkState GetIntelligenceNetwork(const FString& OwnerIso, const FString& TargetIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Intrigue")
	bool BuildSpyNetwork(const FString& OwnerIso, const FString& TargetIso, const FString& SpyCharacterId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Intrigue")
	bool RunSpyOperation(
		const FString& OwnerIso,
		const FString& TargetIso,
		const FString& SpyCharacterId,
		EWLSpyOperationType Operation,
		FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Events")
	TArray<FWLPoliticalEventInstance> GetQueuedEvents(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Events")
	bool ResolveEvent(const FString& InstanceId, const FString& OptionId, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	FWLCampaignOutcomeState GetCampaignOutcome() const { return CampaignOutcome; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Politics")
	TArray<FString> GetLeaderAgendaTraits(const FString& NationIso) const;

	void WriteSaveSnapshot(
		TArray<FWLInternalPowerState>& OutInternalPower,
		TArray<FWLDiplomaticRelationState>& OutRelations,
		TArray<FWLIntelligenceNetworkState>& OutNetworks,
		TArray<FWLPoliticalEventInstance>& OutEvents,
		FWLCampaignOutcomeState& OutOutcome) const;

	bool RestoreSaveSnapshot(
		const TArray<FWLInternalPowerState>& SavedInternalPower,
		const TArray<FWLDiplomaticRelationState>& SavedRelations,
		const TArray<FWLIntelligenceNetworkState>& SavedNetworks,
		const TArray<FWLPoliticalEventInstance>& SavedEvents,
		const FWLCampaignOutcomeState& SavedOutcome,
		FString& OutMessage);

private:
	UPROPERTY()
	TMap<FString, FWLInternalPowerState> InternalPowerByNation;

	UPROPERTY()
	TMap<FString, FWLDiplomaticRelationState> RelationsByPair;

	UPROPERTY()
	TMap<FString, FWLIntelligenceNetworkState> IntelligenceByPair;

	UPROPERTY()
	TArray<FWLPoliticalEventDefinition> EventDefinitions;

	UPROPERTY()
	TArray<FWLPoliticalEventInstance> EventQueue;

	UPROPERTY()
	FWLCampaignOutcomeState CampaignOutcome;

	int32 NextEventInstanceNumber = 1;

	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;
	UWLCharacterSubsystem* GetCharacters() const;

	void SeedInternalPower();
	void SeedRelations();
	void LoadEventDefinitions();
	void UpdateInternalPowerForNation(const FString& NationIso);
	void QueueTriggeredEventsForNation(const FString& NationIso);
	void CheckCampaignOutcome();

	FWLInternalPowerState& EnsureInternalPower(const FString& NationIso);
	FWLDiplomaticRelationState& EnsureRelation(const FString& NationA, const FString& NationB);
	FWLIntelligenceNetworkState& EnsureNetwork(const FString& OwnerIso, const FString& TargetIso);

	static FString NormalizeIso(const FString& In);
	static FString NormalizeCharacterId(const FString& In);
	static FString RelationKey(const FString& NationA, const FString& NationB);
	static FString NetworkKey(const FString& OwnerIso, const FString& TargetIso);
	static int32 ClampPercent(int32 Value);
	static FString TreatyToString(EWLTreatyType Treaty);
	static FString OperationToString(EWLSpyOperationType Operation);

	bool ValidateNation(const FString& NationIso) const;
	/** ISO de la nacion del jugador (vacio sin campana activa). Distingue golpe=derrota de golpe=cambio de regimen IA. */
	FString GetPlayerNationIso() const;
	/** La IA resuelve sus propios eventos pendientes (elige la opcion que mas baja su oposicion). */
	void AutoResolveEventsForAI(const FString& NationIso);
	/** Fase 3: skill extra que aporta el ministro de Inteligencia a los espias de una nacion. */
	int32 GetIntelligenceMinisterSkillBonus(const FString& OwnerIso) const;
	bool ValidateSpy(const FString& OwnerIso, const FString& SpyCharacterId, int32& OutSkill, FString& OutMessage) const;
	int32 GetAveragePublicOrder(const FString& NationIso) const;
	int32 GetLeaderAgendaPressure(const FString& NationIso) const;
	bool HasQueuedUnresolvedEvent(const FString& NationIso, const FString& EventId) const;
};
