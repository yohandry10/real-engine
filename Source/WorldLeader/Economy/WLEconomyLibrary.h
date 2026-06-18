// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/WLGameTypes.h"
#include "WLEconomyLibrary.generated.h"

/**
 * Calculo economico provincial simplificado (Phase 0/1).
 * Implementa una version reducida de las formulas del roadmap
 * (seccion "ECONOMIA"): ingreso por recursos + impuestos, menos upkeep.
 */
UCLASS()
class WORLDLEADER_API UWLEconomyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Ingreso bruto mensual de una provincia (recursos + impuestos + industria). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceIncome(const FWLProvinceData& Province);

	/** Gasto mensual de una provincia (mantenimiento de infraestructura). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceUpkeep(const FWLProvinceData& Province);

	/** Balance neto mensual = ingreso - upkeep. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceBalance(const FWLProvinceData& Province);

	/** Ingreso mensual extra que aporta un edificio (sus bonus de recursos). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateBuildingIncome(const FWLBuildingData& Building);
};
