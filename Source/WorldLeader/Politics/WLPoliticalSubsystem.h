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

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLGovernmentAgendaState GetGovernmentAgenda(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool SetGovernmentAgenda(const FString& NationIso, const TArray<EWLGovernmentPriority>& Priorities, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLMinistryProgramDefinition> GetAvailableMinistryPrograms(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLMinistryProgramState> GetActiveMinistryPrograms(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool StartMinistryProgram(const FString& NationIso, const FString& ProgramId, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLCabinetDynamicsState GetCabinetDynamics(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLInstitutionalPowerState GetInstitutionalPower(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLPublicGroupSupportState> GetPublicGroups(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLStateCapacityState GetStateCapacity(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLPoliticalMemoryRecord> GetPoliticalMemory(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLPoliticalAIPlanState GetGovernmentAIPlan(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool PassGovernmentReform(const FString& NationIso, const FString& ReformId, int32 PoliticalCapitalCost, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLPolicyReformDefinition> GetAvailablePolicyReforms(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLActiveReformState> GetActivePolicyReforms(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLEnactedPolicyReformState> GetEnactedPolicyReforms(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool EnactPolicyReform(const FString& NationIso, const FString& ReformId, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLPartyState> GetPoliticalParties(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool NegotiatePartySupport(const FString& NationIso, const FString& PartyId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool HoldPartyInternalElection(const FString& NationIso, const FString& PartyId, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLElectionState GetElectionState(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool MakeCampaignPromise(const FString& NationIso, const FString& ReformId, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLCharacterPoliticalProfile> GetCharacterPoliticalProfiles(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	bool GetCharacterPoliticalProfile(const FString& CharacterId, FWLCharacterPoliticalProfile& OutProfile) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLPatronageState GetPatronageState(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool UsePatronage(const FString& NationIso, EWLPatronageActionType Action, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLMediaPublicOpinionState GetMediaPublicOpinion(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool RunMediaAction(const FString& NationIso, EWLMediaActionType Action, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLRegionGovernorState> GetRegionGovernors(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Government")
	bool RunRegionPolicy(const FString& NationIso, const FString& RegionId, EWLRegionPolicyActionType Action, FString& OutMessage);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	TArray<FWLCrisisChainState> GetActiveCrisisChains(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Government")
	FWLGovernmentCalibrationState GetGovernmentCalibration(const FString& NationIso) const;

	/** Feed de noticias del mundo (guerras, golpes, espias detectados, movimientos de la IA...). Nuevas primero. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Politics")
	TArray<FString> GetNewsLog() const { return NewsLog; }

	void WriteSaveSnapshot(
		TArray<FWLInternalPowerState>& OutInternalPower,
		TArray<FWLDiplomaticRelationState>& OutRelations,
		TArray<FWLIntelligenceNetworkState>& OutNetworks,
		TArray<FWLPoliticalEventInstance>& OutEvents,
		FWLCampaignOutcomeState& OutOutcome) const;

	void WriteGovernmentSaveSnapshot(
		TArray<FWLGovernmentAgendaState>& OutAgendas,
		TArray<FWLMinistryProgramState>& OutPrograms,
		TArray<FWLCabinetDynamicsState>& OutCabinetDynamics,
		TArray<FWLInstitutionalPowerState>& OutInstitutions,
		TArray<FWLPublicGroupSupportState>& OutPublicGroups,
		TArray<FWLStateCapacityState>& OutStateCapacity,
		TArray<FWLPoliticalMemoryRecord>& OutMemory,
		TArray<FWLPoliticalAIPlanState>& OutAIPlans) const;

	void WriteGovernmentP2SaveSnapshot(
		TArray<FWLActiveReformState>& OutReforms,
		TArray<FWLEnactedPolicyReformState>& OutEnactedReforms,
		TArray<FWLPartyState>& OutParties,
		TArray<FWLElectionState>& OutElections,
		TArray<FWLCharacterPoliticalProfile>& OutCharacterProfiles,
		TArray<FWLPatronageState>& OutPatronage,
		TArray<FWLMediaPublicOpinionState>& OutMedia,
		TArray<FWLRegionGovernorState>& OutRegions,
		TArray<FWLCrisisChainState>& OutCrisisChains,
		TArray<FWLGovernmentCalibrationState>& OutCalibration) const;

	bool RestoreSaveSnapshot(
		const TArray<FWLInternalPowerState>& SavedInternalPower,
		const TArray<FWLDiplomaticRelationState>& SavedRelations,
		const TArray<FWLIntelligenceNetworkState>& SavedNetworks,
		const TArray<FWLPoliticalEventInstance>& SavedEvents,
		const FWLCampaignOutcomeState& SavedOutcome,
		FString& OutMessage);

	bool RestoreGovernmentSaveSnapshot(
		const TArray<FWLGovernmentAgendaState>& SavedAgendas,
		const TArray<FWLMinistryProgramState>& SavedPrograms,
		const TArray<FWLCabinetDynamicsState>& SavedCabinetDynamics,
		const TArray<FWLInstitutionalPowerState>& SavedInstitutions,
		const TArray<FWLPublicGroupSupportState>& SavedPublicGroups,
		const TArray<FWLStateCapacityState>& SavedStateCapacity,
		const TArray<FWLPoliticalMemoryRecord>& SavedMemory,
		const TArray<FWLPoliticalAIPlanState>& SavedAIPlans,
		FString& OutMessage);

	bool RestoreGovernmentP2SaveSnapshot(
		const TArray<FWLActiveReformState>& SavedReforms,
		const TArray<FWLEnactedPolicyReformState>& SavedEnactedReforms,
		const TArray<FWLPartyState>& SavedParties,
		const TArray<FWLElectionState>& SavedElections,
		const TArray<FWLCharacterPoliticalProfile>& SavedCharacterProfiles,
		const TArray<FWLPatronageState>& SavedPatronage,
		const TArray<FWLMediaPublicOpinionState>& SavedMedia,
		const TArray<FWLRegionGovernorState>& SavedRegions,
		const TArray<FWLCrisisChainState>& SavedCrisisChains,
		const TArray<FWLGovernmentCalibrationState>& SavedCalibration,
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

	UPROPERTY()
	TMap<FString, FWLGovernmentAgendaState> GovernmentAgendaByNation;

	UPROPERTY()
	TArray<FWLMinistryProgramState> ActiveMinistryPrograms;

	UPROPERTY()
	TMap<FString, FWLCabinetDynamicsState> CabinetDynamicsByNation;

	UPROPERTY()
	TMap<FString, FWLInstitutionalPowerState> InstitutionalPowerByNation;

	UPROPERTY()
	TMap<FString, FWLPublicGroupSupportState> PublicGroupSupportByKey;

	UPROPERTY()
	TMap<FString, FWLStateCapacityState> StateCapacityByNation;

	UPROPERTY()
	TMap<FString, FWLPoliticalMemoryRecord> PoliticalMemoryByKey;

	UPROPERTY()
	TMap<FString, FWLPoliticalAIPlanState> GovernmentAIPlanByNation;

	UPROPERTY()
	TArray<FWLActiveReformState> ActivePolicyReforms;

	UPROPERTY()
	TArray<FWLEnactedPolicyReformState> EnactedPolicyReforms;

	UPROPERTY()
	TArray<FWLPartyState> PoliticalParties;

	UPROPERTY()
	TMap<FString, FWLElectionState> ElectionStateByNation;

	UPROPERTY()
	TMap<FString, FWLCharacterPoliticalProfile> CharacterPoliticalProfilesById;

	UPROPERTY()
	TMap<FString, FWLPatronageState> PatronageStateByNation;

	UPROPERTY()
	TMap<FString, FWLMediaPublicOpinionState> MediaStateByNation;

	UPROPERTY()
	TArray<FWLRegionGovernorState> RegionGovernors;

	UPROPERTY()
	TArray<FWLCrisisChainState> CrisisChains;

	UPROPERTY()
	TMap<FString, FWLGovernmentCalibrationState> GovernmentCalibrationByNation;

	int32 NextEventInstanceNumber = 1;

	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;
	UWLCharacterSubsystem* GetCharacters() const;

	void SeedInternalPower();
	void SeedRelations();
	void SeedGovernmentState();
	void LoadEventDefinitions();
	void UpdateInternalPowerForNation(const FString& NationIso);
	void QueueTriggeredEventsForNation(const FString& NationIso);
	void ProcessGovernmentMonthForNation(const FString& NationIso, bool bRunAI);
	void CheckCampaignOutcome();

	FWLInternalPowerState& EnsureInternalPower(const FString& NationIso);
	FWLDiplomaticRelationState& EnsureRelation(const FString& NationA, const FString& NationB);
	FWLIntelligenceNetworkState& EnsureNetwork(const FString& OwnerIso, const FString& TargetIso);
	FWLGovernmentAgendaState& EnsureGovernmentAgenda(const FString& NationIso);
	FWLCabinetDynamicsState& EnsureCabinetDynamics(const FString& NationIso);
	FWLInstitutionalPowerState& EnsureInstitutionalPower(const FString& NationIso);
	FWLPublicGroupSupportState& EnsurePublicGroup(const FString& NationIso, EWLPublicGroup Group);
	FWLStateCapacityState& EnsureStateCapacity(const FString& NationIso);
	FWLPoliticalAIPlanState& EnsureGovernmentAIPlan(const FString& NationIso);
	FWLElectionState& EnsureElectionState(const FString& NationIso);
	FWLPatronageState& EnsurePatronageState(const FString& NationIso);
	FWLMediaPublicOpinionState& EnsureMediaState(const FString& NationIso);
	FWLGovernmentCalibrationState& EnsureGovernmentCalibration(const FString& NationIso);
	FWLCharacterPoliticalProfile& EnsureCharacterPoliticalProfile(const FWLCharacter& Character);

	static FString NormalizeIso(const FString& In);
	static FString NormalizeCharacterId(const FString& In);
	static FString RelationKey(const FString& NationA, const FString& NationB);
	static FString NetworkKey(const FString& OwnerIso, const FString& TargetIso);
	static FString PublicGroupKey(const FString& NationIso, EWLPublicGroup Group);
	static FString PoliticalMemoryKey(const FString& NationIso, const FString& MemoryKey);
	static int32 ClampPercent(int32 Value);
	static FString TreatyToString(EWLTreatyType Treaty);
	static FString OperationToString(EWLSpyOperationType Operation);
	static FString GovernmentPriorityToString(EWLGovernmentPriority Priority);
	static FString PublicGroupToString(EWLPublicGroup Group);
	static FString GovernmentAIObjectiveToString(EWLGovernmentAIObjective Objective);
	static FString ReformMemoryKey(const FString& ReformId);
	static FString PartyKey(const FString& NationIso, const FString& PartyId);
	static FString CrisisChainKey(const FString& NationIso, EWLCrisisChainType Type);

	bool ValidateNation(const FString& NationIso) const;
	/** ISO de la nacion del jugador (vacio sin campana activa). Distingue golpe=derrota de golpe=cambio de regimen IA. */
	FString GetPlayerNationIso() const;
	/** La IA resuelve sus propios eventos pendientes (elige la opcion que mas baja su oposicion). */
	void AutoResolveEventsForAI(const FString& NationIso);
	/**
	 * IA estrategica mensual de una nacion no-jugador: ajusta impuestos/aranceles, firma o rompe
	 * relaciones segun opinion y fuerza, espia al rival y recluta si va por detras. Determinista.
	 */
	void RunStrategicAIForNation(const FString& NationIso);
	void RunGovernmentAIForNation(const FString& NationIso);
	void ApplyGovernmentAgendaMonthly(const FString& NationIso);
	void ApplyMinistryProgramsMonthly(const FString& NationIso);
	void ApplyPolicyReformsMonthly(const FString& NationIso);
	void UpdateCabinetDynamicsForNation(const FString& NationIso);
	void UpdatePoliticalPartiesForNation(const FString& NationIso);
	void UpdateElectionForNation(const FString& NationIso);
	void UpdateCharacterProfilesForNation(const FString& NationIso);
	void UpdatePatronageForNation(const FString& NationIso);
	void UpdateMediaForNation(const FString& NationIso);
	void UpdateRegionsForNation(const FString& NationIso);
	void UpdateCrisisChainsForNation(const FString& NationIso);
	void UpdateGovernmentCalibrationForNation(const FString& NationIso);
	void UpdateInstitutionalPowerForNation(const FString& NationIso);
	void UpdateStateCapacityForNation(const FString& NationIso);
	void UpdatePublicGroupsForNation(const FString& NationIso);
	void AdvancePoliticalMemoryForNation(const FString& NationIso);
	void QueueGovernmentCrisisEventsForNation(const FString& NationIso);
	void SeedPoliticalPartiesForNation(const FString& NationIso);
	void SeedRegionsForNation(const FString& NationIso);
	void SeedCharacterProfilesForNation(const FString& NationIso);
	bool GetPolicyReformDefinition(const FString& ReformId, FWLPolicyReformDefinition& OutDefinition) const;
	bool HasReformMemory(const FString& NationIso, const FString& ReformId) const;
	bool HasEnactedPolicyReform(const FString& NationIso, const FString& ReformId) const;
	bool TrySpendTreasury(const FString& NationIso, int64 Amount, const FString& Reason, FString& OutMessage);
	void RecordEnactedPolicyReform(const FString& NationIso, const FWLPolicyReformDefinition& Definition, const FString& Report);
	void MarkCampaignPromiseFulfilled(const FString& NationIso, const FWLPolicyReformDefinition& Definition);
	FWLPartyState* FindMutableParty(const FString& NationIso, const FString& PartyId);
	FWLRegionGovernorState* FindMutableRegion(const FString& NationIso, const FString& RegionId);
	FWLCrisisChainState& EnsureCrisisChain(const FString& NationIso, EWLCrisisChainType Type, const FString& Reason);
	void TriggerGovernmentP2Crisis(const FString& NationIso, EWLCrisisChainType Type, int32 Intensity, const FString& Reason);
	void AddPoliticalMemory(const FString& NationIso, const FString& MemoryKey, int32 Delta, int32 Months, const FString& Reason);
	int32 GetPoliticalMemoryValue(const FString& NationIso, const FString& MemoryKey) const;
	bool GetMinistryProgramDefinition(const FString& ProgramId, FWLMinistryProgramDefinition& OutDefinition) const;
	int32 GetMinisterProgramModifier(const FString& NationIso, EWLMinisterOffice Office) const;
	void AdjustPublicGroupSupport(const FString& NationIso, EWLPublicGroup Group, int32 SupportDelta, const FString& Reason);
	bool ApplyProgramEffect(const FString& NationIso, const FWLMinistryProgramDefinition& Definition, FWLMinistryProgramState& Program);
	FString SelectGovernmentAITarget(const FString& NationIso) const;
	/** Fase 3: skill extra que aporta el ministro de Inteligencia a los espias de una nacion. */
	int32 GetIntelligenceMinisterSkillBonus(const FString& OwnerIso) const;
	/** Anota una noticia (con fecha de juego) en el feed; REGISTROS y el HUD la muestran. */
	void AddNews(const FString& Item);

	/** Noticias del mundo (nuevas primero, tope 40). No se persiste: es cronica de la sesion. */
	TArray<FString> NewsLog;
	bool ValidateSpy(const FString& OwnerIso, const FString& SpyCharacterId, int32& OutSkill, FString& OutMessage) const;
	int32 GetAveragePublicOrder(const FString& NationIso) const;
	int32 GetLeaderAgendaPressure(const FString& NationIso) const;
	bool HasQueuedUnresolvedEvent(const FString& NationIso, const FString& EventId) const;
};
