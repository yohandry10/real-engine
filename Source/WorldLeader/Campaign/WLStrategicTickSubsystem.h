// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Balance/WLBalanceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/WLLocalSaveGame.h"
#include "WLStrategicTickSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWLOnMonthAdvanced, int32, Year, int32, Month);

/** FE1.3: presupuesto mensual de una nacion desglosado por categorias (todo en creditos/mes). */
USTRUCT(BlueprintType)
struct FWLNationBudget
{
	GENERATED_BODY()

	// Ingresos
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ResourceIncome = 0;     // recursos, industria y edificios (tras orden publico)

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 TaxIncome = 0;          // impuesto poblacional con la tasa vigente (tras orden publico)

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ExportIncome = 0;       // FE4.1: ventas de excedentes al mercado regional/global

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 TariffIncome = 0;       // FE4.3: aranceles cobrados sobre importaciones

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ForeignAidIncome = 0;   // FE5.3: ayuda exterior que entra al tesoro

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ForeignInvestmentInflow = 0;   // FE5.3: FDI activo; no entra directo al tesoro

	// Gastos
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 MilitaryUpkeep = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 InfrastructureUpkeep = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 PublicWages = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 SocialSpending = 0;

	/** FE1.4: interes mensual de la deuda (0 si el tesoro no es negativo). */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 DebtInterest = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 DebtService = 0;        // FE5.1: pagos mensuales de bonos/prestamos/FMI

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ImportCost = 0;         // FE4.1: compras de deficit de bienes criticos

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 CorruptionLoss = 0;     // FE6.2: filtracion del presupuesto por corrupcion sistemica

	int64 TotalIncome() const { return ResourceIncome + TaxIncome + ExportIncome + TariffIncome + ForeignAidIncome; }
	int64 TotalSpending() const { return MilitaryUpkeep + InfrastructureUpkeep + PublicWages + SocialSpending + DebtInterest + DebtService + ImportCost + CorruptionLoss; }
	int64 Net() const { return TotalIncome() - TotalSpending(); }
};

/** FE2.2: unidades producidas de un bien al mes. */
USTRUCT(BlueprintType)
struct FWLGoodOutput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	FString GoodId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Units = 0;
};

/** FE2.2: empleo de una provincia repartido por sector. */
USTRUCT(BlueprintType)
struct FWLSectorEmployment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Workforce = 0;        // poblacion activa (poblacion x participacion laboral)

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Extraction = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Manufacturing = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Services = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Unemployed = 0;

	/** 0..1: cuanta de la mano de obra que piden extraccion+manufactura queda cubierta. */
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double LaborCoverage = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double UnemploymentRate = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double Productivity = 1.0;
};

/** FE3-FE4: balance nacional de un bien tras produccion, demanda, precio y comercio. */
USTRUCT(BlueprintType)
struct FWLGoodMarketBalance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	FString GoodId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Production = 0;       // produccion bruta real; puede usarse como insumo

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 UsedAsInput = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 DomesticSupply = 0;   // produccion final disponible antes de comercio

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Demand = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Imports = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Exports = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Supply = 0;           // oferta interna despues de importar y exportar

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Deficit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Surplus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double UnitPrice = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double PriceMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double MarketShockMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ProductionValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ImportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ImportTariffIncome = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 ExportRevenue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int32 TariffRatePercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double TariffImportVolumeMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double RegionalTradeRouteMultiplier = 1.0;
};

/** FE2.4: empleo nacional agregado para mostrar desempleo y productividad. */
USTRUCT(BlueprintType)
struct FWLNationLaborStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Workforce = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Employed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int64 Unemployed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double UnemploymentRate = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double Productivity = 1.0;
};

/** FE6: gobernanza economica derivada del gabinete y la tecnologia provincial. */
USTRUCT(BlueprintType)
struct FWLEconomicGovernanceStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	FString EconomyMinisterId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	FString EconomyMinisterName;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int32 EconomyMinisterSkill = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int32 SystemicCorruption = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int32 TechnologyLevel = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	int32 InvestmentClimate = 50;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double TaxCollectionMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double ProductivityMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Economy")
	double CorruptionSkimRate = 0.0;
};

/** Backend de edificios provinciales: efectos agregados de todos los slots construidos. */
USTRUCT(BlueprintType)
struct FWLProvinceBuildingEffects
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 TotalLevels = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int64 MonthlyIncome = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int64 MonthlyUpkeep = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusOil = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusGas = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusFood = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusMinerals = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusIndustry = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int64 BonusFinancialIncome = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusInfrastructure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusPublicOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusRecruitmentCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusMilitaryPower = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusDefense = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusAirCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusNavalCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Construction")
	int32 BonusTechnology = 0;
};

struct FWLProductionLedger
{
	TMap<FString, int64> Produced;
	TMap<FString, int64> UsedAsInput;
	TMap<FString, int64> FinalSupply;
};

// --- Reclutamiento de tropas por turnos (estilo Total War) ---
struct FWLRecruitOption   // una entrada del catalogo (RecruitableUnits.json)
{
	FString UnitType;     // mbt|ifv|apc|infantry|artillery|heli|aircraft|ship
	FString Label;
	FString Category;     // land|air|naval
	int32   Batch = 0;    // efectivos que produce una orden completada
	int64   Cost = 0;     // coste en tesoro
	int32   Turns = 0;    // turnos que tarda
};

struct FWLRecruitOrder    // una orden en la cola de una base
{
	FString UnitType;
	FString Label;
	int32   Batch = 0;
	int32   TurnsRemaining = 0;
	int32   TurnsTotal = 0;
};

struct FWLGarrisonGroup   // tropa YA producida en una base (para mostrar en cartas)
{
	FString UnitType;
	FString Label;
	int32   Count = 0;
};

/**
 * Motor de campania estrategica. Mantiene la fecha de juego (1 tick = 1 mes,
 * ver roadmap) y aplica la economia mensual a cada nacion. Es deliberadamente
 * minimo para la vertical slice de Phase 0/1: solo tesoro nacional a partir
 * del balance de provincias propias.
 */
UCLASS()
class WORLDLEADER_API UWLStrategicTickSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Avanza un tick (un mes). Recalcula economia y emite OnMonthAdvanced. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	void AdvanceMonth();

	// Avanza UN DIA (accion del jugador): corre economia/reclutamiento/IA una vez y adelanta la fecha 1 dia
	// (con rollover a mes/anio). Es el "avanzar tiempo" clicable del HUD (antes solo existia el mes por tecla).
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	void AdvanceDay();

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	int32 GetCurrentYear() const { return CurrentYear; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	int32 GetCurrentMonth() const { return CurrentMonth; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	int32 GetCurrentDay() const { return CurrentDay; }

	/** Tesoro actual de una nacion (creditos). 0 si no existe. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetTreasury(const FString& NationIso) const;

	/** Ajuste directo de tesoro para decisiones politicas/eventos. Delta positivo suma, negativo gasta. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Economy")
	bool AdjustTreasury(const FString& NationIso, int64 Delta, FString& OutMessage);

	/** Balance mensual neto de una nacion segun sus provincias actuales. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetMonthlyBalance(const FString& NationIso) const;

	/** FE1.3: presupuesto mensual desglosado por categorias. GetMonthlyBalance == su neto. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	FWLNationBudget GetNationBudget(const FString& NationIso) const;

	/** FE1.4: limite de credito de una nacion (hasta cuanto puede endeudarse gastando). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetCreditLimit(const FString& NationIso) const;

	/** FE5.1: perfil financiero soberano calculado desde deuda, servicio, ingresos y gobernanza. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Finance")
	FWLFinancialProfile GetFinancialProfile(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Finance")
	TArray<FWLFinancialInstrumentState> GetFinancialInstrumentsForNation(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Finance")
	bool IssueBond(const FString& NationIso, int64 Principal, int32 TermMonths, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Finance")
	bool RegisterBilateralLoan(const FString& CreditorIso, const FString& RecipientIso, int64 Principal, int32 TermMonths, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Finance")
	bool RequestIMFProgram(const FString& NationIso, int64 Principal, int32 TermMonths, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Finance")
	bool MarkDebtDefault(const FString& NationIso, FString& OutMessage);

	/** FE5.3: ayuda exterior e inversion extranjera directa activas. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Finance")
	TArray<FWLForeignSupportState> GetForeignSupportForNation(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Finance")
	bool GrantForeignAid(const FString& SponsorIso, const FString& RecipientIso, int64 MonthlyAmount, int32 DurationMonths, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Finance")
	bool StartForeignInvestment(
		const FString& SponsorIso,
		const FString& RecipientIso,
		const FString& ProvinceId,
		const FString& BuildingId,
		int64 MonthlyAmount,
		int32 DurationMonths,
		FString& OutMessage);

	/** FE1.5: PIB mensual = produccion a precios de mercado + actividad de la poblacion, por orden publico. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetNationGDP(const FString& NationIso) const;

	/** FE1.5: crecimiento del PIB entre los dos ultimos ticks economicos (0.01 = +1%). 0 hasta el segundo tick. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	double GetNationGDPGrowth(const FString& NationIso) const;

	/** FE2.2: bienes que produce una provincia al mes (extraccion por bases + manufactura por industria). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	TArray<FWLGoodOutput> GetProvinceProduction(const FString& ProvinceId) const;

	/** FE2.2: produccion mensual agregada de una nacion, por bien. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	TArray<FWLGoodOutput> GetNationProduction(const FString& NationIso) const;

	/** FE2.2: empleo por sector de una provincia (el trabajo limita la produccion). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	FWLSectorEmployment GetProvinceEmployment(const FString& ProvinceId) const;

	/** FE2.4: empleo/desempleo/productividad de una nacion. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	FWLNationLaborStats GetNationLaborStats(const FString& NationIso) const;

	/** FE3-FE4: balance por bien con demanda, precios dinamicos, importaciones y exportaciones. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	TArray<FWLGoodMarketBalance> GetNationGoodMarketBalance(const FString& NationIso) const;

	/** FE3.4: shocks temporales activos que multiplican precios de bienes o de todo el mercado. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	TArray<FWLMarketShockState> GetActiveMarketShocks() const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	double GetMarketShockMultiplier(const FString& GoodId) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Economy")
	bool ApplyMarketShock(
		const FString& GoodId,
		double PriceMultiplier,
		int32 DurationMonths,
		const FString& Title,
		const FString& SourceEventId,
		FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Economy")
	bool ClearMarketShock(const FString& ShockId, FString& OutMessage);

	/** FE4.2-FE4.5: rutas comerciales efectivas desde tratados, embargos, aranceles y guerra. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	TArray<FWLTradeRouteState> GetTradeRoutesForNation(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	FWLTradeRouteState GetTradeRouteBetween(const FString& NationA, const FString& NationB) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int32 GetTariffRate(const FString& NationIso) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Economy")
	int32 SetTariffRate(const FString& NationIso, int32 RatePercent);

	/** FE5.2: inflacion mensual estimada desde presion de precios del mercado. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	double GetNationInflationRate(const FString& NationIso) const;

	/** FE5.4: indicador sintetico de ciclo; positivo expansion, negativo recesion. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	double GetNationEconomicCycleScore(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	FString GetNationEconomicCycleLabel(const FString& NationIso) const;

	/** FE6: ministro de economia, corrupcion sistemica y tecnologia que modifican la economia. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	FWLEconomicGovernanceStats GetEconomicGovernanceStats(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int32 GetNationSystemicCorruption(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int32 GetNationTechnologyLevel(const FString& NationIso) const;

	/** FE1.2: tasa de impuestos de una nacion (%). Si nunca se toco, el default de balance. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int32 GetTaxRate(const FString& NationIso) const;

	/** FE1.2: fija la tasa de impuestos (clampeada a los limites de balance). Devuelve la tasa efectiva. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Economy")
	int32 SetTaxRate(const FString& NationIso, int32 RatePercent);

	/** FE1.2: orden publico/mes que cuesta la tasa actual (negativo = la tasa baja recupera orden). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int32 GetTaxPublicOrderPressure(const FString& NationIso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetProvinceMonthlyIncome(const FString& ProvinceId) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetProvinceMonthlyUpkeep(const FString& ProvinceId) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetProvinceMonthlyBalance(const FString& ProvinceId) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Province")
	bool GetProvinceState(const FString& ProvinceId, FWLProvinceRuntimeState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Province")
	FString GetProvinceControllerIso(const FString& ProvinceId) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Province")
	bool SetProvinceController(const FString& ProvinceId, const FString& NewControllerIso, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Province")
	bool AdjustProvincePublicOrder(const FString& ProvinceId, int32 Delta, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Province")
	int32 AdjustNationPublicOrder(const FString& NationIso, int32 Delta);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Balance")
	FWLBalanceRules GetBalanceRules() const;

	/** Devuelve si una provincia tiene base economica para soportar ese edificio. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Construction")
	bool IsBuildingSupportedInProvince(const FString& ProvinceId, const FString& BuildingId) const;

	/**
	 * Construye un edificio en una provincia: descuenta el coste del tesoro de
	 * la nacion duena y registra el edificio. Devuelve false si faltan datos o
	 * fondos; OutMessage explica el resultado.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Construction")
	bool BuildBuilding(const FString& ProvinceId, const FString& BuildingId, FString& OutMessage);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Construction")
	bool UpgradeBuilding(const FString& ProvinceId, const FString& BuildingId, FString& OutMessage);

	/** IDs de edificios construidos en una provincia. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Construction")
	TArray<FString> GetProvinceBuildings(const FString& ProvinceId) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Construction")
	int32 GetProvinceBuildingLevel(const FString& ProvinceId, const FString& BuildingId) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Construction")
	int64 GetProvinceBuildingUpgradeCost(const FString& ProvinceId, const FString& BuildingId) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Construction")
	FWLProvinceBuildingEffects GetProvinceBuildingEffects(const FString& ProvinceId) const;

	// --- Reclutamiento (Fase 2: cola + avance por turno) ---
	/** Catalogo de unidades reclutables (coste/turnos/lote). */
	TArray<FWLRecruitOption> GetRecruitOptions() const;
	/** Encola una unidad en una base; cobra el coste del tesoro de la nacion. */
	bool QueueRecruit(const FString& BaseId, const FString& NationIso, const FString& UnitType, FString& OutMessage);
	/** Cola de ordenes pendientes en una base (la [0] es la que se entrena). */
	TArray<FWLRecruitOrder> GetRecruitQueue(const FString& BaseId) const;
	/** Tropa ya producida (acumulada) en una base. */
	TArray<FWLGarrisonGroup> GetGarrisonRecruited(const FString& BaseId) const;

	/** Efectivos militares totales de una nacion (fuerzas desplegadas + guarniciones reclutadas). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetNationMilitaryStrength(const FString& NationIso) const;

	/** Upkeep militar mensual de una nacion = efectivos * MilitaryUpkeepPerStrength (FE1.1). */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetNationMilitaryUpkeep(const FString& NationIso) const;

	/**
	 * Ejecuta la IA economica para todas las naciones salvo PlayerNationIso.
	 * Es determinista: una decision igual con datos iguales produce el mismo edificio.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|AI")
	int32 RunEconomicAI(const FString& PlayerNationIso, TArray<FString>& OutReports);

	UFUNCTION(BlueprintPure, Category = "WorldLeader|AI")
	TArray<FString> GetLastEconomicAIReports() const { return LastEconomicAIReports; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|AI")
	int32 GetLastEconomicAIBuildCount() const { return LastEconomicAIReports.Num(); }

	/** Reinicia fecha, tesoros y edificios al estado inicial de los datos. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	void ResetCampaignState();

	void WriteSaveSnapshot(
		int32& OutYear,
		int32& OutMonth,
		TArray<FWLNationTreasurySave>& OutTreasuries,
		TArray<FWLProvinceBuildingsSave>& OutProvinceBuildings,
		TArray<FWLProvinceRuntimeState>& OutProvinceStates,
		TArray<FWLMarketShockState>* OutMarketShocks = nullptr,
		TArray<FWLFinancialInstrumentState>* OutFinancialInstruments = nullptr,
		TArray<FWLForeignSupportState>* OutForeignSupportStates = nullptr) const;

	bool RestoreSaveSnapshot(
		int32 SavedYear,
		int32 SavedMonth,
		const TArray<FWLNationTreasurySave>& SavedTreasuries,
		const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
		const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
		FString& OutMessage);

	bool RestoreSaveSnapshot(
		int32 SavedYear,
		int32 SavedMonth,
		const TArray<FWLNationTreasurySave>& SavedTreasuries,
		const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
		const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
		const TArray<FWLMarketShockState>& SavedMarketShocks,
		FString& OutMessage);

	bool RestoreSaveSnapshot(
		int32 SavedYear,
		int32 SavedMonth,
		const TArray<FWLNationTreasurySave>& SavedTreasuries,
		const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
		const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
		const TArray<FWLMarketShockState>& SavedMarketShocks,
		const TArray<FWLFinancialInstrumentState>& SavedFinancialInstruments,
		const TArray<FWLForeignSupportState>& SavedForeignSupportStates,
		FString& OutMessage);

	UPROPERTY(BlueprintAssignable, Category = "WorldLeader|Campaign")
	FWLOnMonthAdvanced OnMonthAdvanced;

private:
	int32 CurrentYear = 0;
	int32 CurrentMonth = 0;
	int32 CurrentDay = 1;   // dia del mes (1..30) para el avance por dias del jugador

	/** Tesoro nacional en runtime (ISO -> creditos). */
	TMap<FString, int64> Treasuries;

	/** FE1.2: tasa de impuestos por nacion (ISO -> %). Si falta, se usa TaxRateDefaultPercent. */
	TMap<FString, int32> TaxRates;

	/** FE4.3: tasa de aranceles por nacion (ISO -> %). Si falta, se usa TariffRateDefaultPercent. */
	TMap<FString, int32> TariffRates;

	/** FE5.1/FE5.3: instrumentos financieros y apoyo exterior persistentes. */
	TArray<FWLFinancialInstrumentState> FinancialInstruments;
	TArray<FWLForeignSupportState> ForeignSupportStates;
	int32 NextFinancialInstrumentNumber = 1;
	int32 NextForeignSupportNumber = 1;

	/** FE1.5: PIB del tick anterior y crecimiento medido, por nacion (para la tasa de crecimiento). */
	TMap<FString, int64> PreviousGDP;
	TMap<FString, double> GDPGrowth;
	void UpdateGDPHistory();

	/** FE3.4: shocks temporales de precios de mercado. */
	TArray<FWLMarketShockState> ActiveMarketShocks;
	int32 NextMarketShockNumber = 1;
	void AdvanceMarketShocks();
	void AdvanceFinancialMonth();
	int64 GetNationDebtService(const FString& NationIso) const;
	int64 GetNationOutstandingDebt(const FString& NationIso) const;
	double GetInterestRateForInstrument(const FString& NationIso, EWLFinancialInstrumentType Type) const;
	bool CanAddDebtInstrument(const FString& NationIso, int64 Principal, int32 TermMonths, double MonthlyInterestRate, FString& OutMessage) const;
	FWLFinancialInstrumentState AddDebtInstrument(const FString& NationIso, const FString& CreditorIso, EWLFinancialInstrumentType Type, int64 Principal, int32 TermMonths, double MonthlyInterestRate, const FString& Title);
	bool CompleteForeignInvestment(FWLForeignSupportState& Support, FString& OutMessage);

	/** Edificios construidos por provincia (provinceId -> lista de buildingId). */
	TMap<FString, TArray<FString>> ProvinceBuildings;
	TMap<FString, TMap<FString, int32>> ProvinceBuildingLevels;

	/** Estado mutable por provincia: poblacion actual y orden publico. */
	TMap<FString, FWLProvinceRuntimeState> ProvinceStates;

	/** Reclutamiento: cola de ordenes por base + tropa ya producida por base (baseId -> unit -> count). */
	TMap<FString, TArray<FWLRecruitOrder>> RecruitQueues;
	TMap<FString, TMap<FString, int32>> GarrisonRecruited;
	mutable TArray<FWLRecruitOption> RecruitCatalog;
	mutable bool bRecruitCatalogLoaded = false;

	/** Efectivos desplegados por nacion (de MilitaryForces.json), cache perezoso para el upkeep militar (FE1.1). */
	mutable TMap<FString, int64> PreplacedMilitaryStrength;
	mutable bool bMilitaryCatalogLoaded = false;
	void EnsureMilitaryCatalog() const;

	UPROPERTY()
	TArray<FString> LastEconomicAIReports;

	void InitTreasuriesFromData();
	void InitProvinceStatesFromData();
	void ApplyMonthlyEconomy();
	void ApplyMonthlyProvinceState();
	int32 RunEconomicAIInternal(const FString& PlayerNationIso, TArray<FString>& OutReports);
	bool FindBestEconomicAIBuildCandidate(
		const FString& NationIso,
		FString& OutProvinceId,
		FString& OutBuildingId,
		int64& OutMonthlyGain,
		int64& OutCost,
		int64& OutPaybackMonths) const;
	FString GetActivePlayerNationIsoForAI() const;

	/** Ingreso mensual extra de los edificios construidos en una provincia. */
	int64 GetProvinceBuildingIncome(const FString& ProvinceId, const FWLBalanceRules& Rules) const;

	/** FE1.3: ingreso provincial con desglose exacto de la parte de impuestos (OutTaxIncome <= retorno). */
	int64 GetProvinceMonthlyIncomeSplit(const FString& ProvinceId, int64& OutTaxIncome) const;

	FWLProductionLedger BuildProvinceProductionLedger(const FString& ProvinceId) const;
	FWLProductionLedger BuildNationProductionLedger(const FString& NationIso) const;
	TMap<FString, int64> BuildNationDemandMap(const FString& NationIso) const;
	int64 GetNationPopulation(const FString& NationIso) const;
	int64 GetNationMarketProductionValue(const FString& NationIso) const;
	int64 GetNationTradeBalance(const FString& NationIso) const;
	double GetTariffImportVolumeMultiplier(const FString& NationIso) const;

	void EnsureRecruitCatalog() const;
	const FWLRecruitOption* FindRecruitOption(const FString& UnitType) const;
	void AdvanceRecruitment();   // llamado por AdvanceMonth: avanza colas y completa ordenes

	class UWLDataRegistry* GetDataRegistry() const;
	class UWLBalanceSubsystem* GetBalanceSubsystem() const;
};
