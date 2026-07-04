// Copyright World Leader project. See ROADMAP.md.

#include "Politics/WLPoliticalSubsystemPrivate.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Military/WLMilitarySubsystem.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

using namespace WLPoliticsPrivate;

void UWLPoliticalSubsystem::UpdateInternalPowerForNation(const FString& NationIso)
{
	FWLInternalPowerState& State = EnsureInternalPower(NationIso);
	State.AveragePublicOrder = GetAveragePublicOrder(NationIso);

	const int32 DisorderPressure = FMath::Max(0, 65 - State.AveragePublicOrder) / 2;
	State.OppositionStrength = ClampPercent(State.OppositionStrength + DisorderPressure - 2);
	State.OppositionPopularity = ClampPercent(State.OppositionPopularity + DisorderPressure - 1);

	int32 GeneralPressure = 0;
	int32 GeneralWeight = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		const UWLMilitarySubsystem* Military = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UWLMilitarySubsystem>()
			: nullptr;
		for (const FWLCharacter& General : Characters->GetGenerals(State.NationIso))
		{
			const int32 Weight = GeneralPoliticalWeight(General, Military);
			if (Weight <= 0)
			{
				continue;
			}
			GeneralPressure += FMath::Max(0, General.Ambition - General.Loyalty / 2) * Weight;
			GeneralWeight += Weight;
		}
	}
	const int32 AverageGeneralPressure = GeneralWeight > 0 ? GeneralPressure / GeneralWeight : 0;
	const int32 PublicOrderRisk = FMath::Max(0, 60 - State.AveragePublicOrder);
	State.CoupRisk = ClampPercent(
		AverageGeneralPressure / 2
		+ PublicOrderRisk
		+ State.OppositionStrength / 3
		+ State.ExternalCoupFunding / 2
		+ GetLeaderAgendaPressure(State.NationIso));
}

FWLInternalPowerState UWLPoliticalSubsystem::GetInternalPower(const FString& NationIso) const
{
	if (const FWLInternalPowerState* Found = InternalPowerByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLInternalPowerState State;
	State.NationIso = NormalizeIso(NationIso);
	State.AveragePublicOrder = GetAveragePublicOrder(NationIso);
	return State;
}

void UWLPoliticalSubsystem::ProcessPoliticalMonth()
{
	const UWLDataRegistry* Registry = GetRegistry();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Registry)
	{
		return;
	}

	const FString PlayerIso = GetPlayerNationIso();
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		const bool bIsAI = !PlayerIso.IsEmpty() && Nation.Iso != PlayerIso;
		ProcessGovernmentMonthForNation(Nation.Iso, bIsAI);
		if (UWLStrategicTickSubsystem* Tick = GetTick())
		{
			Tick->InvalidateEconomicQueryCache();
		}
		if (Characters)
		{
			Characters->AddMonthlyRenownToGenerals(Nation.Iso, 1);
		}
		UpdateInternalPowerForNation(Nation.Iso);
		QueueTriggeredEventsForNation(Nation.Iso);
		if (bIsAI)
		{
			AutoResolveEventsForAI(Nation.Iso);   // la IA no deja eventos pudriendose en cola
			RunStrategicAIForNation(Nation.Iso);  // la "computadora" juega: fisco, diplomacia, intriga, reclutamiento
		}
		FWLInternalPowerState& State = EnsureInternalPower(Nation.Iso);
		if (!CampaignOutcome.bGameOver && State.CoupRisk >= CoupAttemptRiskThreshold)
		{
			FString Report;
			AttemptCoup(Nation.Iso, Report);
		}
	}

	// Las redes de espionaje se enfrian con el tiempo: sin operaciones nuevas, la exposicion baja.
	for (TPair<FString, FWLIntelligenceNetworkState>& Pair : IntelligenceByPair)
	{
		Pair.Value.Exposure = ClampPercent(Pair.Value.Exposure - 4);
	}

	// Fase 3 auditoria: un buen ministro de Exterior mejora la opinion con cada pais mes a mes
	// (uno inepto la erosiona). Ambos lados de la relacion aplican su deriva.
	if (Characters)
	{
		const UWLStrategicTickSubsystem* Tick = GetTick();
		const FWLBalanceRules Rules = Tick ? Tick->GetBalanceRules() : FWLBalanceRules::Default();
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			const int32 OpinionDrift = FMath::RoundToInt(
				Characters->GetMinisterEffectFactor(Nation.Iso, EWLMinisterOffice::Foreign)
				* Rules.ForeignMinisterOpinionPerMonth);
			if (OpinionDrift == 0)
			{
				continue;
			}
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA != Nation.Iso && Relation.NationB != Nation.Iso)
				{
					continue;
				}
				Relation.Opinion = FMath::Clamp(Relation.Opinion + OpinionDrift, -100, 100);
				if (Relation.Status != EWLDiplomaticStatus::War)
				{
					Relation.Status = Relation.Opinion < -35 ? EWLDiplomaticStatus::Tension : EWLDiplomaticStatus::Peace;
				}
			}
		}
	}

	CheckCampaignOutcome();
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->InvalidateEconomicQueryCache();
	}
}

void UWLPoliticalSubsystem::ProcessGovernmentMonthForNation(const FString& NationIso, bool bRunAI)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		return;
	}
	if (bRunAI)
	{
		RunGovernmentAIForNation(Iso);
	}
	ApplyGovernmentAgendaMonthly(Iso);
	ApplyMinistryProgramsMonthly(Iso);
	ApplyPolicyReformsMonthly(Iso);
	UpdateCharacterProfilesForNation(Iso);
	UpdatePatronageForNation(Iso);
	UpdateMediaForNation(Iso);
	UpdatePoliticalPartiesForNation(Iso);
	UpdateCabinetDynamicsForNation(Iso);
	UpdateInstitutionalPowerForNation(Iso);
	UpdatePublicGroupsForNation(Iso);
	UpdateStateCapacityForNation(Iso);
	UpdateRegionsForNation(Iso);
	UpdateElectionForNation(Iso);
	AdvancePoliticalMemoryForNation(Iso);
	QueueGovernmentCrisisEventsForNation(Iso);
	UpdateCrisisChainsForNation(Iso);
	UpdateGovernmentCalibrationForNation(Iso);
}

void UWLPoliticalSubsystem::ApplyGovernmentAgendaMonthly(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentAgendaState& Agenda = EnsureGovernmentAgenda(Iso);
	++Agenda.MonthsActive;

	UWLStrategicTickSubsystem* Tick = GetTick();
	FWLInternalPowerState& Internal = EnsureInternalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	int32 AppliedPriorities = 0;
	TArray<FString> SkippedPriorities;

	for (EWLGovernmentPriority Priority : Agenda.Priorities)
	{
		switch (Priority)
		{
		case EWLGovernmentPriority::Security:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, TEXT("agenda seguridad"));
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 1);
			if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
			++AppliedPriorities;
			break;
		case EWLGovernmentPriority::Growth:
			if (Tick)
			{
				FString TreasuryMessage;
				if (TrySpendTreasury(Iso, 600, TEXT("agenda crecimiento"), TreasuryMessage))
				{
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, TEXT("agenda crecimiento"));
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 1, TEXT("agenda crecimiento"));
					Tick->AdjustNationPublicOrder(Iso, 1);
					++AppliedPriorities;
				}
				else
				{
					SkippedPriorities.Add(TEXT("crecimiento sin tesoro"));
				}
			}
			else
			{
				SkippedPriorities.Add(TEXT("crecimiento sin economia"));
			}
			break;
		case EWLGovernmentPriority::Austerity:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, TEXT("agenda austeridad"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -2, TEXT("agenda austeridad"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -2, TEXT("agenda austeridad"));
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength + 1);
			if (Tick)
			{
				FString TreasuryMessage;
				Tick->AdjustTreasury(Iso, 900, TreasuryMessage);
				const int32 CurrentTax = Tick->GetTaxRate(Iso);
				if (Tick->GetMonthlyBalance(Iso) < 0 && Tick->SetTaxRate(Iso, CurrentTax + 1) > CurrentTax)
				{
					++AppliedPriorities;
				}
				else
				{
					SkippedPriorities.Add(TEXT("austeridad en fatiga fiscal"));
				}
			}
			else
			{
				SkippedPriorities.Add(TEXT("austeridad sin economia"));
			}
			break;
		case EWLGovernmentPriority::Industrialization:
			if (Tick)
			{
				FString TreasuryMessage;
				if (TrySpendTreasury(Iso, 800, TEXT("agenda industrial"), TreasuryMessage))
				{
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 2, TEXT("agenda industrial"));
					AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 1, TEXT("agenda industrial"));
					Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + 1);
					++AppliedPriorities;
				}
				else
				{
					SkippedPriorities.Add(TEXT("industrializacion sin tesoro"));
				}
			}
			else
			{
				SkippedPriorities.Add(TEXT("industrializacion sin economia"));
			}
			break;
		case EWLGovernmentPriority::Diplomacy:
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA == Iso || Relation.NationB == Iso)
				{
					Relation.Opinion = FMath::Clamp(Relation.Opinion + 1, -100, 100);
				}
			}
			Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 1);
			++AppliedPriorities;
			break;
		case EWLGovernmentPriority::Control:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 1, TEXT("agenda control"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -1, TEXT("agenda control"));
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -1, TEXT("agenda control"));
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 2);
			Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 2);
			if (Tick) { Tick->AdjustNationPublicOrder(Iso, -1); }
			++AppliedPriorities;
			break;
		default:
			break;
		}
	}
	Agenda.LastAgendaReport = SkippedPriorities.IsEmpty()
		? FString::Printf(TEXT("%s ejecuta agenda de gobierno (%d/%d prioridades)."),
			*Iso, AppliedPriorities, Agenda.Priorities.Num())
		: FString::Printf(TEXT("%s ejecuta agenda parcial (%d/%d); no ejecutada: %s."),
			*Iso,
			AppliedPriorities,
			Agenda.Priorities.Num(),
			*FString::Join(SkippedPriorities, TEXT(", ")));
}

void UWLPoliticalSubsystem::ApplyMinistryProgramsMonthly(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (int32 Index = ActiveMinistryPrograms.Num() - 1; Index >= 0; --Index)
	{
		FWLMinistryProgramState& Program = ActiveMinistryPrograms[Index];
		if (Program.NationIso != Iso || Program.RemainingMonths <= 0)
		{
			continue;
		}

		FWLMinistryProgramDefinition Definition;
		if (!GetMinistryProgramDefinition(Program.ProgramId, Definition))
		{
			ActiveMinistryPrograms.RemoveAt(Index);
			continue;
		}

		const int32 MinisterModifier = GetMinisterProgramModifier(Iso, Program.Office);
		const FWLStateCapacityState Capacity = GetStateCapacity(Iso);
		const int32 Risk = FMath::Clamp(Capacity.PolicyFailureRisk - MinisterModifier, 0, 90);
		const int32 Roll = static_cast<int32>((GetTypeHash(Iso + Program.ProgramId) + Program.RemainingMonths * 17) % 100);
		if (Roll < Risk / 3)
		{
			Program.bBlocked = true;
			Program.LastReport = FString::Printf(TEXT("%s se atasca por baja capacidad estatal (riesgo %d)."),
				*Program.Name, Risk);
			AddPoliticalMemory(Iso, TEXT("policy_failure"), 1, 8, Program.LastReport);
			continue;
		}
		Program.bBlocked = false;
		ApplyProgramEffect(Iso, Definition, Program);
		Program.Progress = FMath::Clamp(
			Program.Progress + 25 + FMath::Max(0, MinisterModifier / 2) - FMath::Max(0, Risk / 10),
			0,
			100);

		--Program.RemainingMonths;
		if (Program.Progress >= 100)
		{
			Program.LastReport = FString::Printf(TEXT("%s completa %s con exito total."), *Iso, *Program.Name);
			AddPoliticalMemory(Iso, TEXT("program_completed"), 1, 10, Program.LastReport);
			ActiveMinistryPrograms.RemoveAt(Index);
		}
		else if (Program.RemainingMonths <= 0)
		{
			Program.LastReport = Program.Progress >= 60
				? FString::Printf(TEXT("%s cierra %s con exito parcial (%d/100)."), *Iso, *Program.Name, Program.Progress)
				: FString::Printf(TEXT("%s fracasa %s por progreso insuficiente (%d/100)."), *Iso, *Program.Name, Program.Progress);
			AddPoliticalMemory(Iso, Program.Progress >= 60 ? TEXT("program_partial_success") : TEXT("program_failed"), 1, 10, Program.LastReport);
			ActiveMinistryPrograms.RemoveAt(Index);
		}
	}
}

void UWLPoliticalSubsystem::ApplyPolicyReformsMonthly(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (int32 Index = ActivePolicyReforms.Num() - 1; Index >= 0; --Index)
	{
		FWLActiveReformState& Reform = ActivePolicyReforms[Index];
		if (Reform.NationIso != Iso || Reform.MonthsRemaining <= 0)
		{
			continue;
		}

		FWLPolicyReformDefinition Definition;
		if (!GetPolicyReformDefinition(Reform.ReformId, Definition))
		{
			ActivePolicyReforms.RemoveAt(Index);
			continue;
		}

		FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
		FWLElectionState& Election = EnsureElectionState(Iso);
		if (UWLStrategicTickSubsystem* Tick = GetTick())
		{
			if (Definition.MonthlyTreasuryDelta != 0)
			{
				FString TreasuryMessage;
				Tick->AdjustTreasury(Iso, Definition.MonthlyTreasuryDelta, TreasuryMessage);
			}
		}
		const int32 MonthlyCapacity = FMath::Clamp(Definition.CapacityDelta, -3, 3);
		const int32 MonthlyCorruption = FMath::Clamp(Definition.CorruptionDelta, -3, 3);
		Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + MonthlyCapacity);
		Capacity.Corruption = ClampPercent(Capacity.Corruption + MonthlyCorruption);
		Election.Legitimacy = ClampPercent(Election.Legitimacy + FMath::Clamp(Definition.LegitimacyDelta, -2, 2));
		Reform.ImplementationProgress = ClampPercent(Reform.ImplementationProgress + FMath::Max(4, Capacity.AdministrativeEfficiency / 12));
		Reform.Backlash = ClampPercent(Reform.Backlash - 2 + EnsureInstitutionalPower(Iso).GridlockRisk / 30);
		--Reform.MonthsRemaining;
		Reform.LastReport = FString::Printf(TEXT("%s implementa %s: progreso %d, backlash %d."),
			*Iso, *Reform.Name, Reform.ImplementationProgress, Reform.Backlash);
		if (Reform.Backlash >= 70)
		{
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Reform.Backlash, Reform.LastReport);
		}
		if (Reform.MonthsRemaining <= 0)
		{
			const FString Report = FString::Printf(TEXT("%s queda consolidada."), *Reform.Name);
			RecordEnactedPolicyReform(Iso, Definition, Report);
			AddPoliticalMemory(Iso, ReformMemoryKey(Reform.ReformId), 1, 84, Report);
			ActivePolicyReforms.RemoveAt(Index);
		}
	}

	for (FWLEnactedPolicyReformState& Reform : EnactedPolicyReforms)
	{
		if (Reform.NationIso == Iso)
		{
			++Reform.MonthsSinceEnacted;
		}
	}
}

void UWLPoliticalSubsystem::UpdatePoliticalPartiesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	SeedPoliticalPartiesForNation(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	int32 CoalitionSeats = 0;
	int32 CoalitionLoyalty = 0;
	int32 CoalitionParties = 0;
	int32 OppositionSeats = 0;
	for (FWLPartyState& Party : PoliticalParties)
	{
		if (Party.NationIso != Iso)
		{
			continue;
		}
		++Party.MonthsSinceInternalElection;
		if (Party.bInCoalition || Party.Role == EWLPartyRole::Ruling || Party.Role == EWLPartyRole::Ally)
		{
			CoalitionSeats += Party.Seats;
			CoalitionLoyalty += Party.LoyaltyToGovernment;
			++CoalitionParties;
		}
		else
		{
			OppositionSeats += Party.Seats;
		}

		const int32 BetrayalRoll = static_cast<int32>((GetTypeHash(Iso + Party.PartyId) + Party.MonthsSinceInternalElection * 13) % 100);
		if (Party.bInCoalition && Party.Role != EWLPartyRole::Ruling
			&& Party.LoyaltyToGovernment < 28
			&& BetrayalRoll < 18)
		{
			Party.bInCoalition = false;
			Party.Role = EWLPartyRole::SoftOpposition;
			Party.LastIncident = FString::Printf(TEXT("%s rompe con el gobierno."), *Party.Name);
			AddPoliticalMemory(Iso, TEXT("party_betrayal"), 1, 10, Party.LastIncident);
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::Impeachment, 35, Party.LastIncident);
		}
		else if (Party.MonthsSinceInternalElection >= 24)
		{
			Party.Discipline = ClampPercent(Party.Discipline - 1);
			Party.LoyaltyToGovernment = ClampPercent(Party.LoyaltyToGovernment - (Party.Role == EWLPartyRole::Ruling ? 0 : 1));
		}
	}
	const int32 AvgCoalitionLoyalty = CoalitionParties > 0 ? CoalitionLoyalty / CoalitionParties : 35;
	Institutions.RulingCoalitionSupport = ClampPercent((CoalitionSeats + AvgCoalitionLoyalty) / 2);
	Institutions.LegislativeOpposition = ClampPercent(OppositionSeats + FMath::Max(0, 60 - AvgCoalitionLoyalty) / 2);
}

void UWLPoliticalSubsystem::UpdateElectionForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLElectionState& Election = EnsureElectionState(Iso);
	FWLMediaPublicOpinionState& Media = EnsureMediaState(Iso);
	const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(Iso);
	const FWLPatronageState Patronage = GetPatronageState(Iso);
	int32 AverageGroupSupport = 50;
	const TArray<FWLPublicGroupSupportState> Groups = GetPublicGroups(Iso);
	if (!Groups.IsEmpty())
	{
		int32 Sum = 0;
		for (const FWLPublicGroupSupportState& Group : Groups)
		{
			Sum += Group.Support - Group.Pressure / 3;
		}
		AverageGroupSupport = FMath::Clamp(Sum / Groups.Num(), 0, 100);
	}

	Election.IncumbentApproval = ClampPercent((AverageGroupSupport + Media.PresidentialApproval + Institutions.RulingCoalitionSupport) / 3);
	Election.FraudRisk = ClampPercent(EnsureStateCapacity(Iso).Corruption / 2 + Media.MediaControl / 4 + Patronage.ClientelistPressure / 3);
	Election.AbstentionRisk = ClampPercent(35 - Election.Legitimacy / 3 + Media.FakeNewsPressure / 3 + Media.CensorshipBacklash / 4);
	Election.PollingGovernment = ClampPercent(Election.IncumbentApproval + Election.CampaignIntensity / 4 + Patronage.PatronagePower / 8);
	Election.PollingOpposition = ClampPercent(100 - Election.PollingGovernment + GetInternalPower(Iso).OppositionPopularity / 5);

	if (Election.MonthsToElection > 0)
	{
		--Election.MonthsToElection;
	}
	if (Election.MonthsToElection <= 12 && Election.MonthsToElection > 6)
	{
		Election.Phase = EWLElectionPhase::PreCampaign;
	}
	else if (Election.MonthsToElection <= 6 && Election.MonthsToElection > 0)
	{
		Election.Phase = EWLElectionPhase::Campaign;
		Election.CampaignIntensity = ClampPercent(Election.CampaignIntensity + 2);
	}
	else if (Election.MonthsToElection <= 0)
	{
		Election.Phase = EWLElectionPhase::Election;
		int32 GovernmentScore = Election.PollingGovernment + Election.Legitimacy / 5 - Election.FraudRisk / 3;
		int32 OppositionScore = Election.PollingOpposition + Election.AbstentionRisk / 4;
		FString ElectionNote;
		if (!Election.CampaignPromiseReformId.IsEmpty())
		{
			if (Election.bCampaignPromiseFulfilled || HasReformMemory(Iso, Election.CampaignPromiseReformId))
			{
				GovernmentScore += 8;
				Election.Legitimacy = ClampPercent(Election.Legitimacy + 3);
				ElectionNote += TEXT(" Promesa cumplida.");
			}
			else
			{
				GovernmentScore -= 12;
				OppositionScore += 5;
				Election.Legitimacy = ClampPercent(Election.Legitimacy - 6);
				ElectionNote += TEXT(" Promesa incumplida.");
				AddPoliticalMemory(Iso, TEXT("campaign_promise_broken"), 1, 24,
					FString::Printf(TEXT("%s no cumplio la promesa %s antes de la eleccion."),
						*Iso,
						*Election.CampaignPromiseReformId));
			}
		}

		const bool bConstitutionalTermRules = HasReformMemory(Iso, TEXT("constitution_term_rules"));
		if (Election.bTermLimited && !bConstitutionalTermRules)
		{
			GovernmentScore -= 18;
			OppositionScore += 8;
			ElectionNote += TEXT(" Limite de mandato fuerza candidato sucesor.");
			AddPoliticalMemory(Iso, TEXT("term_limit_pressure"), 1, 18,
				FString::Printf(TEXT("%s llega a eleccion con limite de mandato."), *Iso));
		}
		Election.bLastElectionWon = GovernmentScore >= OppositionScore;
		if (Election.bLastElectionWon)
		{
			Election.ConsecutiveTermsWon = FMath::Max(1, Election.ConsecutiveTermsWon + 1);
			Election.LastElectionReport = FString::Printf(TEXT("%s gana la eleccion (%d vs %d).%s"),
				*Iso, GovernmentScore, OppositionScore, *ElectionNote);
			Election.Legitimacy = ClampPercent(Election.Legitimacy + 8 - Election.FraudRisk / 8);
			Election.Phase = EWLElectionPhase::Transition;
		}
		else
		{
			Election.ConsecutiveTermsWon = 0;
			Election.LastElectionReport = FString::Printf(TEXT("%s pierde la eleccion (%d vs %d).%s"),
				*Iso, GovernmentScore, OppositionScore, *ElectionNote);
			Election.Legitimacy = ClampPercent(Election.Legitimacy - 10);
			Election.Phase = Election.FraudRisk >= 55 ? EWLElectionPhase::Crisis : EWLElectionPhase::Transition;
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::SoftCoup, 45, Election.LastElectionReport);
			const FString PlayerIso = GetPlayerNationIso();
			if (!PlayerIso.IsEmpty() && PlayerIso == Iso)
			{
				CampaignOutcome.bGameOver = true;
				CampaignOutcome.OutcomeType = TEXT("ElectionDefeat");
				CampaignOutcome.LosingNationIso = Iso;
				CampaignOutcome.Reason = Election.LastElectionReport;
			}
		}
		const int32 MaxConsecutiveTerms = bConstitutionalTermRules ? 3 : 2;
		Election.bTermLimited = Election.ConsecutiveTermsWon >= MaxConsecutiveTerms;
		Election.MonthsToElection = ElectionCycleMonths;
		Election.CampaignIntensity = 0;
		Election.CampaignPromiseReformId.Reset();
		Election.bCampaignPromiseFulfilled = false;
		AddGovernmentLogEntry(EWLGovernmentLogCategory::Government, Iso, TEXT(""),
			TEXT("Resultado electoral"), Election.LastElectionReport, TEXT("election"), 6, true, Iso == GetPlayerNationIso());
	}
	else
	{
		Election.Phase = EWLElectionPhase::Governing;
		Election.LastElectionReport = FString::Printf(TEXT("%s gobierna; eleccion en %d meses."), *Iso, Election.MonthsToElection);
	}
}

void UWLPoliticalSubsystem::UpdateCharacterProfilesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	SeedCharacterProfilesForNation(Iso);
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		for (const FWLCharacter& Character : Characters->GetCharactersByNation(Iso))
		{
			FWLCharacterPoliticalProfile& Profile = EnsureCharacterPoliticalProfile(Character);
			Profile.PresidentialAmbition = ClampPercent(Character.Ambition + Character.Popularity / 5 + Character.Renown / 5);
			Profile.PersonalCorruption = ClampPercent(Profile.PersonalCorruption + (Character.Traits.Contains(TEXT("corrupto")) ? 2 : -1));
			Profile.ScandalHeat = ClampPercent(Profile.ScandalHeat + Profile.PersonalCorruption / 25 + EnsurePatronageState(Iso).ContractCorruption / 30 - 2);
			Profile.SuccessionScore = ClampPercent(Character.Popularity / 2 + Character.Renown / 3 + Profile.PresidentialAmbition / 4 - Profile.ScandalHeat / 5);
			if (Character.Role == EWLCharacterRole::Minister && Character.AssignedOffice != EWLMinisterOffice::None)
			{
				Profile.SuccessionScore = ClampPercent(Profile.SuccessionScore + Character.Skill / 8 + Character.Loyalty / 12);
				Profile.LastProfileEvent = FString::Printf(TEXT("%s gana peso politico desde %s."),
					*Character.Name,
					*UWLCharacterSubsystem::MinisterOfficeToString(Character.AssignedOffice));
			}
			else if (Character.Role == EWLCharacterRole::Opposition)
			{
				Profile.SuccessionScore = ClampPercent(Profile.SuccessionScore + GetInternalPower(Iso).OppositionPopularity / 8);
			}
			if (Profile.ScandalHeat >= 70)
			{
				Profile.LastProfileEvent = FString::Printf(TEXT("%s acumula expediente de escandalo."), *Character.Name);
				AddPoliticalMemory(Iso, TEXT("personal_scandal"), 1, 8, Profile.LastProfileEvent);
				TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::CorruptionScandal, Profile.ScandalHeat, Profile.LastProfileEvent);
			}
		}
	}
}

void UWLPoliticalSubsystem::UpdatePatronageForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPatronageState& Patronage = EnsurePatronageState(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower - 1 + Patronage.RegionalMachines / 35);
	Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure - 1 + Patronage.PatronagePower / 40);
	Patronage.ConcessionBacklash = ClampPercent(Patronage.ConcessionBacklash - 1);
	Capacity.Corruption = ClampPercent(Capacity.Corruption + Patronage.ContractCorruption / 40 + Patronage.ClientelistPressure / 60);
	if (Patronage.ContractCorruption + Patronage.ConcessionBacklash >= 85)
	{
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::CorruptionScandal,
			Patronage.ContractCorruption + Patronage.ConcessionBacklash,
			TEXT("Contratos y concesiones alimentan escandalo de patronazgo."));
	}
}

void UWLPoliticalSubsystem::UpdateMediaForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLMediaPublicOpinionState& Media = EnsureMediaState(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	Media.PresidentialApproval = ClampPercent(
		Media.PresidentialApproval
		+ (GetAveragePublicOrder(Iso) - 55) / 15
		+ Media.PropagandaReach / 40
		- Internal.OppositionStrength / 35
		- Media.CensorshipBacklash / 35);
	Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure - 1 + Internal.OppositionPopularity / 45);
	Media.MediaCrisisRisk = ClampPercent(Internal.OppositionPopularity / 2 + Media.FakeNewsPressure / 2 + Media.CensorshipBacklash / 2 - Media.PressFreedom / 5);
	Media.PropagandaReach = ClampPercent(Media.PropagandaReach - 2);
	Media.CensorshipBacklash = ClampPercent(Media.CensorshipBacklash - 1 + FMath::Max(0, Media.MediaControl - 65) / 20);
	if (Media.MediaCrisisRisk >= 70)
	{
		Media.LastNarrative = FString::Printf(TEXT("%s enfrenta crisis mediatica."), *Iso);
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Media.MediaCrisisRisk, Media.LastNarrative);
	}
	else
	{
		Media.LastNarrative = FString::Printf(TEXT("Aprobacion %d, crisis mediatica %d."),
			Media.PresidentialApproval, Media.MediaCrisisRisk);
	}
}

void UWLPoliticalSubsystem::UpdateRegionsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	SeedRegionsForNation(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	for (FWLRegionGovernorState& Region : RegionGovernors)
	{
		if (Region.NationIso != Iso)
		{
			continue;
		}
		const int32 Authority = Capacity.CentralAuthority;
		Region.Obedience = ClampPercent(Region.Obedience + (Authority - 50) / 20 - Region.Autonomy / 35 + Region.InvestmentLevel / 30);
		Region.ProtestRisk = ClampPercent(Region.ProtestRisk + FMath::Max(0, 55 - Region.Obedience) / 8 - Region.InvestmentLevel / 30);
		Region.SecessionRisk = ClampPercent(Region.Autonomy / 2 + FMath::Max(0, 45 - Region.Obedience) + Region.ProtestRisk / 3 - Region.CenterControl / 3);
		Region.RebellionRisk = ClampPercent(Region.ProtestRisk / 2 + Region.SecessionRisk / 2 - Authority / 5);
		Region.InvestmentLevel = ClampPercent(Region.InvestmentLevel - 1);
		Region.LastReport = FString::Printf(TEXT("%s: obediencia %d, protesta %d, secesion %d."),
			*Region.RegionName, Region.Obedience, Region.ProtestRisk, Region.SecessionRisk);
		if (Region.RebellionRisk >= 70)
		{
			TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Region.RebellionRisk, Region.LastReport);
		}
	}
}

FWLCrisisChainState& UWLPoliticalSubsystem::EnsureCrisisChain(
	const FString& NationIso,
	EWLCrisisChainType Type,
	const FString& Reason)
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Key = CrisisChainKey(Iso, Type);
	for (FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (!Crisis.bResolved && Crisis.CrisisId == Key)
		{
			return Crisis;
		}
	}

	FWLCrisisChainState Crisis;
	Crisis.NationIso = Iso;
	Crisis.CrisisId = Key;
	Crisis.Type = Type;
	Crisis.Stage = 1;
	Crisis.Intensity = 20;
	Crisis.LastReport = Reason;
	CrisisChains.Add(Crisis);
	return CrisisChains.Last();
}

void UWLPoliticalSubsystem::TriggerGovernmentP2Crisis(
	const FString& NationIso,
	EWLCrisisChainType Type,
	int32 Intensity,
	const FString& Reason)
{
	FWLCrisisChainState& Crisis = EnsureCrisisChain(NationIso, Type, Reason);
	Crisis.Intensity = ClampPercent(Crisis.Intensity + FMath::Max(5, Intensity / 4));
	Crisis.LastReport = Reason;
	AddPoliticalMemory(NationIso, TEXT("p2_crisis"), 1, 8, Reason);
}

void UWLPoliticalSubsystem::UpdateCrisisChainsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (Crisis.NationIso != Iso || Crisis.bResolved)
		{
			continue;
		}
		++Crisis.MonthsActive;
		Crisis.Intensity = ClampPercent(Crisis.Intensity + GetInternalPower(Iso).OppositionStrength / 30 + EnsureMediaState(Iso).MediaCrisisRisk / 40 - EnsureStateCapacity(Iso).AdministrativeEfficiency / 35);
		if (Crisis.Intensity >= 80)
		{
			Crisis.Stage = FMath::Min(5, Crisis.Stage + 1);
			if (UWLStrategicTickSubsystem* Tick = GetTick())
			{
				Tick->AdjustNationPublicOrder(Iso, -2);
			}
			EnsureInternalPower(Iso).OppositionStrength = ClampPercent(EnsureInternalPower(Iso).OppositionStrength + 3);
		}
		else if (Crisis.Intensity <= 20 && Crisis.MonthsActive > 2)
		{
			Crisis.bResolved = true;
			Crisis.LastReport = FString::Printf(TEXT("Crisis %s resuelta por desgaste."), *Crisis.CrisisId);
			continue;
		}
		Crisis.LastReport = FString::Printf(TEXT("Crisis %s etapa %d intensidad %d."),
			*Crisis.CrisisId, Crisis.Stage, Crisis.Intensity);
		if (Crisis.Stage >= 4)
		{
			if (Crisis.Type == EWLCrisisChainType::Impeachment || Crisis.Type == EWLCrisisChainType::SoftCoup)
			{
				EnsureInstitutionalPower(Iso).GridlockRisk = ClampPercent(EnsureInstitutionalPower(Iso).GridlockRisk + 8);
				EnsureInternalPower(Iso).CoupRisk = ClampPercent(EnsureInternalPower(Iso).CoupRisk + 8);
			}
			else if (Crisis.Type == EWLCrisisChainType::DebtCrisis)
			{
				EnsureMediaState(Iso).PresidentialApproval = ClampPercent(EnsureMediaState(Iso).PresidentialApproval - 5);
			}
			else if (UWLStrategicTickSubsystem* Tick = GetTick())
			{
				Tick->AdjustNationPublicOrder(Iso, -1);
			}
		}
	}
}

void UWLPoliticalSubsystem::UpdateGovernmentCalibrationForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentCalibrationState& Calibration = EnsureGovernmentCalibration(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(Iso);
	const FWLMediaPublicOpinionState Media = GetMediaPublicOpinion(Iso);
	const FWLPatronageState Patronage = GetPatronageState(Iso);
	const FWLStateCapacityState Capacity = GetStateCapacity(Iso);
	++Calibration.MonthsObserved;
	if (const UWLStrategicTickSubsystem* Tick = GetTick())
	{
		const int64 ClampedTreasury = FMath::Clamp(Tick->GetTreasury(Iso), static_cast<int64>(0), static_cast<int64>(50000));
		Calibration.DebtVsGrowthPressure = ClampPercent(
			(Tick->GetMonthlyBalance(Iso) < 0 ? 35 : 10)
			+ static_cast<int32>((50000 - ClampedTreasury) / 2000));
		Calibration.SubsidyInflationPressure = ClampPercent(GetPoliticalMemoryValue(Iso, TEXT("program_started")) * 4 + FMath::Max(0, 45 - Tick->GetTaxRate(Iso)));
	}
	Calibration.RepressionLegitimacyPressure = ClampPercent(GetPoliticalMemoryValue(Iso, TEXT("recent_repression")) * 18 + FMath::Max(0, 50 - Media.PresidentialApproval));
	Calibration.ReformGridlockPressure = ClampPercent(Institutions.GridlockRisk + GetPoliticalMemoryValue(Iso, TEXT("reform_blocked")) * 10);
	bool bLowMilitarySupport = false;
	for (const FWLPublicGroupSupportState& Group : GetPublicGroups(Iso))
	{
		if (Group.Group == EWLPublicGroup::Military && Group.Support < 45)
		{
			bLowMilitarySupport = true;
			break;
		}
	}
	Calibration.CivilMilitaryTension = ClampPercent((bLowMilitarySupport ? 35 : 10) + Internal.CoupRisk / 2);
	Calibration.SovereigntyAlliancePressure = ClampPercent(Patronage.ConcessionBacklash + FMath::Max(0, 45 - Capacity.CentralAuthority));
	Calibration.LastReport = FString::Printf(TEXT("Calibracion %s: deuda %d, represion %d, gridlock %d, civil-militar %d."),
		*Iso,
		Calibration.DebtVsGrowthPressure,
		Calibration.RepressionLegitimacyPressure,
		Calibration.ReformGridlockPressure,
		Calibration.CivilMilitaryTension);
}

void UWLPoliticalSubsystem::UpdateCabinetDynamicsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLCabinetDynamicsState& Dynamics = EnsureCabinetDynamics(Iso);
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return;
	}

	int32 AmbitionPressure = 0;
	int32 LowLoyaltyPressure = 0;
	int32 CorruptTraits = 0;
	int32 Ministers = 0;
	FWLCharacter LowestLoyaltyMinister;
	bool bHasLowest = false;
	for (const FWLCabinetSeat& Seat : Characters->GetCabinet(Iso))
	{
		if (Seat.CharacterId.IsEmpty())
		{
			continue;
		}
		const FWLCharacter& Minister = Seat.Minister;
		AmbitionPressure += FMath::Max(0, Minister.Ambition - 45);
		LowLoyaltyPressure += FMath::Max(0, 55 - Minister.Loyalty);
		if (Minister.Traits.Contains(TEXT("corrupto")) || Minister.Traits.Contains(TEXT("clientelista")))
		{
			++CorruptTraits;
		}
		if (!bHasLowest || Minister.Loyalty < LowestLoyaltyMinister.Loyalty)
		{
			LowestLoyaltyMinister = Minister;
			bHasLowest = true;
		}
		++Ministers;
	}

	const int32 AvgAmbition = Ministers > 0 ? AmbitionPressure / Ministers : 0;
	const int32 AvgLowLoyalty = Ministers > 0 ? LowLoyaltyPressure / Ministers : 0;
	Dynamics.RivalryPressure = ClampPercent(AvgAmbition + AvgLowLoyalty / 2);
	Dynamics.Factionalism = ClampPercent(Dynamics.RivalryPressure + CorruptTraits * 8);
	Dynamics.ScandalRisk = ClampPercent(Dynamics.Factionalism / 2 + CorruptTraits * 12 + GetStateCapacity(Iso).Corruption / 3);
	Dynamics.SabotageRisk = ClampPercent(Dynamics.RivalryPressure + AvgLowLoyalty);
	Dynamics.ResignationRisk = ClampPercent(AvgLowLoyalty + Dynamics.ScandalRisk / 2);

	if (Dynamics.ScandalRisk >= 75)
	{
		Dynamics.LastIncident = FString::Printf(TEXT("%s enfrenta rumores de escandalo de gabinete."), *Iso);
		AddPoliticalMemory(Iso, TEXT("cabinet_scandal"), 1, 8, Dynamics.LastIncident);
		EnsureInternalPower(Iso).OppositionStrength = ClampPercent(EnsureInternalPower(Iso).OppositionStrength + 2);
	}
	else if (Dynamics.SabotageRisk >= 70)
	{
		Dynamics.LastIncident = FString::Printf(TEXT("%s detecta sabotaje burocratico entre ministros."), *Iso);
		AddPoliticalMemory(Iso, TEXT("cabinet_sabotage"), 1, 6, Dynamics.LastIncident);
		for (FWLMinistryProgramState& Program : ActiveMinistryPrograms)
		{
			if (Program.NationIso == Iso)
			{
				Program.bBlocked = true;
				Program.LastReport = Dynamics.LastIncident;
				break;
			}
		}
	}
	else if (Dynamics.ResignationRisk >= 95 && bHasLowest)
	{
		if (UWLCharacterSubsystem* MutableCharacters = GetCharacters())
		{
			FString RetireMessage;
			if (MutableCharacters->RetireCharacter(LowestLoyaltyMinister.Id, RetireMessage))
			{
				Dynamics.LastIncident = FString::Printf(TEXT("%s renuncia al gabinete de %s."),
					*LowestLoyaltyMinister.Name, *Iso);
				AddPoliticalMemory(Iso, TEXT("minister_resigned"), 1, 10, Dynamics.LastIncident);
			}
		}
	}
	else
	{
		Dynamics.LastIncident = TEXT("Gabinete operativo.");
	}
}

void UWLPoliticalSubsystem::UpdateInstitutionalPowerForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);

	int32 AverageGroupSupport = 50;
	const TArray<FWLPublicGroupSupportState> Groups = GetPublicGroups(Iso);
	if (!Groups.IsEmpty())
	{
		int32 Sum = 0;
		for (const FWLPublicGroupSupportState& Group : Groups)
		{
			Sum += Group.Support;
		}
		AverageGroupSupport = Sum / Groups.Num();
	}

	int32 CoalitionSeats = 0;
	int32 OppositionSeats = 0;
	for (const FWLPartyState& Party : GetPoliticalParties(Iso))
	{
		if (Party.bInCoalition || Party.Role == EWLPartyRole::Ruling || Party.Role == EWLPartyRole::Ally)
		{
			CoalitionSeats += Party.Seats;
		}
		else
		{
			OppositionSeats += Party.Seats;
		}
	}
	const FWLPatronageState Patronage = GetPatronageState(Iso);

	Institutions.RulingCoalitionSupport = ClampPercent(
		Institutions.RulingCoalitionSupport
		+ (AverageGroupSupport - 50) / 12
		+ (CoalitionSeats - 50) / 10
		+ Patronage.PatronagePower / 35
		- Internal.OppositionStrength / 40);
	Institutions.LegislativeOpposition = ClampPercent(
		100 - Institutions.RulingCoalitionSupport + Internal.OppositionStrength / 5 + OppositionSeats / 12);
	Institutions.GridlockRisk = ClampPercent(
		Institutions.LegislativeOpposition - Institutions.RulingCoalitionSupport / 2
		+ GetPoliticalMemoryValue(Iso, TEXT("reform_blocked")) * 10
		- Patronage.ClientelistPressure / 8);
	Institutions.ReformCost = FMath::Clamp(8 + Institutions.GridlockRisk / 5, 6, 30);
}

void UWLPoliticalSubsystem::UpdateStateCapacityForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);

	int32 CabinetSkill = 55;
	int32 CabinetCorruptionPressure = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
		if (Stats.FilledOffices > 0)
		{
			CabinetSkill = Stats.AverageSkill;
			CabinetCorruptionPressure = Stats.Corruption / 6;
		}
	}

	Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + (CabinetSkill - 50) / 20);
	const FWLPatronageState Patronage = GetPatronageState(Iso);
	int32 AverageRegionalObedience = 55;
	const TArray<FWLRegionGovernorState> Regions = GetRegionGovernors(Iso);
	if (!Regions.IsEmpty())
	{
		int32 Sum = 0;
		for (const FWLRegionGovernorState& Region : Regions)
		{
			Sum += Region.Obedience;
		}
		AverageRegionalObedience = Sum / Regions.Num();
	}
	Capacity.Corruption = ClampPercent(
		Capacity.Corruption
		+ CabinetCorruptionPressure / 5
		+ Patronage.ContractCorruption / 50
		+ Patronage.ClientelistPressure / 70
		- Capacity.Bureaucracy / 30);
	Capacity.AdministrativeEfficiency = ClampPercent(
		40 + Capacity.Bureaucracy / 2 + CabinetSkill / 3 + Internal.AveragePublicOrder / 5
		+ AverageRegionalObedience / 8
		- Capacity.Corruption / 2);
	Capacity.CentralAuthority = ClampPercent(
		Capacity.CentralAuthority
		+ (Internal.AveragePublicOrder - 55) / 20
		+ (AverageRegionalObedience - 55) / 18
		- Internal.OppositionStrength / 35);
	Capacity.PolicyFailureRisk = ClampPercent(
		70 - Capacity.AdministrativeEfficiency / 2 + Capacity.Corruption / 2
		+ EnsureInstitutionalPower(Iso).GridlockRisk / 4
		+ GetPoliticalMemoryValue(Iso, TEXT("policy_failure")) * 5);
	Capacity.LastReport = FString::Printf(TEXT("Capacidad estatal %s: burocracia %d, corrupcion %d, eficiencia %d, fallo %d."),
		*Iso, Capacity.Bureaucracy, Capacity.Corruption, Capacity.AdministrativeEfficiency, Capacity.PolicyFailureRisk);
}

void UWLPoliticalSubsystem::UpdatePublicGroupsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	for (EWLPublicGroup Group : AllPublicGroups())
	{
		FWLPublicGroupSupportState& State = EnsurePublicGroup(Iso, Group);
		const int32 Disorder = FMath::Max(0, 60 - Internal.AveragePublicOrder);
		State.Pressure = ClampPercent(50 - State.Support + Disorder / 2);
		if (Internal.AveragePublicOrder >= 65 && State.Support < 55)
		{
			State.Support = ClampPercent(State.Support + 1);
			State.LastShiftReason = TEXT("normalizacion por orden publico");
		}
		else if (Internal.AveragePublicOrder <= 45)
		{
			State.Support = ClampPercent(State.Support - 1);
			State.LastShiftReason = TEXT("desgaste por crisis de orden publico");
		}
	}
}

void UWLPoliticalSubsystem::AdvancePoliticalMemoryForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	TArray<FString> RemoveKeys;
	for (TPair<FString, FWLPoliticalMemoryRecord>& Pair : PoliticalMemoryByKey)
	{
		FWLPoliticalMemoryRecord& Memory = Pair.Value;
		if (Memory.NationIso != Iso)
		{
			continue;
		}
		Memory.MonthsRemaining = FMath::Max(0, Memory.MonthsRemaining - 1);
		if (Memory.MonthsRemaining <= 0 || Memory.Value == 0)
		{
			RemoveKeys.Add(Pair.Key);
		}
	}
	for (const FString& Key : RemoveKeys)
	{
		PoliticalMemoryByKey.Remove(Key);
	}
}

void UWLPoliticalSubsystem::QueueGovernmentCrisisEventsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	auto QueueCrisis = [this, &Iso](const FString& EventId, const FString& Title, const FString& Body, const TArray<FWLPoliticalEventOption>& Options)
	{
		if (HasQueuedUnresolvedEvent(Iso, EventId))
		{
			return;
		}
		FWLPoliticalEventInstance Event;
		Event.InstanceId = FString::Printf(TEXT("EV-%04d"), NextEventInstanceNumber++);
		Event.EventId = EventId;
		Event.NationIso = Iso;
		Event.Title = Title;
		Event.Body = Body;
		Event.Options = Options;
		EventQueue.Add(MoveTemp(Event));
		if (Iso == GetPlayerNationIso())
		{
			AddGovernmentLogEntry(EWLGovernmentLogCategory::Crisis, Iso, TEXT(""),
				TEXT("Crisis en desarrollo"),
				FString::Printf(TEXT("%s."), *Title),
				TEXT("crisis_event"), 7, true, true);
		}
	};

	if (GetPoliticalMemoryValue(Iso, TEXT("crisis_unrest")) >= 2)
	{
		FWLPoliticalEventOption Negotiate;
		Negotiate.OptionId = TEXT("negotiate");
		Negotiate.Label = TEXT("Negociar con lideres sociales");
		Negotiate.PoliticalCapitalDelta = -8;
		Negotiate.TreasuryDelta = -1500;
		Negotiate.OppositionDelta = -8;
		Negotiate.PublicOrderDelta = 2;

		FWLPoliticalEventOption Repress;
		Repress.OptionId = TEXT("repress");
		Repress.Label = TEXT("Romper la huelga");
		Repress.OppositionDelta = -12;
		Repress.PublicOrderDelta = -4;
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, 45, TEXT("Protesta escala hacia huelga nacional."));
		QueueCrisis(TEXT("crisis_strike"), TEXT("Huelga nacional"), TEXT("Las protestas escalan a paro coordinado."), { Negotiate, Repress });
	}

	if (GetPoliticalMemoryValue(Iso, TEXT("cabinet_scandal")) >= 2)
	{
		FWLPoliticalEventOption Investigate;
		Investigate.OptionId = TEXT("investigate");
		Investigate.Label = TEXT("Abrir investigacion");
		Investigate.PoliticalCapitalDelta = -6;
		Investigate.OppositionDelta = -5;
		Investigate.PublicOrderDelta = 1;

		FWLPoliticalEventOption Cover;
		Cover.OptionId = TEXT("cover_up");
		Cover.Label = TEXT("Tapar el escandalo");
		Cover.OppositionDelta = 8;
		Cover.PublicOrderDelta = -2;
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::CorruptionScandal, 50, TEXT("Escandalo de gabinete se vuelve crisis nacional."));
		QueueCrisis(TEXT("cabinet_scandal_chain"), TEXT("Escandalo de gabinete"), TEXT("La memoria politica de malos manejos abre una crisis."), { Investigate, Cover });
	}

	auto CrisisTypeTitle = [](EWLCrisisChainType Type) -> FString
	{
		switch (Type)
		{
		case EWLCrisisChainType::NationalProtest: return TEXT("Protesta nacional");
		case EWLCrisisChainType::CorruptionScandal: return TEXT("Escandalo de corrupcion");
		case EWLCrisisChainType::MilitaryCrisis: return TEXT("Crisis militar");
		case EWLCrisisChainType::DebtCrisis: return TEXT("Crisis de deuda");
		case EWLCrisisChainType::StudentProtest: return TEXT("Protesta estudiantil");
		case EWLCrisisChainType::OilStrike: return TEXT("Crisis petrolera");
		case EWLCrisisChainType::BorderCrisis: return TEXT("Crisis fronteriza");
		case EWLCrisisChainType::Impeachment: return TEXT("Juicio politico");
		case EWLCrisisChainType::SoftCoup: return TEXT("Golpe blando");
		case EWLCrisisChainType::StateOfException: return TEXT("Estado de excepcion");
		default: return TEXT("Crisis politica");
		}
	};

	for (const FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (Crisis.NationIso != Iso || Crisis.bResolved || (Crisis.Stage < 3 && Crisis.Intensity < 70))
		{
			continue;
		}
		FString SanitizedId = Crisis.CrisisId;
		SanitizedId.ReplaceInline(TEXT("|"), TEXT("_"));
		const FString EventId = FString::Printf(TEXT("crisis_%s_stage_%d"), *SanitizedId, Crisis.Stage);
		if (HasQueuedUnresolvedEvent(Iso, EventId))
		{
			continue;
		}

		FWLPoliticalEventOption Contain;
		Contain.OptionId = TEXT("contain");
		Contain.Label = TEXT("Contener con negociacion y recursos");
		Contain.PoliticalCapitalDelta = -6;
		Contain.TreasuryDelta = -1200;
		Contain.OppositionDelta = -6;
		Contain.PublicOrderDelta = 1;

		FWLPoliticalEventOption Hardline;
		Hardline.OptionId = TEXT("hardline");
		Hardline.Label = TEXT("Responder con linea dura");
		Hardline.TreasuryDelta = -800;
		Hardline.OppositionDelta = -10;
		Hardline.PublicOrderDelta = -3;

		const FString Title = FString::Printf(TEXT("%s etapa %d"), *CrisisTypeTitle(Crisis.Type), Crisis.Stage);
		const FString Body = FString::Printf(TEXT("%s alcanza intensidad %d. Si se ignora, erosiona orden publico y legitimidad."),
			*Crisis.LastReport,
			Crisis.Intensity);
		QueueCrisis(EventId, Title, Body, { Contain, Hardline });
	}
}

void UWLPoliticalSubsystem::AddPoliticalMemory(
	const FString& NationIso,
	const FString& MemoryKey,
	int32 Delta,
	int32 Months,
	const FString& Reason)
{
	const FString Iso = NormalizeIso(NationIso);
	if (Iso.IsEmpty() || MemoryKey.TrimStartAndEnd().IsEmpty())
	{
		return;
	}
	const FString Key = PoliticalMemoryKey(Iso, MemoryKey);
	FWLPoliticalMemoryRecord& Memory = PoliticalMemoryByKey.FindOrAdd(Key);
	if (Memory.NationIso.IsEmpty())
	{
		Memory.NationIso = Iso;
		Memory.MemoryKey = MemoryKey.TrimStartAndEnd().ToLower();
	}
	Memory.Value = FMath::Clamp(Memory.Value + Delta, -100, 100);
	Memory.MonthsRemaining = FMath::Max(Memory.MonthsRemaining, Months);
	Memory.LastReason = Reason;
}

int32 UWLPoliticalSubsystem::GetPoliticalMemoryValue(const FString& NationIso, const FString& MemoryKey) const
{
	if (const FWLPoliticalMemoryRecord* Found = PoliticalMemoryByKey.Find(PoliticalMemoryKey(NationIso, MemoryKey)))
	{
		return Found->MonthsRemaining > 0 ? Found->Value : 0;
	}
	return 0;
}

