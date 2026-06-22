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
		TreeInstances,
		BrushInstances,
		PortInstances,
		ArmyMarkerInstances
	})
	{
		if (Instanced)
		{
			Instanced->SetVisibility(bVisible, true);
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
		const float Pitch = FMath::GetMappedRangeValueClamped(
			FVector2D(60000.f, 320000.f), FVector2D(-46.f, -84.f), CameraHeight);
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
		// Frontera visible en TEATRO (76k+, se ven varios paises); Region usa solo
		// OverviewMesh para evitar que el relieve/faja andina tape el mapa.
		BoundaryMesh->SetVisibility(bTheaterTerrain && CurrentZoomLOD != EWLCampaign3DZoomLOD::Close, true);
	}

	if (RoadMesh)
	{
		// Carreteras SOLO en detalle fino (Teatro + Cercano), NO al alejar a Region. En
		// Region (vista continental lejana) se lee la forma de los paises por la frontera,
		// no las vias; dibujarlas ahi era ruido/error.
		RoadMesh->SetVisibility(bFineDetail, true);
	}
	for (UStaticMeshComponent* Road : RoadAssetComponents)
	{
		if (Road)
		{
			Road->SetVisibility(bFineDetail, true);
		}
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bFineDetail, true);
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bFineDetail, true);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bFineDetail, true);
		}
	}
	// Etiquetas de PAIS de la capa detallada: solo al acercar. Region usa la capa
	// estrategica/overview, no la presentacion 3D.
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail, true);
		}
	}
	// Etiquetas de CIUDAD: solo al acercar (Teatro/Cercano), nunca al alejar.
	for (UTextRenderComponent* Label : SettlementLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bFineDetail, true);
		}
	}
	for (UStaticMeshComponent* Building : CityBuildingComponents)
	{
		if (Building)
		{
			Building->SetVisibility(bFineDetail, true);
		}
	}
	if (TreeInstances)
	{
		TreeInstances->SetVisibility(bFineDetail, true);
	}
	if (BrushInstances)
	{
		BrushInstances->SetVisibility(bFineDetail, true);
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
		TreeInstances,
		BrushInstances,
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
