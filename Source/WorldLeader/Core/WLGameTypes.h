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

	UPROPERTY(BlueprintReadOnly, Category = "Province") bool bIsCapital = false;
	UPROPERTY(BlueprintReadOnly, Category = "Province") bool bHasPort = false;
	UPROPERTY(BlueprintReadOnly, Category = "Province") bool bHasAirbase = false;

	/** IDs de provincias conectadas por tierra/mar. */
	UPROPERTY(BlueprintReadOnly, Category = "Province") TArray<FString> Neighbors;

	bool IsValid() const { return !Id.IsEmpty(); }
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
	UPROPERTY(BlueprintReadOnly, Category = "Nation") FLinearColor MapColor = FLinearColor::Gray;

	bool IsValid() const { return !Iso.IsEmpty(); }
};
