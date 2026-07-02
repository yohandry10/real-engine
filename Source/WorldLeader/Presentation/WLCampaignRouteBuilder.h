// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

enum class EWLCampaignRouteType : uint8
{
	Primary,
	Secondary,
	Rural,
	PortAccess,
	BorderCrossing
};

struct FWLCampaignRouteSpec
{
	FString Name;
	EWLCampaignRouteType Type = EWLCampaignRouteType::Secondary;
	TArray<FVector2D> Points;
	int32 Smoothness = 5;
	bool bShowJunctions = true;
};

class FWLCampaignRouteBuilder
{
public:
	static void BuildDefaultTheaterRoutes(
		UProceduralMeshComponent* RoadMesh,
		UMaterialInterface* RoadMaterial,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);

	// Anade una lista arbitraria de rutas a la malla (una seccion mas). Sirve para
	// redes generadas (p.ej. conectar las ciudades de cada pais).
	static void AppendRoutes(
		UProceduralMeshComponent* RoadMesh,
		UMaterialInterface* RoadMaterial,
		const TArray<FWLCampaignRouteSpec>& Routes,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat);

	// Circulos de ciudad (X=lon, Y=lat, Z=radio en grados) donde la cinta de carretera se RECORTA: la
	// via flota ~900u sobre el piso de la ciudad, asi que sin recorte la cruza POR ENCIMA. Recortando,
	// la via llega al borde de la ciudad y para (la cuadricula interna hace de calles). Afecta a las
	// rutas construidas DESPUES de llamar esto. Pasar array vacio para desactivar.
	static void SetCityClipCircles(const TArray<FVector>& Circles);

	// Mascara de TIERRA: la cinta SOLO se dibuja sobre tierra. Sin esto la ruta costera flotaba sobre
	// el mar (la via va a Z+990 y el mar a -2350). El view pasa un test (lon,lat)->bool. Test vacio
	// (TFunction nula) = desactivado.
	static void SetRoadLandMask(TFunction<bool(float Lon, float Lat)> LandTest);

	// Todas las rutas (Lon/Lat) de las vias visibles, acumuladas al construirlas. El view las convierte en
	// polilineas de mundo para que los ejercitos NAVEGUEN por el trazado real de la carretera.
	static const TArray<FWLCampaignRouteSpec>& GetAllRoadRoutes();

	// Waypoints (Lon/Lat) SUAVIZADOS (Catmull-Rom) + DENSIFICADOS de una ruta, EXACTAMENTE como la via visible
	// que dibuja AddRoute. El view los usa para que los ejercitos conduzcan por el trazado que se VE (no por los
	// puntos de control crudos y escasos, que hacian que el tanque cortara/saltara y entrara por un punto lejano).
	static TArray<FVector2D> BuildSmoothedRouteLonLat(const FWLCampaignRouteSpec& Spec);
};
