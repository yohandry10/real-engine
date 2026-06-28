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
#include "Presentation/WLCampaign3DViewRoadDetail.h"

namespace
{
	constexpr float DetailedCityAndRoadMaxHeight = 260000.f;
}

void AWLCampaign3DView::SetDetailedLayerVisible(bool bVisible)
{
	if (TerrainMesh)
	{
		TerrainMesh->SetVisibility(bVisible, true);
		TerrainMesh->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (TerrainDetailMesh)
	{
		TerrainDetailMesh->SetVisibility(false, true);
		TerrainDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->SetVisibility(bVisible, true);
	}
	if (RoadMesh)
	{
		RoadMesh->SetVisibility(bVisible, true);
	}
	if (UProceduralMeshComponent* RoadDetailMesh = WLRoadDetail::FindRoadDetailMesh(this))
	{
		RoadDetailMesh->SetVisibility(bVisible, true);
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
	if (BorderOutpostMesh)
	{
		BorderOutpostMesh->SetVisibility(bVisible, true);
		BorderOutpostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (PortFacilityMesh)
	{
		PortFacilityMesh->SetVisibility(bVisible, true);
		PortFacilityMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	FString BorderOutpostVisualTest;
	FString BorderOutpostScreenshotSequence;
	FString PortFacilityVisualTest;
	FString PortFacilityScreenshotSequence;
	const bool bLockBorderOutpostVisualCamera = FParse::Value(
		FCommandLine::Get(),
		TEXT("WLBorderOutpostVisualTest="),
		BorderOutpostVisualTest)
		|| FParse::Value(
			FCommandLine::Get(),
			TEXT("WLBorderOutpostScreenshotSequence="),
			BorderOutpostScreenshotSequence)
		|| FParse::Value(
			FCommandLine::Get(),
			TEXT("WLPortFacilityVisualTest="),
			PortFacilityVisualTest)
		|| FParse::Value(
			FCommandLine::Get(),
			TEXT("WLPortFacilityScreenshotSequence="),
			PortFacilityScreenshotSequence);

	// Camara inclinada con pitch DINAMICO (estilo Total War): casi cenital al alejar
	// (para leer el mapa) e inclinada al acercar (para ver el mundo en 3D). La
	// navegacion y la seleccion usan deproyeccion, asi que funcionan a cualquier angulo.
	if (ViewCamera && !bLockBorderOutpostVisualCamera)
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
	// Detalle pesado rechazado: 90k debe conservar el suelo liviano de teatro hasta rehacer esta
	// capa con assets que realmente mejoren calidad y rendimiento.
	const bool bCloseTerrainDetail = false;
	const bool bCityDetail = bFineDetail && CameraHeight <= DetailedCityAndRoadMaxHeight;
	const bool bStrategicRoads = false;
	const bool bDetailRoads = bFineDetail && CameraHeight <= DetailedCityAndRoadMaxHeight;

	SetStrategicLayerVisible(bStrategic);
	SetDetailedLayerVisible(bTheaterTerrain);
	if (SeaDetailMesh)
	{
		SeaDetailMesh->SetVisibility(bCloseTerrainDetail, true);
		SeaDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (TerrainDetailMesh)
	{
		TerrainDetailMesh->SetVisibility(bCloseTerrainDetail, true);
		TerrainDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Frontera nacional (ribete): AYUDA cuando se ven varios paises (vista continental),
	// ESTORBA pegado a un pais (de muy cerca traza toda la costa recortada -> ruido sobre
	// el mar). Por eso se muestra con terreno detallado y a cierta distancia, y se oculta
	// SOLO al acercarse mucho. (Antes la oculte en todo Teatro: error, dejaba paises sin
	// frontera en la vista continental.)
	if (BoundaryMesh)
	{
		// Frontera (linea blanca) = lectura estrategica de Teatro. Debe entrar antes de que las
		// ciudades pesadas desaparezcan, para que 130k/210k no queden sin lineas ni paises.
		BoundaryMesh->SetVisibility(bTheaterTerrain && CameraHeight >= 120000.f, true);
	}

	if (RoadMesh)
	{
		// La red visible en detalle debe ser la malla recortada contra ciudades,
		// y las ciudades se muestran en el mismo rango para evitar huecos flotantes.
		RoadMesh->SetVisibility(bStrategicRoads, true);
	}
	if (UProceduralMeshComponent* RoadDetailMesh = WLRoadDetail::FindRoadDetailMesh(this))
	{
		RoadDetailMesh->SetVisibility(bDetailRoads, true);
	}
	for (UStaticMeshComponent* Road : RoadAssetComponents)
	{
		if (Road)
		{
			Road->SetVisibility(bDetailRoads, true);
		}
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bCityDetail, true);
	}
	if (BorderOutpostMesh)
	{
		BorderOutpostMesh->SetVisibility(bCityDetail, true);
		BorderOutpostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
			Route->SetVisibility(bDetailRoads, true);
		}
	}
	// Escala de pantalla (tamano casi CONSTANTE en pantalla a cualquier zoom): la usan los nombres de
	// PAIS y de CIUDAD. (ref 52000 => ~22px para una ciudad normal.)
	const float LabelScreenScale = FMath::Clamp(CameraHeight / 70000.f, 0.7f, 8.0f);
	// Etiquetas de PAIS: solo en Theater (Region usa el overview). Se reescalan por altura de camara
	// (antes era tamano FIJO -> diminutas de lejos) y un poco mas grandes que las de ciudad (x1.25).
	const bool bDetailedCountryLabels = bFineDetail
		&& CurrentZoomLOD == EWLCampaign3DZoomLOD::Theater
		&& CameraHeight > DetailedCityAndRoadMaxHeight
		&& CameraHeight <= 620000.f;
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
			Label->SetWorldSize(CountryLabelBaseSizes[LabelIndex] * LabelScreenScale);
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
	// Vegetacion / relieve de naturaleza: detalle cercano pesado; los meshes de ciudad y
	// caminos recortados siguen visibles hasta 260k para que no queden huecos sin ciudad.
	for (UInstancedStaticMeshComponent* Nature : NatureInstances)
	{
		if (Nature)
		{
			Nature->SetVisibility(false, true);
		}
	}
	if (ArmyMarkerInstances)
	{
		ArmyMarkerInstances->SetVisibility(bCityDetail, true);
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bCityDetail, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bCityDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bCityDetail, true);
		}
	}
	if (PortInstances)
	{
		PortInstances->SetVisibility(bCityDetail, true);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bCityDetail && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bCityDetail, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bCityDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bCityDetail, true);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetCollisionEnabled(bCityDetail ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bCityDetail && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
	}
	if (TerritoryLayer)
	{
		TerritoryLayer->ApplyVisibility(CameraHeight);
	}
}

void AWLCampaign3DView::SetComponentSetActive(bool bActive)
{
	const float CameraHeight = ViewCamera ? ViewCamera->GetActorLocation().Z : DefaultCameraLocation.Z;
	const bool bTheaterVisible = bActive
		&& (CurrentZoomLOD == EWLCampaign3DZoomLOD::Close || CurrentZoomLOD == EWLCampaign3DZoomLOD::Theater);
	const bool bCloseDetailVisible = bTheaterVisible && CameraHeight <= DetailedCityAndRoadMaxHeight;
	const bool bStrategicRoadVisible = false;
	const bool bRoadDetailVisible = bTheaterVisible && CameraHeight <= DetailedCityAndRoadMaxHeight;

	if (SeaMesh)
	{
		SeaMesh->SetVisibility(bActive, true);
		SeaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (SeaDetailMesh)
	{
		SeaDetailMesh->SetVisibility(false, true);
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
	if (TerrainDetailMesh)
	{
		TerrainDetailMesh->SetVisibility(false, true);
		TerrainDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->SetVisibility(bTheaterVisible && CameraHeight >= 120000.f, true);
		BoundaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (RoadMesh)
	{
		// La version estrategica sin recorte queda apagada; RoadDetailMesh cubre el
		// rango de ciudades para que los recortes siempre tengan ciudad visible.
		RoadMesh->SetVisibility(bStrategicRoadVisible, true);
		RoadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UProceduralMeshComponent* RoadDetailMesh = WLRoadDetail::FindRoadDetailMesh(this))
	{
		RoadDetailMesh->SetVisibility(bRoadDetailVisible, true);
		RoadDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Road : RoadAssetComponents)
	{
		if (Road)
		{
			Road->SetVisibility(bRoadDetailVisible, true);
			Road->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bCloseDetailVisible, true);
		SettlementMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (BorderOutpostMesh)
	{
		BorderOutpostMesh->SetVisibility(bCloseDetailVisible, true);
		BorderOutpostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UPrimitiveComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bCloseDetailVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : CitySelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bCloseDetailVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->SetVisibility(
			bCloseDetailVisible && (!SelectedProvinceHighlightId.IsEmpty() || !SelectedCityHighlightId.IsEmpty() || !SelectedForceHighlightId.IsEmpty()),
			true);
		SelectionHighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->SetVisibility(bCloseDetailVisible && !PreviewMovementDestinationNodeId.IsEmpty(), true);
		MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bCloseDetailVisible, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bCloseDetailVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bCloseDetailVisible, true);
		}
	}
	for (UStaticMeshComponent* Marker : ForceMarkerComponents)
	{
		if (Marker)
		{
			Marker->SetVisibility(bCloseDetailVisible, true);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UPrimitiveComponent* Marker : ForceSelectionMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(false, true);
			Marker->SetHiddenInGame(false, true);
			Marker->SetCollisionEnabled(bCloseDetailVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UTextRenderComponent* Label : ForceMarkerLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bCloseDetailVisible, true);
		}
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bCloseDetailVisible, true);
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
			Instanced->SetVisibility(bCloseDetailVisible, true);
			Instanced->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UInstancedStaticMeshComponent* Nature : NatureInstances)
	{
		if (Nature)
		{
			Nature->SetVisibility(false, true);
			Nature->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Building : CityBuildingComponents)
	{
		if (Building)
		{
			Building->SetVisibility(bCloseDetailVisible, true);
			Building->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bRoadDetailVisible, true);
		}
	}
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bTheaterVisible && CameraHeight > DetailedCityAndRoadMaxHeight && CameraHeight <= 620000.f, true);
		}
	}
	for (UTextRenderComponent* Label : SettlementLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bTheaterVisible, true);
		}
	}
	for (UTextRenderComponent* Label : OverviewLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive && (CurrentZoomLOD == EWLCampaign3DZoomLOD::Region || CurrentZoomLOD == EWLCampaign3DZoomLOD::Global), true);
		}
	}

	if (bActive)
	{
		ApplyZoomLOD(CameraHeight);
	}
	else
	{
		SetDetailedLayerVisible(false);
		SetStrategicLayerVisible(false);
	}
}
