// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

#include "WorldLeader.h"
#include "Campaign/WLDataRegistry.h"
#include "Presentation/WLCampaignOverviewBuilder.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "Presentation/WLCampaignRouteBuilder.h"
#include "Presentation/WLCampaignSettlementBuilder.h"
#include "Presentation/WLCampaignTerrainBuilder.h"
#include "Presentation/WLCampaignVisualStyle.h"
#include "Presentation/WLCampaignWaterBuilder.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ConstructorHelpers.h"
#include "UnrealClient.h"

void AWLCampaign3DView::SetDetailedLayerVisible(bool bVisible)
{
	if (TerrainMesh)
	{
		TerrainMesh->SetVisibility(bVisible, true);
		TerrainMesh->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->SetVisibility(bVisible, true);
	}
	if (RoadMesh)
	{
		RoadMesh->SetVisibility(bVisible, true);
	}
	for (UStaticMeshComponent* Road : RoadAssetComponents)
	{
		if (Road)
		{
			Road->SetVisibility(bVisible, true);
		}
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bVisible, true);
	}
	for (UPrimitiveComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bVisible && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bVisible && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bVisible, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bVisible, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bVisible, true);
		}
	}
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bVisible, true);
		}
	}
	for (UInstancedStaticMeshComponent* Instanced : {
		SettlementBlockInstances,
		SettlementTowerInstances,
		PortInstances,
		ArmyMarkerInstances
	})
	{
		if (Instanced)
		{
			Instanced->SetVisibility(bVisible, true);
		}
	}
	for (UInstancedStaticMeshComponent* Nature : NatureInstances)
	{
		if (Nature)
		{
			Nature->SetVisibility(bVisible, true);
		}
	}
	for (UStaticMeshComponent* Building : CityBuildingComponents)
	{
		if (Building)
		{
			Building->SetVisibility(bVisible, true);
		}
	}
}

void AWLCampaign3DView::SetStrategicLayerVisible(bool bVisible)
{
	if (OverviewMesh)
	{
		OverviewMesh->SetVisibility(bVisible, true);
		OverviewMesh->SetMeshSectionVisible(0, bVisible);
		OverviewMesh->SetMeshSectionVisible(1, bVisible);
		OverviewMesh->SetMeshSectionVisible(2, bVisible && CurrentZoomLOD == EWLCampaign3DZoomLOD::Region);
		OverviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	UpdateOverviewLabelVisibility(bVisible);
}

void AWLCampaign3DView::ApplyZoomLOD(float CameraHeight)
{
	const bool bActive = !IsHidden();

	// Camara inclinada con pitch DINAMICO (estilo Total War): casi cenital al alejar
	// (para leer el mapa) e inclinada al acercar (para ver el mundo en 3D). La
	// navegacion y la seleccion usan deproyeccion, asi que funcionan a cualquier angulo.
	if (ViewCamera)
	{
		// Pitch en "V": CERCA picado (-64) para VER el suelo/las calles de la ciudad mirando HACIA
		// DENTRO de la cuadricula (a -46 las fachadas tapaban el suelo); TEATRO mas oblicuo (-50) para
		// sensacion 3D con varias ciudades; LEJOS casi cenital (-84) para leer el mapa.
		float Pitch;
		if (CameraHeight <= 60000.f)
		{
			Pitch = FMath::GetMappedRangeValueClamped(FVector2D(15000.f, 60000.f), FVector2D(-64.f, -50.f), CameraHeight);
		}
		else
		{
			Pitch = FMath::GetMappedRangeValueClamped(FVector2D(60000.f, 320000.f), FVector2D(-50.f, -84.f), CameraHeight);
		}
		ViewCamera->SetActorRotation(FRotator(Pitch, 0.f, 0.f));
	}

	if (CameraHeight >= 1200000.f)
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Global;
	}
	else if (CameraHeight >= 620000.f)
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Region;
	}
	else if (CameraHeight <= 76000.f)
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Close;
	}
	else
	{
		CurrentZoomLOD = EWLCampaign3DZoomLOD::Theater;
	}

	const bool bStrategic = bActive
		&& (CurrentZoomLOD == EWLCampaign3DZoomLOD::Region || CurrentZoomLOD == EWLCampaign3DZoomLOD::Global);
	// Region is a strategic map only. Detailed 3D terrain and presentation
	// geometry start at Theater/Close, otherwise relief can bleed through the
	// flat overview map.
	const bool bTheaterTerrain = bActive
		&& (CurrentZoomLOD == EWLCampaign3DZoomLOD::Theater || CurrentZoomLOD == EWLCampaign3DZoomLOD::Close);
	const bool bFineDetail = bActive
		&& (CurrentZoomLOD == EWLCampaign3DZoomLOD::Close || CurrentZoomLOD == EWLCampaign3DZoomLOD::Theater);
	// Detalle de CIUDAD (mallas): aparecen en el mismo umbral que las carreteras (<=250k), no a 200k
	// exacto (a 209k ya deben verse). El amasijo de nombres lo controla ahora el LOD por importancia.
	const bool bCityDetail = bFineDetail && CameraHeight <= 250000.f;
	// Carreteras: solo en zoom de DETALLE (<=250k). Mas lejos = solo las lineas de pais (frontera),
	// sin marana de vias. Relevo limpio en 250k: arriba lineas de pais, abajo carreteras (y las
	// ciudades aparecen <=200k). Resuelve el estado raro de ~318k (vias por todos lados sin ciudades).
	const bool bRoads = bFineDetail && CameraHeight <= 250000.f;

	SetStrategicLayerVisible(bStrategic);
	SetDetailedLayerVisible(bTheaterTerrain);
	if (SeaDetailMesh)
	{
		SeaDetailMesh->SetVisibility(bFineDetail, true);
		SeaDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Frontera nacional (ribete): AYUDA cuando se ven varios paises (vista continental),
	// ESTORBA pegado a un pais (de muy cerca traza toda la costa recortada -> ruido sobre
	// el mar). Por eso se muestra con terreno detallado y a cierta distancia, y se oculta
	// SOLO al acercarse mucho. (Antes la oculte en todo Teatro: error, dejaba paises sin
	// frontera en la vista continental.)
	if (BoundaryMesh)
	{
		// Frontera (linea blanca) = la vista LEJANA: se muestra >=250k, justo donde se ocultan las
		// carreteras. Asi de lejos se leen las formas de los paises (sin maraña de vias) y no hay hueco
		// vacio. De cerca (<250k) se ocultan y mandan las carreteras/ciudades. (Antes >=440k.)
		BoundaryMesh->SetVisibility(bTheaterTerrain && CameraHeight >= 250000.f, true);
	}

	if (RoadMesh)
	{
		// Carreteras solo en zoom de DETALLE (<=250k). Mas lejos manda la frontera (lineas de pais).
		RoadMesh->SetVisibility(bRoads, true);
	}
	for (UStaticMeshComponent* Road : RoadAssetComponents)
	{
		if (Road)
		{
			Road->SetVisibility(bRoads, true);
		}
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bCityDetail, true);
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bCityDetail, true);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bRoads, true);
		}
	}
	// Escala de pantalla (tamano casi CONSTANTE en pantalla a cualquier zoom): la usan los nombres de
	// PAIS y de CIUDAD. (ref 52000 => ~22px para una ciudad normal.)
	const float LabelScreenScale = FMath::Clamp(CameraHeight / 52000.f, 0.7f, 11.f);
	// Etiquetas de PAIS: solo en Theater (Region usa el overview). Se reescalan por altura de camara
	// (antes era tamano FIJO -> diminutas de lejos) y un poco mas grandes que las de ciudad (x1.25).
	const bool bDetailedCountryLabels = bFineDetail && CurrentZoomLOD == EWLCampaign3DZoomLOD::Theater;
	for (int32 LabelIndex = 0; LabelIndex < Labels.Num(); ++LabelIndex)
	{
		UTextRenderComponent* Label = Labels[LabelIndex];
		if (!Label)
		{
			continue;
		}
		Label->SetVisibility(bDetailedCountryLabels, true);
		if (CountryLabelBaseSizes.IsValidIndex(LabelIndex))
		{
			Label->SetWorldSize(CountryLabelBaseSizes[LabelIndex] * LabelScreenScale * 1.25f);
		}
	}
	for (int32 LabelIndex = 0; LabelIndex < SettlementLabels.Num(); ++LabelIndex)
	{
		UTextRenderComponent* Label = SettlementLabels[LabelIndex];
		if (!Label)
		{
			continue;
		}
		// LOD por IMPORTANCIA: cada label tiene su altura maxima (capital 360k, grande 235k, puerto/
		// industrial ~170-195k, fronteriza 110k). De lejos solo capitales; al acercar aparecen las
		// demas -> los nombres no se amontonan ni se cortan. (Antes: todas <=360k = amasijo a 317k.)
		const float LabelMaxHeight = SettlementLabelMaxHeights.IsValidIndex(LabelIndex)
			? SettlementLabelMaxHeights[LabelIndex]
			: 360000.f;
		Label->SetVisibility(bFineDetail && CameraHeight <= LabelMaxHeight, true);
		if (SettlementLabelBaseSizes.IsValidIndex(LabelIndex))
		{
			Label->SetWorldSize(SettlementLabelBaseSizes[LabelIndex] * LabelScreenScale);
		}
	}
	for (UStaticMeshComponent* Building : CityBuildingComponents)
	{
		if (Building)
		{
			Building->SetVisibility(bCityDetail, true);
		}
	}
	// Vegetacion / relieve de naturaleza: detalle fino (Teatro/Cercano), como antes los conos.
	for (UInstancedStaticMeshComponent* Nature : NatureInstances)
	{
		if (Nature)
		{
			Nature->SetVisibility(bFineDetail, true);
		}
	}
	if (ArmyMarkerInstances)
	{
		ArmyMarkerInstances->SetVisibility(bFineDetail, true);
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bFineDetail, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bFineDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail, true);
		}
	}
	if (PortInstances)
	{
		PortInstances->SetVisibility(bFineDetail, true);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bFineDetail && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bFineDetail, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bFineDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail, true);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bFineDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bFineDetail && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
	}
	if (TerritoryLayer)
	{
		TerritoryLayer->ApplyVisibility(CameraHeight);
	}
}

void AWLCampaign3DView::SetComponentSetActive(bool bActive)
{
	const bool bDetailVisible = bActive
		&& (CurrentZoomLOD == EWLCampaign3DZoomLOD::Close || CurrentZoomLOD == EWLCampaign3DZoomLOD::Theater);

	if (SeaMesh)
	{
		SeaMesh->SetVisibility(bActive, true);
		SeaMesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (SeaDetailMesh)
	{
		SeaDetailMesh->SetVisibility(bActive && CurrentZoomLOD != EWLCampaign3DZoomLOD::Global, true);
		SeaDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (OverviewMesh)
	{
		OverviewMesh->SetVisibility(bActive, true);
		OverviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (TerrainMesh)
	{
		TerrainMesh->SetVisibility(bActive, true);
		TerrainMesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->SetVisibility(bActive, true);
		BoundaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (RoadMesh)
	{
		// Las carreteras SOLO en detalle fino (Teatro + Cercano), nunca al alejar a Region.
		// (Antes aqui era bActive y reanulaba la regla de LOD => la red continental de
		// carreteras aparecia en la vista Region y tapaba el mapa.)
		RoadMesh->SetVisibility(bDetailVisible, true);
		RoadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Road : RoadAssetComponents)
	{
		if (Road)
		{
			Road->SetVisibility(bDetailVisible, true);
			Road->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bActive, true);
		SettlementMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UPrimitiveComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bActive && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
		SelectionHighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bActive && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bActive, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bActive, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bActive, true);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UInstancedStaticMeshComponent* Instanced : {
		SettlementBlockInstances,
		SettlementTowerInstances,
		PortInstances,
		ArmyMarkerInstances
	})
	{
		if (Instanced)
		{
			Instanced->SetVisibility(bActive, true);
			Instanced->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UInstancedStaticMeshComponent* Nature : NatureInstances)
	{
		if (Nature)
		{
			Nature->SetVisibility(bActive, true);
			Nature->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Building : CityBuildingComponents)
	{
		if (Building)
		{
			Building->SetVisibility(bActive, true);
			Building->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bDetailVisible, true);
		}
	}
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UTextRenderComponent* Label : SettlementLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UTextRenderComponent* Label : OverviewLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}

	if (bActive)
	{
		const float CameraHeight = ViewCamera ? ViewCamera->GetActorLocation().Z : DefaultCameraLocation.Z;
		ApplyZoomLOD(CameraHeight);
	}
	else
	{
		SetDetailedLayerVisible(false);
		SetStrategicLayerVisible(false);
	}
}
