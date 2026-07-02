// Copyright World Leader project. See ROADMAP.md.

#include "Politics/WLPoliticalSubsystem.h"
#include "Campaign/WLCampaignGameInstance.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "WorldLeader.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr int32 CoupAttemptRiskThreshold = 85;
	constexpr int64 RewardGeneralCost = 3000;
	constexpr int64 RepressOppositionCost = 2000;

	void AddDefaultEventDefinitions(TArray<FWLPoliticalEventDefinition>& OutDefinitions)
	{
		FWLPoliticalEventDefinition CoupTension;
		CoupTension.EventId = TEXT("internal_coup_tension");
		CoupTension.Title = TEXT("Rumores de golpe");
		CoupTension.Body = TEXT("Altos mandos desleales y oposicion organizada presionan al gobierno.");
		CoupTension.Trigger = TEXT("coup_risk");
		CoupTension.MinCoupRisk = 60;
		CoupTension.MinOppositionStrength = 25;
		CoupTension.MaxPublicOrder = 100;

		FWLPoliticalEventOption Negotiate;
		Negotiate.OptionId = TEXT("negotiate");
		Negotiate.Label = TEXT("Negociar con los mandos");
		Negotiate.PoliticalCapitalDelta = -8;
		Negotiate.OppositionDelta = -6;
		Negotiate.PublicOrderDelta = 1;
		CoupTension.Options.Add(Negotiate);

		FWLPoliticalEventOption Crackdown;
		Crackdown.OptionId = TEXT("crackdown");
		Crackdown.Label = TEXT("Ordenar una redada preventiva");
		Crackdown.OppositionDelta = -12;
		Crackdown.PublicOrderDelta = -3;
		CoupTension.Options.Add(Crackdown);
		OutDefinitions.Add(MoveTemp(CoupTension));

		FWLPoliticalEventDefinition Protest;
		Protest.EventId = TEXT("street_protests");
		Protest.Title = TEXT("Protestas nacionales");
		Protest.Body = TEXT("La oposicion capitaliza el deterioro del orden publico.");
		Protest.Trigger = TEXT("opposition");
		Protest.MinCoupRisk = 0;
		Protest.MinOppositionStrength = 35;
		Protest.MaxPublicOrder = 55;

		FWLPoliticalEventOption Concede;
		Concede.OptionId = TEXT("concede");
		Concede.Label = TEXT("Conceder reformas limitadas");
		Concede.PoliticalCapitalDelta = -6;
		Concede.OppositionDelta = -10;
		Concede.PublicOrderDelta = 2;
		Protest.Options.Add(Concede);

		FWLPoliticalEventOption Ignore;
		Ignore.OptionId = TEXT("ignore");
		Ignore.Label = TEXT("Ignorar las marchas");
		Ignore.OppositionDelta = 7;
		Ignore.PublicOrderDelta = -2;
		Protest.Options.Add(Ignore);
		OutDefinitions.Add(MoveTemp(Protest));
	}
}

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
	CampaignOutcome = FWLCampaignOutcomeState();
	NextEventInstanceNumber = 1;
	LoadEventDefinitions();
	SeedInternalPower();
	SeedRelations();
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
		Obj->TryGetNumberField(TEXT("min_coup_risk"), Def.MinCoupRisk);
		Obj->TryGetNumberField(TEXT("min_opposition_strength"), Def.MinOppositionStrength);
		if (!Obj->TryGetNumberField(TEXT("max_public_order"), Def.MaxPublicOrder))
		{
			Def.MaxPublicOrder = 100;
		}

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

void UWLPoliticalSubsystem::UpdateInternalPowerForNation(const FString& NationIso)
{
	FWLInternalPowerState& State = EnsureInternalPower(NationIso);
	State.AveragePublicOrder = GetAveragePublicOrder(NationIso);

	const int32 DisorderPressure = FMath::Max(0, 65 - State.AveragePublicOrder) / 2;
	State.OppositionStrength = ClampPercent(State.OppositionStrength + DisorderPressure - 2);
	State.OppositionPopularity = ClampPercent(State.OppositionPopularity + DisorderPressure - 1);

	int32 GeneralPressure = 0;
	int32 GeneralCount = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		for (const FWLCharacter& General : Characters->GetGenerals(State.NationIso))
		{
			GeneralPressure += FMath::Max(0, General.Ambition - General.Loyalty / 2);
			++GeneralCount;
		}
	}
	const int32 AverageGeneralPressure = GeneralCount > 0 ? GeneralPressure / GeneralCount : 0;
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
		if (Characters)
		{
			Characters->AddMonthlyRenownToGenerals(Nation.Iso, 1);
		}
		UpdateInternalPowerForNation(Nation.Iso);
		QueueTriggeredEventsForNation(Nation.Iso);
		if (!PlayerIso.IsEmpty() && Nation.Iso != PlayerIso)
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
}

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

	// 1) Fisco: en deficit o deuda sube impuestos; con bonanza los relaja hacia el default.
	const int32 Tax = Tick->GetTaxRate(Iso);
	if ((Treasury < 0 || Budget.Net() < 0) && Tax < Rules.TaxRateMaxPercent)
	{
		Tick->SetTaxRate(Iso, Tax + 5);
		UE_LOG(LogWorldLeader, Log, TEXT("IA %s: sube impuestos a %d%%."), *Iso, Tick->GetTaxRate(Iso));
	}
	else if (Treasury > Budget.TotalIncome() * 6 && Tax > Rules.TaxRateDefaultPercent)
	{
		Tick->SetTaxRate(Iso, Tax - 5);
	}

	// 2) Arancel: deficit comercial -> protege; superavit claro -> abre.
	const int32 Tariff = Tick->GetTariffRate(Iso);
	FString TariffMessage;
	if (Budget.ImportCost > Budget.ExportIncome && Tariff < 20)
	{
		SetNationTariffRate(Iso, Tariff + 5, TariffMessage);
	}
	else if (Budget.ExportIncome > Budget.ImportCost * 2 && Tariff > 0)
	{
		SetNationTariffRate(Iso, Tariff - 5, TariffMessage);
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
			if (OwnStrength * 2 < OtherStrength)
			{
				MakePeace(Iso, Other.Iso, Message);
				UE_LOG(LogWorldLeader, Log, TEXT("IA %s: pide la paz con %s."), *Iso, *Other.Iso);
			}
			continue;
		}

		if (Relation.Opinion >= 20 && !Relation.Treaties.Contains(EWLTreatyType::TradeAgreement))
		{
			SignTreaty(Iso, Other.Iso, EWLTreatyType::TradeAgreement, Message);
		}
		if (Relation.Opinion >= 25 && !Relation.Treaties.Contains(EWLTreatyType::NonAggression))
		{
			SignTreaty(Iso, Other.Iso, EWLTreatyType::NonAggression, Message);
		}
		if (Relation.Opinion >= 60 && !Relation.Treaties.Contains(EWLTreatyType::Alliance))
		{
			SignTreaty(Iso, Other.Iso, EWLTreatyType::Alliance, Message);
		}
		if (Relation.Opinion <= -60 && OwnStrength > OtherStrength + OtherStrength / 3)
		{
			DeclareWar(Iso, Other.Iso, Message);
			UE_LOG(LogWorldLeader, Warning, TEXT("IA %s: declara la guerra a %s."), *Iso, *Other.Iso);
		}

		if (WorstIso.IsEmpty() || Relation.Opinion < WorstOpinion)
		{
			WorstIso = Other.Iso;
			WorstOpinion = Relation.Opinion;
		}
	}

	// 4) Intriga contra su peor relacion: primero red, luego alterna financiar golpe / propaganda.
	if (!WorstIso.IsEmpty() && WorstOpinion < -20 && Characters)
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
			if (Network.NetworkStrength < 30)
			{
				BuildSpyNetwork(Iso, WorstIso, SpyId, Message);
			}
			else if (Network.Exposure < 55)
			{
				RunSpyOperation(Iso, WorstIso, SpyId,
					Month % 2 == 0 ? EWLSpyOperationType::FundCoup : EWLSpyOperationType::Propaganda, Message);
				UE_LOG(LogWorldLeader, Log, TEXT("IA %s vs %s: %s"), *Iso, *WorstIso, *Message);
			}
		}
	}

	// 5) Reclutamiento: con caja y por detras militarmente de su peor relacion, encola tropa en su HQ.
	if (!WorstIso.IsEmpty() && Treasury > 40000
		&& OwnStrength < Tick->GetNationMilitaryStrength(WorstIso))
	{
		FString Message;
		if (Tick->QueueRecruit(Iso + TEXT("-AI-HQ"), Iso, TEXT("infantry"), Message))
		{
			UE_LOG(LogWorldLeader, Log, TEXT("IA %s: recluta infanteria (%s)."), *Iso, *Message);
		}
	}
}

bool UWLPoliticalSubsystem::AttemptCoup(const FString& NationIso, FString& OutReport)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutReport = FString::Printf(TEXT("Nacion no disponible: %s"), *NationIso);
		return false;
	}

	UpdateInternalPowerForNation(Iso);
	FWLInternalPowerState& State = EnsureInternalPower(Iso);

	int32 GeneralLoyalty = 60;
	int32 GeneralSkill = 50;
	int32 Count = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		int32 LoyaltySum = 0;
		int32 SkillSum = 0;
		for (const FWLCharacter& General : Characters->GetGenerals(Iso))
		{
			LoyaltySum += General.Loyalty;
			SkillSum += General.Skill;
			++Count;
		}
		if (Count > 0)
		{
			GeneralLoyalty = LoyaltySum / Count;
			GeneralSkill = SkillSum / Count;
		}
	}

	const int32 CoupPressure = State.CoupRisk + State.OppositionStrength / 2 + State.ExternalCoupFunding / 3;
	const int32 LoyalDefense = GeneralLoyalty / 2 + GeneralSkill / 4 + State.AveragePublicOrder / 3;
	State.LastCoupRoll = CoupPressure - LoyalDefense;
	State.bLastCoupSucceeded = State.LastCoupRoll > 0;

	if (State.bLastCoupSucceeded)
	{
		const FString PlayerIso = GetPlayerNationIso();
		if (!PlayerIso.IsEmpty() && Iso != PlayerIso)
		{
			// Golpe en una nacion IA: cambio de regimen, NO fin de la partida del jugador.
			// La junta purga a la oposicion, corta la financiacion externa y desestabiliza el pais.
			State.OppositionStrength = ClampPercent(State.OppositionStrength - 30);
			State.ExternalCoupFunding = 0;
			if (UWLStrategicTickSubsystem* Tick = GetTick())
			{
				Tick->AdjustNationPublicOrder(Iso, -12);
				FString TreasuryMessage;
				Tick->AdjustTreasury(Iso, -Tick->GetTreasury(Iso) / 5, TreasuryMessage);   // fuga de capitales
			}
			State.LastCoupReport = FString::Printf(
				TEXT("Golpe exitoso en %s: una junta toma el poder (presion %d vs defensa %d)."),
				*Iso, CoupPressure, LoyalDefense);
			UpdateInternalPowerForNation(Iso);
			OutReport = State.LastCoupReport;
			return true;
		}

		CampaignOutcome.bGameOver = true;
		CampaignOutcome.OutcomeType = TEXT("Coup");
		CampaignOutcome.LosingNationIso = Iso;
		CampaignOutcome.Reason = FString::Printf(TEXT("Golpe exitoso en %s (presion %d vs defensa %d)."),
			*Iso, CoupPressure, LoyalDefense);
		State.LastCoupReport = CampaignOutcome.Reason;
		OutReport = State.LastCoupReport;
		return true;
	}

	State.ExternalCoupFunding = ClampPercent(State.ExternalCoupFunding - 15);
	State.OppositionStrength = ClampPercent(State.OppositionStrength - 8);
	State.LastCoupReport = FString::Printf(TEXT("Golpe fallido en %s (presion %d vs defensa %d)."),
		*Iso, CoupPressure, LoyalDefense);
	OutReport = State.LastCoupReport;
	return false;
}

bool UWLPoliticalSubsystem::RewardGeneral(
	const FString& NationIso,
	const FString& CharacterId,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Tick || !Characters || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Subsistemas de politica no disponibles.");
		return false;
	}
	FWLCharacter Character;
	if (!Characters->GetCharacter(CharacterId, Character)
		|| Character.CountryIso != Iso
		|| Character.Role != EWLCharacterRole::General)
	{
		OutMessage = FString::Printf(TEXT("General invalido para %s: %s"), *Iso, *CharacterId);
		return false;
	}
	if (Tick->GetTreasury(Iso) < RewardGeneralCost)
	{
		OutMessage = TEXT("Tesoro insuficiente para recompensar al general.");
		return false;
	}
	FString TreasuryMessage;
	Tick->AdjustTreasury(Iso, -RewardGeneralCost, TreasuryMessage);
	FString LoyaltyMessage;
	Characters->AdjustCharacterLoyalty(Character.Id, 10, LoyaltyMessage);
	FString RenownMessage;
	Characters->AddRenownToGeneral(Character.Id, 5, RenownMessage);
	OutMessage = FString::Printf(TEXT("%s %s %s"), *TreasuryMessage, *LoyaltyMessage, *RenownMessage);
	return true;
}

bool UWLPoliticalSubsystem::PurgeCharacter(
	const FString& NationIso,
	const FString& CharacterId,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLCharacterSubsystem* Characters = GetCharacters();
	if (!Characters || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Subsistema de personajes no disponible.");
		return false;
	}
	FWLCharacter Character;
	if (!Characters->GetCharacter(CharacterId, Character) || Character.CountryIso != Iso)
	{
		OutMessage = FString::Printf(TEXT("Personaje invalido para %s: %s"), *Iso, *CharacterId);
		return false;
	}
	if (!Characters->RetireCharacter(Character.Id, OutMessage))
	{
		return false;
	}
	FWLInternalPowerState& State = EnsureInternalPower(Iso);
	State.OppositionStrength = ClampPercent(State.OppositionStrength + 12);
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->AdjustNationPublicOrder(Iso, -2);
	}
	OutMessage += TEXT(" La purga aumenta tension politica.");
	return true;
}

bool UWLPoliticalSubsystem::RepressOpposition(const FString& NationIso, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Subsistema economico/provincial no disponible.");
		return false;
	}
	if (Tick->GetTreasury(Iso) < RepressOppositionCost)
	{
		OutMessage = TEXT("Tesoro insuficiente para reprimir oposicion.");
		return false;
	}
	FWLInternalPowerState& State = EnsureInternalPower(Iso);
	const int32 PreviousOpposition = State.OppositionStrength;
	State.OppositionStrength = ClampPercent(State.OppositionStrength - 18);
	State.OppositionPopularity = ClampPercent(State.OppositionPopularity - 10);
	FString TreasuryMessage;
	Tick->AdjustTreasury(Iso, -RepressOppositionCost, TreasuryMessage);
	Tick->AdjustNationPublicOrder(Iso, -4);
	OutMessage = FString::Printf(TEXT("Oposicion %d -> %d. %s"),
		PreviousOpposition, State.OppositionStrength, *TreasuryMessage);
	return true;
}

TArray<FWLDiplomaticRelationState> UWLPoliticalSubsystem::GetRelationsForNation(const FString& NationIso) const
{
	TArray<FWLDiplomaticRelationState> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
	{
		if (Pair.Value.NationA == Iso || Pair.Value.NationB == Iso)
		{
			Out.Add(Pair.Value);
		}
	}
	Out.Sort([](const FWLDiplomaticRelationState& A, const FWLDiplomaticRelationState& B)
	{
		return (A.NationA + A.NationB) < (B.NationA + B.NationB);
	});
	return Out;
}

bool UWLPoliticalSubsystem::GetRelation(
	const FString& NationA,
	const FString& NationB,
	FWLDiplomaticRelationState& OutRelation) const
{
	if (const FWLDiplomaticRelationState* Found = RelationsByPair.Find(RelationKey(NationA, NationB)))
	{
		OutRelation = *Found;
		return true;
	}
	return false;
}

bool UWLPoliticalSubsystem::SetRelationOpinion(
	const FString& NationA,
	const FString& NationB,
	int32 Opinion,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Relacion diplomatica invalida.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	Relation.Opinion = FMath::Clamp(Opinion, -100, 100);
	if (Relation.Status != EWLDiplomaticStatus::War)
	{
		Relation.Status = Relation.Opinion < -35 ? EWLDiplomaticStatus::Tension : EWLDiplomaticStatus::Peace;
	}
	OutMessage = FString::Printf(TEXT("Opinion %s-%s = %d."), *Relation.NationA, *Relation.NationB, Relation.Opinion);
	return true;
}

bool UWLPoliticalSubsystem::AdjustRelationOpinion(
	const FString& NationA,
	const FString& NationB,
	int32 Delta,
	FString& OutMessage)
{
	FWLDiplomaticRelationState Relation;
	if (!GetRelation(NationA, NationB, Relation))
	{
		EnsureRelation(NationA, NationB);
	}
	return SetRelationOpinion(NationA, NationB, GetRelation(NationA, NationB, Relation) ? Relation.Opinion + Delta : Delta, OutMessage);
}

bool UWLPoliticalSubsystem::DeclareWar(
	const FString& AggressorIso,
	const FString& TargetIso,
	FString& OutMessage)
{
	if (!ValidateNation(AggressorIso) || !ValidateNation(TargetIso) || NormalizeIso(AggressorIso) == NormalizeIso(TargetIso))
	{
		OutMessage = TEXT("Declaracion de guerra invalida.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(AggressorIso, TargetIso);
	Relation.Status = EWLDiplomaticStatus::War;
	Relation.Opinion = -100;
	Relation.CasusBelli = FString::Printf(TEXT("%s declaro la guerra a %s."), *NormalizeIso(AggressorIso), *NormalizeIso(TargetIso));
	Relation.Treaties.Remove(EWLTreatyType::TradeAgreement);
	Relation.Treaties.Remove(EWLTreatyType::NonAggression);
	Relation.Treaties.Remove(EWLTreatyType::Alliance);
	OutMessage = Relation.CasusBelli;
	return true;
}

bool UWLPoliticalSubsystem::MakePeace(
	const FString& NationA,
	const FString& NationB,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Paz invalida.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	if (Relation.Status != EWLDiplomaticStatus::War)
	{
		OutMessage = FString::Printf(TEXT("%s y %s no estan en guerra."), *Relation.NationA, *Relation.NationB);
		return false;
	}
	// La posguerra deja tension, no amistad: opinion arranca en -30 y las rutas reabren a medias.
	Relation.Status = EWLDiplomaticStatus::Tension;
	Relation.Opinion = FMath::Max(Relation.Opinion, -30);
	Relation.CasusBelli.Reset();
	OutMessage = FString::Printf(TEXT("Paz firmada entre %s y %s. La tension persiste."),
		*Relation.NationA, *Relation.NationB);
	return true;
}

bool UWLPoliticalSubsystem::SignTreaty(
	const FString& NationA,
	const FString& NationB,
	EWLTreatyType Treaty,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Tratado invalido.");
		return false;
	}
	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	if (Relation.Status == EWLDiplomaticStatus::War && Treaty != EWLTreatyType::Embargo)
	{
		OutMessage = TEXT("No se puede firmar tratado durante una guerra.");
		return false;
	}
	const int32 RequiredOpinion =
		Treaty == EWLTreatyType::Alliance ? 55 :
		Treaty == EWLTreatyType::NonAggression ? 20 :
		Treaty == EWLTreatyType::TradeAgreement ? 0 : -100;
	if (Relation.Opinion < RequiredOpinion)
	{
		OutMessage = FString::Printf(TEXT("Opinion insuficiente para %s (%d/%d)."),
			*TreatyToString(Treaty), Relation.Opinion, RequiredOpinion);
		return false;
	}
	if (!Relation.Treaties.Contains(Treaty))
	{
		Relation.Treaties.Add(Treaty);
	}
	if (Treaty == EWLTreatyType::TradeAgreement)
	{
		Relation.Treaties.Remove(EWLTreatyType::Embargo);
	}
	if (Treaty == EWLTreatyType::Embargo)
	{
		Relation.Treaties.Remove(EWLTreatyType::TradeAgreement);
		Relation.Opinion = FMath::Clamp(Relation.Opinion - 20, -100, 100);
		Relation.Status = EWLDiplomaticStatus::Tension;
	}
	OutMessage = FString::Printf(TEXT("%s firmado entre %s y %s."),
		*TreatyToString(Treaty), *Relation.NationA, *Relation.NationB);
	return true;
}

bool UWLPoliticalSubsystem::BreakTreaty(
	const FString& NationA,
	const FString& NationB,
	EWLTreatyType Treaty,
	FString& OutMessage)
{
	FWLDiplomaticRelationState& Relation = EnsureRelation(NationA, NationB);
	const int32 Removed = Relation.Treaties.Remove(Treaty);
	if (Removed <= 0)
	{
		OutMessage = FString::Printf(TEXT("No existe %s entre %s y %s."),
			*TreatyToString(Treaty), *Relation.NationA, *Relation.NationB);
		return false;
	}
	Relation.Opinion = FMath::Clamp(Relation.Opinion - 10, -100, 100);
	OutMessage = FString::Printf(TEXT("%s roto entre %s y %s."),
		*TreatyToString(Treaty), *Relation.NationA, *Relation.NationB);
	return true;
}

bool UWLPoliticalSubsystem::SetNationTariffRate(
	const FString& NationIso,
	int32 RatePercent,
	FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	if (!ValidateNation(Iso))
	{
		OutMessage = TEXT("Nacion invalida para aranceles.");
		return false;
	}
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick)
	{
		OutMessage = TEXT("Economia no disponible.");
		return false;
	}

	const int32 PreviousRate = Tick->GetTariffRate(Iso);
	const int32 EffectiveRate = Tick->SetTariffRate(Iso, RatePercent);
	const int32 Delta = EffectiveRate - PreviousRate;
	int32 RelationsTouched = 0;
	if (Delta != 0)
	{
		const FWLBalanceRules Rules = Tick->GetBalanceRules();
		const int32 OpinionDelta = Delta > 0
			? -FMath::CeilToInt(static_cast<double>(Delta) * Rules.TariffRelationPenaltyPerPoint)
			: FMath::CeilToInt(static_cast<double>(-Delta) * Rules.TariffRelationPenaltyPerPoint * 0.5);
		if (OpinionDelta != 0)
		{
			for (TPair<FString, FWLDiplomaticRelationState>& Pair : RelationsByPair)
			{
				FWLDiplomaticRelationState& Relation = Pair.Value;
				if (Relation.NationA != Iso && Relation.NationB != Iso)
				{
					continue;
				}
				Relation.Opinion = FMath::Clamp(Relation.Opinion + OpinionDelta, -100, 100);
				if (Relation.Status != EWLDiplomaticStatus::War)
				{
					Relation.Status = Relation.Opinion < -35 ? EWLDiplomaticStatus::Tension : EWLDiplomaticStatus::Peace;
				}
				++RelationsTouched;
			}
		}
	}

	OutMessage = FString::Printf(TEXT("Arancel %s: %d%% -> %d%% (%d relaciones afectadas)."),
		*Iso, PreviousRate, EffectiveRate, RelationsTouched);
	return true;
}

bool UWLPoliticalSubsystem::ValidateSpy(
	const FString& OwnerIso,
	const FString& SpyCharacterId,
	int32& OutSkill,
	FString& OutMessage) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	FWLCharacter Spy;
	if (!Characters || !Characters->GetCharacter(NormalizeCharacterId(SpyCharacterId), Spy))
	{
		OutMessage = FString::Printf(TEXT("Espia desconocido: %s"), *SpyCharacterId);
		return false;
	}
	if (!Spy.bActive || Spy.CountryIso != NormalizeIso(OwnerIso) || Spy.Role != EWLCharacterRole::Spy)
	{
		OutMessage = FString::Printf(TEXT("%s no es un espia activo de %s."), *Spy.Name, *OwnerIso);
		return false;
	}
	OutSkill = Spy.Skill;
	return true;
}

FWLIntelligenceNetworkState UWLPoliticalSubsystem::GetIntelligenceNetwork(
	const FString& OwnerIso,
	const FString& TargetIso) const
{
	if (const FWLIntelligenceNetworkState* Found = IntelligenceByPair.Find(NetworkKey(OwnerIso, TargetIso)))
	{
		return *Found;
	}
	FWLIntelligenceNetworkState State;
	State.OwnerIso = NormalizeIso(OwnerIso);
	State.TargetIso = NormalizeIso(TargetIso);
	return State;
}

bool UWLPoliticalSubsystem::BuildSpyNetwork(
	const FString& OwnerIso,
	const FString& TargetIso,
	const FString& SpyCharacterId,
	FString& OutMessage)
{
	const FString Owner = NormalizeIso(OwnerIso);
	const FString Target = NormalizeIso(TargetIso);
	if (!ValidateNation(Owner) || !ValidateNation(Target) || Owner == Target)
	{
		OutMessage = TEXT("Red de inteligencia invalida.");
		return false;
	}
	int32 SpySkill = 0;
	if (!ValidateSpy(Owner, SpyCharacterId, SpySkill, OutMessage))
	{
		return false;
	}
	SpySkill += GetIntelligenceMinisterSkillBonus(Owner);   // Fase 3: el ministro de Inteligencia potencia a los espias
	FWLIntelligenceNetworkState& Network = EnsureNetwork(Owner, Target);
	const int32 Previous = Network.NetworkStrength;
	Network.NetworkStrength = ClampPercent(Network.NetworkStrength + 10 + SpySkill / 8);
	Network.Exposure = ClampPercent(Network.Exposure + 2);
	Network.LastOperationReport = FString::Printf(TEXT("Red %s>%s %d -> %d."),
		*Owner, *Target, Previous, Network.NetworkStrength);
	OutMessage = Network.LastOperationReport;
	return true;
}

bool UWLPoliticalSubsystem::RunSpyOperation(
	const FString& OwnerIso,
	const FString& TargetIso,
	const FString& SpyCharacterId,
	EWLSpyOperationType Operation,
	FString& OutMessage)
{
	const FString Owner = NormalizeIso(OwnerIso);
	const FString Target = NormalizeIso(TargetIso);
	if (!ValidateNation(Owner) || !ValidateNation(Target) || Owner == Target)
	{
		OutMessage = TEXT("Operacion de intriga invalida.");
		return false;
	}
	int32 SpySkill = 0;
	if (!ValidateSpy(Owner, SpyCharacterId, SpySkill, OutMessage))
	{
		return false;
	}
	SpySkill += GetIntelligenceMinisterSkillBonus(Owner);   // Fase 3: el ministro de Inteligencia potencia a los espias
	FWLIntelligenceNetworkState& Network = EnsureNetwork(Owner, Target);
	if (Network.NetworkStrength < 15 && Operation != EWLSpyOperationType::CounterIntelligence)
	{
		OutMessage = TEXT("Red de inteligencia insuficiente.");
		return false;
	}

	FWLInternalPowerState& TargetPower = EnsureInternalPower(Target);
	const int32 SuccessScore = SpySkill + Network.NetworkStrength - Network.Exposure / 2;
	// Fase 3 auditoria: el exito ESCALA los efectos (antes "exito/limitado" era solo texto).
	const double Scale = FMath::Clamp(static_cast<double>(SuccessScore) / 90.0, 0.35, 1.25);
	const auto Scaled = [Scale](int32 Base)
	{
		return FMath::Max(1, FMath::RoundToInt(static_cast<double>(Base) * Scale));
	};
	UWLStrategicTickSubsystem* Tick = GetTick();
	FString Effect;
	switch (Operation)
	{
	case EWLSpyOperationType::SabotageEconomy:
		if (Tick) { Tick->AdjustNationPublicOrder(Target, -Scaled(3)); }
		TargetPower.OppositionStrength = ClampPercent(TargetPower.OppositionStrength + Scaled(6));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 8);
		Effect = FString::Printf(TEXT("orden publico rival -%d, oposicion +%d"), Scaled(3), Scaled(6));
		break;
	case EWLSpyOperationType::SabotageArmy:
		TargetPower.CoupRisk = ClampPercent(TargetPower.CoupRisk + Scaled(6));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 6);
		Effect = FString::Printf(TEXT("riesgo militar rival +%d"), Scaled(6));
		break;
	case EWLSpyOperationType::FundCoup:
		TargetPower.ExternalCoupFunding = ClampPercent(TargetPower.ExternalCoupFunding + Scaled(15));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 10);
		Effect = FString::Printf(TEXT("financiacion externa de golpe +%d"), Scaled(15));
		break;
	case EWLSpyOperationType::Propaganda:
		if (Tick) { Tick->AdjustNationPublicOrder(Target, -Scaled(2)); }
		TargetPower.OppositionStrength = ClampPercent(TargetPower.OppositionStrength + Scaled(8));
		TargetPower.OppositionPopularity = ClampPercent(TargetPower.OppositionPopularity + Scaled(5));
		Network.NetworkStrength = ClampPercent(Network.NetworkStrength - 5);
		Effect = FString::Printf(TEXT("oposicion rival +%d, popularidad opositora +%d"), Scaled(8), Scaled(5));
		break;
	case EWLSpyOperationType::CounterIntelligence:
	{
		FWLIntelligenceNetworkState& Reverse = EnsureNetwork(Target, Owner);
		Reverse.NetworkStrength = ClampPercent(Reverse.NetworkStrength - Scaled(12 + SpySkill / 10));
		Reverse.Exposure = ClampPercent(Reverse.Exposure + Scaled(8));
		Effect = TEXT("red enemiga reducida");
		break;
	}
	default:
		break;
	}

	Network.Exposure = ClampPercent(Network.Exposure + 8);
	const bool bDetected = Network.Exposure > SuccessScore / 2;
	if (bDetected)
	{
		FString RelationMessage;
		AdjustRelationOpinion(Owner, Target, -18, RelationMessage);
		Effect += TEXT("; operacion detectada");
	}
	Network.LastOperationReport = FString::Printf(TEXT("%s: %s (%s)."),
		*OperationToString(Operation), SuccessScore >= 45 ? TEXT("exito") : TEXT("resultado limitado"), *Effect);
	OutMessage = Network.LastOperationReport;
	return true;
}

int32 UWLPoliticalSubsystem::GetIntelligenceMinisterSkillBonus(const FString& OwnerIso) const
{
	const UWLCharacterSubsystem* Characters = GetCharacters();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Characters || !Tick)
	{
		return 0;
	}
	return FMath::RoundToInt(
		Characters->GetMinisterEffectFactor(NormalizeIso(OwnerIso), EWLMinisterOffice::Intelligence)
		* Tick->GetBalanceRules().IntelligenceMinisterSpyBonus);
}

bool UWLPoliticalSubsystem::HasQueuedUnresolvedEvent(const FString& NationIso, const FString& EventId) const
{
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (!Event.bResolved && Event.NationIso == Iso && Event.EventId == EventId)
		{
			return true;
		}
	}
	return false;
}

void UWLPoliticalSubsystem::QueueTriggeredEventsForNation(const FString& NationIso)
{
	const FWLInternalPowerState State = GetInternalPower(NationIso);
	for (const FWLPoliticalEventDefinition& Def : EventDefinitions)
	{
		if (HasQueuedUnresolvedEvent(State.NationIso, Def.EventId))
		{
			continue;
		}
		if (State.CoupRisk < Def.MinCoupRisk
			|| State.OppositionStrength < Def.MinOppositionStrength
			|| State.AveragePublicOrder > Def.MaxPublicOrder)
		{
			continue;
		}
		FWLPoliticalEventInstance Event;
		Event.InstanceId = FString::Printf(TEXT("EV-%04d"), NextEventInstanceNumber++);
		Event.EventId = Def.EventId;
		Event.NationIso = State.NationIso;
		Event.Title = Def.Title;
		Event.Body = Def.Body;
		Event.Options = Def.Options;
		EventQueue.Add(MoveTemp(Event));
	}
}

TArray<FWLPoliticalEventInstance> UWLPoliticalSubsystem::GetQueuedEvents(const FString& NationIso) const
{
	TArray<FWLPoliticalEventInstance> Out;
	const FString Iso = NormalizeIso(NationIso);
	for (const FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (!Event.bResolved && (Iso.IsEmpty() || Event.NationIso == Iso))
		{
			Out.Add(Event);
		}
	}
	Out.Sort([](const FWLPoliticalEventInstance& A, const FWLPoliticalEventInstance& B)
	{
		return A.InstanceId < B.InstanceId;
	});
	return Out;
}

bool UWLPoliticalSubsystem::ResolveEvent(
	const FString& InstanceId,
	const FString& OptionId,
	FString& OutMessage)
{
	FWLPoliticalEventInstance* Event = EventQueue.FindByPredicate(
		[&InstanceId](const FWLPoliticalEventInstance& E)
		{
			return E.InstanceId == InstanceId && !E.bResolved;
		});
	if (!Event)
	{
		OutMessage = FString::Printf(TEXT("Evento no disponible: %s"), *InstanceId);
		return false;
	}
	const FWLPoliticalEventOption* Option = Event->Options.FindByPredicate(
		[&OptionId](const FWLPoliticalEventOption& O)
		{
			return O.OptionId == OptionId;
		});
	if (!Option)
	{
		OutMessage = FString::Printf(TEXT("Opcion no disponible: %s"), *OptionId);
		return false;
	}

	FWLInternalPowerState& State = EnsureInternalPower(Event->NationIso);
	State.OppositionStrength = ClampPercent(State.OppositionStrength + Option->OppositionDelta);
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		if (Option->TreasuryDelta != 0)
		{
			FString TreasuryMessage;
			Tick->AdjustTreasury(Event->NationIso, Option->TreasuryDelta, TreasuryMessage);
		}
		if (Option->PublicOrderDelta != 0)
		{
			Tick->AdjustNationPublicOrder(Event->NationIso, Option->PublicOrderDelta);
		}
		if (!Option->MarketShockGoodId.TrimStartAndEnd().IsEmpty()
			&& Option->MarketShockPriceMultiplier > 0.0
			&& Option->MarketShockDurationMonths > 0)
		{
			FString ShockMessage;
			Tick->ApplyMarketShock(
				Option->MarketShockGoodId,
				Option->MarketShockPriceMultiplier,
				Option->MarketShockDurationMonths,
				Option->MarketShockTitle,
				Event->EventId,
				ShockMessage);
		}
	}
	if (Option->PoliticalCapitalDelta != 0)
	{
		if (UWLCharacterSubsystem* Characters = GetCharacters())
		{
			Characters->AdjustPoliticalCapital(Event->NationIso, Option->PoliticalCapitalDelta);
		}
	}
	Event->bResolved = true;
	UpdateInternalPowerForNation(Event->NationIso);
	OutMessage = FString::Printf(TEXT("Evento %s resuelto con %s."), *Event->EventId, *Option->OptionId);
	return true;
}

void UWLPoliticalSubsystem::CheckCampaignOutcome()
{
	if (CampaignOutcome.bGameOver)
	{
		return;
	}
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		return;
	}

	TMap<FString, int32> ControlledByNation;
	int32 TotalProvinces = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		const FString Controller = Tick->GetProvinceControllerIso(Province.Id);
		if (!Controller.IsEmpty())
		{
			++TotalProvinces;
			ControlledByNation.FindOrAdd(Controller) += 1;
		}
	}
	for (const TPair<FString, int32>& Pair : ControlledByNation)
	{
		if (TotalProvinces > 0 && Pair.Value == TotalProvinces)
		{
			CampaignOutcome.bGameOver = true;
			CampaignOutcome.OutcomeType = TEXT("Domination");
			CampaignOutcome.WinningNationIso = Pair.Key;
			CampaignOutcome.Reason = FString::Printf(TEXT("%s controla todas las provincias."), *Pair.Key);
			return;
		}
	}

	// F5.3: victorias no militares. Meses de campana derivados de la fecha (sin estado nuevo que guardar).
	const FWLBalanceRules Rules = Tick->GetBalanceRules();
	const int32 MonthsElapsed =
		(Tick->GetCurrentYear() - Rules.StartYear) * Rules.MonthsPerYear
		+ (Tick->GetCurrentMonth() - Rules.StartMonth);

	// Hegemonia: concentrar la cuota configurada del PIB total (cualquier nacion puede lograrla).
	if (MonthsElapsed >= Rules.HegemonyMinMonths)
	{
		int64 TotalGDP = 0;
		TMap<FString, int64> GDPByNation;
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			const int64 GDP = Tick->GetNationGDP(Nation.Iso);
			GDPByNation.Add(Nation.Iso, GDP);
			TotalGDP += GDP;
		}
		if (TotalGDP > 0)
		{
			for (const TPair<FString, int64>& Pair : GDPByNation)
			{
				const double Share = static_cast<double>(Pair.Value) / static_cast<double>(TotalGDP);
				if (Share >= Rules.HegemonyGDPShare)
				{
					CampaignOutcome.bGameOver = true;
					CampaignOutcome.OutcomeType = TEXT("Hegemony");
					CampaignOutcome.WinningNationIso = Pair.Key;
					CampaignOutcome.Reason = FString::Printf(
						TEXT("%s concentra el %.0f%% del PIB continental."), *Pair.Key, Share * 100.0);
					return;
				}
			}
		}
	}

	// Regimen/Legado: el jugador sobrevive en el poder los meses configurados.
	const FString PlayerIso = GetPlayerNationIso();
	if (!PlayerIso.IsEmpty() && MonthsElapsed >= Rules.RegimeVictoryMonths)
	{
		CampaignOutcome.bGameOver = true;
		CampaignOutcome.OutcomeType = TEXT("Regime");
		CampaignOutcome.WinningNationIso = PlayerIso;
		CampaignOutcome.Reason = FString::Printf(
			TEXT("Te mantuviste en el poder %d meses: tu legado esta asegurado."), MonthsElapsed);
	}
}

void UWLPoliticalSubsystem::WriteSaveSnapshot(
	TArray<FWLInternalPowerState>& OutInternalPower,
	TArray<FWLDiplomaticRelationState>& OutRelations,
	TArray<FWLIntelligenceNetworkState>& OutNetworks,
	TArray<FWLPoliticalEventInstance>& OutEvents,
	FWLCampaignOutcomeState& OutOutcome) const
{
	InternalPowerByNation.GenerateValueArray(OutInternalPower);
	RelationsByPair.GenerateValueArray(OutRelations);
	IntelligenceByPair.GenerateValueArray(OutNetworks);
	OutEvents = EventQueue;
	OutOutcome = CampaignOutcome;
	OutInternalPower.Sort([](const FWLInternalPowerState& A, const FWLInternalPowerState& B) { return A.NationIso < B.NationIso; });
	OutRelations.Sort([](const FWLDiplomaticRelationState& A, const FWLDiplomaticRelationState& B)
	{
		return (A.NationA + A.NationB) < (B.NationA + B.NationB);
	});
	OutNetworks.Sort([](const FWLIntelligenceNetworkState& A, const FWLIntelligenceNetworkState& B)
	{
		return (A.OwnerIso + A.TargetIso) < (B.OwnerIso + B.TargetIso);
	});
}

bool UWLPoliticalSubsystem::RestoreSaveSnapshot(
	const TArray<FWLInternalPowerState>& SavedInternalPower,
	const TArray<FWLDiplomaticRelationState>& SavedRelations,
	const TArray<FWLIntelligenceNetworkState>& SavedNetworks,
	const TArray<FWLPoliticalEventInstance>& SavedEvents,
	const FWLCampaignOutcomeState& SavedOutcome,
	FString& OutMessage)
{
	ResetPoliticalState();

	for (FWLInternalPowerState State : SavedInternalPower)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.AveragePublicOrder = ClampPercent(State.AveragePublicOrder);
			State.OppositionStrength = ClampPercent(State.OppositionStrength);
			State.OppositionPopularity = ClampPercent(State.OppositionPopularity);
			State.ExternalCoupFunding = ClampPercent(State.ExternalCoupFunding);
			State.CoupRisk = ClampPercent(State.CoupRisk);
			InternalPowerByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLDiplomaticRelationState Relation : SavedRelations)
	{
		if (ValidateNation(Relation.NationA) && ValidateNation(Relation.NationB))
		{
			Relation.NationA = NormalizeIso(Relation.NationA);
			Relation.NationB = NormalizeIso(Relation.NationB);
			Relation.Opinion = FMath::Clamp(Relation.Opinion, -100, 100);
			RelationsByPair.Add(RelationKey(Relation.NationA, Relation.NationB), MoveTemp(Relation));
		}
	}
	for (FWLIntelligenceNetworkState Network : SavedNetworks)
	{
		if (ValidateNation(Network.OwnerIso) && ValidateNation(Network.TargetIso))
		{
			Network.OwnerIso = NormalizeIso(Network.OwnerIso);
			Network.TargetIso = NormalizeIso(Network.TargetIso);
			Network.NetworkStrength = ClampPercent(Network.NetworkStrength);
			Network.Exposure = ClampPercent(Network.Exposure);
			IntelligenceByPair.Add(NetworkKey(Network.OwnerIso, Network.TargetIso), MoveTemp(Network));
		}
	}
	EventQueue = SavedEvents;
	for (const FWLPoliticalEventInstance& Event : EventQueue)
	{
		if (Event.InstanceId.StartsWith(TEXT("EV-")))
		{
			NextEventInstanceNumber = FMath::Max(NextEventInstanceNumber, FCString::Atoi(*Event.InstanceId.Mid(3)) + 1);
		}
	}
	CampaignOutcome = SavedOutcome;
	OutMessage = FString::Printf(TEXT("Politica restaurada: %d estados, %d relaciones, %d redes, %d eventos."),
		InternalPowerByNation.Num(), RelationsByPair.Num(), IntelligenceByPair.Num(), EventQueue.Num());
	return true;
}
