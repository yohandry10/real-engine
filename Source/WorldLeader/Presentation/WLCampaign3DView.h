// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Core/WLGameTypes.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "Presentation/WLCampaignTerritoryLayerComponent.h"
#include "GameFramework/Actor.h"
#include "WLCampaign3DView.generated.h"

class ACameraActor;
class ADirectionalLight;
class AExponentialHeightFog;
class ASkyLight;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UWLCampaignTerritoryLayerComponent;
class UWLDataRegistry;
enum class EWLCampaignSettlementType : uint8;
struct FWLCampaignRouteSpec;

UENUM(BlueprintType)
enum class EWLCampaign3DZoomLOD : uint8
{
	Close,
	Theater,
	Region,
	Global
};

// Tipos de asset de NATURALEZA (modelos Blender unlit vertex-color en /Game/GenNature). El indice
// es estable: lo usan NatureInstances/NatureMeshes y el scatter por bioma (ver AddVegetationScatter).
enum class EWLNatureKind : uint8
{
	Conifer = 0,	// conifera (Montaña / bosque)
	Broadleaf,		// hoja ancha (Jungle / Llanos)
	Palm,			// palmera (Coast)
	Rock,			// peñasco (cresta / costa rocosa)
	Peak,			// pico andino con nieve (Montaña alta)
	Mount,			// montaña verde redondeada (acento de relieve)
	Count
};

USTRUCT(BlueprintType)
struct FWLCampaign3DProvinceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Region;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FWLCampaign3DCityView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString TerritoryId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString TerritoryName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CityType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString StrategicRole;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString DetailLevel;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lon = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lat = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bCapital = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bPort = false;
};

USTRUCT(BlueprintType)
struct FWLCampaign3DForceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ForceType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString MarkerCategory;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString MovementNodeId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString MovementStatus;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString LocationName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ProvinceId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ProvinceName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString NearbyCity;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Mobility;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString OperationalState;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Supply;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Morale;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Posture;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString StrategicRole;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString DetailLevel;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") TArray<FString> DisabledActions;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lon = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lat = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") int32 EstimatedStrength = 0;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bAir = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bNaval = false;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bMovable = true;
};

USTRUCT(BlueprintType)
struct FWLCampaign3DMovementNodeView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString CountryIso;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ProvinceId;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString ProvinceName;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FString NodeType;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") FVector WorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lon = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") float Lat = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "WorldLeader|Campaign3D") bool bPort = false;
};

/**
 * Presentation layer principal de campania. Es una escena 3D regional
 * Colombia/Venezuela que lee datos compartidos; no contiene reglas de juego.
 */
UCLASS()
class WORLDLEADER_API AWLCampaign3DView : public AActor
{
	GENERATED_BODY()

public:
	AWLCampaign3DView();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign3D")
	void BuildView(const FString& PlayerNationIso);

	UFUNCTION(BlueprintCallable, Category = "WorldLeader|Campaign3D")
	void SetPresentationActive(bool bActive, bool bSetCamera = true);

	bool TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DProvinceView& OutProvince) const;
	bool TryGetTerritoryForComponent(const UPrimitiveComponent* Component, FWLCampaignTerritoryRegionView& OutTerritory) const;
	bool TryGetTerritoryAtWorldLocation(const FVector& WorldLocation, FWLCampaignTerritoryRegionView& OutTerritory) const;
	bool TryGetCityForComponent(const UPrimitiveComponent* Component, FWLCampaign3DCityView& OutCity) const;
	bool TryGetCityNearWorldLocation(const FVector& WorldLocation, float MaxDistance, FWLCampaign3DCityView& OutCity) const;
	bool TryGetForceById(const FString& ForceId, FWLCampaign3DForceView& OutForce) const;
	bool TryGetForceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DForceView& OutForce) const;
	bool TryGetForceNearWorldLocation(const FVector& WorldLocation, float MaxDistance, FWLCampaign3DForceView& OutForce) const;
	bool TryGetMovementDestinationForComponent(const UPrimitiveComponent* Component, FWLCampaign3DMovementNodeView& OutNode) const;
	bool TryGetMovementDestinationNearWorldLocation(const FVector& WorldLocation, float MaxDistance, FWLCampaign3DMovementNodeView& OutNode) const;
	bool BuildMovementRouteForForce(const FString& ForceId, const FString& DestinationNodeId, TArray<FWLCampaign3DMovementNodeView>& OutRouteNodes, int32& OutEstimatedTurns) const;
	bool UpdateForceMovementLocation(const FString& ForceId, const FString& DestinationNodeId, FWLCampaign3DForceView& OutForce);
	void ShowMovementDestinationOptions(const FString& ForceId);
	void SetMovementDestinationPreview(const FString& ForceId, const FString& DestinationNodeId);
	void ClearMovementPreview();
	void SetSelectedProvinceHighlight(const FString& ProvinceId);
	void SetSelectedCityHighlight(const FString& CityId);
	void SetSelectedForceHighlight(const FString& ForceId);
	void SetHoveredForceHighlight(const FString& ForceId);
	void ClearSelectionHighlight();
	FBox2D GetViewBounds2D() const;
	FBox2D GetCameraBounds2D(float CameraHeight) const;
	// Altura de mundo del terreno bajo un punto de mundo (inverso de ProjectLonLat). La camara la usa
	// para mantener una altura MINIMA sobre el suelo y no enterrarse en el relieve al acercar.
	float GetGroundWorldZAtWorld(float WorldX, float WorldY) const;
	ACameraActor* GetViewCamera() const { return ViewCamera; }
	FVector GetDefaultCameraLocation() const { return DefaultCameraLocation; }
	FRotator GetDefaultCameraRotation() const { return DefaultCameraRotation; }
	FVector GetTheaterFocusPoint() const;
	EWLCampaign3DZoomLOD GetCurrentZoomLOD() const { return CurrentZoomLOD; }
	FString GetCurrentZoomLODLabel() const;
	void ApplyZoomLOD(float CameraHeight);

	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") FVector2D TheaterCenterLonLat = FVector2D(-68.6f, 7.2f);
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float GeoScale = 9000.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float DetailWorldUnitsPerKm = 320.f;
	// Exageracion vertical SOLO de la capa de detalle (ProjectLonLat). El mundo de detalle es
	// ~4x mas ancho que el original, asi que el relieve necesita x4 para no verse plano (Merida).
	// El overview (ProjectStrategicLonLat) NO la usa, a proposito: la vista estrategica plana.
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float VerticalDetailExaggeration = 4.f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMinLon = -125.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMaxLon = -34.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMinLat = -56.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") float RegionMaxLat = 62.0f;
	UPROPERTY(EditAnywhere, Category = "WorldLeader|Campaign3D") bool bAutoBuildOnBeginPlay = false;

private:
	UPROPERTY() USceneComponent* SceneRoot = nullptr;
	UPROPERTY() UProceduralMeshComponent* TerrainMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* TerrainDetailMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SeaMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SeaDetailMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* OverviewMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* BoundaryMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* RoadMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SettlementMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* SelectionHighlightMesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* MovementRoutePreviewMesh = nullptr;
	UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;
	UPROPERTY() UMaterialInterface* VertexColorMaterial = nullptr;
	// Material LIT/PBR del terreno (M1 del estándar visual): mezcla pasto/roca por pendiente, teñido por
	// bioma (vertex color), normal maps, tileable por posición de mundo. /Game/GenTerrain/M_TerrainLit.
	// Reemplaza el VertexColorMaterial unlit del terreno. Ver Docs/CAMPAIGN3D_VISUAL_STANDARD.md.
	UPROPERTY() UMaterialInterface* TerrainLitMaterial = nullptr;
	UPROPERTY() TArray<UMaterialInterface*> TerrainPBRBiomeMaterials;
	UPROPERTY() UStaticMesh* CityMesh = nullptr;
	UPROPERTY() UStaticMesh* RouteMesh = nullptr;
	UPROPERTY() UStaticMesh* CubeMesh = nullptr;
	UPROPERTY() UStaticMesh* ConeMesh = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* SettlementBlockInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* SettlementTowerInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* PortInstances = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* ArmyMarkerInstances = nullptr;
	// VEGETACION / RELIEVE de naturaleza: un ISM por tipo (EWLNatureKind), con su mesh Blender unlit
	// vertex-color de /Game/GenNature. El scatter por bioma (AddVegetationScatter) reparte instancias.
	// Reemplaza a los antiguos conos placeholder (TreeInstances/BrushInstances).
	UPROPERTY() TArray<UInstancedStaticMeshComponent*> NatureInstances;
	UPROPERTY() TArray<UStaticMesh*> NatureMeshes;
	// Modelos de CIUDAD 3D cohesiva (generados en Blender, /Game/GenCity). Se coloca UNO por
	// asentamiento segun tamaño, escalado y anclado al terreno (estilo settlement de Total War).
	// Ver BuildMeshCity.
	UPROPERTY() UStaticMesh* CityModelLarge = nullptr;
	UPROPERTY() UStaticMesh* CityModelMedium = nullptr;
	UPROPERTY() UStaticMesh* CityModelSmall = nullptr;
	// Variantes por tamaño (distintas semillas) -> ciudades vecinas no son clones identicas; se elige
	// una por hash de lon/lat en BuildMeshCity (estable). [0] = el modelo base de arriba.
	UPROPERTY() TArray<UStaticMesh*> CityModelLargeVariants;
	UPROPERTY() TArray<UStaticMesh*> CityModelMediumVariants;
	UPROPERTY() TArray<UStaticMesh*> CityModelSmallVariants;
	UPROPERTY() TArray<UStaticMeshComponent*> CityBuildingComponents;
	UPROPERTY() TArray<UStaticMesh*> RoadAssetMeshes;
	UPROPERTY() TArray<UStaticMeshComponent*> RoadAssetComponents;
	UPROPERTY() TArray<UPrimitiveComponent*> ProvinceMarkers;
	UPROPERTY() TArray<UPrimitiveComponent*> CitySelectionMarkers;
	UPROPERTY() TArray<UStaticMeshComponent*> ForceMarkerComponents;
	UPROPERTY() TArray<UPrimitiveComponent*> ForceSelectionMarkers;
	UPROPERTY() TArray<UTextRenderComponent*> ForceMarkerLabels;
	// Fuerzas militares = placeholder sin gameplay todavia. En false NO se crean conos/etiquetas/
	// hitbox: limpia los "triangulos amarillos" y la etiqueta que duplicaba el nombre de la ciudad
	// (p.ej. el segundo "Caracas"). Los datos siguen en ForceViews. Poner true al activar el gameplay.
	bool bShowMilitaryForceMarkers = false;
	UPROPERTY() TArray<UStaticMeshComponent*> MovementDestinationComponents;
	UPROPERTY() TArray<UPrimitiveComponent*> MovementDestinationSelectionMarkers;
	UPROPERTY() TArray<UTextRenderComponent*> MovementDestinationLabels;
	UPROPERTY() TArray<UStaticMeshComponent*> VisualComponents;
	UPROPERTY() TArray<UStaticMeshComponent*> RouteSegments;
	UPROPERTY() TArray<UTextRenderComponent*> Labels;
	// Tamano base de cada nombre de PAIS (paralelo a Labels). ApplyZoomLOD lo reescala por altura de
	// camara (tamano legible a cualquier zoom; antes era fijo y diminuto de lejos).
	TArray<float> CountryLabelBaseSizes;
	UPROPERTY() TArray<UTextRenderComponent*> SettlementLabels;
	// Tamano base (mundo) de cada label de ciudad, paralelo a SettlementLabels. ApplyZoomLOD
	// reescala el WorldSize por altura de camara para que el nombre se lea con tamano casi
	// CONSTANTE en pantalla a cualquier zoom (estilo medallon de Total War).
	TArray<float> SettlementLabelBaseSizes;
	// Altura MAXIMA de camara a la que se muestra cada label de ciudad (LOD por importancia; paralelo
	// a SettlementLabels). Evita el amontonamiento de nombres en zoom lejano. Ver ApplyZoomLOD.
	TArray<float> SettlementLabelMaxHeights;
	UPROPERTY() TArray<UTextRenderComponent*> OverviewLabels;
	TArray<uint8> OverviewLabelVisibilityMasks;
	UPROPERTY() TArray<FWLCampaign3DProvinceView> ProvinceViews;
	UPROPERTY() TArray<FWLCampaign3DCityView> CityViews;
	UPROPERTY() TArray<FWLCampaign3DForceView> ForceViews;
	UPROPERTY() TArray<FWLCampaign3DMovementNodeView> MovementNodes;
	UPROPERTY() TArray<FWLCampaign3DMovementNodeView> ActiveMovementDestinations;
	UPROPERTY() UWLCampaignTerritoryLayerComponent* TerritoryLayer = nullptr;
	UPROPERTY() ADirectionalLight* ViewDirectionalLight = nullptr;
	UPROPERTY() ASkyLight* ViewSkyLight = nullptr;
	UPROPERTY() AExponentialHeightFog* ViewFog = nullptr;
	UPROPERTY() ACameraActor* ViewCamera = nullptr;

	FString ActivePlayerNationIso;
	FBox2D Bounds;
	FBox2D OverviewBounds;
	FVector DefaultCameraLocation = FVector::ZeroVector;
	FRotator DefaultCameraRotation = FRotator::ZeroRotator;
	EWLCampaign3DZoomLOD CurrentZoomLOD = EWLCampaign3DZoomLOD::Theater;
	TArray<FWLRegionalCountryGeometry> SettlementLandGeometry;
	FString SelectedProvinceHighlightId;
	FString SelectedCityHighlightId;
	FString SelectedForceHighlightId;
	FString HoveredForceHighlightId;
	TArray<FVector> ForceMarkerBaseScales;
	TArray<FLinearColor> ForceMarkerBaseColors;
	TMap<FString, TArray<FString>> MovementAdjacency;
	FString ActiveMovementForceId;
	FString PreviewMovementDestinationNodeId;
	bool bHasBuiltView = false;
	// Flat pads de ciudad: discos donde el terreno se APLANA (a la altura natural del centro) para que
	// la ciudad (un mesh rigido y plano) no quede "comida"/inclinada por el relieve o el ruido sobre su
	// huella (~2600u sobre la huella de la capital). Se recopilan en una pre-pasada (bCollectFlatPadsOnly)
	// ANTES de mallar el terreno; el relieve de alrededor (Andes/Merida) se conserva fuera del pad.
	struct FCityFlatPad { float Lon; float Lat; float FlatRadiusDeg; float OuterRadiusDeg; float Height; };
	TArray<FCityFlatPad> CityFlatPads;
	bool bCollectFlatPadsOnly = false;
	float WaterAnimationTime = 0.f;
	float PendingCityVisualScreenshotSeconds = -1.f;
	float CityVisualQuitCountdownSeconds = -1.f;
	bool bCityVisualScreenshotRequested = false;
	bool bCityVisualQuitAfterScreenshot = false;

	UWLDataRegistry* GetRegistry() const;
	FVector ProjectLonLat(float Lon, float Lat) const;
	FVector ProjectStrategicLonLat(float Lon, float Lat) const;
	float GetVisualTerrainWorldZ(float Lon, float Lat, bool bCoreCountry) const;
	float GetVisualTerrainWorldZ(float Lon, float Lat, const FString& CountryIso) const;
	bool IsCoreTerrainAtLonLat(float Lon, float Lat) const;
	float SampleTerrainHeight(float Lon, float Lat) const;
	float SampleTerrainHeightRaw(float Lon, float Lat) const;
	FLinearColor TerrainColor(EWLTerrainType Terrain, const FString& CountryIso) const;
	UMaterialInstanceDynamic* MakeColorMaterial(const FLinearColor& Color);
	void BuildSea();
	void BuildOverviewLayer();
	void BuildTerrain();
	void BuildVenezuelaTerrainDetail();
	void BuildCampaignVisualLayer();
	FVector2D NudgeSettlementToLand(float Lon, float Lat) const;
	void AddTerrainPatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset);
	void AddPolylineSegment(const FVector& Start, const FVector& End, const FLinearColor& Color, float RadiusScale);
	void AddCoastline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale);
	void AddPathPolyline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale, float ZOffset);
	void AddBiomePatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset);
	void BuildTheaterSettlementLayer();
	void BuildSouthAmericaFrontierSettlementLayer();
	void AddSettlementCluster(
		const FString& CityId,
		const FString& Name,
		const FString& CountryIso,
		const FString& TerritoryId,
		const FString& TerritoryName,
		float Lon,
		float Lat,
		EWLCampaignSettlementType Type,
		const FLinearColor& AccentColor);
	// Construye una ciudad con mallas reales (edificios del pack) en vez de cajas.
	void BuildMeshCity(float Lon, float Lat, EWLCampaignSettlementType Type, const FString& CountryIso);
	// Registra un "flat pad" (disco de terreno aplanado) bajo la ciudad para que el relieve no la coma.
	void RegisterCityFlatPad(float Lon, float Lat, EWLCampaignSettlementType Type);
	void AddCitySelectionProxy(const FWLCampaign3DCityView& City, float RadiusScale);
	void AddMilitaryForceMarkers();
	void AddMilitaryForceMarker(const FWLCampaign3DForceView& Force);
	void RefreshMilitaryForceMarkerVisuals();
	void BuildMovementNodesAndEdges();
	void BuildIntercityRoads();
	void BuildVenezuelaRoadAssetLayer();
	void AddRoadAssetRoute(const FWLCampaignRouteSpec& Spec, int32& InOutSegmentIndex);
	void LogScaleAudit() const;
	void AddMovementEdge(const FString& A, const FString& B);
	const FWLCampaign3DMovementNodeView* FindMovementNodeById(const FString& NodeId) const;
	FString FindNearestMovementNodeId(const FWLCampaign3DForceView& Force) const;
	void GetValidMovementDestinations(const FWLCampaign3DForceView& Force, TArray<FWLCampaign3DMovementNodeView>& OutDestinations) const;
	void RebuildMovementDestinationMarkers(const TArray<FWLCampaign3DMovementNodeView>& Destinations);
	void RebuildMovementRoutePreview(const TArray<FWLCampaign3DMovementNodeView>& RouteNodes);
	void AddMovementRoutePreviewSegment(const FVector& Start, const FVector& End, const FLinearColor& Color, int32 SectionIndex);
	void DestroyMovementDestinationMarkers();
	FVector GetForceMarkerLocationForNode(const FWLCampaign3DForceView& Force, const FWLCampaign3DMovementNodeView& Node) const;
	void RebuildPointSelectionHighlight(const FVector& Location, float Radius, const FLinearColor& Color);
	void AddVegetationScatter();
	void AddInstance(UInstancedStaticMeshComponent* Component, const FVector& Location, const FRotator& Rotation, const FVector& Scale);
	void AddOverviewLabel(
		const FString& Text,
		float Lon,
		float Lat,
		float ZOffset,
		float WorldSize,
		const FColor& Color,
		bool bShowInGlobal,
		bool bShowInRegion);
	void UpdateOverviewLabelVisibility(bool bStrategicVisible);
	void ConfigureInstancedComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, const FLinearColor& Color);
	void AddProvinceMarkers();
	void AddProvinceMarker(const FWLProvinceData& Province);
	void AddRoutes();
	void AddRouteSegment(const FWLProvinceData& A, const FWLProvinceData& B);
	void SetupLightingAndCamera();
	void DestroyPresentationActors();
	void SetComponentSetActive(bool bActive);
	void SetDetailedLayerVisible(bool bVisible);
	void SetStrategicLayerVisible(bool bVisible);
};
