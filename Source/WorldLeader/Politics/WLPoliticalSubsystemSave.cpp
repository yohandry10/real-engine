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

void UWLPoliticalSubsystem::WriteGovernmentSaveSnapshot(
	TArray<FWLGovernmentAgendaState>& OutAgendas,
	TArray<FWLMinistryProgramState>& OutPrograms,
	TArray<FWLCabinetDynamicsState>& OutCabinetDynamics,
	TArray<FWLInstitutionalPowerState>& OutInstitutions,
	TArray<FWLPublicGroupSupportState>& OutPublicGroups,
	TArray<FWLStateCapacityState>& OutStateCapacity,
	TArray<FWLPoliticalMemoryRecord>& OutMemory,
	TArray<FWLPoliticalAIPlanState>& OutAIPlans) const
{
	GovernmentAgendaByNation.GenerateValueArray(OutAgendas);
	OutPrograms = ActiveMinistryPrograms;
	CabinetDynamicsByNation.GenerateValueArray(OutCabinetDynamics);
	InstitutionalPowerByNation.GenerateValueArray(OutInstitutions);
	PublicGroupSupportByKey.GenerateValueArray(OutPublicGroups);
	StateCapacityByNation.GenerateValueArray(OutStateCapacity);
	PoliticalMemoryByKey.GenerateValueArray(OutMemory);
	GovernmentAIPlanByNation.GenerateValueArray(OutAIPlans);

	OutAgendas.Sort([](const FWLGovernmentAgendaState& A, const FWLGovernmentAgendaState& B) { return A.NationIso < B.NationIso; });
	OutPrograms.Sort([](const FWLMinistryProgramState& A, const FWLMinistryProgramState& B)
	{
		return A.NationIso == B.NationIso ? A.ProgramId < B.ProgramId : A.NationIso < B.NationIso;
	});
	OutCabinetDynamics.Sort([](const FWLCabinetDynamicsState& A, const FWLCabinetDynamicsState& B) { return A.NationIso < B.NationIso; });
	OutInstitutions.Sort([](const FWLInstitutionalPowerState& A, const FWLInstitutionalPowerState& B) { return A.NationIso < B.NationIso; });
	OutPublicGroups.Sort([](const FWLPublicGroupSupportState& A, const FWLPublicGroupSupportState& B)
	{
		return A.NationIso == B.NationIso
			? static_cast<int32>(A.Group) < static_cast<int32>(B.Group)
			: A.NationIso < B.NationIso;
	});
	OutStateCapacity.Sort([](const FWLStateCapacityState& A, const FWLStateCapacityState& B) { return A.NationIso < B.NationIso; });
	OutMemory.Sort([](const FWLPoliticalMemoryRecord& A, const FWLPoliticalMemoryRecord& B)
	{
		return A.NationIso == B.NationIso ? A.MemoryKey < B.MemoryKey : A.NationIso < B.NationIso;
	});
	OutAIPlans.Sort([](const FWLPoliticalAIPlanState& A, const FWLPoliticalAIPlanState& B) { return A.NationIso < B.NationIso; });
}

void UWLPoliticalSubsystem::WriteGovernmentP2SaveSnapshot(
	TArray<FWLActiveReformState>& OutReforms,
	TArray<FWLEnactedPolicyReformState>& OutEnactedReforms,
	TArray<FWLPartyState>& OutParties,
	TArray<FWLElectionState>& OutElections,
	TArray<FWLCharacterPoliticalProfile>& OutCharacterProfiles,
	TArray<FWLPatronageState>& OutPatronage,
	TArray<FWLMediaPublicOpinionState>& OutMedia,
	TArray<FWLRegionGovernorState>& OutRegions,
	TArray<FWLCrisisChainState>& OutCrisisChains,
	TArray<FWLGovernmentCalibrationState>& OutCalibration,
	TArray<FWLPoliticalActionRecord>& OutPoliticalActionRecords,
	TArray<FWLGovernmentLogEntry>* OutGovernmentLogEntries) const
{
	OutReforms = ActivePolicyReforms;
	OutEnactedReforms = EnactedPolicyReforms;
	OutParties = PoliticalParties;
	ElectionStateByNation.GenerateValueArray(OutElections);
	CharacterPoliticalProfilesById.GenerateValueArray(OutCharacterProfiles);
	PatronageStateByNation.GenerateValueArray(OutPatronage);
	MediaStateByNation.GenerateValueArray(OutMedia);
	OutRegions = RegionGovernors;
	OutCrisisChains = CrisisChains;
	GovernmentCalibrationByNation.GenerateValueArray(OutCalibration);
	OutPoliticalActionRecords = PoliticalActionRecords;
	if (OutGovernmentLogEntries)
	{
		*OutGovernmentLogEntries = GovernmentLogEntries;
	}

	OutReforms.Sort([](const FWLActiveReformState& A, const FWLActiveReformState& B)
	{
		return A.NationIso == B.NationIso ? A.ReformId < B.ReformId : A.NationIso < B.NationIso;
	});
	OutEnactedReforms.Sort([](const FWLEnactedPolicyReformState& A, const FWLEnactedPolicyReformState& B)
	{
		return A.NationIso == B.NationIso ? A.ReformId < B.ReformId : A.NationIso < B.NationIso;
	});
	OutParties.Sort([](const FWLPartyState& A, const FWLPartyState& B)
	{
		return A.NationIso == B.NationIso ? A.PartyId < B.PartyId : A.NationIso < B.NationIso;
	});
	OutElections.Sort([](const FWLElectionState& A, const FWLElectionState& B) { return A.NationIso < B.NationIso; });
	OutCharacterProfiles.Sort([](const FWLCharacterPoliticalProfile& A, const FWLCharacterPoliticalProfile& B)
	{
		return A.NationIso == B.NationIso ? A.CharacterId < B.CharacterId : A.NationIso < B.NationIso;
	});
	OutPatronage.Sort([](const FWLPatronageState& A, const FWLPatronageState& B) { return A.NationIso < B.NationIso; });
	OutMedia.Sort([](const FWLMediaPublicOpinionState& A, const FWLMediaPublicOpinionState& B) { return A.NationIso < B.NationIso; });
	OutRegions.Sort([](const FWLRegionGovernorState& A, const FWLRegionGovernorState& B)
	{
		return A.NationIso == B.NationIso ? A.RegionId < B.RegionId : A.NationIso < B.NationIso;
	});
	OutCrisisChains.Sort([](const FWLCrisisChainState& A, const FWLCrisisChainState& B)
	{
		return A.NationIso == B.NationIso ? A.CrisisId < B.CrisisId : A.NationIso < B.NationIso;
	});
	OutCalibration.Sort([](const FWLGovernmentCalibrationState& A, const FWLGovernmentCalibrationState& B)
	{
		return A.NationIso < B.NationIso;
	});
	OutPoliticalActionRecords.Sort([](const FWLPoliticalActionRecord& A, const FWLPoliticalActionRecord& B)
	{
		if (A.NationIso != B.NationIso)
		{
			return A.NationIso < B.NationIso;
		}
		if (A.MonthKey != B.MonthKey)
		{
			return A.MonthKey < B.MonthKey;
		}
		return A.ActionKey == B.ActionKey ? A.TargetKey < B.TargetKey : A.ActionKey < B.ActionKey;
	});
	if (OutGovernmentLogEntries)
	{
		OutGovernmentLogEntries->Sort([](const FWLGovernmentLogEntry& A, const FWLGovernmentLogEntry& B)
		{
			if (A.Year != B.Year)
			{
				return A.Year < B.Year;
			}
			if (A.Month != B.Month)
			{
				return A.Month < B.Month;
			}
			if (A.Day != B.Day)
			{
				return A.Day < B.Day;
			}
			return A.EntryId < B.EntryId;
		});
	}
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
	EventQueue.Reset();
	for (FWLPoliticalEventInstance Event : SavedEvents)
	{
		Event.NationIso = NormalizeIso(Event.NationIso);
		Event.TargetIso = NormalizeIso(Event.TargetIso);
		if (!ValidateNation(Event.NationIso))
		{
			continue;
		}
		if (!Event.TargetIso.IsEmpty()
			&& (!ValidateNation(Event.TargetIso) || Event.TargetIso == Event.NationIso))
		{
			continue;
		}
		if (Event.InstanceId.IsEmpty() || Event.EventId.IsEmpty() || Event.Options.IsEmpty())
		{
			continue;
		}
		EventQueue.Add(MoveTemp(Event));
	}
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

bool UWLPoliticalSubsystem::RestoreGovernmentP2SaveSnapshot(
	const TArray<FWLActiveReformState>& SavedReforms,
	const TArray<FWLEnactedPolicyReformState>& SavedEnactedReforms,
	const TArray<FWLPartyState>& SavedParties,
	const TArray<FWLElectionState>& SavedElections,
	const TArray<FWLCharacterPoliticalProfile>& SavedCharacterProfiles,
	const TArray<FWLPatronageState>& SavedPatronage,
	const TArray<FWLMediaPublicOpinionState>& SavedMedia,
	const TArray<FWLRegionGovernorState>& SavedRegions,
	const TArray<FWLCrisisChainState>& SavedCrisisChains,
	const TArray<FWLGovernmentCalibrationState>& SavedCalibration,
	const TArray<FWLPoliticalActionRecord>& SavedPoliticalActionRecords,
	FString& OutMessage,
	const TArray<FWLGovernmentLogEntry>* SavedGovernmentLogEntries)
{
	ActivePolicyReforms.Reset();
	EnactedPolicyReforms.Reset();
	TSet<FString> RestoredActiveReformKeys;
	for (FWLActiveReformState Reform : SavedReforms)
	{
		Reform.NationIso = NormalizeIso(Reform.NationIso);
		FWLPolicyReformDefinition Definition;
		if (!ValidateNation(Reform.NationIso)
			|| !GetPolicyReformDefinition(Reform.ReformId, Definition)
			|| Reform.MonthsRemaining <= 0)
		{
			continue;
		}
		Reform.ReformId = Definition.ReformId;
		Reform.Name = Definition.Name;
		Reform.Area = Definition.Area;
		Reform.MonthsRemaining = FMath::Max(0, Reform.MonthsRemaining);
		Reform.ImplementationProgress = ClampPercent(Reform.ImplementationProgress);
		Reform.Backlash = ClampPercent(Reform.Backlash);
		const FString ReformKey = Reform.NationIso + TEXT("|") + Reform.ReformId;
		if (RestoredActiveReformKeys.Contains(ReformKey))
		{
			continue;
		}
		RestoredActiveReformKeys.Add(ReformKey);
		ActivePolicyReforms.Add(MoveTemp(Reform));
	}
	for (FWLEnactedPolicyReformState Reform : SavedEnactedReforms)
	{
		Reform.NationIso = NormalizeIso(Reform.NationIso);
		Reform.ReformId = Reform.ReformId.TrimStartAndEnd().ToLower();
		FWLPolicyReformDefinition Definition;
		if (!ValidateNation(Reform.NationIso)
			|| !GetPolicyReformDefinition(Reform.ReformId, Definition)
			|| HasEnactedPolicyReform(Reform.NationIso, Reform.ReformId))
		{
			continue;
		}
		Reform.ReformId = Definition.ReformId;
		Reform.Name = Definition.Name;
		Reform.Area = Definition.Area;
		Reform.MonthsSinceEnacted = FMath::Max(0, Reform.MonthsSinceEnacted);
		EnactedPolicyReforms.Add(MoveTemp(Reform));
	}
	ActivePolicyReforms.RemoveAll([this](const FWLActiveReformState& Reform)
	{
		return HasEnactedPolicyReform(Reform.NationIso, Reform.ReformId);
	});

	if (!SavedParties.IsEmpty())
	{
		PoliticalParties.Reset();
		TSet<FString> RestoredPartyKeys;
		for (FWLPartyState Party : SavedParties)
		{
			Party.NationIso = NormalizeIso(Party.NationIso);
			Party.PartyId = Party.PartyId.TrimStartAndEnd().ToLower();
			const FString Key = PartyKey(Party.NationIso, Party.PartyId);
			if (!ValidateNation(Party.NationIso) || Party.PartyId.IsEmpty() || RestoredPartyKeys.Contains(Key))
			{
				continue;
			}
			RestoredPartyKeys.Add(Key);
			Party.Seats = FMath::Clamp(Party.Seats, 0, 100);
			Party.Discipline = ClampPercent(Party.Discipline);
			Party.LoyaltyToGovernment = ClampPercent(Party.LoyaltyToGovernment);
			Party.Corruption = ClampPercent(Party.Corruption);
			Party.MonthsSinceInternalElection = FMath::Max(0, Party.MonthsSinceInternalElection);
			Party.LeaderCharacterId = NormalizeCharacterId(Party.LeaderCharacterId);
			PoliticalParties.Add(MoveTemp(Party));
		}
	}
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			SeedPoliticalPartiesForNation(Nation.Iso);
		}
	}

	if (!SavedElections.IsEmpty())
	{
		ElectionStateByNation.Reset();
		for (FWLElectionState Election : SavedElections)
		{
			Election.NationIso = NormalizeIso(Election.NationIso);
			if (!ValidateNation(Election.NationIso))
			{
				continue;
			}
			Election.MonthsToElection = FMath::Clamp(Election.MonthsToElection, 0, ElectionCycleMonths);
			Election.IncumbentApproval = ClampPercent(Election.IncumbentApproval);
			Election.PollingGovernment = ClampPercent(Election.PollingGovernment);
			Election.PollingOpposition = ClampPercent(Election.PollingOpposition);
			Election.CampaignIntensity = ClampPercent(Election.CampaignIntensity);
			Election.Legitimacy = ClampPercent(Election.Legitimacy);
			Election.FraudRisk = ClampPercent(Election.FraudRisk);
			Election.AbstentionRisk = ClampPercent(Election.AbstentionRisk);
			Election.ConsecutiveTermsWon = FMath::Clamp(Election.ConsecutiveTermsWon, 0, 8);
			Election.CampaignPromiseReformId = Election.CampaignPromiseReformId.TrimStartAndEnd().ToLower();
			ElectionStateByNation.Add(Election.NationIso, MoveTemp(Election));
		}
	}

	if (!SavedCharacterProfiles.IsEmpty())
	{
		CharacterPoliticalProfilesById.Reset();
		for (FWLCharacterPoliticalProfile Profile : SavedCharacterProfiles)
		{
			Profile.NationIso = NormalizeIso(Profile.NationIso);
			Profile.CharacterId = NormalizeCharacterId(Profile.CharacterId);
			if (!ValidateNation(Profile.NationIso) || Profile.CharacterId.IsEmpty())
			{
				continue;
			}
			Profile.PresidentialAmbition = ClampPercent(Profile.PresidentialAmbition);
			Profile.PersonalCorruption = ClampPercent(Profile.PersonalCorruption);
			Profile.ScandalHeat = ClampPercent(Profile.ScandalHeat);
			Profile.SuccessionScore = ClampPercent(Profile.SuccessionScore);
			Profile.PatronCharacterId = NormalizeCharacterId(Profile.PatronCharacterId);
			for (FString& RivalId : Profile.RivalCharacterIds)
			{
				RivalId = NormalizeCharacterId(RivalId);
			}
			for (FString& AllyId : Profile.AllyCharacterIds)
			{
				AllyId = NormalizeCharacterId(AllyId);
			}
			CharacterPoliticalProfilesById.Add(Profile.CharacterId, MoveTemp(Profile));
		}
	}
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			SeedCharacterProfilesForNation(Nation.Iso);
		}
	}

	if (!SavedPatronage.IsEmpty())
	{
		PatronageStateByNation.Reset();
		for (FWLPatronageState Patronage : SavedPatronage)
		{
			Patronage.NationIso = NormalizeIso(Patronage.NationIso);
			if (!ValidateNation(Patronage.NationIso))
			{
				continue;
			}
			Patronage.PatronagePower = ClampPercent(Patronage.PatronagePower);
			Patronage.ClientelistPressure = ClampPercent(Patronage.ClientelistPressure);
			Patronage.ContractCorruption = ClampPercent(Patronage.ContractCorruption);
			Patronage.RegionalMachines = ClampPercent(Patronage.RegionalMachines);
			Patronage.ConcessionBacklash = ClampPercent(Patronage.ConcessionBacklash);
			PatronageStateByNation.Add(Patronage.NationIso, MoveTemp(Patronage));
		}
	}

	if (!SavedMedia.IsEmpty())
	{
		MediaStateByNation.Reset();
		for (FWLMediaPublicOpinionState Media : SavedMedia)
		{
			Media.NationIso = NormalizeIso(Media.NationIso);
			if (!ValidateNation(Media.NationIso))
			{
				continue;
			}
			Media.PressFreedom = ClampPercent(Media.PressFreedom);
			Media.MediaControl = ClampPercent(Media.MediaControl);
			Media.PresidentialApproval = ClampPercent(Media.PresidentialApproval);
			Media.PropagandaReach = ClampPercent(Media.PropagandaReach);
			Media.CensorshipBacklash = ClampPercent(Media.CensorshipBacklash);
			Media.FakeNewsPressure = ClampPercent(Media.FakeNewsPressure);
			Media.MediaCrisisRisk = ClampPercent(Media.MediaCrisisRisk);
			MediaStateByNation.Add(Media.NationIso, MoveTemp(Media));
		}
	}

	if (!SavedRegions.IsEmpty())
	{
		RegionGovernors.Reset();
		TSet<FString> RestoredRegionKeys;
		for (FWLRegionGovernorState Region : SavedRegions)
		{
			Region.NationIso = NormalizeIso(Region.NationIso);
			Region.RegionId = Region.RegionId.TrimStartAndEnd();
			const FString RegionKey = Region.NationIso + TEXT("|") + Region.RegionId;
			if (!ValidateNation(Region.NationIso) || Region.RegionId.IsEmpty() || RestoredRegionKeys.Contains(RegionKey))
			{
				continue;
			}
			RestoredRegionKeys.Add(RegionKey);
			Region.Obedience = ClampPercent(Region.Obedience);
			Region.Autonomy = ClampPercent(Region.Autonomy);
			Region.ProtestRisk = ClampPercent(Region.ProtestRisk);
			Region.CenterControl = ClampPercent(Region.CenterControl);
			Region.InvestmentLevel = ClampPercent(Region.InvestmentLevel);
			Region.SecessionRisk = ClampPercent(Region.SecessionRisk);
			Region.RebellionRisk = ClampPercent(Region.RebellionRisk);
			RegionGovernors.Add(MoveTemp(Region));
		}
	}
	if (const UWLDataRegistry* Registry = GetRegistry())
	{
		for (const FWLNationData& Nation : Registry->GetAllNations())
		{
			SeedRegionsForNation(Nation.Iso);
		}
	}

	CrisisChains.Reset();
	for (FWLCrisisChainState Crisis : SavedCrisisChains)
	{
		Crisis.NationIso = NormalizeIso(Crisis.NationIso);
		if (!ValidateNation(Crisis.NationIso))
		{
			continue;
		}
		Crisis.CrisisId = Crisis.CrisisId.TrimStartAndEnd();
		if (Crisis.CrisisId.IsEmpty())
		{
			Crisis.CrisisId = CrisisChainKey(Crisis.NationIso, Crisis.Type);
		}
		Crisis.Stage = FMath::Clamp(Crisis.Stage, 1, 5);
		Crisis.Intensity = ClampPercent(Crisis.Intensity);
		Crisis.MonthsActive = FMath::Max(0, Crisis.MonthsActive);
		CrisisChains.Add(MoveTemp(Crisis));
	}

	if (!SavedCalibration.IsEmpty())
	{
		GovernmentCalibrationByNation.Reset();
		for (FWLGovernmentCalibrationState Calibration : SavedCalibration)
		{
			Calibration.NationIso = NormalizeIso(Calibration.NationIso);
			if (!ValidateNation(Calibration.NationIso))
			{
				continue;
			}
			Calibration.DebtVsGrowthPressure = ClampPercent(Calibration.DebtVsGrowthPressure);
			Calibration.RepressionLegitimacyPressure = ClampPercent(Calibration.RepressionLegitimacyPressure);
			Calibration.SubsidyInflationPressure = ClampPercent(Calibration.SubsidyInflationPressure);
			Calibration.ReformGridlockPressure = ClampPercent(Calibration.ReformGridlockPressure);
			Calibration.CivilMilitaryTension = ClampPercent(Calibration.CivilMilitaryTension);
			Calibration.SovereigntyAlliancePressure = ClampPercent(Calibration.SovereigntyAlliancePressure);
			Calibration.MonthsObserved = FMath::Max(0, Calibration.MonthsObserved);
			GovernmentCalibrationByNation.Add(Calibration.NationIso, MoveTemp(Calibration));
		}
	}

	PoliticalActionRecords.Reset();
	for (FWLPoliticalActionRecord Record : SavedPoliticalActionRecords)
	{
		Record.NationIso = NormalizeIso(Record.NationIso);
		Record.ActionKey = Record.ActionKey.TrimStartAndEnd().ToLower();
		Record.TargetKey = Record.TargetKey.TrimStartAndEnd().ToLower();
		if (!ValidateNation(Record.NationIso)
			|| Record.ActionKey.IsEmpty()
			|| Record.MonthKey < 0
			|| Record.ActionPointCost < 0)
		{
			continue;
		}
		Record.Year = FMath::Max(0, Record.Year);
		Record.Month = FMath::Clamp(Record.Month, 0, 12);
		Record.Day = FMath::Clamp(Record.Day, 1, 31);
		Record.ActionPointCost = FMath::Clamp(Record.ActionPointCost, 0, MaxPoliticalActionPoints);
		Record.CooldownMonths = FMath::Clamp(Record.CooldownMonths, 0, 120);
		PoliticalActionRecords.Add(MoveTemp(Record));
	}

	GovernmentLogEntries.Reset();
	NextGovernmentLogNumber = 1;
	if (SavedGovernmentLogEntries)
	{
		for (FWLGovernmentLogEntry Entry : *SavedGovernmentLogEntries)
		{
			Entry.NationIso = NormalizeIso(Entry.NationIso);
			Entry.TargetIso = NormalizeIso(Entry.TargetIso);
			if ((!Entry.NationIso.IsEmpty() && !ValidateNation(Entry.NationIso))
				|| (!Entry.TargetIso.IsEmpty() && !ValidateNation(Entry.TargetIso))
				|| (Entry.Title.TrimStartAndEnd().IsEmpty() && Entry.Body.TrimStartAndEnd().IsEmpty()))
			{
				continue;
			}
			Entry.Title = Entry.Title.TrimStartAndEnd();
			Entry.Body = Entry.Body.TrimStartAndEnd();
			Entry.Source = Entry.Source.TrimStartAndEnd();
			Entry.Severity = FMath::Clamp(Entry.Severity, 0, 100);
			Entry.Year = FMath::Max(0, Entry.Year);
			Entry.Month = FMath::Clamp(Entry.Month, 0, 12);
			Entry.Day = FMath::Clamp(Entry.Day, 1, 31);
			Entry.MonthKey = Entry.MonthKey > 0 ? Entry.MonthKey : Entry.Year * 12 + Entry.Month;
			if (Entry.EntryId.IsEmpty())
			{
				Entry.EntryId = FString::Printf(TEXT("LOG-%05d"), NextGovernmentLogNumber++);
			}
			if (Entry.EntryId.StartsWith(TEXT("LOG-")))
			{
				NextGovernmentLogNumber = FMath::Max(NextGovernmentLogNumber, FCString::Atoi(*Entry.EntryId.Mid(4)) + 1);
			}
			GovernmentLogEntries.Add(MoveTemp(Entry));
		}
		GovernmentLogEntries.Sort([](const FWLGovernmentLogEntry& A, const FWLGovernmentLogEntry& B)
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
		if (GovernmentLogEntries.Num() > MaxGovernmentLogEntries)
		{
			GovernmentLogEntries.SetNum(MaxGovernmentLogEntries);
		}
		RebuildNewsLogFromGovernmentLog();
	}

	OutMessage = FString::Printf(
		TEXT("Gobierno P2 restaurado: %d reformas activas, %d reformas consolidadas, %d partidos, %d elecciones, %d regiones, %d crisis, %d acciones politicas, %d registros."),
		ActivePolicyReforms.Num(),
		EnactedPolicyReforms.Num(),
		PoliticalParties.Num(),
		ElectionStateByNation.Num(),
		RegionGovernors.Num(),
		CrisisChains.Num(),
		PoliticalActionRecords.Num(),
		GovernmentLogEntries.Num());
	return true;
}

bool UWLPoliticalSubsystem::RestoreGovernmentSaveSnapshot(
	const TArray<FWLGovernmentAgendaState>& SavedAgendas,
	const TArray<FWLMinistryProgramState>& SavedPrograms,
	const TArray<FWLCabinetDynamicsState>& SavedCabinetDynamics,
	const TArray<FWLInstitutionalPowerState>& SavedInstitutions,
	const TArray<FWLPublicGroupSupportState>& SavedPublicGroups,
	const TArray<FWLStateCapacityState>& SavedStateCapacity,
	const TArray<FWLPoliticalMemoryRecord>& SavedMemory,
	const TArray<FWLPoliticalAIPlanState>& SavedAIPlans,
	FString& OutMessage)
{
	SeedGovernmentState();

	for (FWLGovernmentAgendaState Agenda : SavedAgendas)
	{
		const FString Iso = NormalizeIso(Agenda.NationIso);
		if (!ValidateNation(Iso) || Agenda.Priorities.IsEmpty())
		{
			continue;
		}
		if (Agenda.Priorities.Num() > MaxAgendaPriorities)
		{
			Agenda.Priorities.SetNum(MaxAgendaPriorities);
		}
		Agenda.NationIso = Iso;
		Agenda.MonthsActive = FMath::Max(0, Agenda.MonthsActive);
		GovernmentAgendaByNation.Add(Iso, MoveTemp(Agenda));
	}

	ActiveMinistryPrograms.Reset();
	for (FWLMinistryProgramState Program : SavedPrograms)
	{
		Program.NationIso = NormalizeIso(Program.NationIso);
		FWLMinistryProgramDefinition Definition;
		if (!ValidateNation(Program.NationIso)
			|| !GetMinistryProgramDefinition(Program.ProgramId, Definition)
			|| Program.RemainingMonths <= 0)
		{
			continue;
		}
		Program.Name = Definition.Name;
		Program.Office = Definition.Office;
		Program.Progress = ClampPercent(Program.Progress);
		ActiveMinistryPrograms.Add(MoveTemp(Program));
	}

	for (FWLCabinetDynamicsState State : SavedCabinetDynamics)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.RivalryPressure = ClampPercent(State.RivalryPressure);
			State.Factionalism = ClampPercent(State.Factionalism);
			State.ScandalRisk = ClampPercent(State.ScandalRisk);
			State.SabotageRisk = ClampPercent(State.SabotageRisk);
			State.ResignationRisk = ClampPercent(State.ResignationRisk);
			CabinetDynamicsByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLInstitutionalPowerState State : SavedInstitutions)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.RulingCoalitionSupport = ClampPercent(State.RulingCoalitionSupport);
			State.LegislativeOpposition = ClampPercent(State.LegislativeOpposition);
			State.ReformCost = FMath::Clamp(State.ReformCost, 0, 100);
			State.GridlockRisk = ClampPercent(State.GridlockRisk);
			InstitutionalPowerByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLPublicGroupSupportState State : SavedPublicGroups)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.Support = ClampPercent(State.Support);
			State.Pressure = ClampPercent(State.Pressure);
			PublicGroupSupportByKey.Add(PublicGroupKey(Iso, State.Group), MoveTemp(State));
		}
	}
	for (FWLStateCapacityState State : SavedStateCapacity)
	{
		const FString Iso = NormalizeIso(State.NationIso);
		if (ValidateNation(Iso))
		{
			State.NationIso = Iso;
			State.Bureaucracy = ClampPercent(State.Bureaucracy);
			State.Corruption = ClampPercent(State.Corruption);
			State.AdministrativeEfficiency = ClampPercent(State.AdministrativeEfficiency);
			State.CentralAuthority = ClampPercent(State.CentralAuthority);
			State.PolicyFailureRisk = ClampPercent(State.PolicyFailureRisk);
			StateCapacityByNation.Add(Iso, MoveTemp(State));
		}
	}
	for (FWLPoliticalMemoryRecord Memory : SavedMemory)
	{
		const FString Iso = NormalizeIso(Memory.NationIso);
		Memory.MemoryKey = Memory.MemoryKey.TrimStartAndEnd().ToLower();
		if (ValidateNation(Iso) && !Memory.MemoryKey.IsEmpty() && Memory.MonthsRemaining > 0 && Memory.Value != 0)
		{
			Memory.NationIso = Iso;
			Memory.Value = FMath::Clamp(Memory.Value, -100, 100);
			Memory.MonthsRemaining = FMath::Max(0, Memory.MonthsRemaining);
			PoliticalMemoryByKey.Add(PoliticalMemoryKey(Iso, Memory.MemoryKey), MoveTemp(Memory));
		}
	}
	for (FWLPoliticalAIPlanState Plan : SavedAIPlans)
	{
		const FString Iso = NormalizeIso(Plan.NationIso);
		if (ValidateNation(Iso))
		{
			Plan.NationIso = Iso;
			Plan.TargetIso = NormalizeIso(Plan.TargetIso);
			if (!Plan.TargetIso.IsEmpty() && !ValidateNation(Plan.TargetIso))
			{
				Plan.TargetIso.Reset();
			}
			Plan.MonthsOnPlan = FMath::Max(0, Plan.MonthsOnPlan);
			GovernmentAIPlanByNation.Add(Iso, MoveTemp(Plan));
		}
	}

	OutMessage = FString::Printf(
		TEXT("Gobierno restaurado: %d agendas, %d programas, %d grupos, %d memorias."),
		GovernmentAgendaByNation.Num(),
		ActiveMinistryPrograms.Num(),
		PublicGroupSupportByKey.Num(),
		PoliticalMemoryByKey.Num());
	return true;
}
