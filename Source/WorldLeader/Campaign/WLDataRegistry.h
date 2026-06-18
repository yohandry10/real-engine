// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/WLGameTypes.h"
#include "WLDataRegistry.generated.h"

/**
 * Registro central de datos de campania. Carga provincias y naciones desde
 * los JSON versionados en Content/Data/ y los expone como consulta de solo
 * lectura. Es el unico punto que conoce el formato en disco (regla del
 * roadmap: no cargar datos crudos dispersos por el codigo).
 */
UCLASS()
class WORLDLEADER_API UWLDataRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Recarga todos los datos desde disco. Devuelve true si al menos cargo provincias. */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	bool LoadGameData();

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	bool GetProvince(const FString& Id, FWLProvinceData& OutProvince) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	bool GetNation(const FString& Iso, FWLNationData& OutNation) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	TArray<FWLProvinceData> GetAllProvinces() const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	TArray<FWLNationData> GetAllNations() const;

	/** Todas las provincias cuyo country_iso coincide (controlador inicial = propietario). */
	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	TArray<FWLProvinceData> GetProvincesByNation(const FString& Iso) const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Data")
	int32 GetProvinceCount() const { return Provinces.Num(); }

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Data")
	int32 GetNationCount() const { return Nations.Num(); }

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	bool GetBuilding(const FString& Id, FWLBuildingData& OutBuilding) const;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Data")
	TArray<FWLBuildingData> GetAllBuildings() const;

	UFUNCTION(BlueprintPure, Category = "WorldLeader|Data")
	int32 GetBuildingCount() const { return Buildings.Num(); }

private:
	UPROPERTY()
	TMap<FString, FWLProvinceData> Provinces;

	UPROPERTY()
	TMap<FString, FWLNationData> Nations;

	UPROPERTY()
	TMap<FString, FWLBuildingData> Buildings;

	bool LoadProvincesFromFile(const FString& FilePath);
	bool LoadNationsFromFile(const FString& FilePath);
	bool LoadBuildingsFromFile(const FString& FilePath);

	static EWLTerrainType TerrainFromString(const FString& In);
	static EWLBuildingSlot SlotFromString(const FString& In);
};
