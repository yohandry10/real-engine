// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "WLBalanceTypes.generated.h"

UENUM(BlueprintType)
enum class EWLAIDifficulty : uint8
{
	Easy   UMETA(DisplayName = "Facil"),
	Medium UMETA(DisplayName = "Medio"),
	Hard   UMETA(DisplayName = "Dificil")
};

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

	/** FE1.3: salarios publicos (burocracia/servicios): creditos por habitante / mes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0.0"))
	double PublicWagesPerCapita = 0.0002;

	/** FE1.3: gasto social (salud/educacion/subsidios): creditos por habitante / mes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0.0"))
	double SocialSpendingPerCapita = 0.0001;

	/** FE1.4: interes mensual sobre el tesoro negativo (0.02 = 2%/mes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DebtMonthlyInterestRate = 0.02;

	/** FE1.4: limite de credito = este multiplo del ingreso mensual total de la nacion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debt", meta = (ClampMin = "0.0"))
	double DebtCreditLimitIncomeMonths = 12.0;

	/** FE1.5: actividad economica de la poblacion para el PIB: creditos por habitante / mes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GDP", meta = (ClampMin = "0.0"))
	double GDPPerCapitaActivity = 0.002;

	/** FE2.2: fraccion de la poblacion que es fuerza laboral. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sectors", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double LaborParticipationRate = 0.45;

	/** FE2.2: trabajadores necesarios por punto de base economica para producir a pleno rendimiento. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sectors", meta = (ClampMin = "0"))
	int32 WorkersPerBasePoint = 200;

	/** FE2.2: unidades de bien producidas por punto de base economica al mes (con trabajo pleno). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sectors", meta = (ClampMin = "0.0"))
	double SectorOutputPerBasePoint = 1.0;

	/** FE2.4: empleos de servicios disponibles por habitante; el resto de la fuerza laboral queda desempleada. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sectors", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ServiceEmploymentPerCapita = 0.22;

	/** FE2.4: cuanto castiga el desempleo alto a la productividad agregada (0.25 = -25% con 100% desempleo). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sectors", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double UnemploymentProductivityPenalty = 0.25;

	/** FE3.2: sensibilidad del precio cuando la demanda supera a la oferta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0.0"))
	double PriceShortageSensitivity = 0.45;

	/** FE3.2: sensibilidad del precio cuando hay excedente de oferta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0.0"))
	double PriceSurplusSensitivity = 0.20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0.0"))
	double MinMarketPriceMultiplier = 0.45;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0.0"))
	double MaxMarketPriceMultiplier = 2.75;

	/** FE3.4: clamp inferior para shocks temporales de precio (0.5 = -50%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0.0"))
	double MinMarketShockPriceMultiplier = 0.25;

	/** FE3.4: clamp superior para shocks temporales de precio (2.0 = +100%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "0.0"))
	double MaxMarketShockPriceMultiplier = 4.0;

	/** FE3.4: duracion maxima de un shock de mercado persistido. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (ClampMin = "1"))
	int32 MaxMarketShockDurationMonths = 36;

	/** FE4.1: fraccion de deficit/superavit que puede cubrir o colocar el mercado regional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double RegionalTradeAccess = 0.65;

	/** FE4.1: cobertura abstracta del mercado global cuando la region no alcanza. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double GlobalTradeAccess = 0.20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0"))
	double ImportPriceMarkup = 0.12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ExportPriceDiscount = 0.08;

	/** FE4.2: bonus de volumen regional cuando hay acuerdo comercial bilateral. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0"))
	double TradeAgreementAccessBonus = 0.35;

	/** FE4.3: arancel inicial nacional (%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TariffRateDefaultPercent = 0;

	/** FE4.3: limite superior de aranceles nacionales (%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TariffRateMaxPercent = 50;

	/** FE4.3: cada punto de arancel reduce el volumen importable en esta fraccion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double TariffImportPenaltyPerPoint = 0.005;

	/** FE4.3: penalizacion diplomatica por punto de arancel subido via UWLPoliticalSubsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0"))
	double TariffRelationPenaltyPerPoint = 0.4;

	// --- Gabinete (Fase 3 auditoria): cada ministro tiene efecto real, escalado por su skill.
	// Factor = (skill-50)/50 (-1..+1); cargo vacante = 0 (neutro). Un ministro inepto PERJUDICA.

	/** Defensa: reduccion (o recargo) del upkeep militar a factor +1 (skill 100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	double DefenseMinisterUpkeepEffect = 0.15;

	/** Interior: puntos de orden publico al mes por provincia a factor +1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet", meta = (ClampMin = "0.0"))
	double InteriorMinisterOrderPerMonth = 2.0;

	/** Exterior: deriva mensual de opinion con cada pais a factor +1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet", meta = (ClampMin = "0.0"))
	double ForeignMinisterOpinionPerMonth = 2.0;

	/** Inteligencia: puntos de skill efectivo que suma a los espias a factor +1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cabinet", meta = (ClampMin = "0.0"))
	double IntelligenceMinisterSpyBonus = 20.0;

	// --- Condiciones de victoria (F5.3) ---

	/** Hegemonia: cuota del PIB total que hay que concentrar para ganar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victory", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	double HegemonyGDPShare = 0.65;

	/** Hegemonia: meses minimos de campana antes de poder ganar por economia. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victory", meta = (ClampMin = "0"))
	int32 HegemonyMinMonths = 12;

	/** Regimen/Legado: meses que hay que sobrevivir en el poder para ganar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victory", meta = (ClampMin = "1"))
	int32 RegimeVictoryMonths = 120;

	/** FE4.4: multiplicador de ruta cuando hay embargo/sancion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0"))
	double EmbargoTradeRouteAccessMultiplier = 0.0;

	/** FE4.5: multiplicador de ruta cuando los paises estan en guerra. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0"))
	double WarTradeRouteAccessMultiplier = 0.0;

	/** FE4.5: multiplicador de ruta cuando hay tension diplomatica sin guerra. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0.0"))
	double TensionTradeRouteAccessMultiplier = 0.75;

	/** FE5.2: convierte presion de precios agregada en inflacion mensual mostrable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro", meta = (ClampMin = "0.0"))
	double InflationPressureToMonthlyRate = 0.08;

	/** FE5.1: tasa mensual base para bonos soberanos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double BondBaseMonthlyInterestRate = 0.012;

	/** FE5.1: tasa mensual base para prestamos bilaterales entre paises. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double BilateralLoanMonthlyInterestRate = 0.006;

	/** FE5.1: tasa mensual base para programa tipo FMI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double IMFMonthlyInterestRate = 0.004;

	/** FE5.1: plazo maximo de instrumentos de deuda. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "1"))
	int32 FinancialInstrumentMaxMonths = 120;

	/** FE5.1: servicio de deuda maximo como fraccion del ingreso mensual para emitir deuda nueva. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MaxDebtServiceIncomeRatio = 0.45;

	/** FE5.1: riesgo/default cuando el servicio de deuda supera esta fraccion del ingreso mensual. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DefaultDebtServiceIncomeRatio = 0.75;

	/** FE5.1: penalizacion de orden publico al entrar en default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0", ClampMax = "100"))
	int32 DefaultPublicOrderPenalty = 8;

	/** FE5.3: opinion minima para ayuda exterior directa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "-100", ClampMax = "100"))
	int32 ForeignAidMinOpinion = 20;

	/** FE5.3: opinion minima para inversion extranjera directa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "-100", ClampMax = "100"))
	int32 ForeignInvestmentMinOpinion = 10;

	/** FE5.3: duracion maxima de ayuda exterior/FDI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "1"))
	int32 ForeignSupportMaxMonths = 36;

	/** FE5.3: ayuda mensual maxima como fraccion del ingreso mensual del receptor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ForeignAidMonthlyCapIncomeRatio = 0.25;

	/** FE5.3: FDI mensual maxima como fraccion del ingreso mensual del receptor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ForeignInvestmentMonthlyCapIncomeRatio = 0.35;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double MaxMonthlyInflationRate = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro", meta = (ClampMin = "-1.0", ClampMax = "0.0"))
	double MinMonthlyInflationRate = -0.05;

	/** FE5.4: peso del desempleo en el indicador de ciclo expansion/recesion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro", meta = (ClampMin = "0.0"))
	double CycleUnemploymentWeight = 0.35;

	/** FE5.4: peso de la inflacion en el indicador de ciclo expansion/recesion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro", meta = (ClampMin = "0.0"))
	double CycleInflationWeight = 0.50;

	/** FE5.4: peso del balance comercial sobre PIB en el ciclo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro", meta = (ClampMin = "0.0"))
	double CycleTradeBalanceWeight = 0.20;

	/** FE6.1: eficiencia fiscal por punto de skill del ministro de Economia sobre/bajo 50. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Governance", meta = (ClampMin = "0.0"))
	double EconomyMinisterTaxEfficiencyPerSkill = 0.004;

	/** FE6.2: fuga de recaudacion por punto de corrupcion sistemica. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Governance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double CorruptionTaxLeakPerPoint = 0.003;

	/** FE6.2: fraccion del presupuesto total que se pierde por punto de corrupcion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Governance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double CorruptionBudgetSkimPerPoint = 0.0015;

	/** FE6.3: productividad por punto de tecnologia sobre/bajo 50. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Governance", meta = (ClampMin = "0.0"))
	double TechnologyProductivityPerPoint = 0.004;

	/** FE6.2: penalizacion de productividad por punto de corrupcion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Governance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double CorruptionProductivityPenaltyPerPoint = 0.002;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Governance", meta = (ClampMin = "0.0"))
	double MinGovernanceProductivityMultiplier = 0.65;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Governance", meta = (ClampMin = "0.0"))
	double MaxGovernanceProductivityMultiplier = 1.35;

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

	/** B1: efecto maximo del skill del general sobre poder de combate. 0.25 = skill 100 da +25%, skill 0 da -25%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Military", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double GeneralSkillCombatEffectAtMax = 0.25;

	/** B2: velocidad base de unidades en batalla tactica abstracta. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Battle", meta = (ClampMin = "0.0"))
	double TacticalMoveSpeedUnitsPerSecond = 350.0;

	/** B2: alcance abstracto para fuego directo en batalla tactica. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Battle", meta = (ClampMin = "0.0"))
	double TacticalAttackRangeUnits = 1200.0;

	/** B2: escala de dano por punto de ataque y segundo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Battle", meta = (ClampMin = "0.0"))
	double TacticalDamagePerAttackPerSecond = 0.06;

	/** B2: cuanto mitiga cada punto de defensa tactica. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Battle", meta = (ClampMin = "0.0"))
	double TacticalDefenseMitigationPerPoint = 0.01;

	/** B2: perdida de moral por punto de salud perdido. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Battle", meta = (ClampMin = "0.0"))
	double TacticalMoraleDamagePerHealth = 0.8;

	/** B2: por debajo de esta moral la unidad entra en retirada y deja de combatir. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Battle", meta = (ClampMin = "0", ClampMax = "100"))
	int32 TacticalRoutMoraleThreshold = 15;

	/** B2: segundos necesarios para capturar un objetivo sin oposicion cercana. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Battle", meta = (ClampMin = "1.0"))
	double TacticalObjectiveCaptureSeconds = 20.0;

	/** Dificultad de la IA de campania: afecta economia, fisco, diplomacia, intriga y reclutamiento. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI Difficulty")
	EWLAIDifficulty AIDifficulty = EWLAIDifficulty::Medium;

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

	int32 GetEconomicAIMaxBuildsForDifficulty() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy:
			return FMath::Max(1, EconomicAIMaxBuildsPerNationPerMonth);
		case EWLAIDifficulty::Hard:
			return FMath::Max(1, EconomicAIMaxBuildsPerNationPerMonth + 1);
		case EWLAIDifficulty::Medium:
		default:
			return EconomicAIMaxBuildsPerNationPerMonth;
		}
	}

	int64 GetEconomicAIMinTreasuryReserveForDifficulty() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy:
			return EconomicAIMinTreasuryReserve * 2;
		case EWLAIDifficulty::Hard:
			return EconomicAIMinTreasuryReserve / 2;
		case EWLAIDifficulty::Medium:
		default:
			return EconomicAIMinTreasuryReserve;
		}
	}

	int32 GetEconomicAIMaxPaybackMonthsForDifficulty() const
	{
		if (EconomicAIMaxPaybackMonths <= 0)
		{
			return 0;
		}
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy:
			return FMath::Max(1, FMath::RoundToInt(static_cast<double>(EconomicAIMaxPaybackMonths) * 0.75));
		case EWLAIDifficulty::Hard:
			return EconomicAIMaxPaybackMonths + FMath::Max(12, EconomicAIMaxPaybackMonths / 2);
		case EWLAIDifficulty::Medium:
		default:
			return EconomicAIMaxPaybackMonths;
		}
	}

	int32 GetEconomicAIMinPublicOrderToBuildForDifficulty() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy:
			return FMath::Clamp(EconomicAIMinPublicOrderToBuild + 15, 0, 100);
		case EWLAIDifficulty::Hard:
			return FMath::Clamp(EconomicAIMinPublicOrderToBuild - 10, 0, 100);
		case EWLAIDifficulty::Medium:
		default:
			return EconomicAIMinPublicOrderToBuild;
		}
	}

	int32 GetStrategicAITaxStep() const
	{
		return AIDifficulty == EWLAIDifficulty::Hard ? 10 : 5;
	}

	int32 GetStrategicAITariffStep() const
	{
		return AIDifficulty == EWLAIDifficulty::Hard ? 10 : 5;
	}

	int32 GetStrategicAITariffCeiling() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 15;
		case EWLAIDifficulty::Hard: return 30;
		case EWLAIDifficulty::Medium:
		default: return 20;
		}
	}

	int32 GetStrategicAITreatyOpinionOffset() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 10;
		case EWLAIDifficulty::Hard: return -10;
		case EWLAIDifficulty::Medium:
		default: return 0;
		}
	}

	int32 GetStrategicAIWarOpinionThreshold() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return -80;
		case EWLAIDifficulty::Hard: return -45;
		case EWLAIDifficulty::Medium:
		default: return -60;
		}
	}

	double GetStrategicAIWarStrengthRatio() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 1.75;
		case EWLAIDifficulty::Hard: return 1.10;
		case EWLAIDifficulty::Medium:
		default: return 1.33;
		}
	}

	double GetStrategicAIPeaceStrengthRatio() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 0.85;
		case EWLAIDifficulty::Hard: return 0.35;
		case EWLAIDifficulty::Medium:
		default: return 0.50;
		}
	}

	int32 GetStrategicAIIntrigueOpinionThreshold() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return -45;
		case EWLAIDifficulty::Hard: return -10;
		case EWLAIDifficulty::Medium:
		default: return -20;
		}
	}

	int32 GetStrategicAISpyNetworkTarget() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 20;
		case EWLAIDifficulty::Hard: return 45;
		case EWLAIDifficulty::Medium:
		default: return 30;
		}
	}

	int32 GetStrategicAISpyExposureLimit() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 35;
		case EWLAIDifficulty::Hard: return 75;
		case EWLAIDifficulty::Medium:
		default: return 55;
		}
	}

	int64 GetStrategicAIRecruitTreasuryThreshold() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 75000;
		case EWLAIDifficulty::Hard: return 25000;
		case EWLAIDifficulty::Medium:
		default: return 40000;
		}
	}

	double GetStrategicAIRecruitStrengthRatio() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return 0.75;
		case EWLAIDifficulty::Hard: return 1.15;
		case EWLAIDifficulty::Medium:
		default: return 1.0;
		}
	}

	FString GetAIDifficultyId() const
	{
		switch (AIDifficulty)
		{
		case EWLAIDifficulty::Easy: return TEXT("easy");
		case EWLAIDifficulty::Hard: return TEXT("hard");
		case EWLAIDifficulty::Medium:
		default: return TEXT("medium");
		}
	}

	FWLBalanceRules Sanitized() const
	{
		FWLBalanceRules Out = *this;
		switch (Out.AIDifficulty)
		{
		case EWLAIDifficulty::Easy:
		case EWLAIDifficulty::Medium:
		case EWLAIDifficulty::Hard:
			break;
		default:
			Out.AIDifficulty = EWLAIDifficulty::Medium;
			break;
		}
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
		Out.PublicWagesPerCapita = FMath::Max(0.0, Out.PublicWagesPerCapita);
		Out.SocialSpendingPerCapita = FMath::Max(0.0, Out.SocialSpendingPerCapita);
		Out.DebtMonthlyInterestRate = FMath::Clamp(Out.DebtMonthlyInterestRate, 0.0, 1.0);
		Out.DebtCreditLimitIncomeMonths = FMath::Max(0.0, Out.DebtCreditLimitIncomeMonths);
		Out.GDPPerCapitaActivity = FMath::Max(0.0, Out.GDPPerCapitaActivity);
		Out.LaborParticipationRate = FMath::Clamp(Out.LaborParticipationRate, 0.0, 1.0);
		Out.WorkersPerBasePoint = FMath::Max(0, Out.WorkersPerBasePoint);
		Out.SectorOutputPerBasePoint = FMath::Max(0.0, Out.SectorOutputPerBasePoint);
		Out.ServiceEmploymentPerCapita = FMath::Clamp(Out.ServiceEmploymentPerCapita, 0.0, 1.0);
		Out.UnemploymentProductivityPenalty = FMath::Clamp(Out.UnemploymentProductivityPenalty, 0.0, 1.0);
		Out.PriceShortageSensitivity = FMath::Max(0.0, Out.PriceShortageSensitivity);
		Out.PriceSurplusSensitivity = FMath::Max(0.0, Out.PriceSurplusSensitivity);
		Out.MinMarketPriceMultiplier = FMath::Max(0.0, Out.MinMarketPriceMultiplier);
		Out.MaxMarketPriceMultiplier = FMath::Max(Out.MinMarketPriceMultiplier, Out.MaxMarketPriceMultiplier);
		Out.MinMarketShockPriceMultiplier = FMath::Max(0.0, Out.MinMarketShockPriceMultiplier);
		Out.MaxMarketShockPriceMultiplier = FMath::Max(Out.MinMarketShockPriceMultiplier, Out.MaxMarketShockPriceMultiplier);
		Out.MaxMarketShockDurationMonths = FMath::Max(1, Out.MaxMarketShockDurationMonths);
		Out.RegionalTradeAccess = FMath::Clamp(Out.RegionalTradeAccess, 0.0, 1.0);
		Out.GlobalTradeAccess = FMath::Clamp(Out.GlobalTradeAccess, 0.0, 1.0);
		Out.ImportPriceMarkup = FMath::Max(0.0, Out.ImportPriceMarkup);
		Out.ExportPriceDiscount = FMath::Clamp(Out.ExportPriceDiscount, 0.0, 1.0);
		Out.TradeAgreementAccessBonus = FMath::Max(0.0, Out.TradeAgreementAccessBonus);
		Out.TariffRateMaxPercent = FMath::Clamp(Out.TariffRateMaxPercent, 0, 100);
		Out.TariffRateDefaultPercent = FMath::Clamp(Out.TariffRateDefaultPercent, 0, Out.TariffRateMaxPercent);
		Out.TariffImportPenaltyPerPoint = FMath::Clamp(Out.TariffImportPenaltyPerPoint, 0.0, 1.0);
		Out.TariffRelationPenaltyPerPoint = FMath::Max(0.0, Out.TariffRelationPenaltyPerPoint);
		Out.DefenseMinisterUpkeepEffect = FMath::Clamp(Out.DefenseMinisterUpkeepEffect, 0.0, 0.9);
		Out.HegemonyGDPShare = FMath::Clamp(Out.HegemonyGDPShare, 0.5, 1.0);
		Out.HegemonyMinMonths = FMath::Max(0, Out.HegemonyMinMonths);
		Out.RegimeVictoryMonths = FMath::Max(1, Out.RegimeVictoryMonths);
		Out.InteriorMinisterOrderPerMonth = FMath::Max(0.0, Out.InteriorMinisterOrderPerMonth);
		Out.ForeignMinisterOpinionPerMonth = FMath::Max(0.0, Out.ForeignMinisterOpinionPerMonth);
		Out.IntelligenceMinisterSpyBonus = FMath::Max(0.0, Out.IntelligenceMinisterSpyBonus);
		Out.EmbargoTradeRouteAccessMultiplier = FMath::Max(0.0, Out.EmbargoTradeRouteAccessMultiplier);
		Out.WarTradeRouteAccessMultiplier = FMath::Max(0.0, Out.WarTradeRouteAccessMultiplier);
		Out.TensionTradeRouteAccessMultiplier = FMath::Max(0.0, Out.TensionTradeRouteAccessMultiplier);
		Out.InflationPressureToMonthlyRate = FMath::Max(0.0, Out.InflationPressureToMonthlyRate);
		Out.BondBaseMonthlyInterestRate = FMath::Clamp(Out.BondBaseMonthlyInterestRate, 0.0, 1.0);
		Out.BilateralLoanMonthlyInterestRate = FMath::Clamp(Out.BilateralLoanMonthlyInterestRate, 0.0, 1.0);
		Out.IMFMonthlyInterestRate = FMath::Clamp(Out.IMFMonthlyInterestRate, 0.0, 1.0);
		Out.FinancialInstrumentMaxMonths = FMath::Max(1, Out.FinancialInstrumentMaxMonths);
		Out.MaxDebtServiceIncomeRatio = FMath::Clamp(Out.MaxDebtServiceIncomeRatio, 0.0, 1.0);
		Out.DefaultDebtServiceIncomeRatio = FMath::Clamp(Out.DefaultDebtServiceIncomeRatio, 0.0, 1.0);
		Out.DefaultPublicOrderPenalty = FMath::Clamp(Out.DefaultPublicOrderPenalty, 0, 100);
		Out.ForeignAidMinOpinion = FMath::Clamp(Out.ForeignAidMinOpinion, -100, 100);
		Out.ForeignInvestmentMinOpinion = FMath::Clamp(Out.ForeignInvestmentMinOpinion, -100, 100);
		Out.ForeignSupportMaxMonths = FMath::Max(1, Out.ForeignSupportMaxMonths);
		Out.ForeignAidMonthlyCapIncomeRatio = FMath::Clamp(Out.ForeignAidMonthlyCapIncomeRatio, 0.0, 1.0);
		Out.ForeignInvestmentMonthlyCapIncomeRatio = FMath::Clamp(Out.ForeignInvestmentMonthlyCapIncomeRatio, 0.0, 1.0);
		Out.MaxMonthlyInflationRate = FMath::Clamp(Out.MaxMonthlyInflationRate, 0.0, 1.0);
		Out.MinMonthlyInflationRate = FMath::Clamp(Out.MinMonthlyInflationRate, -1.0, 0.0);
		Out.CycleUnemploymentWeight = FMath::Max(0.0, Out.CycleUnemploymentWeight);
		Out.CycleInflationWeight = FMath::Max(0.0, Out.CycleInflationWeight);
		Out.CycleTradeBalanceWeight = FMath::Max(0.0, Out.CycleTradeBalanceWeight);
		Out.EconomyMinisterTaxEfficiencyPerSkill = FMath::Max(0.0, Out.EconomyMinisterTaxEfficiencyPerSkill);
		Out.CorruptionTaxLeakPerPoint = FMath::Clamp(Out.CorruptionTaxLeakPerPoint, 0.0, 1.0);
		Out.CorruptionBudgetSkimPerPoint = FMath::Clamp(Out.CorruptionBudgetSkimPerPoint, 0.0, 1.0);
		Out.TechnologyProductivityPerPoint = FMath::Max(0.0, Out.TechnologyProductivityPerPoint);
		Out.CorruptionProductivityPenaltyPerPoint = FMath::Clamp(Out.CorruptionProductivityPenaltyPerPoint, 0.0, 1.0);
		Out.MinGovernanceProductivityMultiplier = FMath::Max(0.0, Out.MinGovernanceProductivityMultiplier);
		Out.MaxGovernanceProductivityMultiplier = FMath::Max(Out.MinGovernanceProductivityMultiplier, Out.MaxGovernanceProductivityMultiplier);
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
		Out.GeneralSkillCombatEffectAtMax = FMath::Clamp(Out.GeneralSkillCombatEffectAtMax, 0.0, 1.0);
		Out.TacticalMoveSpeedUnitsPerSecond = FMath::Max(0.0, Out.TacticalMoveSpeedUnitsPerSecond);
		Out.TacticalAttackRangeUnits = FMath::Max(0.0, Out.TacticalAttackRangeUnits);
		Out.TacticalDamagePerAttackPerSecond = FMath::Max(0.0, Out.TacticalDamagePerAttackPerSecond);
		Out.TacticalDefenseMitigationPerPoint = FMath::Max(0.0, Out.TacticalDefenseMitigationPerPoint);
		Out.TacticalMoraleDamagePerHealth = FMath::Max(0.0, Out.TacticalMoraleDamagePerHealth);
		Out.TacticalRoutMoraleThreshold = FMath::Clamp(Out.TacticalRoutMoraleThreshold, 0, 100);
		Out.TacticalObjectiveCaptureSeconds = FMath::Max(1.0, Out.TacticalObjectiveCaptureSeconds);
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
