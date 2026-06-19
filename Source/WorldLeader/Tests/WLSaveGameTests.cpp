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
	Save->NationTreasuries.Add(Treasury);

	FWLProvinceBuildingsSave Buildings;
	Buildings.ProvinceId = TEXT("VE-ZU");
	Buildings.BuildingIds = { TEXT("oil_well"), TEXT("refinery") };
	Save->ProvinceBuildings.Add(Buildings);

	FWLProvinceRuntimeState ProvinceState;
	ProvinceState.ProvinceId = TEXT("VE-ZU");
	ProvinceState.ControllerIso = TEXT("VE");
	ProvinceState.Population = 123456;
	ProvinceState.PublicOrder = 82;
	Save->ProvinceStates.Add(ProvinceState);

	FWLArmy Army;
	Army.Id = TEXT("A3");
	Army.OwnerIso = TEXT("VE");
	Army.ProvinceId = TEXT("VE-ZU");
	Army.General = TEXT("Miranda");
	Army.Units = { TEXT("infantry"), TEXT("tank") };
	Save->Armies.Add(Army);

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
	TestEqual(TEXT("Version de save"), Loaded->SaveVersion, 3);
	TestEqual(TEXT("Tesoros guardados"), Loaded->NationTreasuries.Num(), 1);
	TestEqual(TEXT("Tesoro VE"), Loaded->NationTreasuries[0].Treasury, static_cast<int64>(61730));
	TestEqual(TEXT("Provincias con edificios"), Loaded->ProvinceBuildings.Num(), 1);
	TestEqual(TEXT("Edificios en VE-ZU"), Loaded->ProvinceBuildings[0].BuildingIds.Num(), 2);
	TestEqual(TEXT("Estados de provincia guardados"), Loaded->ProvinceStates.Num(), 1);
	TestEqual(TEXT("Controlador guardado"), Loaded->ProvinceStates[0].ControllerIso, FString(TEXT("VE")));
	TestEqual(TEXT("Poblacion guardada"), Loaded->ProvinceStates[0].Population, static_cast<int64>(123456));
	TestEqual(TEXT("Orden publico guardado"), Loaded->ProvinceStates[0].PublicOrder, 82);
	TestEqual(TEXT("Ejercitos guardados"), Loaded->Armies.Num(), 1);
	TestEqual(TEXT("Siguiente numero de ejercito"), Loaded->NextArmyNumber, 4);
	TestEqual(TEXT("General guardado"), Loaded->Armies[0].General, FString(TEXT("Miranda")));

	TestTrue(TEXT("Borrar slot temporal"),
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
