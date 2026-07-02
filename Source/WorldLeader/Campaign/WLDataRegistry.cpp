// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLDataRegistry.h"
#include "WorldLeader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"

namespace
{
	uint32 StableIsoHash(const FString& Iso)
	{
		uint32 Hash = 2166136261u;
		for (const TCHAR Ch : Iso)
		{
			Hash ^= static_cast<uint32>(FChar::ToUpper(Ch));
			Hash *= 16777619u;
		}
		return Hash;
	}

	FLinearColor StableNationColor(const FString& Iso)
	{
		const uint32 Hash = StableIsoHash(Iso);
		return FLinearColor::MakeFromHSV8(
			static_cast<uint8>(Hash % 255),
			170,
			210);
	}

	int64 EstimateAmericaPopulation(const FString& DetailLevel, bool bAdministrable, uint32 Hash)
	{
		const FString Detail = DetailLevel.ToLower();
		int64 Base = 900000;
		if (Detail == TEXT("high"))
		{
			Base = 6500000;
		}
		else if (Detail == TEXT("mid"))
		{
			Base = 2600000;
		}
		else if (Detail == TEXT("low"))
		{
			Base = 850000;
		}
		if (bAdministrable)
		{
			Base += 1500000;
		}
		return Base + static_cast<int64>(Hash % 750000);
	}

	int32 EstimateAmericaInfrastructure(const FString& DetailLevel, bool bAdministrable, uint32 Hash)
	{
		const FString Detail = DetailLevel.ToLower();
		int32 Base = 35;
		if (Detail == TEXT("high"))
		{
			Base = 62;
		}
		else if (Detail == TEXT("mid"))
		{
			Base = 50;
		}
		if (bAdministrable)
		{
			Base += 5;
		}
		return FMath::Clamp(Base + static_cast<int32>(Hash % 9) - 4, 20, 85);
	}

	bool TextSuggestsPort(const FString& Text)
	{
		const FString S = Text.ToLower();
		return S.Contains(TEXT("coast"))
			|| S.Contains(TEXT("costa"))
			|| S.Contains(TEXT("caribbean"))
			|| S.Contains(TEXT("pacific"))
			|| S.Contains(TEXT("atlantic"))
			|| S.Contains(TEXT("island"))
			|| S.Contains(TEXT("isla"))
			|| S.Contains(TEXT("port"));
	}

	struct FAmericaCapitalSeed
	{
		FString Name;
		FString Region;
		double Lon = 0.0;
		double Lat = 0.0;
		bool bHasCoordinates = false;
		bool bHasPort = false;
	};
}

void UWLDataRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadGameData();
}

bool UWLDataRegistry::LoadGameData()
{
	Provinces.Reset();
	Nations.Reset();
	Buildings.Reset();
	Units.Reset();
	Goods.Reset();

	const FString DataDir = FPaths::ProjectContentDir() / TEXT("Data");
	const bool bProvinces = LoadProvincesFromFile(DataDir / TEXT("Provinces") / TEXT("Provinces.json"));
	const bool bNations   = LoadNationsFromFile(DataDir / TEXT("Nations") / TEXT("Nations.json"));
	LoadAmericaDiplomacyNationsFromDirectory(DataDir / TEXT("Campaign3D") / TEXT("AmericaLowDetail"));
	const bool bBuildings = LoadBuildingsFromFile(DataDir / TEXT("Buildings") / TEXT("Buildings.json"));
	const bool bUnits     = LoadUnitsFromFile(DataDir / TEXT("Units") / TEXT("Units.json"));
	const bool bGoods     = LoadGoodsFromFile(DataDir / TEXT("Goods") / TEXT("Goods.json"));
	const bool bValid     = ValidateLoadedData();

	UE_LOG(LogWorldLeader, Log, TEXT("WLDataRegistry: %d provincias, %d naciones, %d edificios, %d unidades, %d bienes cargados."),
		Provinces.Num(), Nations.Num(), Buildings.Num(), Units.Num(), Goods.Num());

	return bProvinces && bNations && bBuildings && bUnits && bGoods && bValid;
}

FString UWLDataRegistry::NormalizeIso(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

FString UWLDataRegistry::NormalizeProvinceId(const FString& In)
{
	return In.TrimStartAndEnd().ToUpper();
}

FString UWLDataRegistry::NormalizeDataId(const FString& In)
{
	return In.TrimStartAndEnd().ToLower();
}

EWLTerrainType UWLDataRegistry::TerrainFromString(const FString& In)
{
	const FString T = In.ToLower();
	if (T.Contains(TEXT("mountain")) || T.Contains(TEXT("montan"))) return EWLTerrainType::Mountain;
	if (T.Contains(TEXT("desert")))                                  return EWLTerrainType::Desert;
	if (T.Contains(TEXT("jungle")) || T.Contains(TEXT("selva")))     return EWLTerrainType::Jungle;
	if (T.Contains(TEXT("urban")))                                   return EWLTerrainType::Urban;
	if (T.Contains(TEXT("arctic")))                                  return EWLTerrainType::Arctic;
	if (T.Contains(TEXT("maritime")) || T.Contains(TEXT("sea")))     return EWLTerrainType::Maritime;
	if (T.Contains(TEXT("coast")))                                   return EWLTerrainType::Coastal;
	return EWLTerrainType::Plain;
}

EWLBuildingSlot UWLDataRegistry::SlotFromString(const FString& In)
{
	const FString S = In.ToLower();
	if (S == TEXT("industrial"))     return EWLBuildingSlot::Industrial;
	if (S == TEXT("military"))       return EWLBuildingSlot::Military;
	if (S == TEXT("naval"))          return EWLBuildingSlot::Naval;
	if (S == TEXT("air"))            return EWLBuildingSlot::Air;
	if (S == TEXT("tech"))           return EWLBuildingSlot::Tech;
	if (S == TEXT("financial"))      return EWLBuildingSlot::Financial;
	if (S == TEXT("infrastructure")) return EWLBuildingSlot::Infrastructure;
	if (S == TEXT("defensive"))      return EWLBuildingSlot::Defensive;
	return EWLBuildingSlot::Economic;
}

EWLUnitType UWLDataRegistry::UnitTypeFromString(const FString& In)
{
	const FString S = In.ToLower();
	if (S == TEXT("armor") || S == TEXT("tank"))            return EWLUnitType::Armor;
	if (S == TEXT("artillery"))                              return EWLUnitType::Artillery;
	if (S == TEXT("airdefense") || S == TEXT("sam"))         return EWLUnitType::AirDefense;
	if (S == TEXT("air"))                                    return EWLUnitType::Air;
	if (S == TEXT("naval"))                                  return EWLUnitType::Naval;
	if (S == TEXT("drone"))                                  return EWLUnitType::Drone;
	if (S == TEXT("special") || S == TEXT("specialforces"))  return EWLUnitType::SpecialForces;
	return EWLUnitType::Infantry;
}

bool UWLDataRegistry::LoadProvincesFromFile(const FString& FilePath)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLDataRegistry: no se pudo leer %s"), *FilePath);
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: JSON de provincias invalido en %s"), *FilePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr) continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FWLProvinceData P;
		Obj->TryGetStringField(TEXT("id"), P.Id);
		P.Id = NormalizeProvinceId(P.Id);
		Obj->TryGetStringField(TEXT("name"), P.Name);
		Obj->TryGetStringField(TEXT("country_iso"), P.CountryIso);
		P.CountryIso = NormalizeIso(P.CountryIso);
		Obj->TryGetStringField(TEXT("region"), P.Region);
		Obj->TryGetStringField(TEXT("capital"), P.Capital);

		FString TerrainStr;
		Obj->TryGetStringField(TEXT("terrain"), TerrainStr);
		P.Terrain = TerrainFromString(TerrainStr);

		int32 Tmp = 0;
		if (Obj->TryGetNumberField(TEXT("base_oil"), Tmp))       P.BaseOil = Tmp;
		if (Obj->TryGetNumberField(TEXT("base_gas"), Tmp))       P.BaseGas = Tmp;
		if (Obj->TryGetNumberField(TEXT("base_food"), Tmp))      P.BaseFood = Tmp;
		if (Obj->TryGetNumberField(TEXT("base_minerals"), Tmp))  P.BaseMinerals = Tmp;
		if (Obj->TryGetNumberField(TEXT("base_industry"), Tmp))  P.BaseIndustry = Tmp;
		if (Obj->TryGetNumberField(TEXT("infrastructure"), Tmp)) P.Infrastructure = Tmp;
		if (Obj->TryGetNumberField(TEXT("strategic_value"), Tmp))P.StrategicValue = Tmp;

		double Coord = 0.0;
		if (Obj->TryGetNumberField(TEXT("map_lat"), Coord)) P.MapLat = static_cast<float>(Coord);
		if (Obj->TryGetNumberField(TEXT("map_lon"), Coord)) P.MapLon = static_cast<float>(Coord);

		double Pop = 0.0;
		if (Obj->TryGetNumberField(TEXT("population"), Pop))     P.Population = static_cast<int64>(Pop);

		Obj->TryGetBoolField(TEXT("is_capital"), P.bIsCapital);
		Obj->TryGetBoolField(TEXT("has_port"), P.bHasPort);
		Obj->TryGetBoolField(TEXT("has_airbase"), P.bHasAirbase);

		const TArray<TSharedPtr<FJsonValue>>* NeighborsArr = nullptr;
		if (Obj->TryGetArrayField(TEXT("neighbors"), NeighborsArr) && NeighborsArr)
		{
			for (const TSharedPtr<FJsonValue>& N : *NeighborsArr)
			{
				FString NId;
				if (N.IsValid() && N->TryGetString(NId)) P.Neighbors.Add(NormalizeProvinceId(NId));
			}
		}

		if (P.IsValid())
		{
			if (Provinces.Contains(P.Id))
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: provincia duplicada ignorada: %s"), *P.Id);
				continue;
			}
			Provinces.Add(P.Id, MoveTemp(P));
		}
	}

	return Provinces.Num() > 0;
}

bool UWLDataRegistry::LoadNationsFromFile(const FString& FilePath)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLDataRegistry: no se pudo leer %s"), *FilePath);
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: JSON de naciones invalido en %s"), *FilePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr) continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FWLNationData N;
		Obj->TryGetStringField(TEXT("iso"), N.Iso);
		N.Iso = NormalizeIso(N.Iso);
		Obj->TryGetStringField(TEXT("name"), N.Name);
		Obj->TryGetStringField(TEXT("capital_province"), N.CapitalProvinceId);
		N.CapitalProvinceId = NormalizeProvinceId(N.CapitalProvinceId);
		Obj->TryGetStringField(TEXT("government"), N.GovernmentType);
		Obj->TryGetStringField(TEXT("leader"), N.Leader);

		double Treasury = 0.0;
		if (Obj->TryGetNumberField(TEXT("starting_treasury"), Treasury))
		{
			N.StartingTreasury = static_cast<int64>(Treasury);
		}

		// Color opcional [r,g,b] en 0..1.
		const TArray<TSharedPtr<FJsonValue>>* ColorArr = nullptr;
		if (Obj->TryGetArrayField(TEXT("map_color"), ColorArr) && ColorArr && ColorArr->Num() >= 3)
		{
			N.MapColor = FLinearColor(
				(float)(*ColorArr)[0]->AsNumber(),
				(float)(*ColorArr)[1]->AsNumber(),
				(float)(*ColorArr)[2]->AsNumber());
		}

		if (N.IsValid())
		{
			if (Nations.Contains(N.Iso))
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: nacion duplicada ignorada: %s"), *N.Iso);
				continue;
			}
			Nations.Add(N.Iso, MoveTemp(N));
		}
	}

	return Nations.Num() > 0;
}

bool UWLDataRegistry::LoadAmericaDiplomacyNationsFromDirectory(const FString& DirectoryPath)
{
	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *(DirectoryPath / TEXT("*.json")), true, false);
	FileNames.Sort();

	int32 AddedNations = 0;
	int32 AddedCapitalProvinces = 0;

	for (const FString& FileName : FileNames)
	{
		const FString FilePath = DirectoryPath / FileName;
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *FilePath))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogWorldLeader, Warning, TEXT("WLDataRegistry: AmericaLowDetail invalido en %s"), *FilePath);
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* Countries = nullptr;
		if (!Root->TryGetArrayField(TEXT("countries"), Countries) || !Countries)
		{
			continue;
		}

		TMap<FString, FAmericaCapitalSeed> CapitalSeedsByIso;
		const TArray<TSharedPtr<FJsonValue>>* Cities = nullptr;
		if (Root->TryGetArrayField(TEXT("cities"), Cities) && Cities)
		{
			for (const TSharedPtr<FJsonValue>& CityValue : *Cities)
			{
				const TSharedPtr<FJsonObject>* CityObjPtr = nullptr;
				if (!CityValue.IsValid() || !CityValue->TryGetObject(CityObjPtr) || !CityObjPtr)
				{
					continue;
				}
				const TSharedPtr<FJsonObject>& CityObj = *CityObjPtr;

				FString CityIso;
				CityObj->TryGetStringField(TEXT("country_iso"), CityIso);
				CityIso = NormalizeIso(CityIso);
				if (CityIso.IsEmpty() || CapitalSeedsByIso.Contains(CityIso))
				{
					continue;
				}

				bool bCapitalCity = false;
				CityObj->TryGetBoolField(TEXT("capital"), bCapitalCity);
				FString MarkerType;
				CityObj->TryGetStringField(TEXT("marker_type"), MarkerType);
				if (!bCapitalCity && !MarkerType.ToLower().Contains(TEXT("capital")))
				{
					continue;
				}

				FAmericaCapitalSeed Seed;
				CityObj->TryGetStringField(TEXT("name"), Seed.Name);
				CityObj->TryGetStringField(TEXT("region"), Seed.Region);
				double Lon = 0.0;
				double Lat = 0.0;
				const bool bHasLon = CityObj->TryGetNumberField(TEXT("lon"), Lon);
				const bool bHasLat = CityObj->TryGetNumberField(TEXT("lat"), Lat);
				if (bHasLon && bHasLat)
				{
					Seed.Lon = Lon;
					Seed.Lat = Lat;
					Seed.bHasCoordinates = true;
				}
				CityObj->TryGetBoolField(TEXT("port"), Seed.bHasPort);
				CapitalSeedsByIso.Add(CityIso, MoveTemp(Seed));
			}
		}

		for (const TSharedPtr<FJsonValue>& CountryValue : *Countries)
		{
			const TSharedPtr<FJsonObject>* CountryObjPtr = nullptr;
			if (!CountryValue.IsValid() || !CountryValue->TryGetObject(CountryObjPtr) || !CountryObjPtr)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& Obj = *CountryObjPtr;

			FString Iso;
			Obj->TryGetStringField(TEXT("iso"), Iso);
			Iso = NormalizeIso(Iso);
			if (Iso.IsEmpty())
			{
				continue;
			}

			FString DisplayName;
			Obj->TryGetStringField(TEXT("display_name"), DisplayName);
			if (DisplayName.TrimStartAndEnd().IsEmpty())
			{
				Obj->TryGetStringField(TEXT("geo_admin"), DisplayName);
			}
			if (DisplayName.TrimStartAndEnd().IsEmpty())
			{
				DisplayName = Iso;
			}

			FString Capital;
			Obj->TryGetStringField(TEXT("capital"), Capital);
			if (Capital.TrimStartAndEnd().IsEmpty())
			{
				Capital = DisplayName;
			}
			const FAmericaCapitalSeed* CapitalSeed = CapitalSeedsByIso.Find(Iso);
			if (CapitalSeed && !CapitalSeed->Name.TrimStartAndEnd().IsEmpty())
			{
				Capital = CapitalSeed->Name;
			}

			FString DetailLevel;
			Obj->TryGetStringField(TEXT("detail_level"), DetailLevel);
			FString Region;
			Obj->TryGetStringField(TEXT("continental_region"), Region);
			FString Biome;
			Obj->TryGetStringField(TEXT("primary_biome"), Biome);
			FString VisualProfile;
			Obj->TryGetStringField(TEXT("visual_profile"), VisualProfile);
			bool bSpecialTerritory = false;
			Obj->TryGetBoolField(TEXT("special_territory"), bSpecialTerritory);
			bool bAdministrable = false;
			Obj->TryGetBoolField(TEXT("administrable"), bAdministrable);
			double LabelLon = 0.0;
			double LabelLat = 0.0;
			Obj->TryGetNumberField(TEXT("label_lon"), LabelLon);
			Obj->TryGetNumberField(TEXT("label_lat"), LabelLat);

			const uint32 Hash = StableIsoHash(Iso);
			FWLNationData* Nation = Nations.Find(Iso);
			if (!Nation)
			{
				FWLNationData NewNation;
				NewNation.Iso = Iso;
				NewNation.Name = DisplayName;
				NewNation.CapitalProvinceId = NormalizeProvinceId(Iso + TEXT("-CAP"));
				NewNation.StartingTreasury = 30000 + static_cast<int64>(Hash % 50000) + (bAdministrable ? 20000 : 0);
				NewNation.GovernmentType = bSpecialTerritory ? TEXT("Administracion territorial") : TEXT("Gobierno nacional");
				NewNation.Leader = FString::Printf(TEXT("Gobierno de %s"), *DisplayName);
				NewNation.MapColor = StableNationColor(Iso);
				Nations.Add(Iso, NewNation);
				Nation = Nations.Find(Iso);
				++AddedNations;
			}

			if (Nation && !Provinces.Contains(Nation->CapitalProvinceId))
			{
				FWLProvinceData CapitalProvince;
				CapitalProvince.Id = Nation->CapitalProvinceId;
				CapitalProvince.Name = Capital;
				CapitalProvince.CountryIso = Iso;
				CapitalProvince.Region = CapitalSeed && !CapitalSeed->Region.TrimStartAndEnd().IsEmpty()
					? CapitalSeed->Region
					: (Region.IsEmpty() ? TEXT("America") : Region);
				CapitalProvince.Capital = Capital;
				CapitalProvince.Terrain = TerrainFromString(Biome + TEXT(" ") + VisualProfile);
				CapitalProvince.Population = EstimateAmericaPopulation(DetailLevel, bAdministrable, Hash);
				CapitalProvince.Infrastructure = EstimateAmericaInfrastructure(DetailLevel, bAdministrable, Hash);
				CapitalProvince.StrategicValue = FMath::Clamp(
					(DetailLevel.ToLower() == TEXT("high") ? 8 : DetailLevel.ToLower() == TEXT("mid") ? 6 : 4)
					+ (bAdministrable ? 1 : 0),
					1,
					10);
				CapitalProvince.MapLon = static_cast<float>(CapitalSeed && CapitalSeed->bHasCoordinates ? CapitalSeed->Lon : LabelLon);
				CapitalProvince.MapLat = static_cast<float>(CapitalSeed && CapitalSeed->bHasCoordinates ? CapitalSeed->Lat : LabelLat);
				CapitalProvince.bIsCapital = true;
				CapitalProvince.bHasPort = (CapitalSeed && CapitalSeed->bHasPort)
					|| TextSuggestsPort(Biome + TEXT(" ") + VisualProfile + TEXT(" ") + Capital);
				CapitalProvince.bHasAirbase = true;
				CapitalProvince.BaseFood = 35 + static_cast<int32>(Hash % 45);
				CapitalProvince.BaseMinerals = 12 + static_cast<int32>((Hash / 7) % 35);
				CapitalProvince.BaseIndustry = 18 + static_cast<int32>((Hash / 13) % 40);
				CapitalProvince.BaseOil = static_cast<int32>((Hash / 17) % 18);
				CapitalProvince.BaseGas = static_cast<int32>((Hash / 19) % 16);
				Provinces.Add(CapitalProvince.Id, MoveTemp(CapitalProvince));
				++AddedCapitalProvinces;
			}
		}
	}

	if (AddedNations > 0 || AddedCapitalProvinces > 0)
	{
		UE_LOG(LogWorldLeader, Log, TEXT("WLDataRegistry: America diplomacy agrego %d naciones y %d capitales backend."),
			AddedNations, AddedCapitalProvinces);
	}
	return AddedNations > 0 || AddedCapitalProvinces > 0;
}

bool UWLDataRegistry::LoadBuildingsFromFile(const FString& FilePath)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLDataRegistry: no se pudo leer %s"), *FilePath);
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: JSON de edificios invalido en %s"), *FilePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr) continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FWLBuildingData B;
		Obj->TryGetStringField(TEXT("id"), B.Id);
		B.Id = NormalizeDataId(B.Id);
		Obj->TryGetStringField(TEXT("name"), B.Name);

		FString SlotStr;
		Obj->TryGetStringField(TEXT("slot"), SlotStr);
		B.Slot = SlotFromString(SlotStr);

		double Cost = 0.0;
		if (Obj->TryGetNumberField(TEXT("cost"), Cost)) B.Cost = static_cast<int64>(Cost);
		double Upkeep = 0.0;
		if (Obj->TryGetNumberField(TEXT("monthly_upkeep"), Upkeep)) B.MonthlyUpkeep = static_cast<int64>(Upkeep);
		double FinancialIncome = 0.0;
		if (Obj->TryGetNumberField(TEXT("bonus_financial_income"), FinancialIncome))
		{
			B.BonusFinancialIncome = static_cast<int64>(FinancialIncome);
		}
		int32 MaxLevel = 0;
		if (Obj->TryGetNumberField(TEXT("max_level"), MaxLevel))
		{
			B.MaxLevel = MaxLevel;
		}
		double UpgradeMultiplier = 0.0;
		if (Obj->TryGetNumberField(TEXT("upgrade_cost_multiplier"), UpgradeMultiplier))
		{
			B.UpgradeCostMultiplier = UpgradeMultiplier;
		}

		int32 Tmp = 0;
		if (Obj->TryGetNumberField(TEXT("bonus_oil"), Tmp))      B.BonusOil = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_gas"), Tmp))      B.BonusGas = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_food"), Tmp))     B.BonusFood = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_minerals"), Tmp)) B.BonusMinerals = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_industry"), Tmp)) B.BonusIndustry = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_infrastructure"), Tmp)) B.BonusInfrastructure = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_public_order"), Tmp)) B.BonusPublicOrder = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_recruitment_capacity"), Tmp)) B.BonusRecruitmentCapacity = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_military_power"), Tmp)) B.BonusMilitaryPower = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_defense"), Tmp)) B.BonusDefense = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_air_capacity"), Tmp)) B.BonusAirCapacity = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_naval_capacity"), Tmp)) B.BonusNavalCapacity = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_technology"), Tmp)) B.BonusTechnology = Tmp;

		if (B.IsValid())
		{
			if (Buildings.Contains(B.Id))
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: edificio duplicado ignorado: %s"), *B.Id);
				continue;
			}
			Buildings.Add(B.Id, MoveTemp(B));
		}
	}

	return Buildings.Num() > 0;
}

bool UWLDataRegistry::LoadUnitsFromFile(const FString& FilePath)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLDataRegistry: no se pudo leer %s"), *FilePath);
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: JSON de unidades invalido en %s"), *FilePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr) continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FWLUnitData U;
		Obj->TryGetStringField(TEXT("id"), U.Id);
		U.Id = NormalizeDataId(U.Id);
		Obj->TryGetStringField(TEXT("name"), U.Name);

		FString TypeStr;
		Obj->TryGetStringField(TEXT("type"), TypeStr);
		U.Type = UnitTypeFromString(TypeStr);

		int32 Tmp = 0;
		if (Obj->TryGetNumberField(TEXT("attack"), Tmp))   U.Attack = Tmp;
		if (Obj->TryGetNumberField(TEXT("defense"), Tmp))  U.Defense = Tmp;
		if (Obj->TryGetNumberField(TEXT("strength"), Tmp)) U.Strength = Tmp;

		double Cost = 0.0;
		if (Obj->TryGetNumberField(TEXT("cost"), Cost)) U.Cost = static_cast<int64>(Cost);

		if (U.IsValid())
		{
			if (Units.Contains(U.Id))
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: unidad duplicada ignorada: %s"), *U.Id);
				continue;
			}
			Units.Add(U.Id, MoveTemp(U));
		}
	}

	return Units.Num() > 0;
}

// FE2.1: catalogo de bienes (crudos y manufacturados con sus insumos).
bool UWLDataRegistry::LoadGoodsFromFile(const FString& FilePath)
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FilePath))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WLDataRegistry: no se pudo leer %s"), *FilePath);
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Array;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Array))
	{
		UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: JSON de bienes invalido en %s"), *FilePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : Array)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjPtr) || !ObjPtr) continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FWLGoodData G;
		Obj->TryGetStringField(TEXT("id"), G.Id);
		G.Id = NormalizeDataId(G.Id);
		Obj->TryGetStringField(TEXT("name"), G.Name);

		FString CategoryStr;
		Obj->TryGetStringField(TEXT("category"), CategoryStr);
		G.Category = CategoryStr.ToLower() == TEXT("manufactured")
			? EWLGoodCategory::Manufactured
			: EWLGoodCategory::Raw;

		int32 Price = 0;
		if (Obj->TryGetNumberField(TEXT("base_price"), Price)) G.BasePrice = Price;

		Obj->TryGetStringField(TEXT("base_source"), G.BaseSource);
		G.BaseSource = NormalizeDataId(G.BaseSource);

		double BaseShare = 0.0;
		if (Obj->TryGetNumberField(TEXT("base_share"), BaseShare)) G.BaseShare = FMath::Max(0.0, BaseShare);

		double DemandPerMillion = 0.0;
		if (Obj->TryGetNumberField(TEXT("demand_per_million"), DemandPerMillion))
		{
			G.DemandPerMillion = FMath::Max(0.0, DemandPerMillion);
		}

		double Share = 0.0;
		if (Obj->TryGetNumberField(TEXT("industry_share"), Share)) G.IndustryShare = FMath::Max(0.0, Share);

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (Obj->TryGetArrayField(TEXT("inputs"), Inputs) && Inputs)
		{
			for (const TSharedPtr<FJsonValue>& Input : *Inputs)
			{
				FString InputId;
				if (Input.IsValid() && Input->TryGetString(InputId))
				{
					G.Inputs.Add(NormalizeDataId(InputId));
				}
			}
		}

		if (G.IsValid())
		{
			if (Goods.Contains(G.Id))
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: bien duplicado ignorado: %s"), *G.Id);
				continue;
			}
			Goods.Add(G.Id, MoveTemp(G));
		}
	}

	return Goods.Num() > 0;
}

bool UWLDataRegistry::GetGood(const FString& Id, FWLGoodData& OutGood) const
{
	if (const FWLGoodData* Found = Goods.Find(NormalizeDataId(Id)))
	{
		OutGood = *Found;
		return true;
	}
	return false;
}

TArray<FWLGoodData> UWLDataRegistry::GetAllGoods() const
{
	TArray<FWLGoodData> Out;
	Goods.GenerateValueArray(Out);
	Out.Sort([](const FWLGoodData& A, const FWLGoodData& B) { return A.Id < B.Id; });
	return Out;
}

bool UWLDataRegistry::GetProvince(const FString& Id, FWLProvinceData& OutProvince) const
{
	if (const FWLProvinceData* Found = Provinces.Find(NormalizeProvinceId(Id)))
	{
		OutProvince = *Found;
		return true;
	}
	return false;
}

bool UWLDataRegistry::GetNation(const FString& Iso, FWLNationData& OutNation) const
{
	if (const FWLNationData* Found = Nations.Find(NormalizeIso(Iso)))
	{
		OutNation = *Found;
		return true;
	}
	return false;
}

TArray<FWLProvinceData> UWLDataRegistry::GetAllProvinces() const
{
	TArray<FWLProvinceData> Out;
	Provinces.GenerateValueArray(Out);
	return Out;
}

TArray<FWLNationData> UWLDataRegistry::GetAllNations() const
{
	TArray<FWLNationData> Out;
	Nations.GenerateValueArray(Out);
	return Out;
}

TArray<FWLProvinceData> UWLDataRegistry::GetProvincesByNation(const FString& Iso) const
{
	TArray<FWLProvinceData> Out;
	const FString NormalizedIso = NormalizeIso(Iso);
	for (const TPair<FString, FWLProvinceData>& Pair : Provinces)
	{
		if (Pair.Value.CountryIso == NormalizedIso)
		{
			Out.Add(Pair.Value);
		}
	}
	return Out;
}

bool UWLDataRegistry::GetBuilding(const FString& Id, FWLBuildingData& OutBuilding) const
{
	if (const FWLBuildingData* Found = Buildings.Find(NormalizeDataId(Id)))
	{
		OutBuilding = *Found;
		return true;
	}
	return false;
}

TArray<FWLBuildingData> UWLDataRegistry::GetAllBuildings() const
{
	TArray<FWLBuildingData> Out;
	Buildings.GenerateValueArray(Out);
	return Out;
}

bool UWLDataRegistry::GetUnit(const FString& Id, FWLUnitData& OutUnit) const
{
	if (const FWLUnitData* Found = Units.Find(NormalizeDataId(Id)))
	{
		OutUnit = *Found;
		return true;
	}
	return false;
}

TArray<FWLUnitData> UWLDataRegistry::GetAllUnits() const
{
	TArray<FWLUnitData> Out;
	Units.GenerateValueArray(Out);
	return Out;
}

bool UWLDataRegistry::ValidateLoadedData() const
{
	bool bOk = true;

	for (const TPair<FString, FWLNationData>& Pair : Nations)
	{
		const FWLNationData& Nation = Pair.Value;
		if (!Provinces.Contains(Nation.CapitalProvinceId))
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: nacion %s referencia capital inexistente %s."),
				*Nation.Iso, *Nation.CapitalProvinceId);
			bOk = false;
		}
		if (Nation.StartingTreasury < 0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: nacion %s tiene tesoro inicial negativo %lld."),
				*Nation.Iso, Nation.StartingTreasury);
			bOk = false;
		}
	}

	for (const TPair<FString, FWLProvinceData>& Pair : Provinces)
	{
		const FWLProvinceData& Province = Pair.Value;
		if (!Nations.Contains(Province.CountryIso))
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: provincia %s referencia nacion inexistente %s."),
				*Province.Id, *Province.CountryIso);
			bOk = false;
		}
		if (Province.Population < 0 || Province.Infrastructure < 0
			|| Province.BaseOil < 0 || Province.BaseGas < 0 || Province.BaseFood < 0
			|| Province.BaseMinerals < 0 || Province.BaseIndustry < 0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: provincia %s tiene valores economicos negativos."),
				*Province.Id);
			bOk = false;
		}
		for (const FString& NeighborId : Province.Neighbors)
		{
			const FWLProvinceData* Neighbor = Provinces.Find(NeighborId);
			if (!Neighbor)
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: provincia %s referencia vecino inexistente %s."),
					*Province.Id, *NeighborId);
				bOk = false;
				continue;
			}
			if (!Neighbor->Neighbors.Contains(Province.Id))
			{
				UE_LOG(LogWorldLeader, Warning, TEXT("WLDataRegistry: vecindad no reciproca %s -> %s."),
					*Province.Id, *NeighborId);
			}
		}
	}

	for (const TPair<FString, FWLBuildingData>& Pair : Buildings)
	{
		const FWLBuildingData& Building = Pair.Value;
		if (Building.Cost < 0 || Building.MaxLevel < 1 || Building.MaxLevel > 5
			|| Building.UpgradeCostMultiplier < 1.0 || Building.MonthlyUpkeep < 0
			|| Building.BonusOil < 0 || Building.BonusGas < 0
			|| Building.BonusFood < 0 || Building.BonusMinerals < 0 || Building.BonusIndustry < 0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: edificio %s tiene coste/nivel/bonus invalido."), *Building.Id);
			bOk = false;
		}
		if (Building.BonusFinancialIncome < 0 || Building.BonusInfrastructure < 0
			|| Building.BonusRecruitmentCapacity < 0 || Building.BonusMilitaryPower < 0
			|| Building.BonusDefense < 0 || Building.BonusAirCapacity < 0
			|| Building.BonusNavalCapacity < 0 || Building.BonusTechnology < 0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: edificio %s tiene efectos negativos invalidos."), *Building.Id);
			bOk = false;
		}
	}

	// FE2.1: los insumos de cada bien manufacturado deben existir en el catalogo.
	for (const TPair<FString, FWLGoodData>& Pair : Goods)
	{
		const FWLGoodData& Good = Pair.Value;
		if (Good.BasePrice < 0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: bien %s tiene precio negativo."), *Good.Id);
			bOk = false;
		}
		if (Good.BaseShare < 0.0 || Good.DemandPerMillion < 0.0 || Good.IndustryShare < 0.0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: bien %s tiene valores economicos negativos."), *Good.Id);
			bOk = false;
		}
		if (Good.Category == EWLGoodCategory::Raw && Good.BaseShare > 0.0 && Good.BaseSource.IsEmpty())
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: bien crudo %s declara base_share sin base_source."), *Good.Id);
			bOk = false;
		}
		for (const FString& InputId : Good.Inputs)
		{
			if (!Goods.Contains(InputId))
			{
				UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: bien %s referencia insumo inexistente %s."),
					*Good.Id, *InputId);
				bOk = false;
			}
		}
	}

	for (const TPair<FString, FWLUnitData>& Pair : Units)
	{
		const FWLUnitData& Unit = Pair.Value;
		if (Unit.Cost < 0 || Unit.Attack < 0 || Unit.Defense < 0 || Unit.Strength < 0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: unidad %s tiene stats/coste negativos."), *Unit.Id);
			bOk = false;
		}
	}

	return bOk;
}
