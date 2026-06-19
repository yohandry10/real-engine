// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "GameFramework/SaveGame.h"
#include "WLLocalSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FWLNationTreasurySave
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	FString NationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int64 Treasury = 0;
};

USTRUCT(BlueprintType)
struct FWLProvinceBuildingsSave
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	FString ProvinceId;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FString> BuildingIds;
};

/**
 * Save local de campania. Guarda el estado runtime que no debe depender del
 * JSON base: nacion seleccionada, fecha, tesoros, provincias y ejercitos.
 */
UCLASS()
class WORLDLEADER_API UWLLocalSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 SaveVersion = 3;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	FString SelectedNationIso;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 CurrentYear = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 CurrentMonth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLNationTreasurySave> NationTreasuries;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLProvinceBuildingsSave> ProvinceBuildings;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLProvinceRuntimeState> ProvinceStates;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	TArray<FWLArmy> Armies;

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Save")
	int32 NextArmyNumber = 1;
};
