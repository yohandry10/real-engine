// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Balance/WLBalanceTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/WLLocalSaveGame.h"
#include "WLStrategicTickSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWLOnMonthAdvanced, int32, Year, int32, Month);

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

	/** Balance mensual neto de una nacion segun sus provincias actuales. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Economy")
	int64 GetMonthlyBalance(const FString& NationIso) const;

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

	/** IDs de edificios construidos en una provincia. */
	UFUNCTION(BlueprintPure, Category = "WorldLeader|Construction")
	TArray<FString> GetProvinceBuildings(const FString& ProvinceId) const;

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
		TArray<FWLProvinceRuntimeState>& OutProvinceStates) const;

	bool RestoreSaveSnapshot(
		int32 SavedYear,
		int32 SavedMonth,
		const TArray<FWLNationTreasurySave>& SavedTreasuries,
		const TArray<FWLProvinceBuildingsSave>& SavedProvinceBuildings,
		const TArray<FWLProvinceRuntimeState>& SavedProvinceStates,
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

	/** Edificios construidos por provincia (provinceId -> lista de buildingId). */
	TMap<FString, TArray<FString>> ProvinceBuildings;

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

	void EnsureRecruitCatalog() const;
	const FWLRecruitOption* FindRecruitOption(const FString& UnitType) const;
	void AdvanceRecruitment();   // llamado por AdvanceMonth: avanza colas y completa ordenes

	class UWLDataRegistry* GetDataRegistry() const;
	class UWLBalanceSubsystem* GetBalanceSubsystem() const;
};
