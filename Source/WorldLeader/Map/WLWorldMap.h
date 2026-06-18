// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WLWorldMap.generated.h"

class UWLDataRegistry;
class UProceduralMeshComponent;
class UMaterialInterface;
class FJsonValue;

/**
 * Mapa estrategico real: lee siluetas geograficas de paises desde un GeoJSON
 * (Natural Earth, Content/Data/Geo/), las triangula y las renderiza como
 * mallas planas coloreadas por nacion. Sin hardcodear formas: todo viene de
 * los datos (pipeline del roadmap: GeoJSON -> poligonos -> mallas).
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

	/** Unidades de mundo por grado geografico. */
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") float GeoScale = 2000.f;

	/** Solo renderiza paises de este continente (Natural Earth CONTINENT). */
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") FString ContinentFilter = TEXT("South America");

private:
	UPROPERTY() USceneComponent* SceneRoot = nullptr;
	UPROPERTY() UProceduralMeshComponent* MapMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;

	int32 SectionIndex = 0;
	FVector2D BoundsMin = FVector2D(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D BoundsMax = FVector2D(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());

	UWLDataRegistry* GetRegistry() const;

	FVector2D ProjectLonLat(double Lon, double Lat) const;
	TArray<FVector2D> RingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring) const;
	void AddSection(const TArray<FVector2D>& Poly, const FLinearColor& Color);
	void AddLabel(const FString& Text, const FVector2D& Pos);
	void SetupLightingAndCamera();
};
