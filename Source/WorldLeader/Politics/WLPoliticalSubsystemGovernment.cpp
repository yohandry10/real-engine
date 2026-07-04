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

bool UWLPoliticalSubsystem::SetGovernmentAgenda(
	const FString& NationIso,
	const TArray<EWLGovernmentPriority>& Priorities,
	FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::SetAgenda;
	Request.Priorities = Priorities;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::ApplyGovernmentAgendaDirect(
	const FString& NationIso,
	const TArray<EWLGovernmentPriority>& Priorities,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para agenda de gobierno.");
		return false;
	}
	if (Priorities.IsEmpty() || Priorities.Num() > MaxAgendaPriorities)
	{
		OutMessage = FString::Printf(TEXT("La agenda debe tener entre 1 y %d prioridades."), MaxAgendaPriorities);
		return false;
	}

	TArray<EWLGovernmentPriority> UniquePriorities;
	for (EWLGovernmentPriority Priority : Priorities)
	{
		if (!UniquePriorities.Contains(Priority))
		{
			UniquePriorities.Add(Priority);
		}
	}
	if (UniquePriorities.IsEmpty())
	{
		OutMessage = TEXT("Agenda vacia.");
		return false;
	}

	FWLGovernmentAgendaState& Agenda = EnsureGovernmentAgenda(Iso);
	Agenda.Priorities = MoveTemp(UniquePriorities);
	Agenda.MonthsActive = 0;
	TArray<FString> Labels;
	for (EWLGovernmentPriority Priority : Agenda.Priorities)
	{
		Labels.Add(GovernmentPriorityToString(Priority));
	}
	Agenda.LastAgendaReport = FString::Printf(TEXT("%s define agenda: %s."),
		*Iso, *FString::Join(Labels, TEXT(", ")));
	OutMessage = Agenda.LastAgendaReport;
	AddPoliticalMemory(Iso, TEXT("agenda_changed"), 1, 6, OutMessage);
	return true;
}

TArray<FWLMinistryProgramDefinition> UWLPoliticalSubsystem::GetAvailableMinistryPrograms(const FString& NationIso) const
{
	TArray<FWLMinistryProgramDefinition> Out;
	if (!ValidateNation(NationIso))
	{
		return Out;
	}
	Out = GovernmentProgramDefinitions();
	Out.Sort([](const FWLMinistryProgramDefinition& A, const FWLMinistryProgramDefinition& B)
	{
		if (A.Office != B.Office)
		{
			return static_cast<int32>(A.Office) < static_cast<int32>(B.Office);
		}
		return A.ProgramId < B.ProgramId;
	});
	return Out;
}

TArray<FWLMinistryProgramState> UWLPoliticalSubsystem::GetActiveMinistryPrograms(const FString& NationIso) const
{
	TArray<FWLMinistryProgramState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLMinistryProgramState& Program : ActiveMinistryPrograms)
	{
		if (Program.NationIso == Iso && Program.RemainingMonths > 0)
		{
			Out.Add(Program);
		}
	}
	Out.Sort([](const FWLMinistryProgramState& A, const FWLMinistryProgramState& B)
	{
		return A.ProgramId < B.ProgramId;
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetMinistryProgramDefinition(
	const FString& ProgramId,
	FWLMinistryProgramDefinition& OutDefinition) const
{
	const FString Id = ProgramId.TrimStartAndEnd().ToLower();
	for (const FWLMinistryProgramDefinition& Definition : GovernmentProgramDefinitions())
	{
		if (Definition.ProgramId == Id)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	return false;
}

bool UWLPoliticalSubsystem::StartMinistryProgram(const FString& NationIso, const FString& ProgramId, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::StartProgram;
	Request.PrimaryId = ProgramId;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::StartMinistryProgramDirect(const FString& NationIso, const FString& ProgramId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para programa ministerial.");
		return false;
	}

	FWLMinistryProgramDefinition Definition;
	if (!GetMinistryProgramDefinition(ProgramId, Definition))
	{
		OutMessage = FString::Printf(TEXT("Programa desconocido: %s"), *ProgramId);
		return false;
	}
	for (const FWLMinistryProgramState& Active : ActiveMinistryPrograms)
	{
		if (Active.NationIso == Iso && Active.Office == Definition.Office && Active.RemainingMonths > 0)
		{
			OutMessage = FString::Printf(TEXT("%s ya ejecuta un programa de %s."),
				*Iso, *UWLCharacterSubsystem::MinisterOfficeToString(Definition.Office));
			return false;
		}
	}

	if (Definition.bRequiresLegislation)
	{
		FString VoteMessage;
		if (!PassGovernmentReformDirect(Iso, Definition.ProgramId, GetEffectiveReformCost(Iso, Definition.PoliticalCapitalCost), false, VoteMessage))
		{
			OutMessage = VoteMessage;
			return false;
		}
	}

	FWLMinistryProgramState Program;
	Program.NationIso = Iso;
	Program.ProgramId = Definition.ProgramId;
	Program.Name = Definition.Name;
	Program.Office = Definition.Office;
	Program.RemainingMonths = FMath::Max(1, Definition.DurationMonths);
	Program.Progress = 0;
	Program.LastReport = FString::Printf(TEXT("%s inicia %s (%s, %d meses)."),
		*Iso, *Definition.Name, *UWLCharacterSubsystem::MinisterOfficeToString(Definition.Office), Program.RemainingMonths);
	ActiveMinistryPrograms.Add(Program);
	OutMessage = Program.LastReport;
	AddPoliticalMemory(Iso, TEXT("program_started"), 1, Program.RemainingMonths + 3, Program.LastReport);
	return true;
}

FWLCabinetDynamicsState UWLPoliticalSubsystem::GetCabinetDynamics(const FString& NationIso) const
{
	if (const FWLCabinetDynamicsState* Found = CabinetDynamicsByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLCabinetDynamicsState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

FWLInstitutionalPowerState UWLPoliticalSubsystem::GetInstitutionalPower(const FString& NationIso) const
{
	if (const FWLInstitutionalPowerState* Found = InstitutionalPowerByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLInstitutionalPowerState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

TArray<FWLPublicGroupSupportState> UWLPoliticalSubsystem::GetPublicGroups(const FString& NationIso) const
{
	TArray<FWLPublicGroupSupportState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLPublicGroupSupportState>& Pair : PublicGroupSupportByKey)
	{
		if (Pair.Value.NationIso == Iso)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLPublicGroupSupportState& A, const FWLPublicGroupSupportState& B)
	{
		return static_cast<int32>(A.Group) < static_cast<int32>(B.Group);
	});
	return Out;
}

FWLStateCapacityState UWLPoliticalSubsystem::GetStateCapacity(const FString& NationIso) const
{
	if (const FWLStateCapacityState* Found = StateCapacityByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLStateCapacityState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

TArray<FWLPoliticalMemoryRecord> UWLPoliticalSubsystem::GetPoliticalMemory(const FString& NationIso) const
{
	TArray<FWLPoliticalMemoryRecord> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLPoliticalMemoryRecord>& Pair : PoliticalMemoryByKey)
	{
		if (Pair.Value.NationIso == Iso && Pair.Value.MonthsRemaining > 0 && Pair.Value.Value != 0)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLPoliticalMemoryRecord& A, const FWLPoliticalMemoryRecord& B)
	{
		return A.MemoryKey < B.MemoryKey;
	});
	return Out;
}

FWLPoliticalAIPlanState UWLPoliticalSubsystem::GetGovernmentAIPlan(const FString& NationIso) const
{
	if (const FWLPoliticalAIPlanState* Found = GovernmentAIPlanByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLPoliticalAIPlanState Plan;
	Plan.NationIso = NormalizeIso(NationIso);
	return Plan;
}

bool UWLPoliticalSubsystem::PassGovernmentReform(
	const FString& NationIso,
	const FString& ReformId,
	int32 PoliticalCapitalCost,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	const int32 EffectiveCost = GetEffectiveReformCost(Iso, PoliticalCapitalCost);
	return PassGovernmentReformDirect(Iso, ReformId, EffectiveCost, true, OutMessage);
}

bool UWLPoliticalSubsystem::PassGovernmentReformDirect(
	const FString& NationIso,
	const FString& ReformId,
	int32 EffectiveCost,
	bool bSpendCapital,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para reforma.");
		return false;
	}
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (bSpendCapital && (!Characters || Characters->GetPoliticalCapital(Iso) < EffectiveCost))
	{
		OutMessage = FString::Printf(TEXT("Capital politico insuficiente para reforma %s (%d requerido)."),
			*ReformId, EffectiveCost);
		return false;
	}
	if (Institutions.GridlockRisk >= 80 && Institutions.RulingCoalitionSupport < 45)
	{
		OutMessage = FString::Printf(TEXT("Reforma %s bloqueada por Congreso/oposicion."), *ReformId);
		return false;
	}

	if (bSpendCapital && EffectiveCost > 0)
	{
		Characters->AdjustPoliticalCapital(Iso, -EffectiveCost);
	}
	Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport - FMath::Max(1, EffectiveCost / 8));
	Institutions.LegislativeOpposition = ClampPercent(Institutions.LegislativeOpposition + FMath::Max(1, EffectiveCost / 10));
	Institutions.LastVoteReport = FString::Printf(TEXT("Reforma %s aprobada. Coste politico %d."), *ReformId, EffectiveCost);
	OutMessage = Institutions.LastVoteReport;
	AddPoliticalMemory(Iso, TEXT("reform_passed"), 1, 10, OutMessage);
	return true;
}

TArray<FWLPolicyReformDefinition> UWLPoliticalSubsystem::GetAvailablePolicyReforms(const FString& NationIso) const
{
	TArray<FWLPolicyReformDefinition> Out;
	if (!ValidateNation(NationIso))
	{
		return Out;
	}
	Out = PolicyReformDefinitions();
	Out.Sort([](const FWLPolicyReformDefinition& A, const FWLPolicyReformDefinition& B)
	{
		return A.Area == B.Area
			? A.ReformId < B.ReformId
			: static_cast<int32>(A.Area) < static_cast<int32>(B.Area);
	});
	return Out;
}

TArray<FWLActiveReformState> UWLPoliticalSubsystem::GetActivePolicyReforms(const FString& NationIso) const
{
	TArray<FWLActiveReformState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLActiveReformState& Reform : ActivePolicyReforms)
	{
		if (Reform.NationIso == Iso && Reform.MonthsRemaining > 0)
		{
			Out.Add(Reform);
		}
	}
	Out.Sort([](const FWLActiveReformState& A, const FWLActiveReformState& B)
	{
		return A.ReformId < B.ReformId;
	});
	return Out;
}

TArray<FWLEnactedPolicyReformState> UWLPoliticalSubsystem::GetEnactedPolicyReforms(const FString& NationIso) const
{
	TArray<FWLEnactedPolicyReformState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLEnactedPolicyReformState& Reform : EnactedPolicyReforms)
	{
		if (Reform.NationIso == Iso)
		{
			Out.Add(Reform);
		}
	}
	Out.Sort([](const FWLEnactedPolicyReformState& A, const FWLEnactedPolicyReformState& B)
	{
		return A.ReformId < B.ReformId;
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetPolicyReformDefinition(const FString& ReformId, FWLPolicyReformDefinition& OutDefinition) const
{
	const FString Id = ReformId.TrimStartAndEnd().ToLower();
	for (const FWLPolicyReformDefinition& Definition : PolicyReformDefinitions())
	{
		if (Definition.ReformId == Id)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	return false;
}

bool UWLPoliticalSubsystem::HasReformMemory(const FString& NationIso, const FString& ReformId) const
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Id = ReformId.TrimStartAndEnd().ToLower();
	if (HasEnactedPolicyReform(Iso, Id))
	{
		return true;
	}
	if (GetPoliticalMemoryValue(Iso, ReformMemoryKey(Id)) > 0)
	{
		return true;
	}
	return ActivePolicyReforms.ContainsByPredicate([&Iso, &Id](const FWLActiveReformState& Reform)
	{
		return Reform.NationIso == Iso && Reform.ReformId == Id;
	});
}

bool UWLPoliticalSubsystem::HasEnactedPolicyReform(const FString& NationIso, const FString& ReformId) const
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Id = ReformId.TrimStartAndEnd().ToLower();
	return EnactedPolicyReforms.ContainsByPredicate([&Iso, &Id](const FWLEnactedPolicyReformState& Reform)
	{
		return Reform.NationIso == Iso && Reform.ReformId == Id;
	});
}

bool UWLPoliticalSubsystem::TrySpendTreasury(
	const FString& NationIso,
	int64 Amount,
	const FString& Reason,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (Amount <= 0)
	{
		return true;
	}

	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick)
	{
		OutMessage = FString::Printf(TEXT("No hay sistema economico para pagar %s."), *Reason);
		return false;
	}
	const int64 Treasury = Tick->GetTreasury(Iso);
	if (Treasury < Amount)
	{
		OutMessage = FString::Printf(TEXT("Tesoro insuficiente para %s (%lld/%lld)."),
			*Reason,
			static_cast<long long>(Treasury),
			static_cast<long long>(Amount));
		return false;
	}

	FString TreasuryMessage;
	Tick->AdjustTreasury(Iso, -Amount, TreasuryMessage);
	return true;
}

void UWLPoliticalSubsystem::RecordEnactedPolicyReform(
	const FString& NationIso,
	const FWLPolicyReformDefinition& Definition,
	const FString& Report)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso) || Definition.ReformId.IsEmpty())
	{
		return;
	}
	const FString PromiseFulfilledKey = TEXT("campaign_promise_fulfilled_") + Definition.ReformId;
	const FWLElectionState Election = GetElectionState(Iso);
	const bool bFulfilledPromise = Election.CampaignPromiseReformId == Definition.ReformId
		|| Election.bCampaignPromiseFulfilled
		|| GetPoliticalMemoryValue(Iso, PromiseFulfilledKey) > 0;
	for (FWLEnactedPolicyReformState& Existing : EnactedPolicyReforms)
	{
		if (Existing.NationIso == Iso && Existing.ReformId == Definition.ReformId)
		{
			Existing.Name = Definition.Name;
			Existing.Area = Definition.Area;
			Existing.bFulfilledCampaignPromise = Existing.bFulfilledCampaignPromise || bFulfilledPromise;
			Existing.LastReport = Report;
			return;
		}
	}

	FWLEnactedPolicyReformState Enacted;
	Enacted.NationIso = Iso;
	Enacted.ReformId = Definition.ReformId;
	Enacted.Name = Definition.Name;
	Enacted.Area = Definition.Area;
	Enacted.MonthsSinceEnacted = 0;
	Enacted.bFulfilledCampaignPromise = bFulfilledPromise;
	Enacted.LastReport = Report;
	EnactedPolicyReforms.Add(MoveTemp(Enacted));
}

void UWLPoliticalSubsystem::MarkCampaignPromiseFulfilled(
	const FString& NationIso,
	const FWLPolicyReformDefinition& Definition)
{
	FWLElectionState& Election = EnsureElectionState(NationIso);
	if (Election.CampaignPromiseReformId != Definition.ReformId || Election.bCampaignPromiseFulfilled)
	{
		return;
	}

	Election.bCampaignPromiseFulfilled = true;
	Election.Legitimacy = ClampPercent(Election.Legitimacy + 4);
	Election.PollingGovernment = ClampPercent(Election.PollingGovernment + 3);
	Election.LastElectionReport = FString::Printf(TEXT("%s cumple la promesa electoral: %s."),
		*NormalizeIso(NationIso),
		*Definition.Name);
	AddPoliticalMemory(NationIso, TEXT("campaign_promise_fulfilled"), 1, 18, Election.LastElectionReport);
	AddPoliticalMemory(NationIso, TEXT("campaign_promise_fulfilled_") + Definition.ReformId, 1, 84, Election.LastElectionReport);
}

bool UWLPoliticalSubsystem::EnactPolicyReform(const FString& NationIso, const FString& ReformId, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::EnactReform;
	Request.PrimaryId = ReformId;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::EnactPolicyReformDirect(const FString& NationIso, const FString& ReformId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para reforma P2.");
		return false;
	}

	FWLPolicyReformDefinition Definition;
	if (!GetPolicyReformDefinition(ReformId, Definition))
	{
		OutMessage = FString::Printf(TEXT("Reforma desconocida: %s"), *ReformId);
		return false;
	}
	if (HasReformMemory(Iso, Definition.ReformId))
	{
		OutMessage = FString::Printf(TEXT("%s ya aprobo o esta implementando %s."), *Iso, *Definition.Name);
		return false;
	}
	for (const FString& Prerequisite : Definition.PrerequisiteReformIds)
	{
		if (!HasReformMemory(Iso, Prerequisite))
		{
			OutMessage = FString::Printf(TEXT("%s requiere aprobar antes %s."), *Definition.Name, *Prerequisite);
			return false;
		}
	}

	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	if (Institutions.RulingCoalitionSupport < Definition.RequiredCoalitionSupport)
	{
		OutMessage = FString::Printf(TEXT("Coalicion insuficiente para %s (%d/%d)."),
			*Definition.Name, Institutions.RulingCoalitionSupport, Definition.RequiredCoalitionSupport);
		return false;
	}
	if (Capacity.AdministrativeEfficiency < Definition.RequiredStateCapacity)
	{
		OutMessage = FString::Printf(TEXT("Capacidad estatal insuficiente para %s (%d/%d)."),
			*Definition.Name, Capacity.AdministrativeEfficiency, Definition.RequiredStateCapacity);
		return false;
	}

	FString VoteMessage;
	if (!PassGovernmentReformDirect(Iso, Definition.ReformId, GetEffectiveReformCost(Iso, Definition.PoliticalCapitalCost), false, VoteMessage))
	{
		OutMessage = VoteMessage;
		return false;
	}

	FWLInternalPowerState& Internal = EnsureInternalPower(Iso);
	FWLElectionState& Election = EnsureElectionState(Iso);
	Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength + Definition.OppositionDelta);
	Election.Legitimacy = ClampPercent(Election.Legitimacy + Definition.LegitimacyDelta);
	if (Definition.ReformId == TEXT("constitution_term_rules"))
	{
		Election.bTermLimited = false;
		Election.ConsecutiveTermsWon = FMath::Min(Election.ConsecutiveTermsWon, 2);
		AddPoliticalMemory(Iso, TEXT("constitutional_rules_changed"), 1, 36,
			FString::Printf(TEXT("%s cambia reglas de mandato."), *Iso));
	}
	Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + Definition.CapacityDelta);
	Capacity.Corruption = ClampPercent(Capacity.Corruption + Definition.CorruptionDelta);
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		if (Definition.PublicOrderDelta != 0)
		{
			Tick->AdjustNationPublicOrder(Iso, Definition.PublicOrderDelta);
		}
	}
	for (EWLPublicGroup Group : Definition.AffectedGroups)
	{
		AdjustPublicGroupSupport(Iso, Group, Definition.PublicGroupSupportDelta, Definition.Name);
	}

	FWLActiveReformState Active;
	Active.NationIso = Iso;
	Active.ReformId = Definition.ReformId;
	Active.Name = Definition.Name;
	Active.Area = Definition.Area;
	Active.MonthsRemaining = FMath::Max(1, Definition.LongTermMonths);
	Active.ImplementationProgress = 0;
	Active.Backlash = Definition.ProtestRisk;
	Active.LastReport = FString::Printf(TEXT("%s aprobo %s. %s"), *Iso, *Definition.Name, *VoteMessage);
	ActivePolicyReforms.Add(Active);
	MarkCampaignPromiseFulfilled(Iso, Definition);
	AddPoliticalMemory(Iso, ReformMemoryKey(Definition.ReformId), 1, FMath::Max(60, Definition.LongTermMonths + 12), Active.LastReport);

	const int32 Roll = static_cast<int32>((GetTypeHash(Iso + Definition.ReformId) + Definition.ProtestRisk * 7) % 100);
	if (Roll < Definition.ProtestRisk + Institutions.GridlockRisk / 3)
	{
		TriggerGovernmentP2Crisis(Iso, EWLCrisisChainType::NationalProtest, Definition.ProtestRisk,
			FString::Printf(TEXT("Backlash contra %s."), *Definition.Name));
	}

	OutMessage = Active.LastReport;
	AddGovernmentLogEntry(EWLGovernmentLogCategory::Government, Iso, TEXT(""),
		TEXT("Reforma aprobada"), OutMessage, TEXT("policy_reform"), 5, true, Iso == GetPlayerNationIso());
	return true;
}

TArray<FWLPartyState> UWLPoliticalSubsystem::GetPoliticalParties(const FString& NationIso) const
{
	TArray<FWLPartyState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLPartyState& Party : PoliticalParties)
	{
		if (Party.NationIso == Iso)
		{
			Out.Add(Party);
		}
	}
	Out.Sort([](const FWLPartyState& A, const FWLPartyState& B)
	{
		return A.Seats == B.Seats ? A.PartyId < B.PartyId : A.Seats > B.Seats;
	});
	return Out;
}

FWLPartyState* UWLPoliticalSubsystem::FindMutableParty(const FString& NationIso, const FString& PartyId)
{
	const FString Key = PartyKey(NationIso, PartyId);
	return PoliticalParties.FindByPredicate([&Key, this](const FWLPartyState& Party)
	{
		return PartyKey(Party.NationIso, Party.PartyId) == Key;
	});
}

bool UWLPoliticalSubsystem::NegotiatePartySupport(const FString& NationIso, const FString& PartyId, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::NegotiatePartySupport;
	Request.PrimaryId = PartyId;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::NegotiatePartySupportDirect(const FString& NationIso, const FString& PartyId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPartyState* Party = FindMutableParty(Iso, PartyId);
	if (!Party || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Partido invalido para negociacion.");
		return false;
	}
	Party->LoyaltyToGovernment = ClampPercent(Party->LoyaltyToGovernment + 18);
	Party->Discipline = ClampPercent(Party->Discipline + 6);
	Party->bInCoalition = Party->Role != EWLPartyRole::HardOpposition || Party->LoyaltyToGovernment >= 35;
	Party->Role = Party->bInCoalition && Party->Role != EWLPartyRole::Ruling ? EWLPartyRole::Ally : Party->Role;
	Party->Corruption = ClampPercent(Party->Corruption + 4);
	Party->LastIncident = FString::Printf(TEXT("%s negocia apoyo con %s."), *Iso, *Party->Name);
	EnsureInstitutionalPower(Iso).RulingCoalitionSupport = ClampPercent(EnsureInstitutionalPower(Iso).RulingCoalitionSupport + 5);
	EnsurePatronageState(Iso).ClientelistPressure = ClampPercent(EnsurePatronageState(Iso).ClientelistPressure + 3);
	OutMessage = Party->LastIncident;
	return true;
}

bool UWLPoliticalSubsystem::HoldPartyInternalElection(const FString& NationIso, const FString& PartyId, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::HoldPartyInternalElection;
	Request.PrimaryId = PartyId;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::HoldPartyInternalElectionDirect(const FString& NationIso, const FString& PartyId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPartyState* Party = FindMutableParty(Iso, PartyId);
	if (!Party || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Partido invalido para eleccion interna.");
		return false;
	}
	const int32 Renewal = static_cast<int32>((GetTypeHash(Iso + Party->PartyId + TEXT("internal")) % 21));
	Party->MonthsSinceInternalElection = 0;
	const bool bFractureRisk = Party->Discipline < 35 || Party->LoyaltyToGovernment < 35;
	Party->Discipline = ClampPercent(55 + Renewal);
	if (Party->Role == EWLPartyRole::Ruling || Party->Role == EWLPartyRole::Ally)
	{
		Party->LoyaltyToGovernment = ClampPercent(Party->LoyaltyToGovernment + Renewal / 3 - 4);
	}
	else
	{
		Party->LoyaltyToGovernment = ClampPercent(Party->LoyaltyToGovernment - 2);
	}
	Party->LastIncident = FString::Printf(TEXT("%s celebra eleccion interna; disciplina %d."),
		*Party->Name, Party->Discipline);
	if (bFractureRisk)
	{
		Party->LoyaltyToGovernment = ClampPercent(Party->LoyaltyToGovernment - 4);
		EnsureInstitutionalPower(Iso).GridlockRisk = ClampPercent(EnsureInstitutionalPower(Iso).GridlockRisk + 3);
		Party->LastIncident += TEXT(" La baja cohesion deja fractura interna y sube gridlock.");
	}
	OutMessage = Party->LastIncident;
	return true;
}

FWLElectionState UWLPoliticalSubsystem::GetElectionState(const FString& NationIso) const
{
	if (const FWLElectionState* Found = ElectionStateByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLElectionState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

bool UWLPoliticalSubsystem::MakeCampaignPromise(const FString& NationIso, const FString& ReformId, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::MakeCampaignPromise;
	Request.PrimaryId = ReformId;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::MakeCampaignPromiseDirect(const FString& NationIso, const FString& ReformId, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPolicyReformDefinition Definition;
	if (!ValidateNation(Iso) || !GetPolicyReformDefinition(ReformId, Definition))
	{
		OutMessage = TEXT("Promesa de campania invalida.");
		return false;
	}
	FWLElectionState& Election = EnsureElectionState(Iso);
	Election.CampaignPromiseReformId = Definition.ReformId;
	Election.bCampaignPromiseFulfilled = HasReformMemory(Iso, Definition.ReformId);
	Election.CampaignIntensity = ClampPercent(Election.CampaignIntensity + 12);
	Election.PollingGovernment = ClampPercent(Election.PollingGovernment + 4);
	Election.LastElectionReport = FString::Printf(TEXT("%s promete %s en campania."), *Iso, *Definition.Name);
	AddPoliticalMemory(Iso, TEXT("campaign_promise"), 1, FMath::Max(6, Election.MonthsToElection + 3), Election.LastElectionReport);
	OutMessage = Election.LastElectionReport;
	return true;
}

TArray<FWLCharacterPoliticalProfile> UWLPoliticalSubsystem::GetCharacterPoliticalProfiles(const FString& NationIso) const
{
	TArray<FWLCharacterPoliticalProfile> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLCharacterPoliticalProfile>& Pair : CharacterPoliticalProfilesById)
	{
		if (Pair.Value.NationIso == Iso)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLCharacterPoliticalProfile& A, const FWLCharacterPoliticalProfile& B)
	{
		return A.SuccessionScore == B.SuccessionScore ? A.CharacterId < B.CharacterId : A.SuccessionScore > B.SuccessionScore;
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetCharacterPoliticalProfile(const FString& CharacterId, FWLCharacterPoliticalProfile& OutProfile) const
{
	if (const FWLCharacterPoliticalProfile* Found = CharacterPoliticalProfilesById.Find(NormalizeCharacterId(CharacterId)))
	{
		OutProfile = *Found;
		return true;
	}
	return false;
}

FWLPatronageState UWLPoliticalSubsystem::GetPatronageState(const FString& NationIso) const
{
	if (const FWLPatronageState* Found = PatronageStateByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLPatronageState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

bool UWLPoliticalSubsystem::UsePatronage(const FString& NationIso, EWLPatronageActionType Action, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::UsePatronage;
	Request.NumericValue = static_cast<int32>(Action);
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::UsePatronageDirect(const FString& NationIso, EWLPatronageActionType Action, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para patronazgo.");
		return false;
	}
	FWLPatronageState& Patronage = EnsurePatronageState(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);

	switch (Action)
	{
	case EWLPatronageActionType::AppointLoyalist:
		Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower + 8);
		Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure + 5);
		Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 4);
		Capacity.Corruption = ClampPercent(Capacity.Corruption + 2);
		Patronage.LastDeal = TEXT("Leales nombrados en cargos sensibles.");
		break;
	case EWLPatronageActionType::AwardContract:
		Patronage.ContractCorruption = ClampPercent(Patronage.ContractCorruption + 12);
		Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower + 7);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 3, TEXT("contratos publicos"));
		Patronage.LastDeal = TEXT("Contratos publicos repartidos a aliados.");
		break;
	case EWLPatronageActionType::GrantFavor:
		Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure + 8);
		Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 6);
		Patronage.LastDeal = TEXT("Favores politicos aseguran votos temporales.");
		break;
	case EWLPatronageActionType::FundGovernor:
		Patronage.RegionalMachines = ClampPercent(Patronage.RegionalMachines + 10);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Regions, 3, TEXT("financiacion regional"));
		Patronage.LastDeal = TEXT("Gobernadores aliados reciben presupuesto regional.");
		break;
	case EWLPatronageActionType::GrantConcession:
	default:
		Patronage.ConcessionBacklash = ClampPercent(Patronage.ConcessionBacklash + 10);
		Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower + 5);
		Capacity.Corruption = ClampPercent(Capacity.Corruption + 3);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 4, TEXT("concesiones"));
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -2, TEXT("concesiones"));
		Patronage.LastDeal = TEXT("Concesiones economicas compran apoyo y elevan backlash.");
		AddPoliticalMemory(Iso, TEXT("patronage_concession"), 1, 12, Patronage.LastDeal);
		break;
	}
	OutMessage = Patronage.LastDeal;
	return true;
}

FWLMediaPublicOpinionState UWLPoliticalSubsystem::GetMediaPublicOpinion(const FString& NationIso) const
{
	if (const FWLMediaPublicOpinionState* Found = MediaStateByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLMediaPublicOpinionState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

bool UWLPoliticalSubsystem::RunMediaAction(const FString& NationIso, EWLMediaActionType Action, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::RunMediaAction;
	Request.NumericValue = static_cast<int32>(Action);
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::RunMediaActionDirect(const FString& NationIso, EWLMediaActionType Action, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para accion de medios.");
		return false;
	}
	FWLMediaPublicOpinionState& Media = EnsureMediaState(Iso);
	FWLElectionState& Election = EnsureElectionState(Iso);
	switch (Action)
	{
	case EWLMediaActionType::PressBriefing:
		Media.PressFreedom = ClampPercent(Media.PressFreedom + 2);
		Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval + 3);
		Media.MediaCrisisRisk = ClampPercent(Media.MediaCrisisRisk - 4);
		Media.LastNarrative = TEXT("Rueda de prensa reduce incertidumbre publica.");
		break;
	case EWLMediaActionType::StateBroadcast:
		Media.MediaControl = ClampPercent(Media.MediaControl + 5);
		Media.PropagandaReach = ClampPercent(Media.PropagandaReach + 8);
		Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval + 4);
		Media.LastNarrative = TEXT("Cadena nacional instala narrativa oficial.");
		break;
	case EWLMediaActionType::Propaganda:
		Media.PropagandaReach = ClampPercent(Media.PropagandaReach + 12);
		Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure + 3);
		Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval + 5);
		Media.MediaCrisisRisk = ClampPercent(Media.MediaCrisisRisk + 2);
		Media.LastNarrative = TEXT("Propaganda interna moviliza base oficialista.");
		AddPoliticalMemory(Iso, TEXT("propaganda_backlash"), 1, 8, Media.LastNarrative);
		break;
	case EWLMediaActionType::Censorship:
		Media.PressFreedom = ClampPercent(Media.PressFreedom - 10);
		Media.MediaControl = ClampPercent(Media.MediaControl + 12);
		Media.CensorshipBacklash = ClampPercent(Media.CensorshipBacklash + 14);
		Election.Legitimacy = ClampPercent(Election.Legitimacy - 4);
		Media.LastNarrative = TEXT("Censura contiene escandalo y erosiona legitimidad.");
		AddPoliticalMemory(Iso, TEXT("recent_censorship"), 1, 12, Media.LastNarrative);
		break;
	case EWLMediaActionType::CounterFakeNews:
	default:
		Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure - 12);
		Media.MediaCrisisRisk = ClampPercent(Media.MediaCrisisRisk - 8);
		Media.PressFreedom = ClampPercent(Media.PressFreedom + 1);
		Media.LastNarrative = TEXT("Contra fake news estabiliza la opinion publica.");
		break;
	}
	OutMessage = Media.LastNarrative;
	return true;
}

TArray<FWLRegionGovernorState> UWLPoliticalSubsystem::GetRegionGovernors(const FString& NationIso) const
{
	TArray<FWLRegionGovernorState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLRegionGovernorState& Region : RegionGovernors)
	{
		if (Region.NationIso == Iso)
		{
			Out.Add(Region);
		}
	}
	Out.Sort([](const FWLRegionGovernorState& A, const FWLRegionGovernorState& B)
	{
		return A.RegionId < B.RegionId;
	});
	return Out;
}

FWLRegionGovernorState* UWLPoliticalSubsystem::FindMutableRegion(const FString& NationIso, const FString& RegionId)
{
	const FString Iso = NormalizeIso(NationIso);
	const FString Id = RegionId.TrimStartAndEnd();
	return RegionGovernors.FindByPredicate([&Iso, &Id](const FWLRegionGovernorState& Region)
	{
		return Region.NationIso == Iso && Region.RegionId == Id;
	});
}

bool UWLPoliticalSubsystem::RunRegionPolicy(
	const FString& NationIso,
	const FString& RegionId,
	EWLRegionPolicyActionType Action,
	FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::RunRegionPolicy;
	Request.PrimaryId = RegionId;
	Request.NumericValue = static_cast<int32>(Action);
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::RunRegionPolicyDirect(
	const FString& NationIso,
	const FString& RegionId,
	EWLRegionPolicyActionType Action,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLRegionGovernorState* Region = FindMutableRegion(Iso, RegionId);
	if (!Region || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Region invalida para politica territorial.");
		return false;
	}
	switch (Action)
	{
	case EWLRegionPolicyActionType::AppointGovernor:
		Region->GovernorName = FString::Printf(TEXT("Gob. Leal %s"), *Region->RegionName);
		Region->Alignment = EWLPartyRole::Ally;
		Region->Obedience = ClampPercent(Region->Obedience + 15);
		Region->CenterControl = ClampPercent(Region->CenterControl + 8);
		Region->Autonomy = ClampPercent(Region->Autonomy - 5);
		EnsurePatronageState(Iso).RegionalMachines = ClampPercent(EnsurePatronageState(Iso).RegionalMachines + 5);
		Region->LastReport = TEXT("Gobernador leal nombrado.");
		break;
	case EWLRegionPolicyActionType::RegionalInvestment:
		Region->InvestmentLevel = ClampPercent(Region->InvestmentLevel + 12);
		Region->ProtestRisk = ClampPercent(Region->ProtestRisk - 10);
		Region->Obedience = ClampPercent(Region->Obedience + 5);
		Region->LastReport = TEXT("Inversion regional baja protesta.");
		break;
	case EWLRegionPolicyActionType::AutonomyDeal:
		Region->Autonomy = ClampPercent(Region->Autonomy + 10);
		Region->ProtestRisk = ClampPercent(Region->ProtestRisk - 12);
		Region->CenterControl = ClampPercent(Region->CenterControl - 6);
		Region->LastReport = TEXT("Pacto autonomico calma region y reduce control central.");
		AddPoliticalMemory(Iso, TEXT("regional_autonomy_deal"), 1, 18, Region->LastReport);
		break;
	case EWLRegionPolicyActionType::SecurityOperation:
	default:
		Region->CenterControl = ClampPercent(Region->CenterControl + 12);
		Region->ProtestRisk = ClampPercent(Region->ProtestRisk - 5);
		Region->Obedience = ClampPercent(Region->Obedience + 4);
		Region->Autonomy = ClampPercent(Region->Autonomy - 4);
		if (UWLStrategicTickSubsystem* Tick = GetTick())
		{
			Tick->AdjustNationPublicOrder(Iso, -1);
		}
		Region->LastReport = TEXT("Operacion de seguridad aumenta control y tensiona derechos.");
		AddPoliticalMemory(Iso, TEXT("recent_repression"), 1, 10, Region->LastReport);
		break;
	}
	OutMessage = Region->LastReport;
	return true;
}

TArray<FWLCrisisChainState> UWLPoliticalSubsystem::GetActiveCrisisChains(const FString& NationIso) const
{
	TArray<FWLCrisisChainState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (!Crisis.bResolved && (Iso.IsEmpty() || Crisis.NationIso == Iso))
		{
			Out.Add(Crisis);
		}
	}
	Out.Sort([](const FWLCrisisChainState& A, const FWLCrisisChainState& B)
	{
		return A.Intensity == B.Intensity ? A.CrisisId < B.CrisisId : A.Intensity > B.Intensity;
	});
	return Out;
}

FWLGovernmentCalibrationState UWLPoliticalSubsystem::GetGovernmentCalibration(const FString& NationIso) const
{
	if (const FWLGovernmentCalibrationState* Found = GovernmentCalibrationByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLGovernmentCalibrationState State;
	State.NationIso = NormalizeIso(NationIso);
	return State;
}

