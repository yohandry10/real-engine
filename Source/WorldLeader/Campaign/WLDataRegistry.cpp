// Copyright World Leader project. See ROADMAP.md.

#include "Campaign/WLDataRegistry.h"
#include "WorldLeader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

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

		int32 Tmp = 0;
		if (Obj->TryGetNumberField(TEXT("bonus_oil"), Tmp))      B.BonusOil = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_gas"), Tmp))      B.BonusGas = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_food"), Tmp))     B.BonusFood = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_minerals"), Tmp)) B.BonusMinerals = Tmp;
		if (Obj->TryGetNumberField(TEXT("bonus_industry"), Tmp)) B.BonusIndustry = Tmp;

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
		if (Building.Cost < 0 || Building.BonusOil < 0 || Building.BonusGas < 0
			|| Building.BonusFood < 0 || Building.BonusMinerals < 0 || Building.BonusIndustry < 0)
		{
			UE_LOG(LogWorldLeader, Error, TEXT("WLDataRegistry: edificio %s tiene coste/bonus negativo."), *Building.Id);
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
