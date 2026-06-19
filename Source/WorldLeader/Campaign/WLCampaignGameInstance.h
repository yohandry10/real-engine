// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "Engine/GameInstance.h"
#include "WLCampaignGameInstance.generated.h"

class UWLDataRegistry;
class UWLStrategicTickSubsystem;

/**
 * GameInstance de la campania. Punto de entrada del estado global del cliente
 * (regla del roadmap: GameInstance para estado global). Expone comandos de
 * consola para probar la vertical slice antes de tener UI:
 *   WLAdvanceMonth  -> avanza un tick (un mes)
 *   WLPrintState    -> imprime fecha, provincias, naciones y tesoros
 */
UCLASS()
class WORLDLEADER_API UWLCampaignGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	bool StartNewCampaign(const FString& NationIso);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	void ResetCampaignFlow();

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	bool HasActiveCampaign() const { return bHasActiveCampaign; }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Campaign")
	const FString& GetSelectedNationIso() const { return SelectedNationIso; }

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign")
	bool GetSelectedNation(FWLNationData& OutNation) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Save")
	bool HasLocalCampaignSave() const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Save")
	bool SaveLocalCampaign(FString& OutMessage) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Save")
	bool LoadLocalCampaign(FString& OutMessage);

	/** Consola: inicia campania con una nacion. Uso: WLStartCampaign VE */
	UFUNCTION(Exec)
	void WLStartCampaign(const FString& NationIso);

	/** Consola: guarda la campania activa en slot local. */
	UFUNCTION(Exec)
	void WLSave();

	/** Consola: carga la campania local si existe. */
	UFUNCTION(Exec)
	void WLLoad();

	/** Consola: avanza la campania un mes y registra el nuevo estado. */
	UFUNCTION(Exec)
	void WLAdvanceMonth();

	/** Consola: imprime el estado actual de la campania al log. */
	UFUNCTION(Exec)
	void WLPrintState();

	/** Consola: construye un edificio en una provincia. Uso: WLBuild VE-ZU oil_well */
	UFUNCTION(Exec)
	void WLBuild(const FString& ProvinceId, const FString& BuildingId);

	/** Consola: crea un ejercito. Uso: WLSpawnArmy VE VE-ZU tank 3 */
	UFUNCTION(Exec)
	void WLSpawnArmy(const FString& Nation, const FString& ProvinceId, const FString& UnitId, int32 Count);

	/** Consola: lista los ejercitos en el mapa. */
	UFUNCTION(Exec)
	void WLArmies();

	/** Consola: mueve un ejercito a una provincia adyacente. Uso: WLMove A1 VE-BO */
	UFUNCTION(Exec)
	void WLMove(const FString& ArmyId, const FString& ProvinceId);

	/** Consola: auto-resuelve una batalla. Uso: WLBattle A1 A2 */
	UFUNCTION(Exec)
	void WLBattle(const FString& AttackerId, const FString& DefenderId);

private:
	UPROPERTY()
	bool bHasActiveCampaign = false;

	UPROPERTY()
	FString SelectedNationIso;

	UWLDataRegistry* GetRegistry() const;
	UWLStrategicTickSubsystem* GetTick() const;
	class UWLMilitarySubsystem* GetMilitary() const;
};
