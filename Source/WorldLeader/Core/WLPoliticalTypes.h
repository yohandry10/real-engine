// Copyright World Leader project. See ROADMAP.md.
//
// Tipos backend F2-F5. La UI debe leer/escribir estos datos via UWLPoliticalSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLCharacterTypes.h"
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

UENUM(BlueprintType)
enum class EWLGovernmentPriority : uint8
{
	Security          UMETA(DisplayName = "Seguridad"),
	Growth            UMETA(DisplayName = "Crecimiento"),
	Austerity         UMETA(DisplayName = "Austeridad"),
	Industrialization UMETA(DisplayName = "Industrializacion"),
	Diplomacy         UMETA(DisplayName = "Diplomacia"),
	Control           UMETA(DisplayName = "Control interno")
};

UENUM(BlueprintType)
enum class EWLPublicGroup : uint8
{
	Business    UMETA(DisplayName = "Empresarios"),
	Military    UMETA(DisplayName = "Militares"),
	Workers     UMETA(DisplayName = "Trabajadores"),
	Regions     UMETA(DisplayName = "Regiones"),
	MiddleClass  UMETA(DisplayName = "Clase media"),
	Unions      UMETA(DisplayName = "Sindicatos")
};

UENUM(BlueprintType)
enum class EWLGovernmentAIObjective : uint8
{
	Stabilize     UMETA(DisplayName = "Estabilizar"),
	Expand        UMETA(DisplayName = "Expandirse"),
	Borrow        UMETA(DisplayName = "Endeudarse"),
	Militarize    UMETA(DisplayName = "Militarizar"),
	Align         UMETA(DisplayName = "Alinear bloque"),
	Industrialize UMETA(DisplayName = "Industrializar")
};

UENUM(BlueprintType)
enum class EWLPolicyReformArea : uint8
{
	Tax              UMETA(DisplayName = "Tributaria"),
	Labor            UMETA(DisplayName = "Laboral"),
	Security         UMETA(DisplayName = "Seguridad"),
	Education        UMETA(DisplayName = "Educacion"),
	Health           UMETA(DisplayName = "Salud"),
	Decentralization UMETA(DisplayName = "Descentralizacion"),
	Military         UMETA(DisplayName = "Militar"),
	Justice          UMETA(DisplayName = "Justicia"),
	Media            UMETA(DisplayName = "Medios"),
	Energy           UMETA(DisplayName = "Energia"),
	Trade            UMETA(DisplayName = "Comercio"),
	Constitution     UMETA(DisplayName = "Constitucion")
};

UENUM(BlueprintType)
enum class EWLPoliticalIdeology : uint8
{
	Conservative UMETA(DisplayName = "Conservadora"),
	Liberal       UMETA(DisplayName = "Liberal"),
	SocialDemocrat UMETA(DisplayName = "Socialdemocrata"),
	Nationalist   UMETA(DisplayName = "Nacionalista"),
	Technocratic  UMETA(DisplayName = "Tecnocrata"),
	Populist      UMETA(DisplayName = "Populista"),
	Regionalist   UMETA(DisplayName = "Regionalista")
};

UENUM(BlueprintType)
enum class EWLPartyRole : uint8
{
	Ruling         UMETA(DisplayName = "Oficialista"),
	Ally           UMETA(DisplayName = "Aliado"),
	SoftOpposition UMETA(DisplayName = "Oposicion blanda"),
	HardOpposition UMETA(DisplayName = "Oposicion dura")
};

UENUM(BlueprintType)
enum class EWLElectionPhase : uint8
{
	Governing    UMETA(DisplayName = "Gobierno"),
	PreCampaign  UMETA(DisplayName = "Precampania"),
	Campaign     UMETA(DisplayName = "Campania"),
	Election     UMETA(DisplayName = "Eleccion"),
	Transition   UMETA(DisplayName = "Transicion"),
	Crisis       UMETA(DisplayName = "Crisis constitucional")
};

UENUM(BlueprintType)
enum class EWLPatronageActionType : uint8
{
	AppointLoyalist UMETA(DisplayName = "Nombrar leal"),
	AwardContract   UMETA(DisplayName = "Conceder contrato"),
	GrantFavor      UMETA(DisplayName = "Repartir favor"),
	FundGovernor    UMETA(DisplayName = "Financiar gobernador"),
	GrantConcession UMETA(DisplayName = "Otorgar concesion")
};

UENUM(BlueprintType)
enum class EWLMediaActionType : uint8
{
	PressBriefing    UMETA(DisplayName = "Rueda de prensa"),
	StateBroadcast   UMETA(DisplayName = "Cadena nacional"),
	Propaganda       UMETA(DisplayName = "Propaganda interna"),
	Censorship       UMETA(DisplayName = "Censura"),
	CounterFakeNews  UMETA(DisplayName = "Contra fake news")
};

UENUM(BlueprintType)
enum class EWLRegionPolicyActionType : uint8
{
	AppointGovernor   UMETA(DisplayName = "Nombrar gobernador"),
	RegionalInvestment UMETA(DisplayName = "Inversion regional"),
	AutonomyDeal      UMETA(DisplayName = "Pacto autonomico"),
	SecurityOperation UMETA(DisplayName = "Operacion de seguridad")
};

UENUM(BlueprintType)
enum class EWLCrisisChainType : uint8
{
	NationalProtest       UMETA(DisplayName = "Paro nacional"),
	CorruptionScandal     UMETA(DisplayName = "Escandalo de corrupcion"),
	MilitaryCrisis        UMETA(DisplayName = "Crisis militar"),
	DebtCrisis            UMETA(DisplayName = "Crisis de deuda"),
	StudentProtest        UMETA(DisplayName = "Protesta estudiantil"),
	OilStrike             UMETA(DisplayName = "Huelga petrolera"),
	BorderCrisis          UMETA(DisplayName = "Crisis fronteriza"),
	Impeachment           UMETA(DisplayName = "Juicio politico"),
	SoftCoup              UMETA(DisplayName = "Golpe blando"),
	StateOfException      UMETA(DisplayName = "Estado de excepcion")
};

USTRUCT(BlueprintType)
struct FWLGovernmentAgendaState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	TArray<EWLGovernmentPriority> Priorities;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsActive = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastAgendaReport;
};

USTRUCT(BlueprintType)
struct FWLMinistryProgramDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString ProgramId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLMinisterOffice Office = EWLMinisterOffice::None;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 DurationMonths = 1;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PoliticalCapitalCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int64 TreasuryCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bRequiresLegislation = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Description;
};

USTRUCT(BlueprintType)
struct FWLMinistryProgramState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString ProgramId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLMinisterOffice Office = EWLMinisterOffice::None;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RemainingMonths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Progress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bBlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReport;
};

USTRUCT(BlueprintType)
struct FWLCabinetDynamicsState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RivalryPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Factionalism = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ScandalRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 SabotageRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ResignationRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastIncident;
};

USTRUCT(BlueprintType)
struct FWLInstitutionalPowerState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RulingCoalitionSupport = 55;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 LegislativeOpposition = 45;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ReformCost = 10;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 GridlockRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastVoteReport;
};

USTRUCT(BlueprintType)
struct FWLPublicGroupSupportState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLPublicGroup Group = EWLPublicGroup::MiddleClass;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Support = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Pressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastShiftReason;
};

USTRUCT(BlueprintType)
struct FWLStateCapacityState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Bureaucracy = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Corruption = 30;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 AdministrativeEfficiency = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CentralAuthority = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PolicyFailureRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReport;
};

USTRUCT(BlueprintType)
struct FWLPoliticalMemoryRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString MemoryKey;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReason;
};

USTRUCT(BlueprintType)
struct FWLPoliticalAIPlanState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLGovernmentAIObjective Objective = EWLGovernmentAIObjective::Stabilize;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString TargetIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString CurrentProgramId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsOnPlan = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastPlanReason;
};

USTRUCT(BlueprintType)
struct FWLPolicyReformDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString ReformId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLPolicyReformArea Area = EWLPolicyReformArea::Tax;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	TArray<FString> PrerequisiteReformIds;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RequiredCoalitionSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RequiredStateCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PoliticalCapitalCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int64 TreasuryCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ProtestRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 OppositionDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PublicOrderDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 LongTermMonths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int64 MonthlyTreasuryDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CapacityDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CorruptionDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 LegitimacyDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	TArray<EWLPublicGroup> AffectedGroups;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PublicGroupSupportDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Description;
};

USTRUCT(BlueprintType)
struct FWLActiveReformState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString ReformId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLPolicyReformArea Area = EWLPolicyReformArea::Tax;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ImplementationProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Backlash = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReport;
};

USTRUCT(BlueprintType)
struct FWLEnactedPolicyReformState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString ReformId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLPolicyReformArea Area = EWLPolicyReformArea::Tax;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsSinceEnacted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bFulfilledCampaignPromise = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReport;
};

USTRUCT(BlueprintType)
struct FWLPartyState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString PartyId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLPoliticalIdeology Ideology = EWLPoliticalIdeology::Liberal;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLPartyRole Role = EWLPartyRole::SoftOpposition;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Seats = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Discipline = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 LoyaltyToGovernment = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Corruption = 20;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bInCoalition = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsSinceInternalElection = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LeaderCharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastIncident;
};

USTRUCT(BlueprintType)
struct FWLElectionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsToElection = 48;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLElectionPhase Phase = EWLElectionPhase::Governing;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 IncumbentApproval = 55;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PollingGovernment = 52;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PollingOpposition = 42;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CampaignIntensity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Legitimacy = 65;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 FraudRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 AbstentionRisk = 20;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bLastElectionWon = true;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bTermLimited = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ConsecutiveTermsWon = 1;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString CampaignPromiseReformId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bCampaignPromiseFulfilled = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastElectionReport;
};

USTRUCT(BlueprintType)
struct FWLCharacterPoliticalProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString CharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString Biography;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString FactionId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString PatronCharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	TArray<FString> RivalCharacterIds;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	TArray<FString> AllyCharacterIds;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	TArray<FString> AgendaTags;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PresidentialAmbition = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PersonalCorruption = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ScandalHeat = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 SuccessionScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastProfileEvent;
};

USTRUCT(BlueprintType)
struct FWLPatronageState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PatronagePower = 20;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ClientelistPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ContractCorruption = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RegionalMachines = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ConcessionBacklash = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastDeal;
};

USTRUCT(BlueprintType)
struct FWLMediaPublicOpinionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PressFreedom = 60;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MediaControl = 20;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PresidentialApproval = 55;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 PropagandaReach = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CensorshipBacklash = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 FakeNewsPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MediaCrisisRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastNarrative;
};

USTRUCT(BlueprintType)
struct FWLRegionGovernorState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString RegionName;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString GovernorName;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLPartyRole Alignment = EWLPartyRole::Ally;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Obedience = 55;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Autonomy = 30;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ProtestRisk = 15;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CenterControl = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 InvestmentLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 SecessionRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RebellionRisk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReport;
};

USTRUCT(BlueprintType)
struct FWLCrisisChainState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString CrisisId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	EWLCrisisChainType Type = EWLCrisisChainType::NationalProtest;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Stage = 1;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 Intensity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsActive = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	bool bResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReport;
};

USTRUCT(BlueprintType)
struct FWLGovernmentCalibrationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 DebtVsGrowthPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 RepressionLegitimacyPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 SubsidyInflationPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 ReformGridlockPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 CivilMilitaryTension = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 SovereigntyAlliancePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	int32 MonthsObserved = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Government")
	FString LastReport;
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
	FString TargetIso;

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
