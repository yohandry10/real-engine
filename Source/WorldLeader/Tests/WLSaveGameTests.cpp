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
	TestEqual(TEXT("Version de save"), Loaded->SaveVersion, 9);
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

	TestTrue(TEXT("Borrar slot temporal"),
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
