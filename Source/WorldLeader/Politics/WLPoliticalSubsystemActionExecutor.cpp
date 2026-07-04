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

FWLPoliticalActionPreview UWLPoliticalSubsystem::GetPoliticalActionPreview(const FWLPoliticalActionRequest& Request) const
{
	FWLPoliticalActionPreview Preview;
	Preview.ActionType = Request.ActionType;
	Preview.NationIso = NormalizeIso(Request.NationIso);

	auto Block = [&Preview](const FString& Reason)
	{
		if (Preview.BlockReason.IsEmpty())
		{
			Preview.BlockReason = Reason;
		}
	};

	const FString PrimaryId = Request.PrimaryId.TrimStartAndEnd().ToLower();
	const FString SecondaryId = Request.SecondaryId.TrimStartAndEnd().ToLower();

	if (Request.ActionType == EWLPoliticalActionType::ResolveEvent)
	{
		const FWLPoliticalEventInstance* Event = EventQueue.FindByPredicate([&PrimaryId](const FWLPoliticalEventInstance& Candidate)
		{
			return Candidate.InstanceId.ToLower() == PrimaryId && !Candidate.bResolved;
		});
		if (!Event)
		{
			Block(FString::Printf(TEXT("Evento no disponible: %s"), *Request.PrimaryId));
		}
		else
		{
			Preview.NationIso = NormalizeIso(Event->NationIso);
			const FWLPoliticalEventOption* Option = Event->Options.FindByPredicate([&SecondaryId](const FWLPoliticalEventOption& Candidate)
			{
				return Candidate.OptionId.ToLower() == SecondaryId;
			});
			if (!Option)
			{
				Block(FString::Printf(TEXT("Opcion no disponible: %s"), *Request.SecondaryId));
			}
			else
			{
				Preview.ActionPointCost = (FMath::Abs(Option->PoliticalCapitalDelta) >= 10
					|| FMath::Abs(Option->TreasuryDelta) >= 5000
					|| Option->OppositionDelta >= 8
					|| Option->PublicOrderDelta <= -8
					|| Option->MarketShockDurationMonths > 0) ? 2 : 1;
				Preview.PoliticalCapitalCost = FMath::Max(0, -Option->PoliticalCapitalDelta);
				Preview.TreasuryCost = FMath::Max<int64>(0, -Option->TreasuryDelta);
				Preview.EffectsPreview = FString::Printf(TEXT("Oposicion %+d, orden publico %+d, capital %+d, tesoro %+lld."),
					Option->OppositionDelta,
					Option->PublicOrderDelta,
					Option->PoliticalCapitalDelta,
					static_cast<long long>(Option->TreasuryDelta));
				const FString TargetIso = NormalizeIso(Event->TargetIso);
				if (Option->RelationDelta != 0 && (!ValidateNation(TargetIso) || TargetIso == Preview.NationIso))
				{
					Block(TEXT("La opcion requiere un objetivo diplomatico valido."));
				}
			}
		}
	}
	else if (!ValidateNation(Preview.NationIso))
	{
		Block(TEXT("Nacion invalida para accion politica."));
	}

	if (Preview.BlockReason.IsEmpty())
	{
		switch (Request.ActionType)
		{
		case EWLPoliticalActionType::SetAgenda:
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 1;
			if (Request.Priorities.IsEmpty() || Request.Priorities.Num() > MaxAgendaPriorities)
			{
				Block(FString::Printf(TEXT("La agenda debe tener entre 1 y %d prioridades."), MaxAgendaPriorities));
			}
			else
			{
				TSet<EWLGovernmentPriority> Seen;
				for (EWLGovernmentPriority Priority : Request.Priorities)
				{
					Seen.Add(Priority);
				}
				if (Seen.IsEmpty())
				{
					Block(TEXT("Agenda vacia."));
				}
				Preview.EffectsPreview = FString::Printf(TEXT("Define %d prioridades de gobierno."), Seen.Num());
			}
			break;
		}
		case EWLPoliticalActionType::StartProgram:
		{
			Preview.ActionPointCost = 1;
			FWLMinistryProgramDefinition Definition;
			if (!GetMinistryProgramDefinition(PrimaryId, Definition))
			{
				Block(FString::Printf(TEXT("Programa desconocido: %s"), *Request.PrimaryId));
				break;
			}
			for (const FWLMinistryProgramState& Active : ActiveMinistryPrograms)
			{
				if (Active.NationIso == Preview.NationIso && Active.Office == Definition.Office && Active.RemainingMonths > 0)
				{
					Block(FString::Printf(TEXT("%s ya ejecuta un programa de %s."),
						*Preview.NationIso,
						*UWLCharacterSubsystem::MinisterOfficeToString(Definition.Office)));
					break;
				}
			}
			Preview.TreasuryCost = FMath::Max<int64>(0, Definition.TreasuryCost);
			Preview.PoliticalCapitalCost = Definition.bRequiresLegislation
				? GetEffectiveReformCost(Preview.NationIso, Definition.PoliticalCapitalCost)
				: FMath::Max(0, Definition.PoliticalCapitalCost);
			const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(Preview.NationIso);
			if (Definition.bRequiresLegislation && Institutions.GridlockRisk >= 80 && Institutions.RulingCoalitionSupport < 45)
			{
				Block(FString::Printf(TEXT("Programa %s bloqueado por Congreso/oposicion."), *Definition.Name));
			}
			Preview.EffectsPreview = FString::Printf(TEXT("Inicia %s por %d meses."), *Definition.Name, FMath::Max(1, Definition.DurationMonths));
			break;
		}
		case EWLPoliticalActionType::EnactReform:
		{
			Preview.ActionPointCost = 2;
			FWLPolicyReformDefinition Definition;
			if (!GetPolicyReformDefinition(PrimaryId, Definition))
			{
				Block(FString::Printf(TEXT("Reforma desconocida: %s"), *Request.PrimaryId));
				break;
			}
			if (HasReformMemory(Preview.NationIso, Definition.ReformId))
			{
				Block(FString::Printf(TEXT("%s ya aprobo o esta implementando %s."), *Preview.NationIso, *Definition.Name));
			}
			for (const FString& Prerequisite : Definition.PrerequisiteReformIds)
			{
				if (!HasReformMemory(Preview.NationIso, Prerequisite))
				{
					Block(FString::Printf(TEXT("%s requiere aprobar antes %s."), *Definition.Name, *Prerequisite));
					break;
				}
			}
			const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(Preview.NationIso);
			const FWLStateCapacityState Capacity = GetStateCapacity(Preview.NationIso);
			if (Institutions.RulingCoalitionSupport < Definition.RequiredCoalitionSupport)
			{
				Block(FString::Printf(TEXT("Coalicion insuficiente para %s (%d/%d)."),
					*Definition.Name, Institutions.RulingCoalitionSupport, Definition.RequiredCoalitionSupport));
			}
			if (Capacity.AdministrativeEfficiency < Definition.RequiredStateCapacity)
			{
				Block(FString::Printf(TEXT("Capacidad estatal insuficiente para %s (%d/%d)."),
					*Definition.Name, Capacity.AdministrativeEfficiency, Definition.RequiredStateCapacity));
			}
			if (Institutions.GridlockRisk >= 80 && Institutions.RulingCoalitionSupport < 45)
			{
				Block(FString::Printf(TEXT("Reforma %s bloqueada por Congreso/oposicion."), *Definition.Name));
			}
			Preview.PoliticalCapitalCost = GetEffectiveReformCost(Preview.NationIso, Definition.PoliticalCapitalCost);
			Preview.TreasuryCost = FMath::Max<int64>(0, Definition.TreasuryCost);
			Preview.EffectsPreview = FString::Printf(TEXT("Implementa %s; oposicion %+d, orden %+d."),
				*Definition.Name,
				Definition.OppositionDelta,
				Definition.PublicOrderDelta);
			break;
		}
		case EWLPoliticalActionType::RepressOpposition:
			Preview.ActionPointCost = 2;
			Preview.TreasuryCost = RepressOppositionCost;
			Preview.CooldownMonths = 2;
			Preview.EffectsPreview = TEXT("Baja oposicion, baja orden publico y deja memoria de represion.");
			break;
		case EWLPoliticalActionType::NegotiatePartySupport:
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 3;
			const TArray<FWLPartyState> Parties = GetPoliticalParties(Preview.NationIso);
			const FWLPartyState* Party = Parties.FindByPredicate([&PrimaryId](const FWLPartyState& Candidate)
			{
				return Candidate.PartyId.ToLower() == PrimaryId;
			});
			if (!Party)
			{
				Block(TEXT("Partido invalido para negociacion."));
				break;
			}
			if (Party->LoyaltyToGovernment >= 85)
			{
				Block(FString::Printf(TEXT("%s ya tiene lealtad alta (%d/100)."), *Party->Name, Party->LoyaltyToGovernment));
			}
			const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(Preview.NationIso);
			Preview.PoliticalCapitalCost = 6 + Institutions.GridlockRisk / 10 + (Party->Role == EWLPartyRole::HardOpposition ? 4 : 0);
			Preview.EffectsPreview = FString::Printf(TEXT("Sube lealtad/disciplina de %s y apoyo legislativo."), *Party->Name);
			break;
		}
		case EWLPoliticalActionType::HoldPartyInternalElection:
		{
			Preview.ActionPointCost = 1;
			Preview.PoliticalCapitalCost = 4;
			Preview.CooldownMonths = 24;
			const TArray<FWLPartyState> Parties = GetPoliticalParties(Preview.NationIso);
			if (!Parties.ContainsByPredicate([&PrimaryId](const FWLPartyState& Candidate) { return Candidate.PartyId.ToLower() == PrimaryId; }))
			{
				Block(TEXT("Partido invalido para eleccion interna."));
			}
			Preview.EffectsPreview = TEXT("Renueva disciplina; puede fracturar si lealtad/disciplina son bajas.");
			break;
		}
		case EWLPoliticalActionType::MakeCampaignPromise:
		{
			Preview.ActionPointCost = 1;
			FWLPolicyReformDefinition Definition;
			if (!GetPolicyReformDefinition(PrimaryId, Definition))
			{
				Block(TEXT("Promesa de campania invalida."));
				break;
			}
			const FWLElectionState Election = GetElectionState(Preview.NationIso);
			if (Election.MonthsToElection <= 0 || Election.MonthsToElection > CampaignPromiseWindowMonths)
			{
				Block(FString::Printf(TEXT("Promesas solo entre 1 y %d meses antes de eleccion."), CampaignPromiseWindowMonths));
			}
			if (!Election.CampaignPromiseReformId.IsEmpty() && !Election.bCampaignPromiseFulfilled)
			{
				Block(FString::Printf(TEXT("Ya hay una promesa activa: %s."), *Election.CampaignPromiseReformId));
			}
			if (HasReformMemory(Preview.NationIso, Definition.ReformId))
			{
				Block(FString::Printf(TEXT("%s ya esta aprobada, activa o en memoria politica."), *Definition.Name));
			}
			Preview.EffectsPreview = FString::Printf(TEXT("Promete %s y sube intensidad de campania."), *Definition.Name);
			break;
		}
		case EWLPoliticalActionType::UsePatronage:
			Preview.ActionPointCost = 1;
			switch (static_cast<EWLPatronageActionType>(Request.NumericValue))
			{
			case EWLPatronageActionType::AppointLoyalist:
				Preview.PoliticalCapitalCost = 3;
				Preview.CooldownMonths = 2;
				Preview.EffectsPreview = TEXT("Nombra leales, sube patronazgo y corrupcion.");
				break;
			case EWLPatronageActionType::AwardContract:
				Preview.TreasuryCost = 2200;
				Preview.CooldownMonths = 2;
				Preview.EffectsPreview = TEXT("Reparte contratos y sube corrupcion contractual.");
				break;
			case EWLPatronageActionType::GrantFavor:
				Preview.TreasuryCost = 900;
				Preview.PoliticalCapitalCost = 2;
				Preview.CooldownMonths = 2;
				Preview.EffectsPreview = TEXT("Compra votos temporales con favores.");
				break;
			case EWLPatronageActionType::FundGovernor:
				Preview.TreasuryCost = 1800;
				Preview.CooldownMonths = 2;
				Preview.EffectsPreview = TEXT("Financia maquinaria regional.");
				break;
			case EWLPatronageActionType::GrantConcession:
			default:
				Preview.PoliticalCapitalCost = 2;
				Preview.CooldownMonths = 3;
				Preview.EffectsPreview = TEXT("Otorga concesiones, sube patronazgo y backlash.");
				break;
			}
			break;
		case EWLPoliticalActionType::RunMediaAction:
			Preview.ActionPointCost = 1;
			switch (static_cast<EWLMediaActionType>(Request.NumericValue))
			{
			case EWLMediaActionType::PressBriefing:
				Preview.PoliticalCapitalCost = 1;
				Preview.CooldownMonths = 1;
				Preview.EffectsPreview = TEXT("Reduce incertidumbre y sube aprobacion.");
				break;
			case EWLMediaActionType::StateBroadcast:
				Preview.TreasuryCost = 1200;
				Preview.PoliticalCapitalCost = 1;
				Preview.CooldownMonths = 2;
				Preview.EffectsPreview = TEXT("Instala narrativa oficial.");
				break;
			case EWLMediaActionType::Propaganda:
				Preview.TreasuryCost = 2000;
				Preview.PoliticalCapitalCost = 2;
				Preview.CooldownMonths = 2;
				Preview.EffectsPreview = TEXT("Sube aprobacion, fake news y riesgo de backlash.");
				break;
			case EWLMediaActionType::Censorship:
				Preview.CooldownMonths = 3;
				Preview.EffectsPreview = TEXT("Sube control de medios, baja legitimidad y deja memoria de censura.");
				break;
			case EWLMediaActionType::CounterFakeNews:
			default:
				Preview.TreasuryCost = 800;
				Preview.CooldownMonths = 1;
				Preview.EffectsPreview = TEXT("Reduce fake news y riesgo mediatico.");
				break;
			}
			break;
		case EWLPoliticalActionType::RunRegionPolicy:
		{
			const TArray<FWLRegionGovernorState> Regions = GetRegionGovernors(Preview.NationIso);
			if (!Regions.ContainsByPredicate([&Request](const FWLRegionGovernorState& Region)
			{
				return Region.RegionId.Equals(Request.PrimaryId.TrimStartAndEnd(), ESearchCase::IgnoreCase);
			}))
			{
				Block(TEXT("Region invalida para politica territorial."));
				break;
			}
			switch (static_cast<EWLRegionPolicyActionType>(Request.NumericValue))
			{
			case EWLRegionPolicyActionType::AppointGovernor:
				Preview.ActionPointCost = 1;
				Preview.PoliticalCapitalCost = 5;
				Preview.CooldownMonths = 12;
				Preview.EffectsPreview = TEXT("Nombra gobernador leal y baja autonomia.");
				break;
			case EWLRegionPolicyActionType::RegionalInvestment:
				Preview.ActionPointCost = 1;
				Preview.TreasuryCost = 1800;
				Preview.CooldownMonths = 3;
				Preview.EffectsPreview = TEXT("Baja protesta y sube inversion regional.");
				break;
			case EWLRegionPolicyActionType::AutonomyDeal:
				Preview.ActionPointCost = 1;
				Preview.PoliticalCapitalCost = 3;
				Preview.CooldownMonths = 12;
				Preview.EffectsPreview = TEXT("Calma protesta a cambio de autonomia.");
				break;
			case EWLRegionPolicyActionType::SecurityOperation:
			default:
				Preview.ActionPointCost = 2;
				Preview.TreasuryCost = 1000;
				Preview.CooldownMonths = 3;
				Preview.EffectsPreview = TEXT("Sube control central, baja orden publico y deja memoria de represion.");
				break;
			}
			break;
		}
		default:
			Block(TEXT("Accion politica no soportada."));
			break;
		}
	}

	if (ValidateNation(Preview.NationIso))
	{
		const FWLPoliticalActionBudget Budget = GetPoliticalActionBudget(Preview.NationIso);
		Preview.ActionPointsRemaining = Budget.RemainingActionPoints;
		const FString TargetKey = PoliticalActionTargetKey(Request);
		Preview.CooldownRemainingMonths = GetPoliticalActionCooldownRemaining(
			Preview.NationIso,
			Request.ActionType,
			TargetKey);
		if (Preview.CooldownRemainingMonths > 0)
		{
			Block(FString::Printf(TEXT("Cooldown activo: faltan %d meses."), Preview.CooldownRemainingMonths));
		}
		if (Preview.ActionPointCost > 0 && Budget.RemainingActionPoints < Preview.ActionPointCost)
		{
			Block(FString::Printf(TEXT("AP politica insuficiente (%d/%d)."),
				Budget.RemainingActionPoints,
				Preview.ActionPointCost));
		}
		if (Preview.TreasuryCost > 0)
		{
			const UWLStrategicTickSubsystem* Tick = GetTick();
			if (!Tick || Tick->GetTreasury(Preview.NationIso) < Preview.TreasuryCost)
			{
				Block(FString::Printf(TEXT("Tesoro insuficiente (%lld requerido)."),
					static_cast<long long>(Preview.TreasuryCost)));
			}
		}
		if (Preview.PoliticalCapitalCost > 0)
		{
			const UWLCharacterSubsystem* Characters = GetCharacters();
			if (!Characters || Characters->GetPoliticalCapital(Preview.NationIso) < Preview.PoliticalCapitalCost)
			{
				Block(FString::Printf(TEXT("Capital politico insuficiente (%d requerido)."), Preview.PoliticalCapitalCost));
			}
		}
	}

	Preview.bCanExecute = Preview.BlockReason.IsEmpty();
	if (Preview.bCanExecute)
	{
		Preview.BlockReason = TEXT("Disponible.");
	}
	return Preview;
}

bool UWLPoliticalSubsystem::SpendPoliticalActionCosts(const FWLPoliticalActionPreview& Preview, FString& OutMessage)
{
	if (!Preview.bCanExecute)
	{
		OutMessage = Preview.BlockReason;
		return false;
	}
	if (Preview.PoliticalCapitalCost > 0)
	{
		UWLCharacterSubsystem* Characters = GetCharacters();
		if (!Characters || Characters->GetPoliticalCapital(Preview.NationIso) < Preview.PoliticalCapitalCost)
		{
			OutMessage = FString::Printf(TEXT("Capital politico insuficiente (%d requerido)."), Preview.PoliticalCapitalCost);
			return false;
		}
		Characters->AdjustPoliticalCapital(Preview.NationIso, -Preview.PoliticalCapitalCost);
	}
	if (Preview.TreasuryCost > 0)
	{
		UWLStrategicTickSubsystem* Tick = GetTick();
		if (!Tick || Tick->GetTreasury(Preview.NationIso) < Preview.TreasuryCost)
		{
			if (Preview.PoliticalCapitalCost > 0)
			{
				if (UWLCharacterSubsystem* Characters = GetCharacters())
				{
					Characters->AdjustPoliticalCapital(Preview.NationIso, Preview.PoliticalCapitalCost);
				}
			}
			OutMessage = FString::Printf(TEXT("Tesoro insuficiente (%lld requerido)."), static_cast<long long>(Preview.TreasuryCost));
			return false;
		}
		FString TreasuryMessage;
		if (!Tick->AdjustTreasury(Preview.NationIso, -Preview.TreasuryCost, TreasuryMessage))
		{
			if (Preview.PoliticalCapitalCost > 0)
			{
				if (UWLCharacterSubsystem* Characters = GetCharacters())
				{
					Characters->AdjustPoliticalCapital(Preview.NationIso, Preview.PoliticalCapitalCost);
				}
			}
			OutMessage = TreasuryMessage;
			return false;
		}
	}
	return true;
}

void UWLPoliticalSubsystem::RefundPoliticalActionCosts(const FWLPoliticalActionPreview& Preview)
{
	if (Preview.PoliticalCapitalCost > 0)
	{
		if (UWLCharacterSubsystem* Characters = GetCharacters())
		{
			Characters->AdjustPoliticalCapital(Preview.NationIso, Preview.PoliticalCapitalCost);
		}
	}
	if (Preview.TreasuryCost > 0)
	{
		if (UWLStrategicTickSubsystem* Tick = GetTick())
		{
			FString Message;
			Tick->AdjustTreasury(Preview.NationIso, Preview.TreasuryCost, Message);
		}
	}
}

void UWLPoliticalSubsystem::RecordPoliticalAction(
	const FWLPoliticalActionRequest& Request,
	const FWLPoliticalActionPreview& Preview,
	const FString& Result)
{
	FWLPoliticalActionRecord Record;
	Record.NationIso = Preview.NationIso;
	Record.ActionType = Request.ActionType;
	Record.ActionKey = PoliticalActionKey(Request.ActionType);
	Record.TargetKey = PoliticalActionTargetKey(Request);
	Record.MonthKey = GetCurrentPoliticalMonthKey();
	Record.ActionPointCost = Preview.ActionPointCost;
	Record.CooldownMonths = Preview.CooldownMonths;
	if (const UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Record.Year = Tick->GetCurrentYear();
		Record.Month = Tick->GetCurrentMonth();
		Record.Day = Tick->GetCurrentDay();
	}
	Record.Result = Result;
	PoliticalActionRecords.Add(MoveTemp(Record));
}

bool UWLPoliticalSubsystem::ExecutePoliticalAction(const FWLPoliticalActionRequest& Request, FString& OutMessage)
{
	const FWLPoliticalActionPreview Preview = GetPoliticalActionPreview(Request);
	if (!Preview.bCanExecute)
	{
		OutMessage = Preview.BlockReason;
		return false;
	}
	if (!SpendPoliticalActionCosts(Preview, OutMessage))
	{
		return false;
	}

	bool bApplied = false;
	switch (Request.ActionType)
	{
	case EWLPoliticalActionType::SetAgenda:
		bApplied = ApplyGovernmentAgendaDirect(Preview.NationIso, Request.Priorities, OutMessage);
		break;
	case EWLPoliticalActionType::StartProgram:
		bApplied = StartMinistryProgramDirect(Preview.NationIso, Request.PrimaryId, OutMessage);
		break;
	case EWLPoliticalActionType::EnactReform:
		bApplied = EnactPolicyReformDirect(Preview.NationIso, Request.PrimaryId, OutMessage);
		break;
	case EWLPoliticalActionType::ResolveEvent:
		bApplied = ResolveEventDirect(Request.PrimaryId, Request.SecondaryId, OutMessage);
		break;
	case EWLPoliticalActionType::RepressOpposition:
		bApplied = RepressOppositionDirect(Preview.NationIso, OutMessage);
		break;
	case EWLPoliticalActionType::NegotiatePartySupport:
		bApplied = NegotiatePartySupportDirect(Preview.NationIso, Request.PrimaryId, OutMessage);
		break;
	case EWLPoliticalActionType::HoldPartyInternalElection:
		bApplied = HoldPartyInternalElectionDirect(Preview.NationIso, Request.PrimaryId, OutMessage);
		break;
	case EWLPoliticalActionType::MakeCampaignPromise:
		bApplied = MakeCampaignPromiseDirect(Preview.NationIso, Request.PrimaryId, OutMessage);
		break;
	case EWLPoliticalActionType::UsePatronage:
		bApplied = UsePatronageDirect(Preview.NationIso, static_cast<EWLPatronageActionType>(Request.NumericValue), OutMessage);
		break;
	case EWLPoliticalActionType::RunMediaAction:
		bApplied = RunMediaActionDirect(Preview.NationIso, static_cast<EWLMediaActionType>(Request.NumericValue), OutMessage);
		break;
	case EWLPoliticalActionType::RunRegionPolicy:
		bApplied = RunRegionPolicyDirect(Preview.NationIso, Request.PrimaryId, static_cast<EWLRegionPolicyActionType>(Request.NumericValue), OutMessage);
		break;
	default:
		OutMessage = TEXT("Accion politica no soportada.");
		bApplied = false;
		break;
	}

	if (!bApplied)
	{
		RefundPoliticalActionCosts(Preview);
		return false;
	}

	RecordPoliticalAction(Request, Preview, OutMessage);
	AddGovernmentLogEntry(
		PoliticalActionLogCategory(Request.ActionType),
		Preview.NationIso,
		TEXT(""),
		PoliticalActionLogTitle(Request.ActionType),
		OutMessage,
		TEXT("political_action"),
		Preview.ActionPointCost > 0 ? Preview.ActionPointCost * 10 : 5,
		false,
		Preview.NationIso == GetPlayerNationIso());
	return true;
}

