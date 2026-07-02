// Copyright World Leader project. See ROADMAP.md.
//
// Tipos de datos compartidos del juego. NUNCA hardcodear balance aqui:
// estos structs solo describen la forma de los datos que se cargan desde
// JSON / DataTables (ver Content/Data/ y WLDataRegistry).

#pragma once

#include "CoreMinimal.h"
#include "WLGameTypes.generated.h"

/** Tipo de terreno de una provincia (afecta movimiento y combate). */
UENUM(BlueprintType)
enum class EWLTerrainType : uint8
{
	Plain     UMETA(DisplayName = "Plano"),
	Mountain  UMETA(DisplayName = "Montana"),
	Desert    UMETA(DisplayName = "Desierto"),
	Jungle    UMETA(DisplayName = "Selva"),
	Coastal   UMETA(DisplayName = "Costera"),
	Urban     UMETA(DisplayName = "Urbana"),
	Arctic    UMETA(DisplayName = "Artica"),
	Maritime  UMETA(DisplayName = "Maritima")
};

/**
 * Datos estaticos de una provincia. Espejo del formato JSON descrito en el
 * roadmap (seccion "Formato de provincia"). Es solo lectura en runtime; el
 * estado mutable (ocupacion, edificios, orden publico) vivira en structs de
 * estado aparte en fases posteriores.
 */
USTRUCT(BlueprintType)
struct FWLProvinceData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Province") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Province") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Province") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "Province") FString Region;
	UPROPERTY(BlueprintReadOnly, Category = "Province") FString Capital;
	UPROPERTY(BlueprintReadOnly, Category = "Province") EWLTerrainType Terrain = EWLTerrainType::Plain;

	UPROPERTY(BlueprintReadOnly, Category = "Resources") int32 BaseOil = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Resources") int32 BaseGas = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Resources") int32 BaseFood = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Resources") int32 BaseMinerals = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Resources") int32 BaseIndustry = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Province") int64 Population = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Province") int32 Infrastructure = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Province") int32 StrategicValue = 0;

	/** Coordenadas geograficas aproximadas para el mapa estrategico. */
	UPROPERTY(BlueprintReadOnly, Category = "Map") float MapLat = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Map") float MapLon = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Province") bool bIsCapital = false;
	UPROPERTY(BlueprintReadOnly, Category = "Province") bool bHasPort = false;
	UPROPERTY(BlueprintReadOnly, Category = "Province") bool bHasAirbase = false;

	/** IDs de provincias conectadas por tierra/mar. */
	UPROPERTY(BlueprintReadOnly, Category = "Province") TArray<FString> Neighbors;

	bool IsValid() const { return !Id.IsEmpty(); }
};

/** Estado mutable de una provincia durante la campania. */
USTRUCT(BlueprintType)
struct FWLProvinceRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Province") FString ProvinceId;
	UPROPERTY(BlueprintReadOnly, Category = "Province") FString ControllerIso;
	UPROPERTY(BlueprintReadOnly, Category = "Province") int64 Population = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Province") int32 PublicOrder = 70;

	bool IsValid() const { return !ProvinceId.IsEmpty(); }
};

/**
 * Datos estaticos de una nacion jugable / visible. El tesoro y el balance
 * mensual son estado de runtime y los gestiona WLStrategicTickSubsystem.
 */
USTRUCT(BlueprintType)
struct FWLNationData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Nation") FString Iso;
	UPROPERTY(BlueprintReadOnly, Category = "Nation") FString Name;
	/** ID de la provincia capital del pais. */
	UPROPERTY(BlueprintReadOnly, Category = "Nation") FString CapitalProvinceId;
	UPROPERTY(BlueprintReadOnly, Category = "Nation") int64 StartingTreasury = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Nation") FString GovernmentType;
	/** Jefe de Estado (presidente). Opcional en datos; la UI usa un titulo por defecto si viene vacio. */
	UPROPERTY(BlueprintReadOnly, Category = "Nation") FString Leader;
	UPROPERTY(BlueprintReadOnly, Category = "Nation") FLinearColor MapColor = FLinearColor::Gray;

	bool IsValid() const { return !Iso.IsEmpty(); }
};

/** Ranura de construccion a la que pertenece un edificio (ver roadmap). */
UENUM(BlueprintType)
enum class EWLBuildingSlot : uint8
{
	Economic       UMETA(DisplayName = "Economico"),
	Industrial     UMETA(DisplayName = "Industrial"),
	Military       UMETA(DisplayName = "Militar"),
	Naval          UMETA(DisplayName = "Naval"),
	Air            UMETA(DisplayName = "Aereo"),
	Tech           UMETA(DisplayName = "Tecnologico"),
	Financial      UMETA(DisplayName = "Financiero"),
	Infrastructure UMETA(DisplayName = "Infraestructura"),
	Defensive      UMETA(DisplayName = "Defensivo")
};

/**
 * Definicion estatica de un edificio construible. Se carga desde
 * Content/Data/Buildings/Buildings.json. El coste se descuenta del tesoro de
 * la nacion al construir; los bonus de recursos se suman al ingreso de la
 * provincia (ver WLEconomyLibrary::CalculateBuildingIncome).
 */
USTRUCT(BlueprintType)
struct FWLBuildingData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Building") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Building") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Building") EWLBuildingSlot Slot = EWLBuildingSlot::Economic;
	UPROPERTY(BlueprintReadOnly, Category = "Building") int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Building") int32 BonusOil = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Building") int32 BonusGas = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Building") int32 BonusFood = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Building") int32 BonusMinerals = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Building") int32 BonusIndustry = 0;

	bool IsValid() const { return !Id.IsEmpty(); }
};

/** FE2.1: categoria de un bien economico. */
UENUM(BlueprintType)
enum class EWLGoodCategory : uint8
{
	Raw          UMETA(DisplayName = "Crudo"),
	Manufactured UMETA(DisplayName = "Manufacturado")
};

/**
 * FE2.1: definicion estatica de un bien economico. Se carga desde
 * Content/Data/Goods/Goods.json. Los manufacturados declaran sus insumos
 * (Inputs) para las cadenas de produccion (FE2.3).
 */
USTRUCT(BlueprintType)
struct FWLGoodData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Good") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Good") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Good") EWLGoodCategory Category = EWLGoodCategory::Raw;
	UPROPERTY(BlueprintReadOnly, Category = "Good") int32 BasePrice = 0;
	/** IDs de bienes necesarios para producirlo (vacio en crudos). */
	UPROPERTY(BlueprintReadOnly, Category = "Good") TArray<FString> Inputs;
	/** FE2.2: fraccion de la produccion industrial que sale como este bien (solo manufacturados). */
	UPROPERTY(BlueprintReadOnly, Category = "Good") double IndustryShare = 0.0;

	bool IsValid() const { return !Id.IsEmpty(); }
};

/** Tipo de unidad militar (ver roadmap "Tipos de fuerzas"). */
UENUM(BlueprintType)
enum class EWLUnitType : uint8
{
	Infantry      UMETA(DisplayName = "Infanteria"),
	Armor         UMETA(DisplayName = "Blindado"),
	Artillery     UMETA(DisplayName = "Artilleria"),
	AirDefense    UMETA(DisplayName = "Defensa aerea"),
	Air           UMETA(DisplayName = "Aviacion"),
	Naval         UMETA(DisplayName = "Naval"),
	Drone         UMETA(DisplayName = "Drone"),
	SpecialForces UMETA(DisplayName = "Fuerzas especiales")
};

/** Definicion estatica de un tipo de unidad. Se carga desde Content/Data/Units/Units.json. */
USTRUCT(BlueprintType)
struct FWLUnitData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Unit") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") EWLUnitType Type = EWLUnitType::Infantry;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") int32 Attack = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") int32 Defense = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") int32 Strength = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Unit") int64 Cost = 0;

	bool IsValid() const { return !Id.IsEmpty(); }
};

/** Un ejercito: stack de unidades de una nacion, situado en una provincia. */
USTRUCT(BlueprintType)
struct FWLArmy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Army") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Army") FString OwnerIso;
	UPROPERTY(BlueprintReadOnly, Category = "Army") FString ProvinceId;
	UPROPERTY(BlueprintReadOnly, Category = "Army") FString General;
	/** IDs de tipos de unidad que componen el ejercito. */
	UPROPERTY(BlueprintReadOnly, Category = "Army") TArray<FString> Units;

	bool IsValid() const { return !Id.IsEmpty(); }
};

/** Resultado de una batalla por auto-resolucion (ver roadmap "Modo A"). */
UENUM(BlueprintType)
enum class EWLBattleResult : uint8
{
	Invalid                 UMETA(DisplayName = "Invalido"),
	AttackerDecisiveVictory UMETA(DisplayName = "Victoria decisiva atacante"),
	AttackerVictory         UMETA(DisplayName = "Victoria atacante"),
	Stalemate               UMETA(DisplayName = "Empate tactico"),
	DefenderVictory         UMETA(DisplayName = "Victoria defensor"),
	DefenderDecisiveVictory UMETA(DisplayName = "Victoria decisiva defensor")
};
