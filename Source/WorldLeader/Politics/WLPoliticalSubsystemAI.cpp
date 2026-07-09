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

void UWLPoliticalSubsystem::AutoResolveEventsForAI(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	for (FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (Event.bResolved || Event.NationIso != Iso || Event.Options.IsEmpty())
		{
			continue;
		}
		// Criterio IA: la opcion que mas reduce su oposicion (empate -> menor coste de orden publico).
		const FWLPoliticalEventOption* Best = &Event.Options[0];
		for (const FWLPoliticalEventOption& Option : Event.Options)
		{
			if (Option.OppositionDelta < Best->OppositionDelta
				|| (Option.OppositionDelta == Best->OppositionDelta && Option.PublicOrderDelta > Best->PublicOrderDelta))
			{
				Best = &Option;
			}
		}
		FString Message;
		ResolveEvent(Event.InstanceId, Best->OptionId, Message);
	}
}

void UWLPoliticalSubsystem::RunStrategicAIForNation(const FString& NationIso)
{
	UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Tick || !Registry)
	{
		return;
	}

	const FString Iso = NormalizeIso(NationIso);
	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);
	const int64 Treasury = Tick->GetTreasury(Iso);
	const int64 OwnStrength = Tick->GetNationMilitaryStrength(Iso);
	const int32 Month = Tick->GetCurrentMonth();
	const FWLPoliticalAIPlanState DiplomaticPlan = GetGovernmentAIPlan(Iso);
	const bool bBlocDoctrine = DiplomaticPlan.Objective == EWLGovernmentAIObjective::Align;
	const bool bExpansionDoctrine = DiplomaticPlan.Objective == EWLGovernmentAIObjective::Expand
		|| DiplomaticPlan.Objective == EWLGovernmentAIObjective::Militarize;
	const bool bWarCooldown = GetPoliticalMemoryValue(Iso, TEXT("war_declared_recently")) > 0;

	// 1) Fisco: en deficit o deuda sube impuestos; con bonanza los relaja hacia el default.
	const int32 Tax = Tick->GetTaxRate(Iso);
	if ((Treasury < 0 || Budget.Net() < 0) && Tax < Rules.TaxRateMaxPercent)
	{
		Tick->SetTaxRate(Iso, Tax + Rules.GetStrategicAITaxStep());
		UE_LOG(LogWorldLeader, Log, TEXT("IA %s: sube impuestos a %d%%."), *Iso, Tick->GetTaxRate(Iso));
	}
	else if (Treasury > Budget.TotalIncome() * 6 && Tax > Rules.TaxRateDefaultPercent)
	{
		Tick->SetTaxRate(Iso, Tax - Rules.GetStrategicAITaxStep());
	}

	// 2) Arancel: deficit comercial -> protege; superavit claro -> abre.
	const int32 Tariff = Tick->GetTariffRate(Iso);
	FString TariffMessage;
	if (Budget.ImportCost > Budget.ExportIncome && Tariff < Rules.GetStrategicAITariffCeiling())
	{
		SetNationTariffRate(Iso, Tariff + Rules.GetStrategicAITariffStep(), TariffMessage);
	}
	else if (Budget.ExportIncome > Budget.ImportCost * 2 && Tariff > 0)
	{
		SetNationTariffRate(Iso, Tariff - Rules.GetStrategicAITariffStep(), TariffMessage);
	}

	// 3) Diplomacia por pais: tratados si la opinion acompana, paz si pierde la guerra, guerra solo
	//    con opinion hundida y clara superioridad militar.
	FString WorstIso;
	int32 WorstOpinion = 0;
	for (const FWLNationData& Other : Registry->GetAllNations())
	{
		if (Other.Iso == Iso)
		{
			continue;
		}
		FWLDiplomaticRelationState Relation;
		GetRelation(Iso, Other.Iso, Relation);
		const int64 OtherStrength = Tick->GetNationMilitaryStrength(Other.Iso);
		FString Message;

		if (Relation.Status == EWLDiplomaticStatus::War)
		{
			if (static_cast<double>(OwnStrength) < static_cast<double>(OtherStrength) * Rules.GetStrategicAIPeaceStrengthRatio())
			{
				MakePeace(Iso, Other.Iso, Message);
				UE_LOG(LogWorldLeader, Log, TEXT("IA %s: pide la paz con %s."), *Iso, *Other.Iso);
			}
			continue;
		}

		const int32 TreatyOffset = Rules.GetStrategicAITreatyOpinionOffset() + (bBlocDoctrine ? -10 : 0) + (bExpansionDoctrine ? 5 : 0);
		if (Relation.Opinion >= 20 + TreatyOffset && !Relation.Treaties.Contains(EWLTreatyType::TradeAgreement))
		{
			if (SignTreaty(Iso, Other.Iso, EWLTreatyType::TradeAgreement, Message))
			{
				AddPoliticalMemory(Iso, TEXT("diplomatic_bloc_building"), 1, 18, Message);
			}
		}
		if (Relation.Opinion >= 25 + TreatyOffset && !Relation.Treaties.Contains(EWLTreatyType::NonAggression))
		{
			if (SignTreaty(Iso, Other.Iso, EWLTreatyType::NonAggression, Message))
			{
				AddPoliticalMemory(Iso, TEXT("diplomatic_bloc_building"), 1, 18, Message);
			}
		}
		if (Relation.Opinion >= 60 + TreatyOffset && !Relation.Treaties.Contains(EWLTreatyType::Alliance))
		{
			if (SignTreaty(Iso, Other.Iso, EWLTreatyType::Alliance, Message))
			{
				AddPoliticalMemory(Iso, TEXT("diplomatic_bloc_building"), 1, 24, Message);
			}
		}
		const int32 WarOpinionThreshold = Rules.GetStrategicAIWarOpinionThreshold() + (bExpansionDoctrine ? 8 : -8);
		const double WarStrengthRatio = Rules.GetStrategicAIWarStrengthRatio() + (bExpansionDoctrine ? -0.15 : 0.25);
		if (!bWarCooldown
			&& Relation.Opinion <= WarOpinionThreshold
			&& static_cast<double>(OwnStrength) > static_cast<double>(OtherStrength) * WarStrengthRatio)
		{
			if (DeclareWar(Iso, Other.Iso, Message))
			{
				AddPoliticalMemory(Iso, TEXT("war_declared_recently"), 1, 18, Message);
				AddPoliticalMemory(Iso, TEXT("diplomatic_war_doctrine"), 1, 24, Message);
				UE_LOG(LogWorldLeader, Warning, TEXT("IA %s: declara la guerra a %s."), *Iso, *Other.Iso);
			}
		}

		if (WorstIso.IsEmpty() || Relation.Opinion < WorstOpinion)
		{
			WorstIso = Other.Iso;
			WorstOpinion = Relation.Opinion;
		}
	}

	// 4) Intriga contra su peor relacion: primero red, luego alterna financiar golpe / propaganda.
	if (!WorstIso.IsEmpty()
		&& WorstOpinion < Rules.GetStrategicAIIntrigueOpinionThreshold()
		&& GetPoliticalMemoryValue(Iso, TEXT("covert_pressure_recently")) < 3
		&& Characters)
	{
		FString SpyId;
		for (const FWLCharacter& Spy : Characters->GetCharactersByRole(Iso, EWLCharacterRole::Spy))
		{
			if (Spy.bActive)
			{
				SpyId = Spy.Id;
				break;
			}
		}
		if (!SpyId.IsEmpty())
		{
			FString Message;
			const FWLIntelligenceNetworkState Network = GetIntelligenceNetwork(Iso, WorstIso);
			if (Network.NetworkStrength < Rules.GetStrategicAISpyNetworkTarget())
			{
				BuildSpyNetwork(Iso, WorstIso, SpyId, Message);
			}
			else if (Network.Exposure < Rules.GetStrategicAISpyExposureLimit())
			{
				RunSpyOperation(Iso, WorstIso, SpyId,
					Month % 2 == 0 ? EWLSpyOperationType::FundCoup : EWLSpyOperationType::Propaganda, Message);
				AddPoliticalMemory(Iso, TEXT("covert_pressure_recently"), 1, 12, Message);
				UE_LOG(LogWorldLeader, Log, TEXT("IA %s vs %s: %s"), *Iso, *WorstIso, *Message);
			}
		}
	}

	// 5) Reclutamiento: con caja y por detras militarmente de su peor relacion, encola tropa en su HQ.
	if (!WorstIso.IsEmpty() && Treasury > Rules.GetStrategicAIRecruitTreasuryThreshold()
		&& static_cast<double>(OwnStrength)
			< static_cast<double>(Tick->GetNationMilitaryStrength(WorstIso)) * Rules.GetStrategicAIRecruitStrengthRatio())
	{
		FString Message;
		if (Tick->QueueRecruit(Iso + TEXT("-AI-HQ"), Iso, TEXT("infantry"), Message))
		{
			AddGovernmentLogEntry(EWLGovernmentLogCategory::Military, Iso, TEXT(""),
				TEXT("Expansion militar IA"),
				FString::Printf(TEXT("%s expande su ejercito: nueva leva de infanteria."), *Iso),
				TEXT("strategic_ai"), 5, true, Iso == GetPlayerNationIso());
			UE_LOG(LogWorldLeader, Log, TEXT("IA %s: recluta infanteria (%s)."), *Iso, *Message);
		}
	}

	// 6) OFENSIVA: si esta en guerra, la IA no se queda en casa — marcha y ataca.
	RunStrategicAIMilitaryOffensive(Iso);
}

void UWLPoliticalSubsystem::RunStrategicAIMilitaryOffensive(const FString& NationIso)
{
	UWLMilitarySubsystem* Military = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWLMilitarySubsystem>() : nullptr;
	const UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Military || !Tick || !Registry)
	{
		return;
	}
	const FString Iso = NormalizeIso(NationIso);

	// Naciones con las que Iso esta EN GUERRA (sin guerra no hay ofensiva).
	TSet<FString> Enemies;
	for (const FWLNationData& Other : Registry->GetAllNations())
	{
		if (Other.Iso == Iso)
		{
			continue;
		}
		FWLDiplomaticRelationState Relation;
		if (GetRelation(Iso, Other.Iso, Relation) && Relation.Status == EWLDiplomaticStatus::War)
		{
			Enemies.Add(Other.Iso);
		}
	}
	if (Enemies.IsEmpty())
	{
		return;
	}

	// En guerra y sin ejercitos de campo: la IA DESPLIEGA su guarnicion reclutada (base ISO-AI-HQ)
	// como ejercito real. El jugador despliega via sus fuertes del mapa; sin esto la IA reclutaba a
	// guarnicion pero jamas ponia tropas en el campo (el asiento cae a su capital).
	bool bHasFieldArmy = false;
	for (const FWLArmy& Army : Military->GetArmies())
	{
		if (Army.OwnerIso == Iso && Army.Units.Num() > 0)
		{
			bHasFieldArmy = true;
			break;
		}
	}
	if (!bHasFieldArmy)
	{
		const FString BaseId = Iso + TEXT("-AI-HQ");
		TArray<TPair<FString, int32>> GarrisonUnits;
		for (const FWLGarrisonGroup& Group : Tick->GetGarrisonRecruited(BaseId))
		{
			GarrisonUnits.Add(TPair<FString, int32>(Group.UnitType, Group.Count));
		}
		if (GarrisonUnits.Num() > 0)
		{
			const FString DeployedId = Military->SyncArmyFromGarrison(BaseId, Iso, FString(), GarrisonUnits);
			if (!DeployedId.IsEmpty())
			{
				AddGovernmentLogEntry(EWLGovernmentLogCategory::Military, Iso, TEXT(""),
					TEXT("Movilizacion IA"),
					FString::Printf(TEXT("%s moviliza su guarnicion como ejercito de campo."), *Iso),
					TEXT("strategic_ai"), 6, true, false);
				UE_LOG(LogWorldLeader, Warning, TEXT("IA %s: despliega guarnicion como ejercito %s."), *Iso, *DeployedId);
			}
		}
	}

	// Mapa de distancia por el grafo de provincias desde CUALQUIER provincia enemiga (BFS multi-fuente).
	// Cada ejercito propio usara este mapa para marchar cuesta abajo hacia el frente.
	TMap<FString, int32> DistanceToEnemy;
	TArray<FString> Frontier;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (Enemies.Contains(Tick->GetProvinceControllerIso(Province.Id)))
		{
			DistanceToEnemy.Add(Province.Id, 0);
			Frontier.Add(Province.Id);
		}
	}
	for (int32 Head = 0; Head < Frontier.Num(); ++Head)
	{
		const FString Current = Frontier[Head];
		const int32 NextDistance = DistanceToEnemy[Current] + 1;
		FWLProvinceData ProvinceData;
		if (!Registry->GetProvince(Current, ProvinceData))
		{
			continue;
		}
		for (const FString& Neighbor : ProvinceData.Neighbors)
		{
			if (!DistanceToEnemy.Contains(Neighbor))
			{
				DistanceToEnemy.Add(Neighbor, NextDistance);
				Frontier.Add(Neighbor);
			}
		}
	}

	// GetArmies() devuelve una copia: iterar es seguro aunque una batalla elimine ejercitos del
	// estado real (las operaciones revalidan por Id y fallan sin efecto sobre entradas ya muertas).
	for (const FWLArmy& Army : Military->GetArmies())
	{
		if (Army.OwnerIso != Iso || Army.Units.Num() == 0)
		{
			continue;
		}

		// 1) Ejercito enemigo a tiro (misma/adyacente provincia + en guerra): atacarlo.
		const TArray<FString> Targets = Military->GetAttackableTargetIds(Army.Id);
		if (Targets.Num() > 0)
		{
			FString Report;
			Military->ResolveTacticalBattleToEnd(Army.Id, Targets[0], Report);
			AddGovernmentLogEntry(EWLGovernmentLogCategory::Military, Iso, TEXT(""),
				TEXT("Ofensiva IA"), FString::Printf(TEXT("%s ataca a un ejercito enemigo. %s"), *Army.Id, *Report),
				TEXT("strategic_ai"), 8, true, false);
			UE_LOG(LogWorldLeader, Warning, TEXT("IA %s: %s ataca a %s. %s"), *Iso, *Army.Id, *Targets[0], *Report);
			continue;
		}

		// 2) Parado en provincia enemiga sin ejercito defensor: asaltar la ciudad (milicia local).
		FString AssaultReason;
		if (Military->CanAssaultProvince(Army.Id, AssaultReason))
		{
			FString Report;
			Military->ResolveProvinceAssaultToEnd(Army.Id, Report);
			AddGovernmentLogEntry(EWLGovernmentLogCategory::Military, Iso, TEXT(""),
				TEXT("Asalto IA"), FString::Printf(TEXT("%s asalta una provincia enemiga. %s"), *Army.Id, *Report),
				TEXT("strategic_ai"), 8, true, false);
			UE_LOG(LogWorldLeader, Warning, TEXT("IA %s: %s asalta %s. %s"), *Iso, *Army.Id, *Army.ProvinceId, *Report);
			continue;
		}

		// 3) Marchar hacia el frente: el vecino con MENOR distancia a territorio enemigo, si acerca.
		FWLProvinceData ProvinceData;
		if (!Registry->GetProvince(Army.ProvinceId, ProvinceData))
		{
			continue;
		}
		const int32* MyDistance = DistanceToEnemy.Find(Army.ProvinceId);
		int32 BestDistance = MyDistance ? *MyDistance : TNumericLimits<int32>::Max();
		FString BestNeighbor;
		for (const FString& Neighbor : ProvinceData.Neighbors)
		{
			const int32* NeighborDistance = DistanceToEnemy.Find(Neighbor);
			if (NeighborDistance && *NeighborDistance < BestDistance)
			{
				BestDistance = *NeighborDistance;
				BestNeighbor = Neighbor;
			}
		}
		if (!BestNeighbor.IsEmpty())
		{
			FString MoveMessage;
			if (Military->MoveArmy(Army.Id, BestNeighbor, MoveMessage))
			{
				UE_LOG(LogWorldLeader, Log, TEXT("IA %s: %s marcha hacia el frente (%s)."), *Iso, *Army.Id, *BestNeighbor);
			}
		}
	}
}

void UWLPoliticalSubsystem::RunGovernmentAIForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Tick || !Characters || !ValidateNation(Iso))
	{
		return;
	}

	FWLPoliticalAIPlanState& Plan = EnsureGovernmentAIPlan(Iso);
	const FWLNationBudget Budget = Tick->GetNationBudget(Iso);
	const FWLInternalPowerState Internal = GetInternalPower(Iso);
	const int64 Treasury = Tick->GetTreasury(Iso);
	const FString TargetIso = SelectGovernmentAITarget(Iso);
	const int64 OwnStrength = Tick->GetNationMilitaryStrength(Iso);
	const int64 TargetStrength = TargetIso.IsEmpty() ? 0 : Tick->GetNationMilitaryStrength(TargetIso);

	if (Internal.AveragePublicOrder < 50 || Internal.CoupRisk > 60 || Internal.OppositionStrength > 55)
	{
		Plan.Objective = EWLGovernmentAIObjective::Stabilize;
		Plan.LastPlanReason = TEXT("orden publico/oposicion exigen estabilizar");
	}
	else if (Treasury < 0 || Budget.Net() < 0)
	{
		Plan.Objective = EWLGovernmentAIObjective::Borrow;
		Plan.LastPlanReason = TEXT("deficit obliga a financiarse");
	}
	else if (!TargetIso.IsEmpty() && OwnStrength < TargetStrength)
	{
		Plan.Objective = EWLGovernmentAIObjective::Militarize;
		Plan.LastPlanReason = TEXT("rival supera fuerza militar");
	}
	else if (!TargetIso.IsEmpty() && OwnStrength > TargetStrength * 2 && TargetStrength > 0)
	{
		Plan.Objective = EWLGovernmentAIObjective::Expand;
		Plan.LastPlanReason = TEXT("ventaja militar abre expansion");
	}
	else if (GetRelationsForNation(Iso).Num() > 0 && Budget.ExportIncome >= Budget.ImportCost)
	{
		Plan.Objective = EWLGovernmentAIObjective::Align;
		Plan.LastPlanReason = TEXT("economia permite diplomacia de bloque");
	}
	else
	{
		Plan.Objective = EWLGovernmentAIObjective::Industrialize;
		Plan.LastPlanReason = TEXT("base economica pide industrializacion");
	}
	Plan.TargetIso = TargetIso;
	++Plan.MonthsOnPlan;

	TArray<EWLGovernmentPriority> Agenda;
	FString ProgramId;
	switch (Plan.Objective)
	{
	case EWLGovernmentAIObjective::Stabilize:
		Agenda = { EWLGovernmentPriority::Control, EWLGovernmentPriority::Security, EWLGovernmentPriority::Growth };
		ProgramId = Internal.OppositionStrength > 55 ? TEXT("int_public_order") : TEXT("int_governors");
		if (Internal.OppositionStrength > 70)
		{
			FString RepressMessage;
			RepressOpposition(Iso, RepressMessage);
		}
		break;
	case EWLGovernmentAIObjective::Borrow:
		Agenda = { EWLGovernmentPriority::Austerity, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Diplomacy };
		ProgramId = TEXT("econ_tax_reform");
		if (Treasury < 0)
		{
			FString FinanceMessage;
			Tick->IssueBond(Iso, 8000, 24, FinanceMessage);
		}
		break;
	case EWLGovernmentAIObjective::Militarize:
	case EWLGovernmentAIObjective::Expand:
		Agenda = { EWLGovernmentPriority::Security, EWLGovernmentPriority::Industrialization, EWLGovernmentPriority::Control };
		ProgramId = Plan.Objective == EWLGovernmentAIObjective::Expand ? TEXT("def_mobilization") : TEXT("def_procurement");
		break;
	case EWLGovernmentAIObjective::Align:
		Agenda = { EWLGovernmentPriority::Diplomacy, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Security };
		ProgramId = TEXT("for_bloc");
		break;
	case EWLGovernmentAIObjective::Industrialize:
	default:
		Agenda = { EWLGovernmentPriority::Industrialization, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Diplomacy };
		ProgramId = TEXT("econ_public_investment");
		break;
	}

	FString AgendaMessage;
	SetGovernmentAgenda(Iso, Agenda, AgendaMessage);
	if (GetActiveMinistryPrograms(Iso).Num() < 2)
	{
		FString ProgramMessage;
		if (StartMinistryProgram(Iso, ProgramId, ProgramMessage))
		{
			Plan.CurrentProgramId = ProgramId;
		}
	}

	if (EnsureInstitutionalPower(Iso).RulingCoalitionSupport < 50)
	{
		for (const FWLPartyState& Party : GetPoliticalParties(Iso))
		{
			if ((Party.Role == EWLPartyRole::Ally || Party.Role == EWLPartyRole::SoftOpposition) && !Party.bInCoalition)
			{
				FString PartyMessage;
				NegotiatePartySupport(Iso, Party.PartyId, PartyMessage);
				break;
			}
		}
	}

	if (GetActivePolicyReforms(Iso).IsEmpty() && Characters->GetPoliticalCapital(Iso) >= 35)
	{
		const FString ReformId =
			Plan.Objective == EWLGovernmentAIObjective::Stabilize ? TEXT("security_citizen_plan") :
			Plan.Objective == EWLGovernmentAIObjective::Borrow ? TEXT("tax_broad_base") :
			Plan.Objective == EWLGovernmentAIObjective::Militarize || Plan.Objective == EWLGovernmentAIObjective::Expand ? TEXT("military_professionalization") :
			Plan.Objective == EWLGovernmentAIObjective::Align ? TEXT("trade_customs_modernization") :
			TEXT("energy_sovereignty");
		FString ReformMessage;
		EnactPolicyReform(Iso, ReformId, ReformMessage);
	}

	FWLElectionState Election = GetElectionState(Iso);
	if (Election.MonthsToElection <= 8 && Election.CampaignPromiseReformId.IsEmpty())
	{
		FString PromiseMessage;
		MakeCampaignPromise(Iso,
			Plan.Objective == EWLGovernmentAIObjective::Stabilize ? TEXT("security_citizen_plan") : TEXT("edu_public_schools"),
			PromiseMessage);
	}

	if (GetMediaPublicOpinion(Iso).MediaCrisisRisk > 55)
	{
		FString MediaMessage;
		RunMediaAction(Iso, EWLMediaActionType::CounterFakeNews, MediaMessage);
	}
	else if (GetMediaPublicOpinion(Iso).PresidentialApproval < 45)
	{
		FString MediaMessage;
		RunMediaAction(Iso, EWLMediaActionType::StateBroadcast, MediaMessage);
	}

	for (const FWLRegionGovernorState& Region : GetRegionGovernors(Iso))
	{
		if (Region.RebellionRisk > 55 || Region.ProtestRisk > 60)
		{
			FString RegionMessage;
			RunRegionPolicy(Iso, Region.RegionId, EWLRegionPolicyActionType::RegionalInvestment, RegionMessage);
			break;
		}
	}

	if (EnsureInstitutionalPower(Iso).GridlockRisk > 65 && GetPatronageState(Iso).ClientelistPressure < 70)
	{
		FString PatronageMessage;
		UsePatronage(Iso, EWLPatronageActionType::GrantFavor, PatronageMessage);
	}

	FWLGovernmentStats Stats = Characters->GetGovernmentStats(Iso);
	if (Stats.FilledOffices > 0 && Stats.AverageSkill < 56 && Characters->GetPoliticalCapital(Iso) >= 20)
	{
		FWLCharacter Hired;
		FString HireMessage;
		const EWLMinisterOffice Office =
			Plan.Objective == EWLGovernmentAIObjective::Militarize || Plan.Objective == EWLGovernmentAIObjective::Expand
				? EWLMinisterOffice::Defense
				: Plan.Objective == EWLGovernmentAIObjective::Align
					? EWLMinisterOffice::Foreign
					: Plan.Objective == EWLGovernmentAIObjective::Stabilize
						? EWLMinisterOffice::Interior
						: EWLMinisterOffice::Economy;
		Characters->HireMinister(Iso, Office, Hired, HireMessage);
	}
}

int32 UWLPoliticalSubsystem::GetMinisterProgramModifier(const FString& NationIso, EWLMinisterOffice Office) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return 0;
	}
	FWLCharacter Minister;
	if (!Characters->GetCabinetMinister(NationIso, Office, Minister))
	{
		return -12;
	}

	int32 Modifier = FMath::RoundToInt((static_cast<double>(Minister.Skill) - 50.0) / 3.0);
	for (const FString& Trait : Minister.Traits)
	{
		const FString T = Trait.ToLower();
		if (T == TEXT("tecnocrata") || T == TEXT("fiscalista") || T == TEXT("disciplinado") || T == TEXT("diplomatica"))
		{
			Modifier += 4;
		}
		else if (T == TEXT("halcon") && Office == EWLMinisterOffice::Defense)
		{
			Modifier += 3;
		}
		else if (T == TEXT("sigiloso") && Office == EWLMinisterOffice::Intelligence)
		{
			Modifier += 3;
		}
		else if (T == TEXT("corrupto"))
		{
			Modifier -= 10;
		}
		else if (T == TEXT("clientelista"))
		{
			Modifier -= 6;
		}
	}
	return FMath::Clamp(Modifier, -20, 25);
}

void UWLPoliticalSubsystem::AdjustPublicGroupSupport(
	const FString& NationIso,
	EWLPublicGroup Group,
	int32 SupportDelta,
	const FString& Reason)
{
	FWLPublicGroupSupportState& State = EnsurePublicGroup(NationIso, Group);
	State.Support = ClampPercent(State.Support + SupportDelta);
	State.LastShiftReason = Reason;
}

bool UWLPoliticalSubsystem::ApplyProgramEffect(
	const FString& NationIso,
	const FWLMinistryProgramDefinition& Definition,
	FWLMinistryProgramState& Program)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	FWLInternalPowerState& Internal = EnsureInternalPower(Iso);
	FWLStateCapacityState& Capacity = EnsureStateCapacity(Iso);
	FWLInstitutionalPowerState& Institutions = EnsureInstitutionalPower(Iso);

	const FString Id = Definition.ProgramId;
	if (Id == TEXT("econ_tax_reform"))
	{
		if (Tick) { Tick->SetTaxRate(Iso, Tick->GetTaxRate(Iso) + 2); }
		Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + 1);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -1, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -1, Definition.Name);
	}
	else if (Id == TEXT("econ_public_investment"))
	{
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
		Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + 1);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 2, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::MiddleClass, 1, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 1, Definition.Name);
	}
	else if (Id == TEXT("econ_subsidies"))
	{
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 2); }
		Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 2);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 2, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, 2, Definition.Name);
	}
	else if (Id == TEXT("def_doctrine"))
	{
		EnsureCabinetDynamics(Iso).RivalryPressure = ClampPercent(EnsureCabinetDynamics(Iso).RivalryPressure - 2);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, Definition.Name);
	}
	else if (Id == TEXT("def_procurement"))
	{
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, Definition.Name);
		if (Tick)
		{
			FString RecruitMessage;
			Tick->QueueRecruit(Iso + TEXT("-AI-HQ"), Iso, TEXT("infantry"), RecruitMessage);
		}
	}
	else if (Id == TEXT("def_mobilization"))
	{
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 3, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -2, Definition.Name);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -1, Definition.Name);
		if (Tick)
		{
			Tick->AdjustNationPublicOrder(Iso, -1);
			FString RecruitMessage;
			Tick->QueueRecruit(Iso + TEXT("-AI-HQ"), Iso, TEXT("infantry"), RecruitMessage);
		}
	}
	else if (Id == TEXT("int_police_reform"))
	{
		Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 2);
		Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 2);
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
	}
	else if (Id == TEXT("int_governors"))
	{
		Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 2);
		Institutions.RulingCoalitionSupport = ClampPercent(Institutions.RulingCoalitionSupport + 1);
		AdjustPublicGroupSupport(Iso, EWLPublicGroup::Regions, 3, Definition.Name);
	}
	else if (Id == TEXT("int_public_order"))
	{
		Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 3);
		if (Tick) { Tick->AdjustNationPublicOrder(Iso, 2); }
	}
	else if (Id == TEXT("for_bloc") || Id == TEXT("for_trade_drive"))
	{
		FString BestIso;
		int32 BestOpinion = -101;
		for (const FWLDiplomaticRelationState& Relation : GetRelationsForNation(Iso))
		{
			const FString OtherIso = Relation.NationA == Iso ? Relation.NationB : Relation.NationA;
			if (Relation.Opinion > BestOpinion)
			{
				BestOpinion = Relation.Opinion;
				BestIso = OtherIso;
			}
		}
		if (!BestIso.IsEmpty())
		{
			FString Message;
			AdjustRelationOpinion(Iso, BestIso, 2, Message);
			SignTreaty(Iso, BestIso,
				Id == TEXT("for_trade_drive") ? EWLTreatyType::TradeAgreement : EWLTreatyType::NonAggression,
				Message);
		}
	}
	else if (Id == TEXT("for_sanctions"))
	{
		const FString Target = SelectGovernmentAITarget(Iso);
		if (!Target.IsEmpty())
		{
			FString Message;
			SignTreaty(Iso, Target, EWLTreatyType::Embargo, Message);
		}
	}
	else if (Id == TEXT("spy_networks") || Id == TEXT("spy_covert_ops"))
	{
		const FString Target = SelectGovernmentAITarget(Iso);
		if (const UWLCharacterSubsystem* Characters = GetCharacters())
		{
			FString SpyId;
			for (const FWLCharacter& Spy : Characters->GetCharactersByRole(Iso, EWLCharacterRole::Spy))
			{
				if (Spy.bActive)
				{
					SpyId = Spy.Id;
					break;
				}
			}
			if (!SpyId.IsEmpty() && !Target.IsEmpty())
			{
				FString Message;
				if (Id == TEXT("spy_networks"))
				{
					BuildSpyNetwork(Iso, Target, SpyId, Message);
				}
				else
				{
					if (GetIntelligenceNetwork(Iso, Target).NetworkStrength < 20)
					{
						BuildSpyNetwork(Iso, Target, SpyId, Message);
					}
					else
					{
						RunSpyOperation(Iso, Target, SpyId, EWLSpyOperationType::Propaganda, Message);
					}
				}
			}
		}
	}
	else if (Id == TEXT("spy_counterintel"))
	{
		for (TPair<FString, FWLIntelligenceNetworkState>& Pair : IntelligenceByPair)
		{
			FWLIntelligenceNetworkState& Network = Pair.Value;
			if (Network.TargetIso == Iso)
			{
				Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 8);
				Network.Exposure = ClampPercent(Network.Exposure + 5);
			}
			else if (Network.OwnerIso == Iso)
			{
				Network.Exposure = ClampPercent(Network.Exposure - 8);
			}
		}
	}
	else
	{
		switch (Definition.Office)
		{
		case EWLMinisterOffice::Economy:
			Capacity.Bureaucracy = ClampPercent(Capacity.Bureaucracy + (Id.Contains(TEXT("corruption")) ? 2 : 1));
			if (Id.Contains(TEXT("privatization")) || Id.Contains(TEXT("concession")))
			{
				if (Tick)
				{
					FString TreasuryMessage;
					Tick->AdjustTreasury(Iso, 900, TreasuryMessage);
				}
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Business, 2, Definition.Name);
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Unions, -2, Definition.Name);
			}
			else
			{
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, 1, Definition.Name);
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::MiddleClass, 1, Definition.Name);
			}
			break;
		case EWLMinisterOffice::Defense:
			AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 2, Definition.Name);
			EnsureCabinetDynamics(Iso).RivalryPressure = ClampPercent(EnsureCabinetDynamics(Iso).RivalryPressure - 1);
			if (Id.Contains(TEXT("service")) || Id.Contains(TEXT("counterinsurgency")))
			{
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Workers, -1, Definition.Name);
				if (Tick) { Tick->AdjustNationPublicOrder(Iso, -1); }
			}
			break;
		case EWLMinisterOffice::Interior:
			Capacity.CentralAuthority = ClampPercent(Capacity.CentralAuthority + 1);
			Internal.OppositionStrength = ClampPercent(Internal.OppositionStrength - 1);
			if (Id.Contains(TEXT("decentralization")) || Id.Contains(TEXT("dialogue")))
			{
				AdjustPublicGroupSupport(Iso, EWLPublicGroup::Regions, 2, Definition.Name);
			}
			if (Tick) { Tick->AdjustNationPublicOrder(Iso, 1); }
			break;
		case EWLMinisterOffice::Foreign:
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA == Iso || Relation.NationB == Iso)
				{
					Relation.Opinion = FMath::Clamp(Relation.Opinion + 1, -100, 100);
				}
			}
			EnsureElectionState(Iso).Legitimacy = ClampPercent(EnsureElectionState(Iso).Legitimacy + 1);
			break;
		case EWLMinisterOffice::Intelligence:
			EnsureMediaState(Iso).FakeNewsPressure = ClampPercent(EnsureMediaState(Iso).FakeNewsPressure - 2);
			EnsureCabinetDynamics(Iso).SabotageRisk = ClampPercent(EnsureCabinetDynamics(Iso).SabotageRisk - 1);
			if (Id.Contains(TEXT("black_budget")))
			{
				EnsurePatronageState(Iso).ContractCorruption = ClampPercent(EnsurePatronageState(Iso).ContractCorruption + 2);
			}
			break;
		default:
			return false;
		}
	}

	Program.LastReport = FString::Printf(TEXT("%s avanza: %s (%d%%)."), *Definition.Name, *Iso, Program.Progress);
	return true;
}

FString UWLPoliticalSubsystem::SelectGovernmentAITarget(const FString& NationIso) const
{
	const FString Iso = NormalizeIso(NationIso);
	FString WorstIso;
	int32 WorstOpinion = 101;
	for (const FWLDiplomaticRelationState& Relation : GetRelationsForNation(Iso))
	{
		const FString OtherIso = Relation.NationA == Iso ? Relation.NationB : Relation.NationA;
		if (Relation.Opinion < WorstOpinion)
		{
			WorstOpinion = Relation.Opinion;
			WorstIso = OtherIso;
		}
	}
	return WorstIso;
}

