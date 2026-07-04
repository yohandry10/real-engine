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

namespace
{
FString CrisisTypeToEventKey(EWLCrisisChainType Type)
{
	switch (Type)
	{
	case EWLCrisisChainType::NationalProtest:   return TEXT("national_protest");
	case EWLCrisisChainType::CorruptionScandal: return TEXT("corruption_scandal");
	case EWLCrisisChainType::MilitaryCrisis:    return TEXT("military_crisis");
	case EWLCrisisChainType::DebtCrisis:        return TEXT("debt_crisis");
	case EWLCrisisChainType::StudentProtest:    return TEXT("student_protest");
	case EWLCrisisChainType::OilStrike:         return TEXT("oil_strike");
	case EWLCrisisChainType::BorderCrisis:      return TEXT("border_crisis");
	case EWLCrisisChainType::Impeachment:       return TEXT("impeachment");
	case EWLCrisisChainType::SoftCoup:          return TEXT("soft_coup");
	case EWLCrisisChainType::StateOfException:  return TEXT("state_of_exception");
	default:                                    return FString();
	}
}

bool PoliticalEventHasMatchingCrisis(
	const TArray<FWLCrisisChainState>& CrisisChains,
	const FString& NationIso,
	const FWLPoliticalEventDefinition& Def)
{
	const FString ExpectedType = Def.CrisisType.TrimStartAndEnd().ToLower();
	for (const FWLCrisisChainState& Crisis : CrisisChains)
	{
		if (Crisis.NationIso != NationIso || Crisis.bResolved)
		{
			continue;
		}
		if (!ExpectedType.IsEmpty() && CrisisTypeToEventKey(Crisis.Type) != ExpectedType)
		{
			continue;
		}
		if (Crisis.Stage < Def.MinCrisisStage || Crisis.Stage > Def.MaxCrisisStage)
		{
			continue;
		}
		if (Crisis.Intensity < Def.MinCrisisIntensity || Crisis.Intensity > Def.MaxCrisisIntensity)
		{
			continue;
		}
		return true;
	}
	return false;
}
} // namespace

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
	int32 WeightSum = 0;
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		const UWLMilitarySubsystem* Military = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UWLMilitarySubsystem>()
			: nullptr;
		int32 LoyaltySum = 0;
		int32 SkillSum = 0;
		for (const FWLCharacter& General : Characters->GetGenerals(Iso))
		{
			const int32 Weight = GeneralPoliticalWeight(General, Military);
			if (Weight <= 0)
			{
				continue;
			}
			LoyaltySum += General.Loyalty * Weight;
			SkillSum += General.Skill * Weight;
			WeightSum += Weight;
		}
		if (WeightSum > 0)
		{
			GeneralLoyalty = LoyaltySum / WeightSum;
			GeneralSkill = SkillSum / WeightSum;
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
			AddGovernmentLogEntry(EWLGovernmentLogCategory::Crisis, Iso, TEXT(""),
				TEXT("Golpe exitoso"), State.LastCoupReport, TEXT("coup"), 10, true, Iso == GetPlayerNationIso());
			OutReport = State.LastCoupReport;
			return true;
		}

		CampaignOutcome.bGameOver = true;
		CampaignOutcome.OutcomeType = TEXT("Coup");
		CampaignOutcome.LosingNationIso = Iso;
		CampaignOutcome.Reason = FString::Printf(TEXT("Golpe exitoso en %s (presion %d vs defensa %d)."),
			*Iso, CoupPressure, LoyalDefense);
		State.LastCoupReport = CampaignOutcome.Reason;
		AddGovernmentLogEntry(EWLGovernmentLogCategory::Crisis, Iso, TEXT(""),
			TEXT("Golpe exitoso"), State.LastCoupReport, TEXT("coup"), 10, true, true);
		OutReport = State.LastCoupReport;
		return true;
	}

	State.ExternalCoupFunding = ClampPercent(State.ExternalCoupFunding - 15);
	State.OppositionStrength = ClampPercent(State.OppositionStrength - 8);
	State.LastCoupReport = FString::Printf(TEXT("Golpe fallido en %s (presion %d vs defensa %d)."),
		*Iso, CoupPressure, LoyalDefense);
	AddGovernmentLogEntry(EWLGovernmentLogCategory::Crisis, Iso, TEXT(""),
		TEXT("Golpe fallido"), State.LastCoupReport, TEXT("coup"), 8, true, Iso == GetPlayerNationIso());
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
		|| Character.Role != EWLCharacterRole::General
		|| !Character.bActive)
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
	if (!Characters->AdjustCharacterLoyalty(Character.Id, 10, LoyaltyMessage))
	{
		FString RefundMessage;
		Tick->AdjustTreasury(Iso, RewardGeneralCost, RefundMessage);
		OutMessage = LoyaltyMessage;
		return false;
	}
	FString RenownMessage;
	if (!Characters->AddRenownToGeneral(Character.Id, 5, RenownMessage))
	{
		FString RollbackMessage;
		Characters->AdjustCharacterLoyalty(Character.Id, -10, RollbackMessage);
		FString RefundMessage;
		Tick->AdjustTreasury(Iso, RewardGeneralCost, RefundMessage);
		OutMessage = RenownMessage;
		return false;
	}
	AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, 1, TEXT("recompensa a general"));
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
	int32 LoyaltyHits = 0;
	for (const FWLCharacter& General : Characters->GetGenerals(Iso))
	{
		if (General.Id == Character.Id)
		{
			continue;
		}
		FString LoyaltyMessage;
		if (Characters->AdjustCharacterLoyalty(General.Id, -4, LoyaltyMessage))
		{
			++LoyaltyHits;
		}
	}
	AdjustPublicGroupSupport(Iso, EWLPublicGroup::Military, -2, TEXT("purga militar"));
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->AdjustNationPublicOrder(Iso, -2);
	}
	OutMessage += FString::Printf(TEXT(" La purga aumenta tension politica y reduce lealtad de %d generales."), LoyaltyHits);
	return true;
}

bool UWLPoliticalSubsystem::RepressOpposition(const FString& NationIso, FString& OutMessage)
{
	FWLPoliticalActionRequest Request;
	Request.NationIso = NationIso;
	Request.ActionType = EWLPoliticalActionType::RepressOpposition;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::RepressOppositionDirect(const FString& NationIso, FString& OutMessage)
{
	const FString Iso = NormalizeIso(NationIso);
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Tick || !ValidateNation(Iso))
	{
		OutMessage = TEXT("Subsistema economico/provincial no disponible.");
		return false;
	}
	FWLInternalPowerState& State = EnsureInternalPower(Iso);
	const int32 PreviousOpposition = State.OppositionStrength;
	State.OppositionStrength = ClampPercent(State.OppositionStrength - 18);
	State.OppositionPopularity = ClampPercent(State.OppositionPopularity - 10);
	Tick->AdjustNationPublicOrder(Iso, -4);
	OutMessage = FString::Printf(TEXT("Oposicion %d -> %d. Represion reduce orden publico y deja memoria politica."),
		PreviousOpposition, State.OppositionStrength);
	AddPoliticalMemory(Iso, TEXT("recent_repression"), 1, 10, OutMessage);
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
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->InvalidateEconomicQueryCache();
	}
	return true;
}

bool UWLPoliticalSubsystem::AdjustRelationOpinion(
	const FString& NationA,
	const FString& NationB,
	int32 Delta,
	FString& OutMessage)
{
	if (!ValidateNation(NationA) || !ValidateNation(NationB) || NormalizeIso(NationA) == NormalizeIso(NationB))
	{
		OutMessage = TEXT("Relacion diplomatica invalida.");
		return false;
	}

	FWLDiplomaticRelationState Relation;
	const int32 BaseOpinion = GetRelation(NationA, NationB, Relation) ? Relation.Opinion : 0;
	return SetRelationOpinion(NationA, NationB, BaseOpinion + Delta, OutMessage);
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
	{
		const FString PlayerIso = GetPlayerNationIso();
		AddGovernmentLogEntry(EWLGovernmentLogCategory::Military, Relation.NationA, Relation.NationB,
			TEXT("Guerra declarada"), OutMessage, TEXT("diplomacy"), 10, true,
			Relation.NationA == PlayerIso || Relation.NationB == PlayerIso);
	}
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->InvalidateEconomicQueryCache();
	}
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
	{
		const FString PlayerIso = GetPlayerNationIso();
		AddGovernmentLogEntry(EWLGovernmentLogCategory::Diplomacy, Relation.NationA, Relation.NationB,
			TEXT("Paz firmada"), OutMessage, TEXT("diplomacy"), 7, true,
			Relation.NationA == PlayerIso || Relation.NationB == PlayerIso);
	}
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->InvalidateEconomicQueryCache();
	}
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
	{
		const FString PlayerIso = GetPlayerNationIso();
		AddGovernmentLogEntry(EWLGovernmentLogCategory::Diplomacy, Relation.NationA, Relation.NationB,
			TEXT("Tratado firmado"), OutMessage, TEXT("diplomacy"), 4, true,
			Relation.NationA == PlayerIso || Relation.NationB == PlayerIso);
	}
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->InvalidateEconomicQueryCache();
	}
	return true;
}

bool UWLPoliticalSubsystem::BreakTreaty(
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
	{
		const FString PlayerIso = GetPlayerNationIso();
		AddGovernmentLogEntry(EWLGovernmentLogCategory::Diplomacy, Relation.NationA, Relation.NationB,
			TEXT("Tratado roto"), OutMessage, TEXT("diplomacy"), 5, true,
			Relation.NationA == PlayerIso || Relation.NationB == PlayerIso);
	}
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->InvalidateEconomicQueryCache();
	}
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
	if (Delta != 0)
	{
		AddGovernmentLogEntry(
			EWLGovernmentLogCategory::Economy,
			Iso,
			TEXT(""),
			TEXT("Arancel nacional ajustado"),
			OutMessage,
			TEXT("tariff"),
			FMath::Clamp(FMath::Abs(Delta), 1, 10),
			false,
			Iso == GetPlayerNationIso());
	}
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
		AddGovernmentLogEntry(EWLGovernmentLogCategory::Intelligence, Owner, Target,
			TEXT("Operacion de espionaje detectada"),
			FString::Printf(TEXT("Operacion de espionaje de %s detectada en %s: la relacion se deteriora."), *Owner, *Target),
			TEXT("intelligence"), 7, true, Owner == GetPlayerNationIso() || Target == GetPlayerNationIso());
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
		const FString Trigger = Def.Trigger.TrimStartAndEnd().ToLower();
		const bool bCrisisTrigger = Trigger == TEXT("crisis") || Trigger == TEXT("crisis_chain");
		if (bCrisisTrigger && !PoliticalEventHasMatchingCrisis(CrisisChains, State.NationIso, Def))
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
		Event.TargetIso = NormalizeIso(Def.TargetIso);
		Event.Title = Def.Title;
		Event.Body = Def.Body;
		Event.Options = Def.Options;

		// F5.5: la agenda del lider modifica las opciones. Un lider autoritario reprime mas fuerte (y mas
		// sucio); un reformista negocia mas barato; uno polarizante agrava el coste de ceder o ignorar.
		const TArray<FString> AgendaTraits = GetLeaderAgendaTraits(State.NationIso);
		for (FWLPoliticalEventOption& Option : Event.Options)
		{
			for (const FString& Trait : AgendaTraits)
			{
				const FString T = Trait.ToLower();
				if (T == TEXT("autoritarismo") && Option.OppositionDelta < 0)
				{
					Option.OppositionDelta = FMath::RoundToInt(Option.OppositionDelta * 1.5f);
					Option.PublicOrderDelta -= 1;
				}
				else if (T == TEXT("reformista") && Option.PoliticalCapitalDelta < 0)
				{
					Option.PoliticalCapitalDelta = Option.PoliticalCapitalDelta / 2;
				}
				else if (T == TEXT("polarizante") && Option.OppositionDelta > 0)
				{
					Option.OppositionDelta = FMath::RoundToInt(Option.OppositionDelta * 1.5f);
				}
			}
		}

		if (State.NationIso == GetPlayerNationIso())
		{
			AddGovernmentLogEntry(EWLGovernmentLogCategory::Event, State.NationIso, Event.TargetIso,
				TEXT("Nuevo evento pendiente"),
				FString::Printf(TEXT("%s - decision pendiente en POLITICA."), *Event.Title),
				TEXT("event_queue"), 6, true, true);
		}
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
	FWLPoliticalActionRequest Request;
	Request.ActionType = EWLPoliticalActionType::ResolveEvent;
	Request.PrimaryId = InstanceId;
	Request.SecondaryId = OptionId;
	return ExecutePoliticalAction(Request, OutMessage);
}

bool UWLPoliticalSubsystem::ResolveEventDirect(
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

	const FString EventNationIso = NormalizeIso(Event->NationIso);
	if (!ValidateNation(EventNationIso))
	{
		OutMessage = FString::Printf(TEXT("Evento %s tiene nacion invalida: %s"), *Event->InstanceId, *Event->NationIso);
		return false;
	}

	const FString RelationTargetIso = NormalizeIso(Event->TargetIso);
	if (Option->RelationDelta != 0
		&& (!ValidateNation(RelationTargetIso) || RelationTargetIso == EventNationIso))
	{
		OutMessage = FString::Printf(TEXT("Evento %s requiere objetivo diplomatico valido."), *Event->InstanceId);
		return false;
	}

	Event->NationIso = EventNationIso;
	Event->TargetIso = RelationTargetIso;

	FWLInternalPowerState& State = EnsureInternalPower(EventNationIso);
	State.OppositionStrength = ClampPercent(State.OppositionStrength + Option->OppositionDelta);
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		if (Option->TreasuryDelta > 0)
		{
			FString TreasuryMessage;
			Tick->AdjustTreasury(EventNationIso, Option->TreasuryDelta, TreasuryMessage);
		}
		if (Option->PublicOrderDelta != 0)
		{
			Tick->AdjustNationPublicOrder(EventNationIso, Option->PublicOrderDelta);
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
	if (Option->PoliticalCapitalDelta > 0)
	{
		if (UWLCharacterSubsystem* Characters = GetCharacters())
		{
			Characters->AdjustPoliticalCapital(EventNationIso, Option->PoliticalCapitalDelta);
		}
	}
	if (Option->RelationDelta != 0)
	{
		FString RelationMessage;
		AdjustRelationOpinion(EventNationIso, RelationTargetIso, Option->RelationDelta, RelationMessage);
	}
	if (Option->OppositionDelta > 0 || Option->PublicOrderDelta < 0)
	{
		AddPoliticalMemory(EventNationIso, TEXT("crisis_unrest"), 1, 8,
			FString::Printf(TEXT("Evento %s eleva tension social."), *Event->EventId));
	}
	if (Option->OppositionDelta < 0 && Option->PublicOrderDelta < 0)
	{
		AddPoliticalMemory(EventNationIso, TEXT("recent_repression"), 1, 10,
			FString::Printf(TEXT("Evento %s resuelto con coercion."), *Event->EventId));
		AdjustPublicGroupSupport(EventNationIso, EWLPublicGroup::Workers, -2, TEXT("represion reciente"));
		AdjustPublicGroupSupport(EventNationIso, EWLPublicGroup::Unions, -2, TEXT("represion reciente"));
	}
	if (Option->PoliticalCapitalDelta < 0 && Option->OppositionDelta < 0)
	{
		AddPoliticalMemory(EventNationIso, TEXT("negotiated_concession"), 1, 8,
			FString::Printf(TEXT("Evento %s deja concesiones politicas."), *Event->EventId));
		AdjustPublicGroupSupport(EventNationIso, EWLPublicGroup::MiddleClass, 1, TEXT("concesion politica"));
	}
	Event->bResolved = true;
	UpdateInternalPowerForNation(EventNationIso);
	OutMessage = FString::Printf(TEXT("Evento %s resuelto con %s."), *Event->EventId, *Option->OptionId);
	return true;
}
