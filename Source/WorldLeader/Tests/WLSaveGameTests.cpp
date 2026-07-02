// Copyright World Leader project. See ROADMAP.md.
//
// Test funcional del SaveGame local minimo de Phase 1.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Save/WLLocalSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLLocalSaveGameRoundTripTest,
	"WorldLeader.Save.LocalCampaignRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLLocalSaveGameRoundTripTest::RunTest(const FString& Parameters)
{
	const FString SlotName = TEXT("WorldLeader_Automation_LocalCampaign");
	constexpr int32 UserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);

	UWLLocalSaveGame* Save = Cast<UWLLocalSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UWLLocalSaveGame::StaticClass()));
	TestNotNull(TEXT("Crear SaveGame local"), Save);
	if (!Save)
	{
		return false;
	}

	Save->SelectedNationIso = TEXT("VE");
	Save->AIDifficulty = EWLAIDifficulty::Hard;
	Save->CurrentYear = 2024;
	Save->CurrentMonth = 2;
	Save->NextArmyNumber = 4;

	FWLNationTreasurySave Treasury;
	Treasury.NationIso = TEXT("VE");
	Treasury.Treasury = 61730;
	Treasury.TariffRatePercent = 18;
	Save->NationTreasuries.Add(Treasury);

	FWLProvinceBuildingsSave Buildings;
	Buildings.ProvinceId = TEXT("VE-ZU");
	Buildings.BuildingIds = { TEXT("oil_well"), TEXT("refinery") };
	Buildings.BuildingLevels = { 2, 1 };
	Save->ProvinceBuildings.Add(Buildings);

	FWLProvinceRuntimeState ProvinceState;
	ProvinceState.ProvinceId = TEXT("VE-ZU");
	ProvinceState.ControllerIso = TEXT("VE");
	ProvinceState.Population = 123456;
	ProvinceState.PublicOrder = 82;
	Save->ProvinceStates.Add(ProvinceState);

	FWLMarketShockState Shock;
	Shock.ShockId = TEXT("MS-0001");
	Shock.GoodId = TEXT("oil");
	Shock.Title = TEXT("Crisis petrolera");
	Shock.PriceMultiplier = 1.5;
	Shock.RemainingMonths = 2;
	Shock.TotalMonths = 3;
	Save->ActiveMarketShocks.Add(Shock);

	FWLFinancialInstrumentState Instrument;
	Instrument.InstrumentId = TEXT("BOND-0001");
	Instrument.NationIso = TEXT("VE");
	Instrument.CreditorIso = TEXT("MARKET");
	Instrument.Type = EWLFinancialInstrumentType::Bond;
	Instrument.OriginalPrincipal = 5000;
	Instrument.PrincipalRemaining = 4500;
	Instrument.MonthlyPayment = 250;
	Instrument.MonthlyInterestRate = 0.01;
	Instrument.TotalMonths = 24;
	Instrument.RemainingMonths = 20;
	Save->FinancialInstruments.Add(Instrument);

	FWLForeignSupportState Support;
	Support.SupportId = TEXT("AID-0001");
	Support.RecipientIso = TEXT("VE");
	Support.SponsorIso = TEXT("CO");
	Support.Type = EWLForeignSupportType::ForeignAid;
	Support.TotalAmount = 2400;
	Support.MonthlyAmount = 400;
	Support.AmountDelivered = 800;
	Support.TotalMonths = 6;
	Support.RemainingMonths = 4;
	Save->ForeignSupportStates.Add(Support);

	FWLArmy Army;
	Army.Id = TEXT("A3");
	Army.OwnerIso = TEXT("VE");
	Army.ProvinceId = TEXT("VE-ZU");
	Army.General = TEXT("Miranda");
	Army.Units = { TEXT("infantry"), TEXT("tank") };
	Save->Armies.Add(Army);

	FWLCharacter Character;
	Character.Id = TEXT("VE-MIN-ECO-SERRANO");
	Character.Name = TEXT("Irene Serrano");
	Character.CountryIso = TEXT("VE");
	Character.Role = EWLCharacterRole::Minister;
	Character.PreferredOffice = EWLMinisterOffice::Economy;
	Character.AssignedOffice = EWLMinisterOffice::Economy;
	Character.Skill = 67;
	Character.Loyalty = 76;
	Save->Characters.Add(Character);

	FWLPoliticalCapitalSave PoliticalCapital;
	PoliticalCapital.NationIso = TEXT("VE");
	PoliticalCapital.PoliticalCapital = 88;
	Save->PoliticalCapital.Add(PoliticalCapital);

	FWLInternalPowerState InternalPower;
	InternalPower.NationIso = TEXT("VE");
	InternalPower.OppositionStrength = 31;
	InternalPower.CoupRisk = 44;
	Save->InternalPowerStates.Add(InternalPower);

	FWLDiplomaticRelationState Relation;
	Relation.NationA = TEXT("CO");
	Relation.NationB = TEXT("VE");
	Relation.Opinion = -25;
	Relation.Status = EWLDiplomaticStatus::Tension;
	Relation.Treaties = { EWLTreatyType::Embargo };
	Save->DiplomaticRelations.Add(Relation);

	FWLIntelligenceNetworkState Network;
	Network.OwnerIso = TEXT("CO");
	Network.TargetIso = TEXT("VE");
	Network.NetworkStrength = 42;
	Network.Exposure = 11;
	Save->IntelligenceNetworks.Add(Network);

	FWLPoliticalEventInstance Event;
	Event.InstanceId = TEXT("EV-0007");
	Event.EventId = TEXT("street_protests");
	Event.NationIso = TEXT("VE");
	Event.Title = TEXT("Protestas nacionales");
	Save->PoliticalEvents.Add(Event);

	Save->CampaignOutcome.OutcomeType = TEXT("None");

	FWLGovernmentAgendaState Agenda;
	Agenda.NationIso = TEXT("VE");
	Agenda.Priorities = { EWLGovernmentPriority::Security, EWLGovernmentPriority::Growth, EWLGovernmentPriority::Diplomacy };
	Agenda.MonthsActive = 3;
	Save->GovernmentAgendas.Add(Agenda);

	FWLMinistryProgramState Program;
	Program.NationIso = TEXT("VE");
	Program.ProgramId = TEXT("econ_public_investment");
	Program.Name = TEXT("Inversion publica");
	Program.Office = EWLMinisterOffice::Economy;
	Program.RemainingMonths = 2;
	Program.Progress = 35;
	Save->MinistryPrograms.Add(Program);

	FWLCabinetDynamicsState Cabinet;
	Cabinet.NationIso = TEXT("VE");
	Cabinet.RivalryPressure = 22;
	Cabinet.ScandalRisk = 18;
	Save->CabinetDynamics.Add(Cabinet);

	FWLInstitutionalPowerState Institution;
	Institution.NationIso = TEXT("VE");
	Institution.RulingCoalitionSupport = 61;
	Institution.GridlockRisk = 24;
	Save->InstitutionalPower.Add(Institution);

	FWLPublicGroupSupportState Group;
	Group.NationIso = TEXT("VE");
	Group.Group = EWLPublicGroup::Workers;
	Group.Support = 57;
	Group.Pressure = 8;
	Save->PublicGroupSupport.Add(Group);

	FWLStateCapacityState Capacity;
	Capacity.NationIso = TEXT("VE");
	Capacity.Bureaucracy = 58;
	Capacity.PolicyFailureRisk = 21;
	Save->StateCapacity.Add(Capacity);

	FWLPoliticalMemoryRecord Memory;
	Memory.NationIso = TEXT("VE");
	Memory.MemoryKey = TEXT("crisis_unrest");
	Memory.Value = 2;
	Memory.MonthsRemaining = 5;
	Save->PoliticalMemory.Add(Memory);

	FWLPoliticalAIPlanState Plan;
	Plan.NationIso = TEXT("VE");
	Plan.Objective = EWLGovernmentAIObjective::Industrialize;
	Plan.CurrentProgramId = TEXT("econ_public_investment");
	Save->GovernmentAIPlans.Add(Plan);

	FWLActiveReformState Reform;
	Reform.NationIso = TEXT("VE");
	Reform.ReformId = TEXT("tax_broad_base");
	Reform.Name = TEXT("Base tributaria amplia");
	Reform.Area = EWLPolicyReformArea::Tax;
	Reform.MonthsRemaining = 18;
	Reform.ImplementationProgress = 35;
	Save->ActivePolicyReforms.Add(Reform);

	FWLPartyState Party;
	Party.NationIso = TEXT("VE");
	Party.PartyId = TEXT("ve_gov");
	Party.Name = TEXT("Partido de Gobierno");
	Party.Role = EWLPartyRole::Ruling;
	Party.Seats = 36;
	Party.LoyaltyToGovernment = 72;
	Save->PoliticalParties.Add(Party);

	FWLElectionState Election;
	Election.NationIso = TEXT("VE");
	Election.MonthsToElection = 12;
	Election.CampaignPromiseReformId = TEXT("edu_public_schools");
	Save->ElectionStates.Add(Election);

	FWLCharacterPoliticalProfile Profile;
	Profile.NationIso = TEXT("VE");
	Profile.CharacterId = TEXT("VE-MIN-ECO-SERRANO");
	Profile.Biography = TEXT("Perfil politico de prueba");
	Profile.FactionId = TEXT("technocrats");
	Profile.PresidentialAmbition = 44;
	Save->CharacterPoliticalProfiles.Add(Profile);

	FWLPatronageState Patronage;
	Patronage.NationIso = TEXT("VE");
	Patronage.ContractCorruption = 31;
	Save->PatronageStates.Add(Patronage);

	FWLMediaPublicOpinionState Media;
	Media.NationIso = TEXT("VE");
	Media.PresidentialApproval = 48;
	Media.CensorshipBacklash = 13;
	Save->MediaStates.Add(Media);

	FWLRegionGovernorState Region;
	Region.NationIso = TEXT("VE");
	Region.RegionId = TEXT("VE-ZU");
	Region.RegionName = TEXT("Zulia");
	Region.Obedience = 62;
	Save->RegionGovernors.Add(Region);

	FWLCrisisChainState Crisis;
	Crisis.NationIso = TEXT("VE");
	Crisis.CrisisId = TEXT("VE|2");
	Crisis.Type = EWLCrisisChainType::CorruptionScandal;
	Crisis.Intensity = 66;
	Save->CrisisChains.Add(Crisis);

	FWLGovernmentCalibrationState Calibration;
	Calibration.NationIso = TEXT("VE");
	Calibration.MonthsObserved = 7;
	Calibration.ReformGridlockPressure = 29;
	Save->GovernmentCalibration.Add(Calibration);

	TestTrue(TEXT("Guardar slot temporal"),
		UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex));

	UWLLocalSaveGame* Loaded = Cast<UWLLocalSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	TestNotNull(TEXT("Cargar slot temporal"), Loaded);
	if (!Loaded)
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
		return false;
	}

	TestEqual(TEXT("Nacion seleccionada"), Loaded->SelectedNationIso, FString(TEXT("VE")));
	TestEqual(TEXT("Anio"), Loaded->CurrentYear, 2024);
	TestEqual(TEXT("Mes"), Loaded->CurrentMonth, 2);
	TestEqual(TEXT("Version de save"), Loaded->SaveVersion, 12);
	TestEqual(TEXT("Dificultad IA guardada"), static_cast<int32>(Loaded->AIDifficulty),
		static_cast<int32>(EWLAIDifficulty::Hard));
	TestEqual(TEXT("Tesoros guardados"), Loaded->NationTreasuries.Num(), 1);
	TestEqual(TEXT("Tesoro VE"), Loaded->NationTreasuries[0].Treasury, static_cast<int64>(61730));
	TestEqual(TEXT("Arancel VE guardado"), Loaded->NationTreasuries[0].TariffRatePercent, 18);
	TestEqual(TEXT("Provincias con edificios"), Loaded->ProvinceBuildings.Num(), 1);
	TestEqual(TEXT("Edificios en VE-ZU"), Loaded->ProvinceBuildings[0].BuildingIds.Num(), 2);
	TestEqual(TEXT("Niveles de edificios en VE-ZU"), Loaded->ProvinceBuildings[0].BuildingLevels.Num(), 2);
	TestEqual(TEXT("Nivel pozo guardado"), Loaded->ProvinceBuildings[0].BuildingLevels[0], 2);
	TestEqual(TEXT("Estados de provincia guardados"), Loaded->ProvinceStates.Num(), 1);
	TestEqual(TEXT("Controlador guardado"), Loaded->ProvinceStates[0].ControllerIso, FString(TEXT("VE")));
	TestEqual(TEXT("Poblacion guardada"), Loaded->ProvinceStates[0].Population, static_cast<int64>(123456));
	TestEqual(TEXT("Orden publico guardado"), Loaded->ProvinceStates[0].PublicOrder, 82);
	TestEqual(TEXT("Shocks de mercado guardados"), Loaded->ActiveMarketShocks.Num(), 1);
	TestEqual(TEXT("Shock petrolero guardado"), Loaded->ActiveMarketShocks[0].GoodId, FString(TEXT("oil")));
	TestEqual(TEXT("Duracion restante shock"), Loaded->ActiveMarketShocks[0].RemainingMonths, 2);
	TestEqual(TEXT("Instrumentos financieros guardados"), Loaded->FinancialInstruments.Num(), 1);
	TestEqual(TEXT("Principal financiero guardado"), Loaded->FinancialInstruments[0].PrincipalRemaining, static_cast<int64>(4500));
	TestEqual(TEXT("Apoyos exteriores guardados"), Loaded->ForeignSupportStates.Num(), 1);
	TestEqual(TEXT("Ayuda mensual guardada"), Loaded->ForeignSupportStates[0].MonthlyAmount, static_cast<int64>(400));
	TestEqual(TEXT("Ejercitos guardados"), Loaded->Armies.Num(), 1);
	TestEqual(TEXT("Siguiente numero de ejercito"), Loaded->NextArmyNumber, 4);
	TestEqual(TEXT("General guardado"), Loaded->Armies[0].General, FString(TEXT("Miranda")));
	TestEqual(TEXT("Personajes guardados"), Loaded->Characters.Num(), 1);
	TestEqual(TEXT("Ministro guardado"), Loaded->Characters[0].Id, FString(TEXT("VE-MIN-ECO-SERRANO")));
	TestEqual(TEXT("Cargo guardado"), static_cast<int32>(Loaded->Characters[0].AssignedOffice),
		static_cast<int32>(EWLMinisterOffice::Economy));
	TestEqual(TEXT("Capital politico guardado"), Loaded->PoliticalCapital.Num(), 1);
	TestEqual(TEXT("Capital politico VE"), Loaded->PoliticalCapital[0].PoliticalCapital, 88);
	TestEqual(TEXT("Poder interno guardado"), Loaded->InternalPowerStates.Num(), 1);
	TestEqual(TEXT("Riesgo golpe guardado"), Loaded->InternalPowerStates[0].CoupRisk, 44);
	TestEqual(TEXT("Relacion diplomatica guardada"), Loaded->DiplomaticRelations.Num(), 1);
	TestEqual(TEXT("Opinion guardada"), Loaded->DiplomaticRelations[0].Opinion, -25);
	TestEqual(TEXT("Red inteligencia guardada"), Loaded->IntelligenceNetworks.Num(), 1);
	TestEqual(TEXT("Fuerza red guardada"), Loaded->IntelligenceNetworks[0].NetworkStrength, 42);
	TestEqual(TEXT("Evento politico guardado"), Loaded->PoliticalEvents.Num(), 1);
	TestEqual(TEXT("Outcome guardado"), Loaded->CampaignOutcome.OutcomeType, FString(TEXT("None")));
	TestEqual(TEXT("Agendas de gobierno guardadas"), Loaded->GovernmentAgendas.Num(), 1);
	TestEqual(TEXT("Prioridades de agenda guardadas"), Loaded->GovernmentAgendas[0].Priorities.Num(), 3);
	TestEqual(TEXT("Programas ministeriales guardados"), Loaded->MinistryPrograms.Num(), 1);
	TestEqual(TEXT("Programa guardado"), Loaded->MinistryPrograms[0].ProgramId, FString(TEXT("econ_public_investment")));
	TestEqual(TEXT("Dinamica gabinete guardada"), Loaded->CabinetDynamics.Num(), 1);
	TestEqual(TEXT("Instituciones guardadas"), Loaded->InstitutionalPower.Num(), 1);
	TestEqual(TEXT("Coalicion guardada"), Loaded->InstitutionalPower[0].RulingCoalitionSupport, 61);
	TestEqual(TEXT("Grupos sociales guardados"), Loaded->PublicGroupSupport.Num(), 1);
	TestEqual(TEXT("Apoyo trabajadores guardado"), Loaded->PublicGroupSupport[0].Support, 57);
	TestEqual(TEXT("Capacidad estatal guardada"), Loaded->StateCapacity.Num(), 1);
	TestEqual(TEXT("Riesgo de fallo guardado"), Loaded->StateCapacity[0].PolicyFailureRisk, 21);
	TestEqual(TEXT("Memoria politica guardada"), Loaded->PoliticalMemory.Num(), 1);
	TestEqual(TEXT("Valor memoria guardado"), Loaded->PoliticalMemory[0].Value, 2);
	TestEqual(TEXT("Planes IA gobierno guardados"), Loaded->GovernmentAIPlans.Num(), 1);
	TestEqual(TEXT("Objetivo IA guardado"), static_cast<int32>(Loaded->GovernmentAIPlans[0].Objective),
		static_cast<int32>(EWLGovernmentAIObjective::Industrialize));
	TestEqual(TEXT("Reformas P2 guardadas"), Loaded->ActivePolicyReforms.Num(), 1);
	TestEqual(TEXT("Reforma P2 guardada"), Loaded->ActivePolicyReforms[0].ReformId, FString(TEXT("tax_broad_base")));
	TestEqual(TEXT("Partidos guardados"), Loaded->PoliticalParties.Num(), 1);
	TestEqual(TEXT("Partido guardado"), Loaded->PoliticalParties[0].PartyId, FString(TEXT("ve_gov")));
	TestEqual(TEXT("Elecciones guardadas"), Loaded->ElectionStates.Num(), 1);
	TestEqual(TEXT("Promesa electoral guardada"), Loaded->ElectionStates[0].CampaignPromiseReformId, FString(TEXT("edu_public_schools")));
	TestEqual(TEXT("Perfiles politicos guardados"), Loaded->CharacterPoliticalProfiles.Num(), 1);
	TestEqual(TEXT("Faccion perfil guardada"), Loaded->CharacterPoliticalProfiles[0].FactionId, FString(TEXT("technocrats")));
	TestEqual(TEXT("Patronazgo guardado"), Loaded->PatronageStates.Num(), 1);
	TestEqual(TEXT("Corrupcion contrato guardada"), Loaded->PatronageStates[0].ContractCorruption, 31);
	TestEqual(TEXT("Medios guardados"), Loaded->MediaStates.Num(), 1);
	TestEqual(TEXT("Backlash censura guardado"), Loaded->MediaStates[0].CensorshipBacklash, 13);
	TestEqual(TEXT("Regiones guardadas"), Loaded->RegionGovernors.Num(), 1);
	TestEqual(TEXT("Region guardada"), Loaded->RegionGovernors[0].RegionId, FString(TEXT("VE-ZU")));
	TestEqual(TEXT("Crisis guardadas"), Loaded->CrisisChains.Num(), 1);
	TestEqual(TEXT("Intensidad crisis guardada"), Loaded->CrisisChains[0].Intensity, 66);
	TestEqual(TEXT("Calibracion guardada"), Loaded->GovernmentCalibration.Num(), 1);
	TestEqual(TEXT("Meses calibracion guardados"), Loaded->GovernmentCalibration[0].MonthsObserved, 7);

	TestTrue(TEXT("Borrar slot temporal"),
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
