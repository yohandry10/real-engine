// Copyright World Leader project. See ROADMAP.md.

#include "Save/WLSaveMigration.h"

#include "Save/WLLocalSaveGame.h"

namespace
{
	void MigrateV16ToV17(UWLLocalSaveGame& Save)
	{
		Save.CurrentDay = FMath::Max(1, Save.CurrentDay);
		for (FWLProvinceBuildingsSave& Province : Save.ProvinceBuildings)
		{
			while (Province.BuildingLevels.Num() < Province.BuildingIds.Num())
			{
				Province.BuildingLevels.Add(1);
			}
			Province.BuildingLevels.SetNum(Province.BuildingIds.Num());
		}
		Save.SaveVersion = 17;
	}

	void MigrateV17ToV18(UWLLocalSaveGame& Save)
	{
		// V18 introduced persisted garrisons/recruit queues. Empty is the only truthful
		// reconstruction for old saves; deployed armies remain preserved separately.
		Save.GarrisonUnits.Reset();
		Save.RecruitOrders.Reset();
		Save.SaveVersion = 18;
	}

	void MigrateV18ToV19(UWLLocalSaveGame& Save)
	{
		// V19 establishes deterministic turn phases. No serialized field changed.
		Save.SaveVersion = 19;
	}
}

bool FWLSaveMigration::MigrateToCurrent(UWLLocalSaveGame& Save, FString& OutMessage)
{
	if (Save.SaveVersion < WLSaveVersion::OldestMigratable)
	{
		OutMessage = FString::Printf(TEXT("Version de save demasiado antigua: %d (minima: %d)."),
			Save.SaveVersion, WLSaveVersion::OldestMigratable);
		return false;
	}
	if (Save.SaveVersion > WLSaveVersion::Current)
	{
		OutMessage = FString::Printf(TEXT("Version de save futura no soportada: %d (actual: %d)."),
			Save.SaveVersion, WLSaveVersion::Current);
		return false;
	}

	const int32 OriginalVersion = Save.SaveVersion;
	while (Save.SaveVersion < WLSaveVersion::Current)
	{
		switch (Save.SaveVersion)
		{
		case 16: MigrateV16ToV17(Save); break;
		case 17: MigrateV17ToV18(Save); break;
		case 18: MigrateV18ToV19(Save); break;
		default:
			OutMessage = FString::Printf(TEXT("No existe migracion desde save v%d."), Save.SaveVersion);
			return false;
		}
	}

	OutMessage = OriginalVersion == WLSaveVersion::Current
		? TEXT("Save ya usa la version actual.")
		: FString::Printf(TEXT("Save migrado de v%d a v%d."), OriginalVersion, WLSaveVersion::Current);
	return true;
}
