// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Balance/WLBalanceTypes.h"
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

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceIncomeWithRules(const FWLProvinceData& Province, const FWLBalanceRules& Rules);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceIncomeWithState(
		const FWLProvinceData& Province,
		const FWLBalanceRules& Rules,
		int64 RuntimePopulation,
		int32 PublicOrder);

	/** Gasto mensual de una provincia (mantenimiento de infraestructura). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceUpkeep(const FWLProvinceData& Province);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceUpkeepWithRules(const FWLProvinceData& Province, const FWLBalanceRules& Rules);

	/** Balance neto mensual = ingreso - upkeep. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceBalance(const FWLProvinceData& Province);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceBalanceWithRules(const FWLProvinceData& Province, const FWLBalanceRules& Rules);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvinceBalanceWithState(
		const FWLProvinceData& Province,
		const FWLBalanceRules& Rules,
		int64 RuntimePopulation,
		int32 PublicOrder);

	/** Ingreso mensual extra que aporta un edificio (sus bonus de recursos). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateBuildingIncome(const FWLBuildingData& Building);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateBuildingIncomeWithRules(const FWLBuildingData& Building, const FWLBalanceRules& Rules);

	/** Solo la parte de impuestos a la poblacion del ingreso provincial (a tasa default). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 CalculateProvincePopulationTax(const FWLProvinceData& Province, const FWLBalanceRules& Rules);

	/**
	 * FE1.2: multiplicador de recaudacion segun la tasa de impuestos (curva tipo Laffer).
	 * Normalizado: a TaxRateDefaultPercent devuelve 1.0; subir la tasa rinde cada vez menos.
	 */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static double CalculateTaxRateIncomeMultiplier(int32 TaxRatePercent, const FWLBalanceRules& Rules);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static double CalculatePublicOrderIncomeMultiplier(int32 PublicOrder, const FWLBalanceRules& Rules);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	static int64 ApplyPublicOrderIncomeModifier(int64 GrossIncome, int32 PublicOrder, const FWLBalanceRules& Rules);
};
