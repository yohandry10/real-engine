// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "WLBalanceTypes.generated.h"

/**
 * Reglas editables de balance economico/campania. Estos valores no deben
 * vivir dispersos en gameplay: el runtime los lee desde el balance subsystem,
 * que puede tomar un DataAsset configurado desde Project Settings.
 */
USTRUCT(BlueprintType)
struct FWLBalanceRules
{
	GENERATED_BODY()

	/** Precio de mercado simplificado: creditos por unidad de recurso base / mes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0"))
	int32 OilPrice = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0"))
	int32 GasPrice = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0"))
	int32 FoodPrice = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0"))
	int32 MineralsPrice = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0"))
	int32 IndustryValue = 5;

	/** Impuesto a la poblacion: creditos por habitante / mes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population", meta = (ClampMin = "0.0"))
	double PopulationTaxPerCapita = 0.0005;

	/** Crecimiento demografico mensual base. 0.001 equivale a ~1.2% anual compuesto. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population", meta = (ClampMin = "0.0"))
	double MonthlyPopulationGrowthRate = 0.001;

	/** FE1.2: tasa de impuestos con la que arranca cada nacion (% 0..100). A esta tasa la recaudacion es la base (x1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taxes", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TaxRateDefaultPercent = 30;

	/** Limite inferior de la palanca de impuestos (%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taxes", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TaxRateMinPercent = 10;

	/** Limite superior de la palanca de impuestos (%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taxes", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TaxRateMaxPercent = 60;

	/** Curva Laffer: fraccion de la recaudacion que se evade/pierde a tasa 100%. 0 = lineal; 0.7 = pico en ~71%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taxes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double TaxLafferEvasionAtFullRate = 0.7;

	/** Orden publico que drena cada mes cada punto de impuesto sobre el default (por debajo, lo recupera). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taxes", meta = (ClampMin = "0.0"))
	double TaxPublicOrderPerPointPerMonth = 0.15;

	/** Orden publico inicial para provincias sin estado guardado. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "0", ClampMax = "100"))
	int32 InitialPublicOrder = 72;

	/** Punto neutro: por encima hay bonus economico, por debajo hay penalizacion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "1", ClampMax = "100"))
	int32 PublicOrderNeutral = 70;

	/** Penalizacion maxima de ingresos cuando el orden publico cae a 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double LowOrderIncomePenaltyAtZero = 0.35;

	/** Bonus maximo de ingresos cuando el orden publico llega a 100. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double HighOrderIncomeBonusAtMax = 0.10;

	/** Deriva mensual hacia el punto neutro de orden publico. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "0"))
	int32 PublicOrderDriftPerMonth = 1;

	/** Penalizacion mensual si el balance provincial sale negativo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "0"))
	int32 PublicOrderDeficitPenalty = 1;

	/** Penalizacion mensual si el tesoro nacional queda en negativo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "0"))
	int32 PublicOrderBankruptcyPenalty = 2;

	/** Penalizacion inmediata al orden publico cuando cambia el controlador de una provincia. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Public Order", meta = (ClampMin = "0", ClampMax = "100"))
	int32 OccupationPublicOrderPenalty = 15;

	/** Mantenimiento de infraestructura: upkeep = infraestructura * factor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infrastructure", meta = (ClampMin = "0"))
	int32 InfrastructureUpkeepFactor = 8;

	/** Upkeep militar mensual por punto de efectivo (fuerza). 0 = ejercitos gratis (estado previo a FE1.1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Military", meta = (ClampMin = "0.0"))
	double MilitaryUpkeepPerStrength = 0.35;

	/** Activa la IA economica mensual para naciones no controladas por el jugador. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economic AI")
	bool bEnableEconomicAI = true;

	/** Reserva minima que la IA no debe gastar al construir. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economic AI", meta = (ClampMin = "0"))
	int64 EconomicAIMinTreasuryReserve = 5000;

	/** Limite de construcciones que una nacion IA puede iniciar por mes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economic AI", meta = (ClampMin = "0"))
	int32 EconomicAIMaxBuildsPerNationPerMonth = 1;

	/** Retorno maximo aceptable para inversiones de IA; 0 desactiva el filtro. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economic AI", meta = (ClampMin = "0"))
	int32 EconomicAIMaxPaybackMonths = 72;

	/** La IA no construye en provincias demasiado inestables. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economic AI", meta = (ClampMin = "0", ClampMax = "100"))
	int32 EconomicAIMinPublicOrderToBuild = 35;

	/** Inicio de campania por defecto (escenario 2024 del roadmap). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Calendar", meta = (ClampMin = "1"))
	int32 StartYear = 2024;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Calendar", meta = (ClampMin = "1"))
	int32 StartMonth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Calendar", meta = (ClampMin = "1"))
	int32 MonthsPerYear = 12;

	FWLBalanceRules Sanitized() const
	{
		FWLBalanceRules Out = *this;
		Out.OilPrice = FMath::Max(0, Out.OilPrice);
		Out.GasPrice = FMath::Max(0, Out.GasPrice);
		Out.FoodPrice = FMath::Max(0, Out.FoodPrice);
		Out.MineralsPrice = FMath::Max(0, Out.MineralsPrice);
		Out.IndustryValue = FMath::Max(0, Out.IndustryValue);
		Out.PopulationTaxPerCapita = FMath::Max(0.0, Out.PopulationTaxPerCapita);
		Out.MonthlyPopulationGrowthRate = FMath::Max(0.0, Out.MonthlyPopulationGrowthRate);
		Out.TaxRateMinPercent = FMath::Clamp(Out.TaxRateMinPercent, 0, 100);
		Out.TaxRateMaxPercent = FMath::Clamp(Out.TaxRateMaxPercent, Out.TaxRateMinPercent, 100);
		Out.TaxRateDefaultPercent = FMath::Clamp(Out.TaxRateDefaultPercent, Out.TaxRateMinPercent, Out.TaxRateMaxPercent);
		Out.TaxLafferEvasionAtFullRate = FMath::Clamp(Out.TaxLafferEvasionAtFullRate, 0.0, 1.0);
		Out.TaxPublicOrderPerPointPerMonth = FMath::Max(0.0, Out.TaxPublicOrderPerPointPerMonth);
		Out.InitialPublicOrder = FMath::Clamp(Out.InitialPublicOrder, 0, 100);
		Out.PublicOrderNeutral = FMath::Clamp(Out.PublicOrderNeutral, 1, 100);
		Out.LowOrderIncomePenaltyAtZero = FMath::Clamp(Out.LowOrderIncomePenaltyAtZero, 0.0, 1.0);
		Out.HighOrderIncomeBonusAtMax = FMath::Clamp(Out.HighOrderIncomeBonusAtMax, 0.0, 1.0);
		Out.PublicOrderDriftPerMonth = FMath::Max(0, Out.PublicOrderDriftPerMonth);
		Out.PublicOrderDeficitPenalty = FMath::Max(0, Out.PublicOrderDeficitPenalty);
		Out.PublicOrderBankruptcyPenalty = FMath::Max(0, Out.PublicOrderBankruptcyPenalty);
		Out.OccupationPublicOrderPenalty = FMath::Clamp(Out.OccupationPublicOrderPenalty, 0, 100);
		Out.InfrastructureUpkeepFactor = FMath::Max(0, Out.InfrastructureUpkeepFactor);
		Out.MilitaryUpkeepPerStrength = FMath::Max(0.0, Out.MilitaryUpkeepPerStrength);
		Out.EconomicAIMinTreasuryReserve = FMath::Max<int64>(0, Out.EconomicAIMinTreasuryReserve);
		Out.EconomicAIMaxBuildsPerNationPerMonth = FMath::Max(0, Out.EconomicAIMaxBuildsPerNationPerMonth);
		Out.EconomicAIMaxPaybackMonths = FMath::Max(0, Out.EconomicAIMaxPaybackMonths);
		Out.EconomicAIMinPublicOrderToBuild = FMath::Clamp(Out.EconomicAIMinPublicOrderToBuild, 0, 100);
		Out.StartYear = FMath::Max(1, Out.StartYear);
		Out.MonthsPerYear = FMath::Max(1, Out.MonthsPerYear);
		Out.StartMonth = FMath::Clamp(Out.StartMonth, 1, Out.MonthsPerYear);
		return Out;
	}

	static FWLBalanceRules Default()
	{
		return FWLBalanceRules().Sanitized();
	}
};
