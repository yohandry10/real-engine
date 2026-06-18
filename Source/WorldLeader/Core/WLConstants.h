// Copyright World Leader project. See ROADMAP.md.
//
// Coeficientes economicos SEMILLA para la vertical slice de Phase 0/1.
// Son provisionales y deben migrarse a un DataAsset de balance editable
// (regla del roadmap: "NUNCA hardcodear valores de balance"). Viven aqui
// temporalmente para que el calculo economico simple funcione sin editor.

#pragma once

#include "CoreMinimal.h"

namespace WLConstants
{
	// Precio de mercado simplificado: creditos por unidad de recurso base / mes.
	constexpr int32 PriceOil       = 4;
	constexpr int32 PriceGas       = 3;
	constexpr int32 PriceFood      = 1;
	constexpr int32 PriceMinerals  = 2;
	constexpr int32 IndustryValue  = 5;

	// Impuesto a la poblacion: creditos por habitante / mes.
	constexpr double PopulationTaxPerCapita = 0.0005;

	// Mantenimiento de infraestructura: upkeep ~= infraestructura * factor.
	constexpr int32 InfrastructureUpkeepFactor = 8;

	// Inicio de campania por defecto (escenario 2024 del roadmap).
	constexpr int32 StartYear  = 2024;
	constexpr int32 StartMonth = 1; // 1 = enero

	constexpr int32 MonthsPerYear = 12;
}
