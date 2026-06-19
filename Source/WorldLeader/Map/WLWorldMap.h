// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "GameFramework/Actor.h"
#include "WLWorldMap.generated.h"

class UWLDataRegistry;
class UProceduralMeshComponent;
class UMaterialInterface;
class UTextRenderComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;
class ADirectionalLight;
class ASkyLight;
class ACameraActor;
class FJsonValue;

USTRUCT(BlueprintType)
struct FWLMapCountryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FString Iso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FString Continent;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") float ProjectedArea = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") bool bHasLargeLabel = false;
};

USTRUCT(BlueprintType)
struct FWLMapProvinceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FString Region;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Map") FVector WorldLocation = FVector::ZeroVector;
};

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Map")
	void BuildMap();

	FBox2D GetMapBounds2D() const;
	bool TryGetCountryForComponent(const UPrimitiveComponent* Component, FWLMapCountryView& OutCountry) const;
	bool TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLMapProvinceView& OutProvince) const;

	/** Unidades de mundo por grado geografico. */
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") float GeoScale = 2000.f;

	/** Vacio renderiza todo el mundo. Por defecto: America completa. */
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map") TArray<FString> ContinentFilters;

	/** Foco inicial en longitud/latitud. Por defecto: Caribe/Venezuela. */
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map|Camera") FVector2D InitialCameraLonLat = FVector2D(-75.f, 12.f);

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map|Camera") float InitialCameraHeight = 140000.f;

	/** Umbral de area proyectada para mostrar nombre fijo; paises menores usan marcador. */
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map|Labels") float CountryLabelAreaThreshold = 90000000.f;

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map|Labels") float CountryLabelWorldSize = 3000.f;

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map|Markers") FVector MarkerScale = FVector(10.f, 10.f, 3.f);

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Map|Markers") FVector ProvinceMarkerScale = FVector(4.5f, 4.5f, 1.6f);

private:
	UPROPERTY() USceneComponent* SceneRoot = nullptr;
	UPROPERTY() UProceduralMeshComponent* MapMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;
	UPROPERTY() TArray<UTextRenderComponent*> CountryLabels;
	UPROPERTY() UStaticMesh* MarkerMesh = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> CountryMarkers;
	UPROPERTY() TArray<FWLMapCountryView> Countries;
	UPROPERTY() TArray<UStaticMeshComponent*> ProvinceMarkers;
	UPROPERTY() TArray<FWLMapProvinceView> Provinces;
	UPROPERTY() ADirectionalLight* MapDirectionalLight = nullptr;
	UPROPERTY() ASkyLight* MapSkyLight = nullptr;
	UPROPERTY() ACameraActor* MapCamera = nullptr;

	int32 SectionIndex = 0;
	FVector2D BoundsMin = FVector2D(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D BoundsMax = FVector2D(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());
	TSet<FString> RenderedCountryIsos;

	UWLDataRegistry* GetRegistry() const;

	FVector2D ProjectLonLat(double Lon, double Lat) const;
	bool ShouldRenderContinent(const FString& Continent) const;
	TArray<FVector2D> RingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring) const;
	void AddSection(const TArray<FVector2D>& Poly, const FLinearColor& Color);
	void AddLabel(const FString& Text, const FVector2D& Pos);
	void AddCountryMarker(const FWLMapCountryView& Country, const FLinearColor& Color);
	void AddProvinceMarker(const FWLProvinceData& Province);
	void AddProvinceMarkers();
	void SetupLightingAndCamera();
	void DestroyWorldActors();
};
