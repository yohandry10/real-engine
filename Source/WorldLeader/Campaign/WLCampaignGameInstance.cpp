// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLCampaignGameInstance.h"
#include "Balance/WLBalanceSubsystem.h"
#include "Campaign/WLDataRegistry.h"
#include "Campaign/WLStrategicTickSubsystem.h"
#include "Characters/WLCharacterSubsystem.h"
#include "Military/WLMilitarySubsystem.h"
#include "Politics/WLPoliticalSubsystem.h"
#include "Save/WLLocalSaveGame.h"
#include "WorldLeader.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FString WLLocalCampaignSlot = TEXT("WorldLeader_LocalCampaign");
	constexpr int32 WLLocalCampaignUserIndex = 0;
	constexpr int32 WLLocalCampaignSaveVersion = 13;
}

UWLDataRegistry* UWLCampaignGameInstance::GetRegistry() const
{
	return GetSubsystem<UWLDataRegistry>();
}

UWLStrategicTickSubsystem* UWLCampaignGameInstance::GetTick() const
{
	return GetSubsystem<UWLStrategicTickSubsystem>();
}

UWLCharacterSubsystem* UWLCampaignGameInstance::GetCharacters() const
{
	return GetSubsystem<UWLCharacterSubsystem>();
}

UWLPoliticalSubsystem* UWLCampaignGameInstance::GetPolitics() const
{
	return GetSubsystem<UWLPoliticalSubsystem>();
}

bool UWLCampaignGameInstance::StartNewCampaign(const FString& NationIso)
{
	const UWLDataRegistry* Registry = GetRegistry();
	UWLStrategicTickSubsystem* Tick = GetTick();
	FWLNationData Nation;
	const FString NormalizedIso = NationIso.TrimStartAndEnd().ToUpper();
	if (!Registry || !Tick || !Registry->GetNation(NormalizedIso, Nation))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("StartNewCampaign: nacion no disponible: %s"), *NationIso);
		return false;
	}

	SelectedNationIso = Nation.Iso;
	bHasActiveCampaign = true;
	Tick->ResetCampaignState();
	if (UWLMilitarySubsystem* Military = GetMilitary())
	{
		Military->ResetMilitaryState();
	}
	if (UWLCharacterSubsystem* Characters = GetCharacters())
	{
		Characters->ResetCharacterState();
	}
	if (UWLPoliticalSubsystem* Politics = GetPolitics())
	{
		Politics->ResetPoliticalState();
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Campania iniciada con %s (%s)."), *Nation.Name, *Nation.Iso);
	// Instantanea economica al iniciar (incluye el upkeep militar de FE1.1) para diagnostico/verificacion.
	for (const TCHAR* SnapIso : { TEXT("CO"), TEXT("VE") })
	{
		UE_LOG(LogWorldLeader, Log,
			TEXT("[Economia %s] fuerza=%lld  upkeepMilitar/mes=%lld  balance/mes=%lld  tesoro=%lld"),
			SnapIso,
			static_cast<long long>(Tick->GetNationMilitaryStrength(SnapIso)),
			static_cast<long long>(Tick->GetNationMilitaryUpkeep(SnapIso)),
			static_cast<long long>(Tick->GetMonthlyBalance(SnapIso)),
			static_cast<long long>(Tick->GetTreasury(SnapIso)));
	}
	return true;
}

void UWLCampaignGameInstance::ResetCampaignFlow()
{
	bHasActiveCampaign = false;
	SelectedNationIso.Reset();
	if (UWLMilitarySubsystem* Military = GetMilitary())
	{
		Military->ResetMilitaryState();
	}
	if (UWLCharacterSubsystem* Characters = GetCharacters())
	{
		Characters->ResetCharacterState();
	}
	if (UWLPoliticalSubsystem* Politics = GetPolitics())
	{
		Politics->ResetPoliticalState();
	}
}

bool UWLCampaignGameInstance::GetSelectedNation(FWLNationData& OutNation) const
{
	const UWLDataRegistry* Registry = GetRegistry();
	return bHasActiveCampaign && Registry && Registry->GetNation(SelectedNationIso, OutNation);
}

bool UWLCampaignGameInstance::HasLocalCampaignSave() const
{
	return UGameplayStatics::DoesSaveGameExist(WLLocalCampaignSlot, WLLocalCampaignUserIndex);
}

bool UWLCampaignGameInstance::SaveLocalCampaign(FString& OutMessage) const
{
	if (!bHasActiveCampaign || SelectedNationIso.IsEmpty())
	{
		OutMessage = TEXT("No hay campania activa para guardar.");
		return false;
	}

	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	FWLNationData Nation;
	if (!Registry || !Tick || !Registry->GetNation(SelectedNationIso, Nation))
	{
		OutMessage = TEXT("No se pudo validar la nacion seleccionada.");
		return false;
	}

	UWLLocalSaveGame* Save = Cast<UWLLocalSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UWLLocalSaveGame::StaticClass()));
	if (!Save)
	{
		OutMessage = TEXT("No se pudo crear el objeto SaveGame.");
		return false;
	}

	Save->SaveVersion = WLLocalCampaignSaveVersion;
	Save->SelectedNationIso = Nation.Iso;
	if (const UWLBalanceSubsystem* Balance = GetSubsystem<UWLBalanceSubsystem>())
	{
		Save->AIDifficulty = Balance->GetAIDifficulty();
	}
	Tick->WriteSaveSnapshot(
		Save->CurrentYear,
		Save->CurrentMonth,
		Save->NationTreasuries,
		Save->ProvinceBuildings,
		Save->ProvinceStates,
		&Save->ActiveMarketShocks,
		&Save->FinancialInstruments,
		&Save->ForeignSupportStates);
	if (const UWLMilitarySubsystem* Military = GetMilitary())
	{
		Military->WriteSaveSnapshot(Save->Armies, Save->NextArmyNumber);
	}
	if (const UWLCharacterSubsystem* Characters = GetCharacters())
	{
		Characters->WriteSaveSnapshot(Save->Characters, Save->PoliticalCapital);
	}
	if (const UWLPoliticalSubsystem* Politics = GetPolitics())
	{
		Politics->WriteSaveSnapshot(
			Save->InternalPowerStates,
			Save->DiplomaticRelations,
			Save->IntelligenceNetworks,
			Save->PoliticalEvents,
			Save->CampaignOutcome);
		Politics->WriteGovernmentSaveSnapshot(
			Save->GovernmentAgendas,
			Save->MinistryPrograms,
			Save->CabinetDynamics,
			Save->InstitutionalPower,
			Save->PublicGroupSupport,
			Save->StateCapacity,
			Save->PoliticalMemory,
			Save->GovernmentAIPlans);
		Politics->WriteGovernmentP2SaveSnapshot(
			Save->ActivePolicyReforms,
			Save->EnactedPolicyReforms,
			Save->PoliticalParties,
			Save->ElectionStates,
			Save->CharacterPoliticalProfiles,
			Save->PatronageStates,
			Save->MediaStates,
			Save->RegionGovernors,
			Save->CrisisChains,
			Save->GovernmentCalibration);
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, WLLocalCampaignSlot, WLLocalCampaignUserIndex);
	OutMessage = bSaved
		? FString::Printf(TEXT("Campania guardada: %s %02d/%d."), *Nation.Iso, Save->CurrentMonth, Save->CurrentYear)
		: TEXT("SaveGameToSlot fallo.");
	return bSaved;
}

bool UWLCampaignGameInstance::LoadLocalCampaign(FString& OutMessage)
{
	if (!HasLocalCampaignSave())
	{
		OutMessage = TEXT("No hay save local de campania.");
		return false;
	}

	UWLLocalSaveGame* Save = Cast<UWLLocalSaveGame>(
		UGameplayStatics::LoadGameFromSlot(WLLocalCampaignSlot, WLLocalCampaignUserIndex));
	if (!Save)
	{
		OutMessage = TEXT("El save local no tiene el formato esperado.");
		return false;
	}
	if (Save->SaveVersion < 1 || Save->SaveVersion > WLLocalCampaignSaveVersion)
	{
		OutMessage = FString::Printf(TEXT("Version de save no soportada: %d."), Save->SaveVersion);
		return false;
	}

	const UWLDataRegistry* Registry = GetRegistry();
	UWLStrategicTickSubsystem* Tick = GetTick();
	FWLNationData Nation;
	if (!Registry || !Tick || !Registry->GetNation(Save->SelectedNationIso, Nation))
	{
		OutMessage = FString::Printf(TEXT("Nacion guardada no disponible: %s"), *Save->SelectedNationIso);
		return false;
	}

	if (UWLBalanceSubsystem* Balance = GetSubsystem<UWLBalanceSubsystem>())
	{
		Balance->SetAIDifficulty(Save->AIDifficulty);
	}

	FString RestoreMessage;
	if (!Tick->RestoreSaveSnapshot(
		Save->CurrentYear,
		Save->CurrentMonth,
		Save->NationTreasuries,
		Save->ProvinceBuildings,
		Save->ProvinceStates,
		Save->ActiveMarketShocks,
		Save->FinancialInstruments,
		Save->ForeignSupportStates,
		RestoreMessage))
	{
		OutMessage = RestoreMessage;
		return false;
	}

	FString MilitaryMessage;
	if (UWLMilitarySubsystem* Military = GetMilitary())
	{
		if (!Military->RestoreSaveSnapshot(Save->Armies, Save->NextArmyNumber, MilitaryMessage))
		{
			OutMessage = MilitaryMessage;
			return false;
		}
	}

	FString CharacterMessage;
	if (UWLCharacterSubsystem* Characters = GetCharacters())
	{
		if (!Characters->RestoreSaveSnapshot(Save->Characters, Save->PoliticalCapital, CharacterMessage))
		{
			OutMessage = CharacterMessage;
			return false;
		}
	}

	FString PoliticsMessage;
	if (UWLPoliticalSubsystem* Politics = GetPolitics())
	{
		if (!Politics->RestoreSaveSnapshot(
			Save->InternalPowerStates,
			Save->DiplomaticRelations,
			Save->IntelligenceNetworks,
			Save->PoliticalEvents,
			Save->CampaignOutcome,
			PoliticsMessage))
		{
			OutMessage = PoliticsMessage;
			return false;
		}
		if (!Politics->RestoreGovernmentSaveSnapshot(
			Save->GovernmentAgendas,
			Save->MinistryPrograms,
			Save->CabinetDynamics,
			Save->InstitutionalPower,
			Save->PublicGroupSupport,
			Save->StateCapacity,
			Save->PoliticalMemory,
			Save->GovernmentAIPlans,
			PoliticsMessage))
		{
			OutMessage = PoliticsMessage;
			return false;
		}
		if (!Politics->RestoreGovernmentP2SaveSnapshot(
			Save->ActivePolicyReforms,
			Save->EnactedPolicyReforms,
			Save->PoliticalParties,
			Save->ElectionStates,
			Save->CharacterPoliticalProfiles,
			Save->PatronageStates,
			Save->MediaStates,
			Save->RegionGovernors,
			Save->CrisisChains,
			Save->GovernmentCalibration,
			PoliticsMessage))
		{
			OutMessage = PoliticsMessage;
			return false;
		}
	}

	SelectedNationIso = Nation.Iso;
	bHasActiveCampaign = true;
	OutMessage = FString::Printf(TEXT("Campania cargada: %s (%s). %s %s %s %s"),
		*Nation.Name, *Nation.Iso, *RestoreMessage, *MilitaryMessage, *CharacterMessage, *PoliticsMessage);
	return true;
}

void UWLCampaignGameInstance::WLStartCampaign(const FString& NationIso)
{
	if (StartNewCampaign(NationIso))
	{
		WLPrintState();
	}
}

void UWLCampaignGameInstance::WLSave()
{
	FString Message;
	const bool bOk = SaveLocalCampaign(Message);
	UE_LOG(LogWorldLeader, Log, TEXT("WLSave: %s"), *Message);
	if (!bOk)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLSave fallo."));
	}
}

void UWLCampaignGameInstance::WLLoad()
{
	FString Message;
	const bool bOk = LoadLocalCampaign(Message);
	UE_LOG(LogWorldLeader, Log, TEXT("WLLoad: %s"), *Message);
	if (bOk)
	{
		WLPrintState();
	}
}

void UWLCampaignGameInstance::WLAdvanceMonth()
{
	if (UWLStrategicTickSubsystem* Tick = GetTick())
	{
		Tick->AdvanceMonth();
		if (UWLPoliticalSubsystem* Politics = GetPolitics())
		{
			Politics->ProcessPoliticalMonth();
		}
		WLPrintState();
	}
}

void UWLCampaignGameInstance::WLPrintState()
{
	const UWLDataRegistry* Registry = GetRegistry();
	const UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLPrintState: subsistemas no disponibles."));
		return;
	}

	UE_LOG(LogWorldLeader, Log, TEXT("=== World Leader | %02d/%d ==="),
		Tick->GetCurrentMonth(), Tick->GetCurrentYear());
	UE_LOG(LogWorldLeader, Log, TEXT("Provincias: %d | Naciones: %d"),
		Registry->GetProvinceCount(), Registry->GetNationCount());

	for (const FWLNationData& Nation : Registry->GetAllNations())
	{
		UE_LOG(LogWorldLeader, Log, TEXT("  %s (%s): tesoro %lld | balance/mes %lld"),
			*Nation.Name, *Nation.Iso,
			Tick->GetTreasury(Nation.Iso), Tick->GetMonthlyBalance(Nation.Iso));
	}
}

void UWLCampaignGameInstance::WLBuild(const FString& ProvinceId, const FString& BuildingId)
{
	if (!bHasActiveCampaign || SelectedNationIso.IsEmpty())
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLBuild: no hay campania activa."));
		return;
	}

	const UWLDataRegistry* Registry = GetRegistry();
	UWLStrategicTickSubsystem* Tick = GetTick();
	if (!Registry || !Tick)
	{
		return;
	}

	FWLProvinceData Province;
	if (!Registry->GetProvince(ProvinceId, Province))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLBuild: provincia desconocida %s"), *ProvinceId);
		return;
	}
	const FString ControllerIso = Tick->GetProvinceControllerIso(Province.Id);
	if (ControllerIso != SelectedNationIso)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLBuild: %s esta controlada por %s, no por la nacion activa %s."),
			*Province.Id, *ControllerIso, *SelectedNationIso);
		return;
	}

	FString Message;
	const bool bOk = Tick->BuildBuilding(Province.Id, BuildingId, Message);
	UE_LOG(LogWorldLeader, Log, TEXT("WLBuild: %s"), *Message);
	if (bOk)
	{
		WLPrintState();
	}
}

UWLMilitarySubsystem* UWLCampaignGameInstance::GetMilitary() const
{
	return GetSubsystem<UWLMilitarySubsystem>();
}

void UWLCampaignGameInstance::WLSpawnArmy(const FString& Nation, const FString& ProvinceId, const FString& UnitId, int32 Count)
{
	if (UWLMilitarySubsystem* Mil = GetMilitary())
	{
		const FString Id = Mil->CreateArmy(Nation, ProvinceId, UnitId, Count, TEXT(""));
		if (Id.IsEmpty())
		{
			UE_LOG(LogWorldLeader, Warning, TEXT("WLSpawnArmy: no se pudo crear (revisa nacion/provincia/unidad)."));
		}
	}
}

void UWLCampaignGameInstance::WLArmies()
{
	const UWLMilitarySubsystem* Mil = GetMilitary();
	if (!Mil)
	{
		return;
	}

	const TArray<FWLArmy> Armies = Mil->GetArmies();
	UE_LOG(LogWorldLeader, Log, TEXT("=== Ejercitos (%d) ==="), Armies.Num());
	for (const FWLArmy& Army : Armies)
	{
		UE_LOG(LogWorldLeader, Log, TEXT("  %s [%s] en %s: %d unidades (atk %d / def %d) - Gen. %s"),
			*Army.Id, *Army.OwnerIso, *Army.ProvinceId, Army.Units.Num(),
			Mil->GetArmyAttack(Army.Id), Mil->GetArmyDefense(Army.Id), *Army.General);
	}
}

void UWLCampaignGameInstance::WLMove(const FString& ArmyId, const FString& ProvinceId)
{
	if (UWLMilitarySubsystem* Mil = GetMilitary())
	{
		FString Message;
		Mil->MoveArmy(ArmyId, ProvinceId, Message);
		UE_LOG(LogWorldLeader, Log, TEXT("WLMove: %s"), *Message);
	}
}

void UWLCampaignGameInstance::WLBattle(const FString& AttackerId, const FString& DefenderId)
{
	if (UWLMilitarySubsystem* Mil = GetMilitary())
	{
		FString Report;
		Mil->AutoResolveBattle(AttackerId, DefenderId, Report);
		UE_LOG(LogWorldLeader, Log, TEXT("WLBattle: %s"), *Report);
	}
}
