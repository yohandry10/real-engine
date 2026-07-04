// Copyright World Leader project. See ROADMAP.md.

#include "Politics/WLPoliticalSubsystemPrivate.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Military/WLMilitarySubsystem.h"
#include "WorldLeader.h"

using namespace WLPoliticsPrivate;

namespace
{
	constexpr int64 GeneralCreationTreasuryCost = 1500;
	constexpr int32 GeneralCreationPoliticalCost = 8;

	void ParseGovernmentActionId(const FString& ActionId, TArray<FString>& OutParts)
	{
		OutParts.Reset();
		ActionId.TrimStartAndEnd().ParseIntoArray(OutParts, TEXT(":"), true);
		for (FString& Part : OutParts)
		{
			Part = Part.TrimStartAndEnd();
		}
	}

	FString GovernmentCommandTitle(const FString& ActionKey)
	{
		if (ActionKey == TEXT("taxup") || ActionKey == TEXT("taxdown")) { return TEXT("Politica fiscal"); }
		if (ActionKey == TEXT("tariffup") || ActionKey == TEXT("tariffdown")) { return TEXT("Politica arancelaria"); }
		if (ActionKey == TEXT("bond")) { return TEXT("Bono soberano"); }
		if (ActionKey == TEXT("imf")) { return TEXT("Programa FMI"); }
		if (ActionKey == TEXT("default")) { return TEXT("Default soberano"); }
		if (ActionKey == TEXT("aid")) { return TEXT("Ayuda exterior"); }
		if (ActionKey == TEXT("fdi")) { return TEXT("Inversion extranjera"); }
		if (ActionKey == TEXT("build") || ActionKey == TEXT("upgrade")) { return TEXT("Construccion provincial"); }
		if (ActionKey == TEXT("creategeneral")) { return TEXT("Crear general"); }
		if (ActionKey == TEXT("promote")) { return TEXT("Ascenso militar"); }
		if (ActionKey == TEXT("retire")) { return TEXT("Retiro de personaje"); }
		if (ActionKey == TEXT("reward")) { return TEXT("Recompensa a general"); }
		if (ActionKey == TEXT("purge")) { return TEXT("Purga"); }
		if (ActionKey == TEXT("appoint") || ActionKey == TEXT("appointc") || ActionKey == TEXT("dismiss") || ActionKey == TEXT("hire")) { return TEXT("Gabinete"); }
		if (ActionKey == TEXT("war")) { return TEXT("Declaracion de guerra"); }
		if (ActionKey == TEXT("peace")) { return TEXT("Paz negociada"); }
		if (ActionKey == TEXT("treaty")) { return TEXT("Tratado firmado"); }
		if (ActionKey == TEXT("breaktreaty")) { return TEXT("Tratado roto"); }
		if (ActionKey == TEXT("spynet") || ActionKey == TEXT("spy")) { return TEXT("Operacion de inteligencia"); }
		if (ActionKey == TEXT("reorg")) { return TEXT("Reorganizacion militar"); }
		if (ActionKey == TEXT("assigngen")) { return TEXT("Asignacion de mando"); }
		if (ActionKey == TEXT("autoresolve")) { return TEXT("Batalla auto-resuelta"); }
		return TEXT("Accion de gobierno");
	}

	EWLGovernmentLogCategory GovernmentCommandCategory(const FString& ActionKey)
	{
		if (ActionKey == TEXT("taxup") || ActionKey == TEXT("taxdown")
			|| ActionKey == TEXT("tariffup") || ActionKey == TEXT("tariffdown")
			|| ActionKey == TEXT("bond") || ActionKey == TEXT("imf") || ActionKey == TEXT("default")
			|| ActionKey == TEXT("aid") || ActionKey == TEXT("fdi")
			|| ActionKey == TEXT("build") || ActionKey == TEXT("upgrade"))
		{
			return EWLGovernmentLogCategory::Economy;
		}
		if (ActionKey == TEXT("war") || ActionKey == TEXT("peace") || ActionKey == TEXT("treaty") || ActionKey == TEXT("breaktreaty"))
		{
			return EWLGovernmentLogCategory::Diplomacy;
		}
		if (ActionKey == TEXT("spynet") || ActionKey == TEXT("spy"))
		{
			return EWLGovernmentLogCategory::Intelligence;
		}
		if (ActionKey == TEXT("creategeneral") || ActionKey == TEXT("promote") || ActionKey == TEXT("retire")
			|| ActionKey == TEXT("reward") || ActionKey == TEXT("purge")
			|| ActionKey == TEXT("reorg") || ActionKey == TEXT("assigngen") || ActionKey == TEXT("autoresolve"))
		{
			return EWLGovernmentLogCategory::Military;
		}
		return EWLGovernmentLogCategory::Government;
	}

	int64 SuggestedDebtPrincipal(int64 AvailableCredit, int32 Divisor)
	{
		if (AvailableCredit <= 0)
		{
			return 0;
		}
		const int64 Raw = FMath::Max<int64>(1000, AvailableCredit / FMath::Max(1, Divisor));
		return FMath::Clamp<int64>(Raw, 1000, AvailableCredit);
	}

	bool ParseMinisterOffice(const FString& Raw, EWLMinisterOffice& OutOffice)
	{
		const int32 RawValue = FCString::Atoi(*Raw);
		if (RawValue < static_cast<int32>(EWLMinisterOffice::None)
			|| RawValue > static_cast<int32>(EWLMinisterOffice::Intelligence))
		{
			return false;
		}
		OutOffice = static_cast<EWLMinisterOffice>(RawValue);
		return OutOffice != EWLMinisterOffice::None;
	}

	bool ValidateCharacterForNation(
		const UWLCharacterSubsystem* Characters,
		const FString& NationIso,
		const FString& CharacterId,
		EWLCharacterRole ExpectedRole,
		FWLCharacter& OutCharacter,
		FString& OutReason)
	{
		if (!Characters || !Characters->GetCharacter(CharacterId, OutCharacter))
		{
			OutReason = FString::Printf(TEXT("Personaje no disponible: %s"), *CharacterId);
			return false;
		}
		if (!OutCharacter.bActive || OutCharacter.CountryIso != NationIso || OutCharacter.Role != ExpectedRole)
		{
			OutReason = FString::Printf(TEXT("%s no es valido para esta accion."), *OutCharacter.Name);
			return false;
		}
		return true;
	}
}

bool UWLPoliticalSubsystem::BuildPoliticalActionRequest(
	const FWLGovernmentActionRequest& Request,
	FWLPoliticalActionRequest& OutRequest) const
{
	TArray<FString> Parts;
	ParseGovernmentActionId(Request.ActionId, Parts);
	if (Parts.IsEmpty())
	{
		return false;
	}

	const FString Verb = Parts[0].ToLower();
	const FString Arg1 = Parts.Num() > 1 ? Parts[1] : FString();
	const FString Arg2 = Parts.Num() > 2 ? Parts[2] : FString();

	OutRequest = FWLPoliticalActionRequest();
	OutRequest.NationIso = Request.NationIso;

	if (Verb == TEXT("agendaset"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::SetAgenda;
		OutRequest.Priorities = Request.Priorities;
		return true;
	}
	if (Verb == TEXT("program"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::StartProgram;
		OutRequest.PrimaryId = Arg1;
		return true;
	}
	if (Verb == TEXT("reform"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::EnactReform;
		OutRequest.PrimaryId = Arg1;
		return true;
	}
	if (Verb == TEXT("promise"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::MakeCampaignPromise;
		OutRequest.PrimaryId = Arg1;
		return true;
	}
	if (Verb == TEXT("negotiate"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::NegotiatePartySupport;
		OutRequest.PrimaryId = Arg1;
		return true;
	}
	if (Verb == TEXT("partyelect"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::HoldPartyInternalElection;
		OutRequest.PrimaryId = Arg1;
		return true;
	}
	if (Verb == TEXT("patronage"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::UsePatronage;
		OutRequest.NumericValue = FCString::Atoi(*Arg1);
		return true;
	}
	if (Verb == TEXT("media"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::RunMediaAction;
		OutRequest.NumericValue = FCString::Atoi(*Arg1);
		return true;
	}
	if (Verb == TEXT("region"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::RunRegionPolicy;
		OutRequest.PrimaryId = Arg2;
		OutRequest.NumericValue = FCString::Atoi(*Arg1);
		return true;
	}
	if (Verb == TEXT("repress"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::RepressOpposition;
		return true;
	}
	if (Verb == TEXT("event"))
	{
		OutRequest.ActionType = EWLPoliticalActionType::ResolveEvent;
		OutRequest.PrimaryId = Arg1;
		OutRequest.SecondaryId = Arg2;
		return true;
	}
	return false;
}

FString UWLPoliticalSubsystem::GovernmentActionTargetKey(const FWLGovernmentActionRequest& Request) const
{
	TArray<FString> Parts;
	ParseGovernmentActionId(Request.ActionId, Parts);
	if (Parts.IsEmpty())
	{
		return TEXT("none");
	}

	const FString Verb = Parts[0].ToLower();
	if (Verb == TEXT("taxup") || Verb == TEXT("taxdown"))
	{
		return TEXT("tax");
	}
	if (Verb == TEXT("tariffup") || Verb == TEXT("tariffdown"))
	{
		return TEXT("tariff");
	}
	if (Verb == TEXT("build") || Verb == TEXT("upgrade"))
	{
		return FString::Printf(TEXT("%s|%s"), *Request.ContextId.TrimStartAndEnd().ToLower(), *(Parts.Num() > 1 ? Parts[1].ToLower() : FString()));
	}
	if (Verb == TEXT("fdi"))
	{
		return FString::Printf(TEXT("%s|%s|%s"),
			*(Parts.Num() > 1 ? Parts[1].ToLower() : FString()),
			*(Parts.Num() > 2 ? Parts[2].ToLower() : FString()),
			*(Parts.Num() > 3 ? Parts[3].ToLower() : FString()));
	}
	if (Verb == TEXT("war") || Verb == TEXT("peace") || Verb == TEXT("aid") || Verb == TEXT("spynet"))
	{
		return Parts.Num() > 1 ? Parts[1].ToLower() : TEXT("none");
	}
	if (Verb == TEXT("treaty") || Verb == TEXT("breaktreaty") || Verb == TEXT("spy") || Verb == TEXT("assigngen") || Verb == TEXT("autoresolve"))
	{
		return FString::Printf(TEXT("%s|%s"),
			*(Parts.Num() > 1 ? Parts[1].ToLower() : FString()),
			*(Parts.Num() > 2 ? Parts[2].ToLower() : FString()));
	}
	if (Parts.Num() > 1)
	{
		return Parts[1].ToLower();
	}
	return Verb;
}

int32 UWLPoliticalSubsystem::GetGovernmentActionCooldownRemaining(
	const FString& NationIso,
	const FString& ActionKey,
	const FString& TargetKey) const
{
	const FString Iso = NormalizeIso(NationIso);
	const FString NormalizedAction = ActionKey.TrimStartAndEnd().ToLower();
	const FString Target = TargetKey.TrimStartAndEnd().ToLower();
	const int32 MonthKey = GetCurrentPoliticalMonthKey();
	int32 Remaining = 0;
	for (const FWLPoliticalActionRecord& Record : PoliticalActionRecords)
	{
		if (Record.NationIso != Iso
			|| Record.ActionType != EWLPoliticalActionType::GovernmentCommand
			|| Record.ActionKey != NormalizedAction
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

void UWLPoliticalSubsystem::RecordGovernmentAction(const FWLGovernmentActionPreview& Preview, const FString& Result)
{
	FWLPoliticalActionRecord Record;
	Record.NationIso = Preview.NationIso;
	Record.ActionType = EWLPoliticalActionType::GovernmentCommand;
	Record.ActionKey = Preview.ActionKey;
	Record.TargetKey = Preview.TargetKey;
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

FWLGovernmentActionPreview UWLPoliticalSubsystem::GetGovernmentActionPreview(const FWLGovernmentActionRequest& Request) const
{
	FWLPoliticalActionRequest PoliticalRequest;
	if (BuildPoliticalActionRequest(Request, PoliticalRequest))
	{
		const FWLPoliticalActionPreview PoliticalPreview = GetPoliticalActionPreview(PoliticalRequest);
		FWLGovernmentActionPreview Preview;
		Preview.NationIso = PoliticalPreview.NationIso;
		Preview.ActionId = Request.ActionId;
		Preview.ActionKey = PoliticalActionKey(PoliticalRequest.ActionType);
		Preview.TargetKey = PoliticalActionTargetKey(PoliticalRequest);
		Preview.Category = PoliticalActionLogCategory(PoliticalRequest.ActionType);
		Preview.bCanExecute = PoliticalPreview.bCanExecute;
		Preview.BlockReason = PoliticalPreview.BlockReason;
		Preview.TreasuryCost = PoliticalPreview.TreasuryCost;
		Preview.PoliticalCapitalCost = PoliticalPreview.PoliticalCapitalCost;
		Preview.ActionPointCost = PoliticalPreview.ActionPointCost;
		Preview.CooldownRemainingMonths = PoliticalPreview.CooldownRemainingMonths;
		Preview.CooldownMonths = PoliticalPreview.CooldownMonths;
		Preview.ActionPointsRemaining = PoliticalPreview.ActionPointsRemaining;
		Preview.EffectsPreview = PoliticalPreview.EffectsPreview;
		return Preview;
	}

	FWLGovernmentActionPreview Preview;
	Preview.NationIso = NormalizeIso(Request.NationIso);
	Preview.ActionId = Request.ActionId.TrimStartAndEnd();

	TArray<FString> Parts;
	ParseGovernmentActionId(Request.ActionId, Parts);
	if (Parts.IsEmpty())
	{
		Preview.BlockReason = TEXT("Accion de gobierno vacia.");
		return Preview;
	}

	const FString Verb = Parts[0].ToLower();
	const FString Arg1 = Parts.Num() > 1 ? Parts[1] : FString();
	const FString Arg2 = Parts.Num() > 2 ? Parts[2] : FString();
	const FString Arg3 = Parts.Num() > 3 ? Parts[3] : FString();
	Preview.ActionKey = Verb;
	Preview.TargetKey = GovernmentActionTargetKey(Request);
	Preview.Category = GovernmentCommandCategory(Verb);

	auto Block = [&Preview](const FString& Reason)
	{
		if (Preview.BlockReason.IsEmpty())
		{
			Preview.BlockReason = Reason;
		}
	};

	const UWLStrategicTickSubsystem* Tick = GetTick();
	const UWLCharacterSubsystem* Characters = GetCharacters();
	const UWLMilitarySubsystem* Military = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWLMilitarySubsystem>() : nullptr;
	const UWLDataRegistry* Registry = GetRegistry();
	bool bAllowCreditSpend = false;

	if (!ValidateNation(Preview.NationIso))
	{
		Block(TEXT("Nacion invalida para accion de gobierno."));
	}

	if (Preview.BlockReason.IsEmpty())
	{
		if (Verb == TEXT("taxup") || Verb == TEXT("taxdown"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 0;
			if (!Tick)
			{
				Block(TEXT("Economia no disponible."));
			}
			else
			{
				const int32 Delta = Verb == TEXT("taxup") ? 5 : -5;
				const int32 Current = Tick->GetTaxRate(Preview.NationIso);
				const int32 Next = FMath::Clamp(Current + Delta,
					Tick->GetBalanceRules().TaxRateMinPercent,
					Tick->GetBalanceRules().TaxRateMaxPercent);
				if (Next == Current)
				{
					Block(TEXT("La tasa ya esta en su limite."));
				}
				Preview.EffectsPreview = FString::Printf(TEXT("Impuestos %d%% -> %d%%."), Current, Next);
			}
		}
		else if (Verb == TEXT("tariffup") || Verb == TEXT("tariffdown"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 0;
			if (!Tick)
			{
				Block(TEXT("Economia no disponible."));
			}
			else
			{
				const int32 Delta = Verb == TEXT("tariffup") ? 5 : -5;
				const int32 Current = Tick->GetTariffRate(Preview.NationIso);
				const int32 Next = FMath::Clamp(Current + Delta, 0, Tick->GetBalanceRules().TariffRateMaxPercent);
				if (Next == Current)
				{
					Block(TEXT("El arancel ya esta en su limite."));
				}
				Preview.EffectsPreview = FString::Printf(TEXT("Aranceles %d%% -> %d%%."), Current, Next);
			}
		}
		else if (Verb == TEXT("bond") || Verb == TEXT("imf"))
		{
			Preview.ActionPointCost = Verb == TEXT("bond") ? 1 : 2;
			Preview.CooldownMonths = Verb == TEXT("bond") ? 1 : 6;
			if (!Tick)
			{
				Block(TEXT("Finanzas no disponibles."));
			}
			else
			{
				const FWLFinancialProfile Profile = Tick->GetFinancialProfile(Preview.NationIso);
				const int64 Principal = SuggestedDebtPrincipal(Profile.AvailableCredit, Verb == TEXT("bond") ? 4 : 3);
				if (Principal <= 0 || (Verb == TEXT("bond") && Profile.bInDefault))
				{
					Block(Profile.bInDefault ? TEXT("Pais en default: bonos bloqueados.") : TEXT("Sin credito disponible."));
				}
				if (Verb == TEXT("imf") && !Profile.bIMFEligible)
				{
					Block(FString::Printf(TEXT("FMI no disponible con rating %s."), *Profile.CreditRatingLabel));
				}
				Preview.EffectsPreview = FString::Printf(TEXT("Financia %lld a %d meses."),
					static_cast<long long>(Principal),
					Verb == TEXT("bond") ? 24 : 36);
			}
		}
		else if (Verb == TEXT("default"))
		{
			Preview.ActionPointCost = 2;
			Preview.CooldownMonths = 24;
			Preview.EffectsPreview = TEXT("Elimina deuda viva y hunde rating/orden publico.");
		}
		else if (Verb == TEXT("aid"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 3;
			if (!ValidateNation(NormalizeIso(Arg1)) || NormalizeIso(Arg1) == Preview.NationIso)
			{
				Block(TEXT("Objetivo invalido para ayuda exterior."));
			}
			Preview.EffectsPreview = FString::Printf(TEXT("Ayuda a %s por 1000/mes durante 12 meses."), *NormalizeIso(Arg1));
		}
		else if (Verb == TEXT("fdi"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 3;
			const FString Recipient = NormalizeIso(Arg1);
			if (!Registry || !ValidateNation(Recipient) || Recipient == Preview.NationIso)
			{
				Block(TEXT("Objetivo invalido para FDI."));
			}
			else
			{
				FWLProvinceData Province;
				FWLBuildingData Building;
				if (!Registry->GetProvince(Arg2, Province) || !Registry->GetBuilding(Arg3, Building))
				{
					Block(TEXT("Provincia o edificio invalido para FDI."));
				}
				else if (!Tick || Tick->GetProvinceControllerIso(Province.Id) != Recipient)
				{
					Block(FString::Printf(TEXT("%s no controla %s."), *Recipient, *Arg2));
				}
				else
				{
					Preview.EffectsPreview = FString::Printf(TEXT("FDI en %s para construir %s."), *Province.Name, *Building.Name);
				}
			}
		}
		else if (Verb == TEXT("build") || Verb == TEXT("upgrade"))
		{
			bAllowCreditSpend = true;
			Preview.ActionPointCost = 0;
			Preview.CooldownMonths = 0;
			if (!Registry || !Tick)
			{
				Block(TEXT("Construccion no disponible."));
			}
			else
			{
				FWLProvinceData Province;
				FWLBuildingData Building;
				if (!Registry->GetProvince(Request.ContextId, Province) || !Registry->GetBuilding(Arg1, Building))
				{
					Block(TEXT("Provincia o edificio invalido."));
				}
				else if (Tick->GetProvinceControllerIso(Province.Id) != Preview.NationIso)
				{
					Block(FString::Printf(TEXT("%s no controla %s."), *Preview.NationIso, *Province.Id));
				}
				else
				{
					Preview.TreasuryCost = Verb == TEXT("build")
						? FMath::Max<int64>(0, Building.Cost)
						: FMath::Max<int64>(0, Tick->GetProvinceBuildingUpgradeCost(Province.Id, Building.Id));
					Preview.EffectsPreview = FString::Printf(TEXT("%s %s en %s. Coste %lld."),
						Verb == TEXT("build") ? TEXT("Construye") : TEXT("Mejora"),
						*Building.Name,
						*Province.Name,
						static_cast<long long>(Preview.TreasuryCost));
				}
			}
		}
		else if (Verb == TEXT("creategeneral"))
		{
			Preview.ActionPointCost = 1;
			Preview.PoliticalCapitalCost = GeneralCreationPoliticalCost;
			Preview.TreasuryCost = GeneralCreationTreasuryCost;
			Preview.CooldownMonths = 1;
			Preview.EffectsPreview = TEXT("Crea un general activo si hay cupo militar.");
		}
		else if (Verb == TEXT("promote") || Verb == TEXT("retire") || Verb == TEXT("reward") || Verb == TEXT("purge"))
		{
			Preview.ActionPointCost = Verb == TEXT("purge") ? 2 : 1;
			Preview.CooldownMonths = Verb == TEXT("reward") || Verb == TEXT("purge") ? 1 : 0;
			if (Verb == TEXT("reward"))
			{
				Preview.TreasuryCost = RewardGeneralCost;
			}
			FWLCharacter Character;
			FString Reason;
			const EWLCharacterRole ExpectedRole = (Verb == TEXT("promote") || Verb == TEXT("reward") || Verb == TEXT("purge"))
				? EWLCharacterRole::General
				: EWLCharacterRole::Minister;
			if (!ValidateCharacterForNation(Characters, Preview.NationIso, Arg1, ExpectedRole, Character, Reason))
			{
				if (Verb == TEXT("retire") && Characters && Characters->GetCharacter(Arg1, Character)
					&& Character.bActive && Character.CountryIso == Preview.NationIso)
				{
					Reason.Reset();
				}
				else
				{
					Block(Reason);
				}
			}
			Preview.EffectsPreview = FString::Printf(TEXT("%s: %s."), *GovernmentCommandTitle(Verb), *Arg1);
		}
		else if (Verb == TEXT("appoint") || Verb == TEXT("appointc") || Verb == TEXT("dismiss") || Verb == TEXT("hire"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 0;
			EWLMinisterOffice Office = EWLMinisterOffice::None;
			if (!ParseMinisterOffice(Arg1, Office) || !Characters)
			{
				Block(TEXT("Cartera invalida."));
			}
			else
			{
				Preview.PoliticalCapitalCost = Verb == TEXT("dismiss")
					? Characters->GetMinisterDismissalCost()
					: Characters->GetMinisterAppointmentCost();
				if (Verb == TEXT("appointc"))
				{
					FWLCharacter Candidate;
					FString Reason;
					if (!ValidateCharacterForNation(Characters, Preview.NationIso, Arg2, EWLCharacterRole::Minister, Candidate, Reason)
						|| Candidate.AssignedOffice != EWLMinisterOffice::None)
					{
						Block(Reason.IsEmpty() ? TEXT("Candidato no disponible.") : Reason);
					}
				}
				Preview.EffectsPreview = FString::Printf(TEXT("Gestiona %s."), *UWLCharacterSubsystem::MinisterOfficeToString(Office));
			}
		}
		else if (Verb == TEXT("war") || Verb == TEXT("peace") || Verb == TEXT("treaty") || Verb == TEXT("breaktreaty"))
		{
			Preview.ActionPointCost = Verb == TEXT("war") ? 2 : 1;
			Preview.CooldownMonths = Verb == TEXT("war") ? 3 : 1;
			const FString Target = NormalizeIso((Verb == TEXT("treaty") || Verb == TEXT("breaktreaty")) ? Arg2 : Arg1);
			if (!ValidateNation(Target) || Target == Preview.NationIso)
			{
				Block(TEXT("Objetivo diplomatico invalido."));
			}
			Preview.EffectsPreview = FString::Printf(TEXT("%s con %s."), *GovernmentCommandTitle(Verb), *Target);
		}
		else if (Verb == TEXT("spynet") || Verb == TEXT("spy"))
		{
			Preview.ActionPointCost = Verb == TEXT("spy") && static_cast<EWLSpyOperationType>(FCString::Atoi(*Arg1)) == EWLSpyOperationType::FundCoup ? 2 : 1;
			Preview.CooldownMonths = Verb == TEXT("spy") ? 2 : 1;
			const FString Target = NormalizeIso(Verb == TEXT("spynet") ? Arg1 : Arg2);
			if (!ValidateNation(Target) || Target == Preview.NationIso)
			{
				Block(TEXT("Objetivo de inteligencia invalido."));
			}
			FWLCharacter Spy;
			FString Reason;
			if (!ValidateCharacterForNation(Characters, Preview.NationIso, Request.AgentCharacterId, EWLCharacterRole::Spy, Spy, Reason))
			{
				Block(TEXT("Sin espias activos disponibles."));
			}
			Preview.EffectsPreview = FString::Printf(TEXT("Operacion contra %s."), *Target);
		}
		else if (Verb == TEXT("reorg"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 0;
			FWLArmy Army;
			if (!Military || !Military->GetArmy(Arg1, Army) || Army.OwnerIso != Preview.NationIso)
			{
				Block(TEXT("Ejercito invalido para reorganizar."));
			}
			Preview.EffectsPreview = FString::Printf(TEXT("Reorganiza reservas de %s."), *Arg1);
		}
		else if (Verb == TEXT("assigngen"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 0;
			FWLCharacter General;
			FString Reason;
			if (!ValidateCharacterForNation(Characters, Preview.NationIso, Arg1, EWLCharacterRole::General, General, Reason))
			{
				Block(Reason);
			}
			FWLArmy Army;
			if (!Military || !Military->GetArmy(Arg2, Army) || Army.OwnerIso != Preview.NationIso)
			{
				Block(TEXT("Ejercito invalido para asignar mando."));
			}
			Preview.EffectsPreview = FString::Printf(TEXT("Asigna %s a %s."), *Arg1, *Arg2);
		}
		else if (Verb == TEXT("autoresolve"))
		{
			Preview.ActionPointCost = 1;
			Preview.CooldownMonths = 0;
			FWLBattlePreview BattlePreview;
			if (!Military || !Military->PreviewBattle(Arg1, Arg2, BattlePreview) || !BattlePreview.bValid)
			{
				Block(BattlePreview.Reason.IsEmpty() ? TEXT("Batalla invalida.") : BattlePreview.Reason);
			}
			else
			{
				Preview.EffectsPreview = FString::Printf(TEXT("%s: ataque %d vs defensa %d."),
					*BattlePreview.OddsLabel,
					BattlePreview.AttackerPower,
					BattlePreview.DefenderPower);
			}
		}
		else
		{
			Block(FString::Printf(TEXT("Accion de gobierno no soportada: %s"), *Request.ActionId));
		}
	}

	if (ValidateNation(Preview.NationIso))
	{
		const FWLPoliticalActionBudget Budget = GetPoliticalActionBudget(Preview.NationIso);
		Preview.ActionPointsRemaining = Budget.RemainingActionPoints;
		Preview.CooldownRemainingMonths = GetGovernmentActionCooldownRemaining(Preview.NationIso, Preview.ActionKey, Preview.TargetKey);
		if (Preview.CooldownRemainingMonths > 0)
		{
			Block(FString::Printf(TEXT("Cooldown activo: faltan %d meses."), Preview.CooldownRemainingMonths));
		}
		if (Preview.ActionPointCost > 0 && Budget.RemainingActionPoints < Preview.ActionPointCost)
		{
			Block(FString::Printf(TEXT("AP de gobierno insuficiente (%d/%d)."), Budget.RemainingActionPoints, Preview.ActionPointCost));
		}
		if (Preview.TreasuryCost > 0 && Tick)
		{
			const int64 Treasury = Tick->GetTreasury(Preview.NationIso);
			if (bAllowCreditSpend)
			{
				if (Treasury - Preview.TreasuryCost < -Tick->GetCreditLimit(Preview.NationIso))
				{
					Block(FString::Printf(TEXT("Credito insuficiente: requiere %lld."), static_cast<long long>(Preview.TreasuryCost)));
				}
			}
			else if (Treasury < Preview.TreasuryCost)
			{
				Block(FString::Printf(TEXT("Tesoro insuficiente (%lld requerido)."), static_cast<long long>(Preview.TreasuryCost)));
			}
		}
		if (Preview.PoliticalCapitalCost > 0 && Characters
			&& Characters->GetPoliticalCapital(Preview.NationIso) < Preview.PoliticalCapitalCost)
		{
			Block(FString::Printf(TEXT("Capital politico insuficiente (%d requerido)."), Preview.PoliticalCapitalCost));
		}
	}

	Preview.bCanExecute = Preview.BlockReason.IsEmpty();
	if (Preview.bCanExecute)
	{
		Preview.BlockReason = TEXT("Disponible.");
	}
	return Preview;
}

bool UWLPoliticalSubsystem::ExecuteGovernmentAction(const FWLGovernmentActionRequest& Request, FString& OutMessage)
{
	FWLPoliticalActionRequest PoliticalRequest;
	if (BuildPoliticalActionRequest(Request, PoliticalRequest))
	{
		return ExecutePoliticalAction(PoliticalRequest, OutMessage);
	}

	const FWLGovernmentActionPreview Preview = GetGovernmentActionPreview(Request);
	if (!Preview.bCanExecute)
	{
		OutMessage = Preview.BlockReason;
		return false;
	}

	if (!ExecuteGovernmentActionDirect(Request, OutMessage))
	{
		return false;
	}

	RecordGovernmentAction(Preview, OutMessage);
	AddGovernmentLogEntry(
		Preview.Category,
		Preview.NationIso,
		TEXT(""),
		GovernmentCommandTitle(Preview.ActionKey),
		OutMessage,
		TEXT("government_action"),
		Preview.ActionPointCost > 0 ? Preview.ActionPointCost * 8 : 3,
		Preview.Category == EWLGovernmentLogCategory::Diplomacy
			|| Preview.Category == EWLGovernmentLogCategory::Military
			|| Preview.Category == EWLGovernmentLogCategory::Intelligence,
		Preview.NationIso == GetPlayerNationIso());
	return true;
}

bool UWLPoliticalSubsystem::ExecuteGovernmentActionDirect(const FWLGovernmentActionRequest& Request, FString& OutMessage)
{
	TArray<FString> Parts;
	ParseGovernmentActionId(Request.ActionId, Parts);
	if (Parts.IsEmpty())
	{
		OutMessage = TEXT("Accion de gobierno vacia.");
		return false;
	}

	const FString Iso = NormalizeIso(Request.NationIso);
	const FString Verb = Parts[0].ToLower();
	const FString Arg1 = Parts.Num() > 1 ? Parts[1] : FString();
	const FString Arg2 = Parts.Num() > 2 ? Parts[2] : FString();
	const FString Arg3 = Parts.Num() > 3 ? Parts[3] : FString();

	UWLStrategicTickSubsystem* Tick = GetTick();
	UWLCharacterSubsystem* Characters = GetCharacters();
	UWLMilitarySubsystem* Military = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWLMilitarySubsystem>() : nullptr;

	if (Verb == TEXT("taxup") || Verb == TEXT("taxdown"))
	{
		if (!Tick)
		{
			OutMessage = TEXT("Economia no disponible.");
			return false;
		}
		const int32 Delta = Verb == TEXT("taxup") ? 5 : -5;
		const int32 Current = Tick->GetTaxRate(Iso);
		const int32 Next = Tick->SetTaxRate(Iso, Current + Delta);
		OutMessage = FString::Printf(TEXT("Tasa nacional ajustada: %d%% -> %d%%."), Current, Next);
		return Next != Current;
	}
	if (Verb == TEXT("tariffup") || Verb == TEXT("tariffdown"))
	{
		if (!Tick)
		{
			OutMessage = TEXT("Economia no disponible.");
			return false;
		}
		const int32 Delta = Verb == TEXT("tariffup") ? 5 : -5;
		return SetNationTariffRate(Iso, Tick->GetTariffRate(Iso) + Delta, OutMessage);
	}
	if (Verb == TEXT("bond"))
	{
		if (!Tick)
		{
			OutMessage = TEXT("Finanzas no disponibles.");
			return false;
		}
		const FWLFinancialProfile Profile = Tick->GetFinancialProfile(Iso);
		return Tick->IssueBond(Iso, SuggestedDebtPrincipal(Profile.AvailableCredit, 4), 24, OutMessage);
	}
	if (Verb == TEXT("imf"))
	{
		if (!Tick)
		{
			OutMessage = TEXT("Finanzas no disponibles.");
			return false;
		}
		const FWLFinancialProfile Profile = Tick->GetFinancialProfile(Iso);
		return Tick->RequestIMFProgram(Iso, SuggestedDebtPrincipal(Profile.AvailableCredit, 3), 36, OutMessage);
	}
	if (Verb == TEXT("default"))
	{
		return Tick && Tick->MarkDebtDefault(Iso, OutMessage);
	}
	if (Verb == TEXT("aid"))
	{
		return Tick && Tick->GrantForeignAid(Iso, Arg1, 1000, 12, OutMessage);
	}
	if (Verb == TEXT("fdi"))
	{
		return Tick && Tick->StartForeignInvestment(Iso, Arg1, Arg2, Arg3, 1500, 12, OutMessage);
	}
	if (Verb == TEXT("build"))
	{
		return Tick && Tick->BuildBuilding(Request.ContextId, Arg1, OutMessage);
	}
	if (Verb == TEXT("upgrade"))
	{
		return Tick && Tick->UpgradeBuilding(Request.ContextId, Arg1, OutMessage);
	}
	if (Verb == TEXT("creategeneral"))
	{
		FWLCharacter NewGeneral;
		return Characters && Characters->CreateGeneral(Iso, NewGeneral, OutMessage);
	}
	if (Verb == TEXT("promote"))
	{
		return Characters && Characters->PromoteGeneral(Arg1, OutMessage);
	}
	if (Verb == TEXT("retire"))
	{
		return Characters && Characters->RetireCharacter(Arg1, OutMessage);
	}
	if (Verb == TEXT("reward"))
	{
		return RewardGeneral(Iso, Arg1, OutMessage);
	}
	if (Verb == TEXT("purge"))
	{
		return PurgeCharacter(Iso, Arg1, OutMessage);
	}
	if (Verb == TEXT("appoint"))
	{
		if (!Characters)
		{
			OutMessage = TEXT("Personajes no disponibles.");
			return false;
		}
		const EWLMinisterOffice Office = static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1));
		TArray<FWLCharacter> Candidates = Characters->GetCharactersByRole(Iso, EWLCharacterRole::Minister);
		Candidates.RemoveAll([](const FWLCharacter& Candidate)
		{
			return !Candidate.bActive || Candidate.AssignedOffice != EWLMinisterOffice::None;
		});
		Candidates.Sort([Office](const FWLCharacter& A, const FWLCharacter& B)
		{
			const bool bAPreferred = A.PreferredOffice == Office;
			const bool bBPreferred = B.PreferredOffice == Office;
			if (bAPreferred != bBPreferred)
			{
				return bAPreferred;
			}
			return A.Skill > B.Skill;
		});
		if (Candidates.IsEmpty())
		{
			OutMessage = TEXT("No hay candidatos a ministro disponibles.");
			return false;
		}
		return Characters->AppointMinister(Iso, Office, Candidates[0].Id, OutMessage);
	}
	if (Verb == TEXT("appointc"))
	{
		return Characters && Characters->AppointMinister(
			Iso,
			static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1)),
			Arg2,
			OutMessage);
	}
	if (Verb == TEXT("dismiss"))
	{
		return Characters && Characters->DismissMinister(Iso, static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1)), OutMessage);
	}
	if (Verb == TEXT("hire"))
	{
		FWLCharacter NewMinister;
		return Characters && Characters->HireMinister(Iso, static_cast<EWLMinisterOffice>(FCString::Atoi(*Arg1)), NewMinister, OutMessage);
	}
	if (Verb == TEXT("war"))
	{
		return DeclareWar(Iso, Arg1, OutMessage);
	}
	if (Verb == TEXT("peace"))
	{
		return MakePeace(Iso, Arg1, OutMessage);
	}
	if (Verb == TEXT("treaty"))
	{
		return SignTreaty(Iso, Arg2, static_cast<EWLTreatyType>(FCString::Atoi(*Arg1)), OutMessage);
	}
	if (Verb == TEXT("breaktreaty"))
	{
		return BreakTreaty(Iso, Arg2, static_cast<EWLTreatyType>(FCString::Atoi(*Arg1)), OutMessage);
	}
	if (Verb == TEXT("spynet"))
	{
		return BuildSpyNetwork(Iso, Arg1, Request.AgentCharacterId, OutMessage);
	}
	if (Verb == TEXT("spy"))
	{
		return RunSpyOperation(Iso, Arg2, Request.AgentCharacterId, static_cast<EWLSpyOperationType>(FCString::Atoi(*Arg1)), OutMessage);
	}
	if (Verb == TEXT("reorg"))
	{
		return Military && Military->ReorganizeArmy(Arg1, 0, OutMessage);
	}
	if (Verb == TEXT("assigngen"))
	{
		return Characters && Characters->AssignGeneralToArmy(Arg1, Arg2, OutMessage);
	}
	if (Verb == TEXT("autoresolve"))
	{
		if (!Military)
		{
			OutMessage = TEXT("Sistema militar no disponible.");
			return false;
		}
		const EWLBattleResult Result = Military->AutoResolveBattle(Arg1, Arg2, OutMessage);
		return Result != EWLBattleResult::Invalid;
	}

	OutMessage = FString::Printf(TEXT("Accion de gobierno no soportada: %s"), *Request.ActionId);
	return false;
}
