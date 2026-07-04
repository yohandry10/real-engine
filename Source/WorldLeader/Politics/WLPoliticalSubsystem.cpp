// Copyright World Leader project. See ROADMAP.md.

#include "Politics/WLPoliticalSubsystem.h"
#include "Politics/WLPoliticalSubsystemPrivate.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Military/WLMilitarySubsystem.h"
#include "WorldLeader.h"
#include "Engine/GameInstance.h"

using namespace WLPoliticsPrivate;

void UWLPoliticalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UWLDataRegistry::StaticClass());
	Collection.InitializeDependency(UWLStrategicTickSubsystem::StaticClass());
	Collection.InitializeDependency(UWLCharacterSubsystem::StaticClass());
	ResetPoliticalState();
}

UWLDataRegistry* UWLPoliticalSubsystem::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

UWLStrategicTickSubsystem* UWLPoliticalSubsystem::GetTick() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLStrategicTickSubsystem>() : nullptr;
}

UWLCharacterSubsystem* UWLPoliticalSubsystem::GetCharacters() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLCharacterSubsystem>() : nullptr;
}

FString UWLPoliticalSubsystem::NormalizeIso(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

FString UWLPoliticalSubsystem::NormalizeCharacterId(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

int32 UWLPoliticalSubsystem::ClampPercent(int32 Value)
{
	return FMath::Clamp(Value, 0, 100);
}

FString UWLPoliticalSubsystem::RelationKey(const FString& NationA, const FString& NationB)
{
	const FString A = NormalizeIso(NationA);
	const FString B = NormalizeIso(NationB);
	return A < B ? A + TEXT("|") + B : B + TEXT("|") + A;
}

FString UWLPoliticalSubsystem::NetworkKey(const FString& OwnerIso, const FString& TargetIso)
{
	return NormalizeIso(OwnerIso) + TEXT(">") + NormalizeIso(TargetIso);
}

FString UWLPoliticalSubsystem::PublicGroupKey(const FString& NationIso, EWLPublicGroup Group)
{
	return NormalizeIso(NationIso) + TEXT("|") + FString::FromInt(static_cast<int32>(Group));
}

FString UWLPoliticalSubsystem::PoliticalMemoryKey(const FString& NationIso, const FString& MemoryKey)
{
	return NormalizeIso(NationIso) + TEXT("|") + MemoryKey.TrimStartAndEnd().ToLower();
}

FString UWLPoliticalSubsystem::TreatyToString(EWLTreatyType Treaty)
{
	switch (Treaty)
	{
	case EWLTreatyType::TradeAgreement: return TEXT("acuerdo comercial");
	case EWLTreatyType::NonAggression:  return TEXT("pacto de no agresion");
	case EWLTreatyType::Alliance:       return TEXT("alianza");
	case EWLTreatyType::Embargo:        return TEXT("embargo");
	default:                            return TEXT("tratado");
	}
}

FString UWLPoliticalSubsystem::OperationToString(EWLSpyOperationType Operation)
{
	switch (Operation)
	{
	case EWLSpyOperationType::SabotageEconomy:     return TEXT("sabotaje economico");
	case EWLSpyOperationType::SabotageArmy:        return TEXT("sabotaje militar");
	case EWLSpyOperationType::FundCoup:            return TEXT("financiar golpe");
	case EWLSpyOperationType::Propaganda:          return TEXT("propaganda");
	case EWLSpyOperationType::CounterIntelligence: return TEXT("contraespionaje");
	default:                                       return TEXT("operacion");
	}
}

FString UWLPoliticalSubsystem::GovernmentPriorityToString(EWLGovernmentPriority Priority)
{
	switch (Priority)
	{
	case EWLGovernmentPriority::Security:          return TEXT("seguridad");
	case EWLGovernmentPriority::Growth:            return TEXT("crecimiento");
	case EWLGovernmentPriority::Austerity:         return TEXT("austeridad");
	case EWLGovernmentPriority::Industrialization: return TEXT("industrializacion");
	case EWLGovernmentPriority::Diplomacy:         return TEXT("diplomacia");
	case EWLGovernmentPriority::Control:           return TEXT("control interno");
	default:                                       return TEXT("prioridad");
	}
}

FString UWLPoliticalSubsystem::PublicGroupToString(EWLPublicGroup Group)
{
	switch (Group)
	{
	case EWLPublicGroup::Business:   return TEXT("empresarios");
	case EWLPublicGroup::Military:   return TEXT("militares");
	case EWLPublicGroup::Workers:    return TEXT("trabajadores");
	case EWLPublicGroup::Regions:    return TEXT("regiones");
	case EWLPublicGroup::MiddleClass:return TEXT("clase media");
	case EWLPublicGroup::Unions:     return TEXT("sindicatos");
	default:                         return TEXT("grupo social");
	}
}

FString UWLPoliticalSubsystem::GovernmentAIObjectiveToString(EWLGovernmentAIObjective Objective)
{
	switch (Objective)
	{
	case EWLGovernmentAIObjective::Stabilize:     return TEXT("estabilizar");
	case EWLGovernmentAIObjective::Expand:        return TEXT("expandirse");
	case EWLGovernmentAIObjective::Borrow:        return TEXT("financiarse");
	case EWLGovernmentAIObjective::Militarize:    return TEXT("militarizar");
	case EWLGovernmentAIObjective::Align:         return TEXT("alinear bloque");
	case EWLGovernmentAIObjective::Industrialize: return TEXT("industrializar");
	default:                                      return TEXT("planificar");
	}
}

FString UWLPoliticalSubsystem::ReformMemoryKey(const FString& ReformId)
{
	return FString::Printf(TEXT("reform_%s"), *ReformId.TrimStartAndEnd().ToLower());
}

FString UWLPoliticalSubsystem::PartyKey(const FString& NationIso, const FString& PartyId)
{
	return NormalizeIso(NationIso) + TEXT("|") + PartyId.TrimStartAndEnd().ToLower();
}

FString UWLPoliticalSubsystem::CrisisChainKey(const FString& NationIso, EWLCrisisChainType Type)
{
	return FString::Printf(TEXT("%s|%d"), *NormalizeIso(NationIso), static_cast<int32>(Type));
}

bool UWLPoliticalSubsystem::ValidateNation(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetRegistry();
	FWLNationData Nation;
	return Registry && Registry->GetNation(NormalizeIso(NationIso), Nation);
}

FString UWLPoliticalSubsystem::GetPlayerNationIso() const
{
	if (const UWLCampaignGameInstance* GI = Cast<UWLCampaignGameInstance>(GetGameInstance()))
	{
		return NormalizeIso(GI->GetSelectedNationIso());
	}
	return FString();
}

void UWLPoliticalSubsystem::ResetPoliticalState()
{
	InternalPowerByNation.Reset();
	RelationsByPair.Reset();
	IntelligenceByPair.Reset();
	EventQueue.Reset();
	EventDefinitions.Reset();
	NewsLog.Reset();
	GovernmentAgendaByNation.Reset();
	ActiveMinistryPrograms.Reset();
	CabinetDynamicsByNation.Reset();
	InstitutionalPowerByNation.Reset();
	PublicGroupSupportByKey.Reset();
	StateCapacityByNation.Reset();
	PoliticalMemoryByKey.Reset();
	GovernmentAIPlanByNation.Reset();
	ActivePolicyReforms.Reset();
	EnactedPolicyReforms.Reset();
	PoliticalParties.Reset();
	ElectionStateByNation.Reset();
	CharacterPoliticalProfilesById.Reset();
	PatronageStateByNation.Reset();
	MediaStateByNation.Reset();
	RegionGovernors.Reset();
	CrisisChains.Reset();
	GovernmentCalibrationByNation.Reset();
	PoliticalActionRecords.Reset();
	GovernmentLogEntries.Reset();
	CampaignOutcome = FWLCampaignOutcomeState();
	NextEventInstanceNumber = 1;
	NextGovernmentLogNumber = 1;
	LoadEventDefinitions();
	SeedInternalPower();
	SeedRelations();
	SeedGovernmentState();
}

TArray<FWLGovernmentLogEntry> UWLPoliticalSubsystem::GetGovernmentLog(const FString& ViewerNationIso) const
{
	const FString Viewer = NormalizeIso(ViewerNationIso);
	TArray<FWLGovernmentLogEntry> Out;
	for (const FWLGovernmentLogEntry& Entry : GovernmentLogEntries)
	{
		const bool bVisibleToViewer = Entry.bPublic
			|| Entry.bPlayerVisible
			|| (!Viewer.IsEmpty() && (Entry.NationIso == Viewer || Entry.TargetIso == Viewer));
		if (bVisibleToViewer)
		{
			Out.Add(Entry);
		}
	}
	Out.Sort([](const FWLGovernmentLogEntry& A, const FWLGovernmentLogEntry& B)
	{
		if (A.Year != B.Year)
		{
			return A.Year > B.Year;
		}
		if (A.Month != B.Month)
		{
			return A.Month > B.Month;
		}
		if (A.Day != B.Day)
		{
			return A.Day > B.Day;
		}
		return A.EntryId > B.EntryId;
	});
	return Out;
}

void UWLPoliticalSubsystem::AddGovernmentLogEntry(
	EWLGovernmentLogCategory Category,
	const FString& NationIso,
	const FString& TargetIso,
	const FString& Title,
	const FString& Body,
	const FString& Source,
	int32 Severity,
	bool bPublic,
	bool bPlayerVisible)
{
	FWLGovernmentLogEntry Entry;
	Entry.EntryId = FString::Printf(TEXT("LOG-%05d"), NextGovernmentLogNumber++);
	Entry.NationIso = NormalizeIso(NationIso);
	Entry.TargetIso = NormalizeIso(TargetIso);
	Entry.Category = Category;
	Entry.Severity = FMath::Clamp(Severity, 0, 100);
	Entry.Title = Title.TrimStartAndEnd();
	Entry.Body = Body.TrimStartAndEnd();
	Entry.Source = Source.TrimStartAndEnd();
	Entry.bPublic = bPublic;
	const FString PlayerIso = GetPlayerNationIso();
	Entry.bPlayerVisible = bPlayerVisible
		|| (!PlayerIso.IsEmpty() && (Entry.NationIso == PlayerIso || Entry.TargetIso == PlayerIso));
	if (const UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Entry.Year = Tick->GetCurrentYear();
		Entry.Month = Tick->GetCurrentMonth();
		Entry.Day = Tick->GetCurrentDay();
		Entry.MonthKey = Entry.Year * 12 + Entry.Month;
	}
	if (Entry.Title.IsEmpty())
	{
		Entry.Title = Entry.Body.IsEmpty() ? TEXT("Registro de gobierno") : Entry.Body;
	}

	GovernmentLogEntries.Insert(Entry, 0);
	if (GovernmentLogEntries.Num() > MaxGovernmentLogEntries)
	{
		GovernmentLogEntries.SetNum(MaxGovernmentLogEntries);
	}
	RebuildNewsLogFromGovernmentLog();
}

void UWLPoliticalSubsystem::RebuildNewsLogFromGovernmentLog()
{
	NewsLog.Reset();
	TArray<FWLGovernmentLogEntry> PublicEntries = GovernmentLogEntries.FilterByPredicate(
		[](const FWLGovernmentLogEntry& Entry)
		{
			return Entry.bPublic;
		});
	PublicEntries.Sort([](const FWLGovernmentLogEntry& A, const FWLGovernmentLogEntry& B)
	{
		if (A.Year != B.Year)
		{
			return A.Year > B.Year;
		}
		if (A.Month != B.Month)
		{
			return A.Month > B.Month;
		}
		if (A.Day != B.Day)
		{
			return A.Day > B.Day;
		}
		return A.EntryId > B.EntryId;
	});
	for (const FWLGovernmentLogEntry& Entry : PublicEntries)
	{
		if (NewsLog.Num() >= MaxNewsLogEntries)
		{
			break;
		}
		const FString Summary = Entry.Body.IsEmpty() || Entry.Body == Entry.Title
			? Entry.Title
			: FString::Printf(TEXT("%s: %s"), *Entry.Title, *Entry.Body);
		NewsLog.Add(FString::Printf(TEXT("[%02d/%02d/%d] %s"), Entry.Day, Entry.Month, Entry.Year, *Summary));
	}
}

void UWLPoliticalSubsystem::AddNews(const FString& Item)
{
	AddGovernmentLogEntry(
		EWLGovernmentLogCategory::General,
		TEXT(""),
		TEXT(""),
		Item,
		TEXT(""),
		TEXT("news"),
		1,
		true,
		true);
}

void UWLPoliticalSubsystem::LoadEventDefinitions()
{
	const FString FilePath = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Political") / TEXT("PoliticalEvents.json");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		AddDefaultEventDefinitions(EventDefinitions);
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLPoliticalSubsystem: PoliticalEvents.json invalido, usando defaults."));
		AddDefaultEventDefinitions(EventDefinitions);
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;
		FWLPoliticalEventDefinition Def;
		Obj->TryGetStringField(TEXT("event_id"), Def.EventId);
		Obj->TryGetStringField(TEXT("title"), Def.Title);
		Obj->TryGetStringField(TEXT("body"), Def.Body);
		Obj->TryGetStringField(TEXT("trigger"), Def.Trigger);
		Obj->TryGetStringField(TEXT("target_iso"), Def.TargetIso);
		Obj->TryGetStringField(TEXT("crisis_type"), Def.CrisisType);
		Def.Trigger = Def.Trigger.TrimStartAndEnd().ToLower();
		Def.TargetIso = NormalizeIso(Def.TargetIso);
		Def.CrisisType = Def.CrisisType.TrimStartAndEnd().ToLower();
		Obj->TryGetNumberField(TEXT("min_coup_risk"), Def.MinCoupRisk);
		Obj->TryGetNumberField(TEXT("min_opposition_strength"), Def.MinOppositionStrength);
		if (!Obj->TryGetNumberField(TEXT("max_public_order"), Def.MaxPublicOrder))
		{
			Def.MaxPublicOrder = 100;
		}
		Obj->TryGetNumberField(TEXT("min_crisis_stage"), Def.MinCrisisStage);
		if (!Obj->TryGetNumberField(TEXT("max_crisis_stage"), Def.MaxCrisisStage))
		{
			Def.MaxCrisisStage = 5;
		}
		Obj->TryGetNumberField(TEXT("min_crisis_intensity"), Def.MinCrisisIntensity);
		if (!Obj->TryGetNumberField(TEXT("max_crisis_intensity"), Def.MaxCrisisIntensity))
		{
			Def.MaxCrisisIntensity = 100;
		}
		Def.MinCoupRisk = ClampPercent(Def.MinCoupRisk);
		Def.MinOppositionStrength = ClampPercent(Def.MinOppositionStrength);
		Def.MaxPublicOrder = ClampPercent(Def.MaxPublicOrder);
		Def.MinCrisisStage = FMath::Clamp(Def.MinCrisisStage, 0, 5);
		Def.MaxCrisisStage = FMath::Clamp(Def.MaxCrisisStage, Def.MinCrisisStage, 5);
		Def.MinCrisisIntensity = ClampPercent(Def.MinCrisisIntensity);
		Def.MaxCrisisIntensity = FMath::Clamp(Def.MaxCrisisIntensity, Def.MinCrisisIntensity, 100);

		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if (Obj->TryGetArrayField(TEXT("options"), Options) && Options)
		{
			for (const TSharedPtr<FJsonValue>& OptionValue : *Options)
			{
				const TSharedPtr<FJsonObject>* OptionObjPtr = nullptr;
				if (!OptionValue.IsValid() || !OptionValue->TryGetObject(OptionObjPtr) || !OptionObjPtr)
				{
					continue;
				}
				const TSharedPtr<FJsonObject>& OptionObj = *OptionObjPtr;
				FWLPoliticalEventOption Option;
				OptionObj->TryGetStringField(TEXT("option_id"), Option.OptionId);
				OptionObj->TryGetStringField(TEXT("label"), Option.Label);
				OptionObj->TryGetNumberField(TEXT("political_capital_delta"), Option.PoliticalCapitalDelta);
				double TreasuryDelta = 0.0;
				if (OptionObj->TryGetNumberField(TEXT("treasury_delta"), TreasuryDelta))
				{
					Option.TreasuryDelta = static_cast<int64>(TreasuryDelta);
				}
				OptionObj->TryGetNumberField(TEXT("opposition_delta"), Option.OppositionDelta);
				OptionObj->TryGetNumberField(TEXT("public_order_delta"), Option.PublicOrderDelta);
				OptionObj->TryGetNumberField(TEXT("relation_delta"), Option.RelationDelta);
				OptionObj->TryGetStringField(TEXT("market_shock_good_id"), Option.MarketShockGoodId);
				OptionObj->TryGetStringField(TEXT("market_shock_title"), Option.MarketShockTitle);
				OptionObj->TryGetNumberField(TEXT("market_shock_price_multiplier"), Option.MarketShockPriceMultiplier);
				OptionObj->TryGetNumberField(TEXT("market_shock_duration_months"), Option.MarketShockDurationMonths);
				if (!Option.OptionId.IsEmpty())
				{
					Def.Options.Add(MoveTemp(Option));
				}
			}
		}
		if (!Def.EventId.IsEmpty() && !Def.Options.IsEmpty())
		{
			EventDefinitions.Add(MoveTemp(Def));
		}
	}

	if (EventDefinitions.IsEmpty())
	{
		AddDefaultEventDefinitions(EventDefinitions);
	}
}

void UWLPoliticalSubsystem::SeedInternalPower()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		FWLInternalPowerState State;
		State.NationIso = Nation.Iso;
		State.AveragePublicOrder = GetAveragePublicOrder(Nation.Iso);
		InternalPowerByNation.Add(Nation.Iso, MoveTemp(State));
	}
}

void UWLPoliticalSubsystem::SeedRelations()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}
	const TArray<FWLNationData> Nations = Registry->GetAllNations();
	for (int32 i = 0; i < Nations.Num(); ++i)
	{
		for (int32 j = i + 1; j < Nations.Num(); ++j)
		{
			EnsureRelation(Nations[i].Iso, Nations[j].Iso);
		}
	}
}

void UWLPoliticalSubsystem::SeedGovernmentState()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}
	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		EnsureGovernmentAgenda(Nation.Iso);
		EnsureCabinetDynamics(Nation.Iso);
		EnsureInstitutionalPower(Nation.Iso);
		EnsureStateCapacity(Nation.Iso);
		EnsureGovernmentAIPlan(Nation.Iso);
		EnsureElectionState(Nation.Iso);
		EnsurePatronageState(Nation.Iso);
		EnsureMediaState(Nation.Iso);
		EnsureGovernmentCalibration(Nation.Iso);
		SeedPoliticalPartiesForNation(Nation.Iso);
		SeedRegionsForNation(Nation.Iso);
		SeedCharacterProfilesForNation(Nation.Iso);
		for (EWLPublicGroup Group : AllPublicGroups())
		{
			EnsurePublicGroup(Nation.Iso, Group);
		}
	}
}

FWLInternalPowerState& UWLPoliticalSubsystem::EnsureInternalPower(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLInternalPowerState& State = InternalPowerByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.AveragePublicOrder = GetAveragePublicOrder(Iso);
	}
	return State;
}

FWLDiplomaticRelationState& UWLPoliticalSubsystem::EnsureRelation(const FString& NationA, const FString& NationB)
{
	const FString Key = RelationKey(NationA, NationB);
	FWLDiplomaticRelationState& Relation = RelationsByPair.FindOrAdd(Key);
	if (Relation.NationA.IsEmpty() || Relation.NationB.IsEmpty())
	{
		const FString A = NormalizeIso(NationA);
		const FString B = NormalizeIso(NationB);
		Relation.NationA = A < B ? A : B;
		Relation.NationB = A < B ? B : A;
		Relation.Opinion = 0;
		Relation.Status = EWLDiplomaticStatus::Peace;
	}
	return Relation;
}

FWLIntelligenceNetworkState& UWLPoliticalSubsystem::EnsureNetwork(const FString& OwnerIso, const FString& TargetIso)
{
	const FString Key = NetworkKey(OwnerIso, TargetIso);
	FWLIntelligenceNetworkState& Network = IntelligenceByPair.FindOrAdd(Key);
	if (Network.OwnerIso.IsEmpty() || Network.TargetIso.IsEmpty())
	{
		Network.OwnerIso = NormalizeIso(OwnerIso);
		Network.TargetIso = NormalizeIso(TargetIso);
	}
	return Network;
}

FWLGovernmentAgendaState& UWLPoliticalSubsystem::EnsureGovernmentAgenda(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentAgendaState& Agenda = GovernmentAgendaByNation.FindOrAdd(Iso);
	if (Agenda.NationIso.IsEmpty())
	{
		Agenda.NationIso = Iso;
		Agenda.Priorities = DefaultGovernmentAgenda();
		Agenda.LastAgendaReport = TEXT("Agenda inicial: seguridad, crecimiento y diplomacia.");
	}
	return Agenda;
}

FWLCabinetDynamicsState& UWLPoliticalSubsystem::EnsureCabinetDynamics(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLCabinetDynamicsState& State = CabinetDynamicsByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
	}
	return State;
}

FWLInstitutionalPowerState& UWLPoliticalSubsystem::EnsureInstitutionalPower(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLInstitutionalPowerState& State = InstitutionalPowerByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
	}
	return State;
}

FWLPublicGroupSupportState& UWLPoliticalSubsystem::EnsurePublicGroup(const FString& NationIso, EWLPublicGroup Group)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPublicGroupSupportState& State = PublicGroupSupportByKey.FindOrAdd(PublicGroupKey(Iso, Group));
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.Group = Group;
		State.Support = 50;
	}
	return State;
}

FWLStateCapacityState& UWLPoliticalSubsystem::EnsureStateCapacity(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLStateCapacityState& State = StateCapacityByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.Bureaucracy = 50;
		State.Corruption = 30;
		State.AdministrativeEfficiency = 50;
		State.CentralAuthority = 50;
		State.PolicyFailureRisk = 25;
	}
	return State;
}

FWLPoliticalAIPlanState& UWLPoliticalSubsystem::EnsureGovernmentAIPlan(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPoliticalAIPlanState& Plan = GovernmentAIPlanByNation.FindOrAdd(Iso);
	if (Plan.NationIso.IsEmpty())
	{
		Plan.NationIso = Iso;
		Plan.Objective = EWLGovernmentAIObjective::Stabilize;
		Plan.LastPlanReason = TEXT("Plan inicial: estabilizar gobierno.");
	}
	return Plan;
}

FWLElectionState& UWLPoliticalSubsystem::EnsureElectionState(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLElectionState& State = ElectionStateByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.MonthsToElection = ElectionCycleMonths;
		State.Phase = EWLElectionPhase::Governing;
		State.IncumbentApproval = 55;
		State.PollingGovernment = 52;
		State.PollingOpposition = 42;
		State.Legitimacy = 65;
		State.AbstentionRisk = 20;
		State.LastElectionReport = TEXT("Mandato en curso.");
	}
	return State;
}

FWLPatronageState& UWLPoliticalSubsystem::EnsurePatronageState(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLPatronageState& State = PatronageStateByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.PatronagePower = 20;
		State.LastDeal = TEXT("Red clientelar limitada.");
	}
	return State;
}

FWLMediaPublicOpinionState& UWLPoliticalSubsystem::EnsureMediaState(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLMediaPublicOpinionState& State = MediaStateByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.PressFreedom = 60;
		State.MediaControl = 20;
		State.PresidentialApproval = 55;
		State.LastNarrative = TEXT("Opinion publica estable.");
	}
	return State;
}

FWLGovernmentCalibrationState& UWLPoliticalSubsystem::EnsureGovernmentCalibration(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	FWLGovernmentCalibrationState& State = GovernmentCalibrationByNation.FindOrAdd(Iso);
	if (State.NationIso.IsEmpty())
	{
		State.NationIso = Iso;
		State.LastReport = TEXT("Sin meses observados.");
	}
	return State;
}

FWLCharacterPoliticalProfile& UWLPoliticalSubsystem::EnsureCharacterPoliticalProfile(const FWLCharacter& Character)
{
	const FString CharacterId = NormalizeCharacterId(Character.Id);
	FWLCharacterPoliticalProfile& Profile = CharacterPoliticalProfilesById.FindOrAdd(CharacterId);
	if (Profile.CharacterId.IsEmpty())
	{
		Profile.NationIso = NormalizeIso(Character.CountryIso);
		Profile.CharacterId = CharacterId;
		Profile.FactionId = Character.PreferredOffice == EWLMinisterOffice::Defense || Character.Role == EWLCharacterRole::General
			? TEXT("security")
			: Character.PreferredOffice == EWLMinisterOffice::Economy
				? TEXT("technocrats")
				: Character.PreferredOffice == EWLMinisterOffice::Interior
					? TEXT("regional_machines")
					: Character.Role == EWLCharacterRole::Opposition
						? TEXT("opposition")
						: TEXT("presidential_circle");
		Profile.Biography = FString::Printf(TEXT("%s surge de la faccion %s y opera en la politica de %s."),
			*Character.Name, *Profile.FactionId, *Profile.NationIso);
		Profile.PresidentialAmbition = FMath::Clamp(Character.Ambition + Character.Popularity / 4, 0, 100);
		Profile.PersonalCorruption = Character.Traits.Contains(TEXT("corrupto")) || Character.Traits.Contains(TEXT("clientelista")) ? 55 : 20;
		Profile.ScandalHeat = Profile.PersonalCorruption / 4;
		Profile.SuccessionScore = FMath::Clamp(Character.Popularity / 2 + Character.Renown / 3 + Character.Ambition / 3, 0, 100);
		Profile.AgendaTags = Character.Traits;
		if (Profile.AgendaTags.IsEmpty())
		{
			Profile.AgendaTags.Add(Profile.FactionId);
		}
		Profile.LastProfileEvent = TEXT("Perfil politico inicial.");
	}
	return Profile;
}

void UWLPoliticalSubsystem::SeedPoliticalPartiesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		return;
	}
	const FString Prefix = Iso.ToLower();
	TSet<FString> ExistingPartyKeys;
	for (const FWLPartyState& Party : PoliticalParties)
	{
		if (NormalizeIso(Party.NationIso) == Iso)
		{
			ExistingPartyKeys.Add(PartyKey(Party.NationIso, Party.PartyId));
		}
	}

	auto SelectLeader = [this, &Iso](EWLPartyRole Role, EWLPoliticalIdeology Ideology)
	{
		if (const UWLCharacterSubsystem* Characters = GetCharacters())
		{
			const TArray<FWLCharacter> Roster = Characters->GetCharactersByNation(Iso);
			auto FindByPredicate = [&Roster](auto Predicate)
			{
				for (const FWLCharacter& Candidate : Roster)
				{
					if (Candidate.bActive && Predicate(Candidate))
					{
						return UWLPoliticalSubsystem::NormalizeCharacterId(Candidate.Id);
					}
				}
				return FString();
			};

			if (Role == EWLPartyRole::Ruling)
			{
				const FString LeaderId = FindByPredicate([](const FWLCharacter& Candidate)
				{
					return Candidate.Role == EWLCharacterRole::ForeignLeader;
				});
				if (!LeaderId.IsEmpty())
				{
					return LeaderId;
				}
			}
			if (Role == EWLPartyRole::HardOpposition || Role == EWLPartyRole::SoftOpposition)
			{
				const FString OppositionId = FindByPredicate([](const FWLCharacter& Candidate)
				{
					return Candidate.Role == EWLCharacterRole::Opposition;
				});
				if (!OppositionId.IsEmpty())
				{
					return OppositionId;
				}
			}

			const FString MinisterId = FindByPredicate([Ideology](const FWLCharacter& Candidate)
			{
				if (Candidate.Role != EWLCharacterRole::Minister)
				{
					return false;
				}
				if (Ideology == EWLPoliticalIdeology::Technocratic)
				{
					return Candidate.PreferredOffice == EWLMinisterOffice::Economy
						|| Candidate.PreferredOffice == EWLMinisterOffice::Foreign;
				}
				if (Ideology == EWLPoliticalIdeology::Regionalist)
				{
					return Candidate.PreferredOffice == EWLMinisterOffice::Interior;
				}
				return true;
			});
			if (!MinisterId.IsEmpty())
			{
				return MinisterId;
			}
		}
		return FString();
	};

	auto AddParty = [this, &Iso, &ExistingPartyKeys, &SelectLeader](const FString& Suffix, const FString& Name, EWLPoliticalIdeology Ideology, EWLPartyRole Role,
		int32 Seats, int32 Discipline, int32 Loyalty, bool bCoalition)
	{
		FWLPartyState Party;
		Party.NationIso = Iso;
		Party.PartyId = Iso.ToLower() + TEXT("_") + Suffix;
		if (ExistingPartyKeys.Contains(PartyKey(Party.NationIso, Party.PartyId)))
		{
			return;
		}
		Party.Name = Name;
		Party.Ideology = Ideology;
		Party.Role = Role;
		Party.Seats = Seats;
		Party.Discipline = Discipline;
		Party.LoyaltyToGovernment = Loyalty;
		Party.Corruption = Role == EWLPartyRole::Ruling ? 28 : 18;
		Party.bInCoalition = bCoalition;
		Party.LeaderCharacterId = SelectLeader(Role, Ideology);
		Party.LastIncident = TEXT("Bancada constituida.");
		PoliticalParties.Add(Party);
	};

	AddParty(TEXT("gov"), TEXT("Partido de Gobierno"), EWLPoliticalIdeology::Liberal, EWLPartyRole::Ruling, 36, 72, 78, true);
	AddParty(TEXT("ally_reg"), TEXT("Bloque Regional"), EWLPoliticalIdeology::Regionalist, EWLPartyRole::Ally, 16, 58, 60, true);
	AddParty(TEXT("ally_market"), TEXT("Alianza de Centro"), EWLPoliticalIdeology::Technocratic, EWLPartyRole::Ally, 12, 62, 55, true);
	AddParty(TEXT("soft_opp"), TEXT("Oposicion Institucional"), EWLPoliticalIdeology::SocialDemocrat, EWLPartyRole::SoftOpposition, 20, 64, 28, false);
	AddParty(TEXT("hard_opp"), TEXT("Frente Duro"), EWLPoliticalIdeology::Nationalist, EWLPartyRole::HardOpposition, 16, 70, 8, false);

	for (FWLPartyState& Party : PoliticalParties)
	{
		if (Party.NationIso == Iso && Party.LeaderCharacterId.IsEmpty())
		{
			Party.LeaderCharacterId = SelectLeader(Party.Role, Party.Ideology);
		}
	}
}

void UWLPoliticalSubsystem::SeedRegionsForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry || !ValidateNation(Iso))
	{
		return;
	}
	TSet<FString> ExistingRegionIds;
	for (const FWLRegionGovernorState& Region : RegionGovernors)
	{
		if (Region.NationIso == Iso)
		{
			ExistingRegionIds.Add(Region.RegionId);
		}
	}
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (NormalizeIso(Province.CountryIso) != Iso)
		{
			continue;
		}
		const FString RegionName = Province.Region.TrimStartAndEnd().IsEmpty() ? Province.Name : Province.Region;
		const FString RegionId = Province.Id;
		if (ExistingRegionIds.Contains(RegionId))
		{
			continue;
		}
		FWLRegionGovernorState Region;
		Region.NationIso = Iso;
		Region.RegionId = RegionId;
		Region.RegionName = RegionName;
		Region.GovernorName = FString::Printf(TEXT("Gob. %s"), *RegionName);
		const uint32 Hash = GetTypeHash(Iso + RegionId);
		Region.Alignment = Hash % 5 == 0 ? EWLPartyRole::SoftOpposition : EWLPartyRole::Ally;
		Region.Obedience = 50 + static_cast<int32>(Hash % 18);
		Region.Autonomy = 25 + static_cast<int32>(Hash % 20);
		Region.CenterControl = 55;
		Region.ProtestRisk = 15 + static_cast<int32>(Hash % 12);
		Region.LastReport = TEXT("Gobernacion operativa.");
		RegionGovernors.Add(Region);
	}

	if (!RegionGovernors.ContainsByPredicate([&Iso](const FWLRegionGovernorState& Region)
	{
		return Region.NationIso == Iso;
	}))
	{
		FWLRegionGovernorState Region;
		Region.NationIso = Iso;
		Region.RegionId = Iso + TEXT("-CAPITAL");
		Region.RegionName = TEXT("Capital nacional");
		Region.GovernorName = TEXT("Gob. Capital");
		Region.Alignment = EWLPartyRole::Ally;
		Region.LastReport = TEXT("Gobernacion sintetica para pais sin provincias detalladas.");
		RegionGovernors.Add(Region);
	}
}

void UWLPoliticalSubsystem::SeedCharacterProfilesForNation(const FString& NationIso)
{
	const FString Iso = NormalizeIso(NationIso);
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		const TArray<FWLCharacter> Roster = Characters->GetCharactersByNation(Iso);
		FString HeadOfStateId;
		for (const FWLCharacter& Character : Roster)
		{
			if (Character.bActive && Character.Role == EWLCharacterRole::ForeignLeader)
			{
				HeadOfStateId = NormalizeCharacterId(Character.Id);
				break;
			}
		}
		for (const FWLCharacter& Character : Roster)
		{
			EnsureCharacterPoliticalProfile(Character);
		}
		for (int32 Index = 0; Index < Roster.Num(); ++Index)
		{
			FWLCharacterPoliticalProfile& Profile = EnsureCharacterPoliticalProfile(Roster[Index]);
			if (Profile.PatronCharacterId.IsEmpty()
				&& !HeadOfStateId.IsEmpty()
				&& Profile.CharacterId != HeadOfStateId
				&& Roster[Index].Role != EWLCharacterRole::Opposition)
			{
				Profile.PatronCharacterId = HeadOfStateId;
				Profile.LastProfileEvent = TEXT("Alineado con el circulo presidencial.");
			}
			if (Roster.Num() > 1 && Profile.RivalCharacterIds.IsEmpty())
			{
				const FWLCharacter& Rival = Roster[(Index + 1) % Roster.Num()];
				if (Rival.Id != Roster[Index].Id)
				{
					Profile.RivalCharacterIds.Add(NormalizeCharacterId(Rival.Id));
				}
			}
			if (Roster.Num() > 2 && Profile.AllyCharacterIds.IsEmpty())
			{
				const FWLCharacter& Ally = Roster[(Index + 2) % Roster.Num()];
				if (Ally.Id != Roster[Index].Id)
				{
					Profile.AllyCharacterIds.Add(NormalizeCharacterId(Ally.Id));
				}
			}
		}
	}
}

int32 UWLPoliticalSubsystem::GetAveragePublicOrder(const FString& NationIso) const
{
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		return 70;
	}

	const FString Iso = NormalizeIso(NationIso);
	int32 Sum = 0;
	int32 Count = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (Tick->GetProvinceControllerIso(Province.Id) != Iso)
		{
			continue;
		}
		FWLProvinceRuntimeState State;
		if (Tick->GetProvinceState(Province.Id, State))
		{
			Sum += ClampPercent(State.PublicOrder);
			++Count;
		}
	}
	return Count > 0 ? Sum / Count : 70;
}

TArray<FString> UWLPoliticalSubsystem::GetLeaderAgendaTraits(const FString& NationIso) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters)
	{
		return TArray<FString>();
	}
	for (const FWLCharacter& Character : Characters->GetCharactersByRole(NationIso, EWLCharacterRole::ForeignLeader))
	{
		if (Character.CountryIso == NormalizeIso(NationIso) && Character.bActive)
		{
			return Character.Traits;
		}
	}
	return TArray<FString>();
}

int32 UWLPoliticalSubsystem::GetLeaderAgendaPressure(const FString& NationIso) const
{
	int32 Pressure = 0;
	for (const FString& Trait : GetLeaderAgendaTraits(NationIso))
	{
		const FString T = Trait.ToLower();
		if (T == TEXT("polarizante"))
		{
			Pressure += 8;
		}
		else if (T == TEXT("autoritarismo"))
		{
			Pressure += 6;
		}
		else if (T == TEXT("reformista"))
		{
			Pressure += 3;
		}
		else if (T == TEXT("resistente"))
		{
			Pressure -= 4;
		}
	}
	return FMath::Clamp(Pressure, -10, 15);
}

FWLGovernmentAgendaState UWLPoliticalSubsystem::GetGovernmentAgenda(const FString& NationIso) const
{
	if (const FWLGovernmentAgendaState* Found = GovernmentAgendaByNation.Find(NormalizeIso(NationIso)))
	{
		return *Found;
	}
	FWLGovernmentAgendaState State;
	State.NationIso = NormalizeIso(NationIso);
	State.Priorities = DefaultGovernmentAgenda();
	return State;
}

int32 UWLPoliticalSubsystem::GetCurrentPoliticalMonthKey() const
{
	if (const UWLStrategicTickSubsystem* Tick = GetTick())
	{
		return Tick->GetCurrentYear() * 12 + Tick->GetCurrentMonth();
	}
	return 0;
}

int32 UWLPoliticalSubsystem::GetPoliticalActionUsedThisMonth(const FString& NationIso) const
{
	const FString Iso = NormalizeIso(NationIso);
	const int32 MonthKey = GetCurrentPoliticalMonthKey();
	int32 Used = 0;
	for (const FWLPoliticalActionRecord& Record : PoliticalActionRecords)
	{
		if (Record.NationIso == Iso && Record.MonthKey == MonthKey)
		{
			Used += FMath::Max(0, Record.ActionPointCost);
		}
	}
	return Used;
}

FString UWLPoliticalSubsystem::PoliticalActionKey(EWLPoliticalActionType ActionType) const
{
	return PoliticalActionTypeToString(ActionType);
}

FString UWLPoliticalSubsystem::PoliticalActionTargetKey(const FWLPoliticalActionRequest& Request) const
{
	switch (Request.ActionType)
	{
	case EWLPoliticalActionType::SetAgenda:
		return TEXT("agenda");
	case EWLPoliticalActionType::StartProgram:
	case EWLPoliticalActionType::EnactReform:
	case EWLPoliticalActionType::ResolveEvent:
	case EWLPoliticalActionType::NegotiatePartySupport:
	case EWLPoliticalActionType::HoldPartyInternalElection:
	case EWLPoliticalActionType::MakeCampaignPromise:
		return Request.PrimaryId.TrimStartAndEnd().ToLower();
	case EWLPoliticalActionType::UsePatronage:
	case EWLPoliticalActionType::RunMediaAction:
		return FString::Printf(TEXT("%d"), Request.NumericValue);
	case EWLPoliticalActionType::RunRegionPolicy:
		return FString::Printf(TEXT("%s|%d"), *Request.PrimaryId.TrimStartAndEnd().ToLower(), Request.NumericValue);
	case EWLPoliticalActionType::RepressOpposition:
		return TEXT("opposition");
	default:
		return Request.PrimaryId.TrimStartAndEnd().ToLower();
	}
}

int32 UWLPoliticalSubsystem::GetPoliticalActionCooldownRemaining(
	const FString& NationIso,
	EWLPoliticalActionType ActionType,
	const FString& TargetKey) const
{
	const FString Iso = NormalizeIso(NationIso);
	const FString ActionKey = PoliticalActionKey(ActionType);
	const FString Target = TargetKey.TrimStartAndEnd().ToLower();
	const int32 MonthKey = GetCurrentPoliticalMonthKey();
	int32 Remaining = 0;
	for (const FWLPoliticalActionRecord& Record : PoliticalActionRecords)
	{
		if (Record.NationIso != Iso
			|| Record.ActionKey != ActionKey
			|| Record.TargetKey != Target
			|| Record.CooldownMonths <= 0)
		{
			continue;
		}
		const int32 CooldownEnd = Record.MonthKey + Record.CooldownMonths;
		if (CooldownEnd > MonthKey)
		{
			Remaining = FMath::Max(Remaining, CooldownEnd - MonthKey);
		}
	}
	return Remaining;
}

FWLPoliticalActionBudget UWLPoliticalSubsystem::GetPoliticalActionBudget(const FString& NationIso) const
{
	FWLPoliticalActionBudget Budget;
	Budget.NationIso = NormalizeIso(NationIso);
	Budget.MonthKey = GetCurrentPoliticalMonthKey();
	if (!ValidateNation(Budget.NationIso))
	{
		Budget.LastReport = TEXT("Nacion invalida para presupuesto politico.");
		return Budget;
	}

	const FWLStateCapacityState Capacity = GetStateCapacity(Budget.NationIso);
	int32 Monthly = MinPoliticalActionPoints;
	if (Capacity.AdministrativeEfficiency >= 60)
	{
		++Monthly;
	}
	if (Capacity.Bureaucracy >= 65)
	{
		++Monthly;
	}
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		const FWLGovernmentStats Stats = Characters->GetGovernmentStats(Budget.NationIso);
		if (Stats.FilledOffices > 0 && Stats.AverageSkill >= 65)
		{
			++Monthly;
		}
	}
	Budget.MonthlyActionPoints = FMath::Clamp(Monthly, MinPoliticalActionPoints, MaxPoliticalActionPoints);
	Budget.UsedActionPoints = FMath::Clamp(GetPoliticalActionUsedThisMonth(Budget.NationIso), 0, Budget.MonthlyActionPoints);
	Budget.RemainingActionPoints = FMath::Max(0, Budget.MonthlyActionPoints - Budget.UsedActionPoints);
	Budget.LastReport = FString::Printf(TEXT("AP politica %d/%d disponibles."),
		Budget.RemainingActionPoints,
		Budget.MonthlyActionPoints);
	return Budget;
}

int32 UWLPoliticalSubsystem::GetEffectiveReformCost(const FString& NationIso, int32 BaseCost) const
{
	const FWLInstitutionalPowerState Institutions = GetInstitutionalPower(NationIso);
	return FMath::Max(0, BaseCost + Institutions.ReformCost / 2);
}
