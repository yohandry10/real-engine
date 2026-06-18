// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WLWorldMap.generated.h"

class UWLDataRegistry;
class UStaticMesh;
class UMaterialInterface;

/**
 * Construye el mapa estrategico en runtime: un marcador (cubo + etiqueta) por
 * provincia, situado segun sus coordenadas y coloreado por la nacion duena.
 * Lee los datos de WLDataRegistry (no hardcodea nada). Lo genera el GameMode
 * al iniciar la partida.
 */
UCLASS()
class WORLDLEADER_API AWLWorldMap : public AActor
{
	GENERATED_BODY()

public:
	AWLWorldMap();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Map")
	void BuildMap();

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") float DegreeScale = 1500.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") float OriginLat = 9.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") float OriginLon = -69.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") FVector MarkerScale = FVector(12.f, 12.f, 4.f);

	/** Altura de la camara cenital sobre el mapa. */
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") float CameraHeight = 22000.f;

private:
	UPROPERTY() USceneComponent* SceneRoot = nullptr;
	UPROPERTY() UStaticMesh* CubeMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;

	UWLDataRegistry* GetRegistry() const;

	/** Genera luz de escena y una camara cenital fijada como vista del jugador. */
	void SetupViewAndLighting();
};
