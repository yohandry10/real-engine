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

bool AWLCampaign3DView::TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DProvinceView& OutProvince) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ProvinceMarkers.Num() && Index < ProvinceViews.Num(); ++Index)
	{
		if (ProvinceMarkers[Index] == Component)
		{
			OutProvince = ProvinceViews[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetTerritoryForComponent(
	const UPrimitiveComponent* Component,
	FWLCampaignTerritoryRegionView& OutTerritory) const
{
	return TerritoryLayer && TerritoryLayer->TryGetTerritoryForComponent(Component, OutTerritory);
}

bool AWLCampaign3DView::TryGetTerritoryAtWorldLocation(
	const FVector& WorldLocation,
	FWLCampaignTerritoryRegionView& OutTerritory) const
{
	return TerritoryLayer && TerritoryLayer->TryGetTerritoryAtWorldLocation(WorldLocation, true, OutTerritory);
}

bool AWLCampaign3DView::TryGetCityForComponent(const UPrimitiveComponent* Component, FWLCampaign3DCityView& OutCity) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < CitySelectionMarkers.Num() && Index < CityViews.Num(); ++Index)
	{
		if (CitySelectionMarkers[Index] == Component)
		{
			OutCity = CityViews[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetCityNearWorldLocation(
	const FVector& WorldLocation,
	float MaxDistance,
	FWLCampaign3DCityView& OutCity) const
{
	float BestDistanceSq = FMath::Square(FMath::Max(1000.f, MaxDistance));
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < CityViews.Num(); ++Index)
	{
		const FWLCampaign3DCityView& City = CityViews[Index];
		const float DistanceSq = FVector::DistSquared2D(WorldLocation, City.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	OutCity = CityViews[BestIndex];
	return true;
}

bool AWLCampaign3DView::TryGetForceById(const FString& ForceId, FWLCampaign3DForceView& OutForce) const
{
	for (const FWLCampaign3DForceView& Force : ForceViews)
	{
		if (Force.Id.Equals(ForceId, ESearchCase::IgnoreCase))
		{
			OutForce = Force;
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetForceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DForceView& OutForce) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		const bool bProxyHit = ForceSelectionMarkers.IsValidIndex(Index) && ForceSelectionMarkers[Index] == Component;
		const bool bVisualHit = ForceMarkerComponents.IsValidIndex(Index) && ForceMarkerComponents[Index] == Component;
		if (bProxyHit || bVisualHit)
		{
			OutForce = ForceViews[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetForceNearWorldLocation(
	const FVector& WorldLocation,
	float MaxDistance,
	FWLCampaign3DForceView& OutForce) const
{
	float BestDistanceSq = FMath::Square(FMath::Max(1000.f, MaxDistance));
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		if (!IsForceSelectableByProximity(Index))
		{
			continue;
		}
		const FWLCampaign3DForceView& Force = ForceViews[Index];
		const float DistanceSq = FVector::DistSquared2D(WorldLocation, Force.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	OutForce = ForceViews[BestIndex];
	return true;
}

bool AWLCampaign3DView::IsForceSelectableByProximity(int32 Index) const
{
	if (!ForceViews.IsValidIndex(Index))
	{
		return false;
	}

	const FWLCampaign3DForceView& Force = ForceViews[Index];
	if (Force.bIsRecruitmentBase)
	{
		// El FUERTE (base de reclutamiento) es un EDIFICIO sin token: SIEMPRE seleccionable por proximidad
		// (empieza VACIO, y hay que poder clicarlo para abrir el panel y reclutar). Antes devolvia false, asi
		// que clicar el fuerte caia al terreno y agarraba la ciudad cercana (18500u) o nada. Esta proximidad
		// (2800u, cubre el edificio) corre ANTES que la de ciudad y el fuerte esta a >=14km de toda ciudad,
		// asi que no roba clics de ciudad.
		return true;
	}
	if (!ForceHasTroopsForToken(Index))
	{
		return false;
	}
	if (!ForceMarkerComponents.IsValidIndex(Index) || !ForceMarkerComponents[Index])
	{
		return false;
	}
	return ForceMarkerComponents[Index]->IsVisible();
}

bool AWLCampaign3DView::TryGetMovementDestinationForComponent(
	const UPrimitiveComponent* Component,
	FWLCampaign3DMovementNodeView& OutNode) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ActiveMovementDestinations.Num(); ++Index)
	{
		const bool bProxyHit = MovementDestinationSelectionMarkers.IsValidIndex(Index)
			&& MovementDestinationSelectionMarkers[Index] == Component;
		const bool bVisualHit = MovementDestinationComponents.IsValidIndex(Index)
			&& MovementDestinationComponents[Index] == Component;
		if (bProxyHit || bVisualHit)
		{
			OutNode = ActiveMovementDestinations[Index];
			return true;
		}
	}
	return false;
}

bool AWLCampaign3DView::TryGetMovementDestinationNearWorldLocation(
	const FVector& WorldLocation,
	float MaxDistance,
	FWLCampaign3DMovementNodeView& OutNode) const
{
	float BestDistanceSq = FMath::Square(FMath::Max(1000.f, MaxDistance));
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ActiveMovementDestinations.Num(); ++Index)
	{
		const FWLCampaign3DMovementNodeView& Node = ActiveMovementDestinations[Index];
		const float DistanceSq = FVector::DistSquared2D(WorldLocation, Node.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	OutNode = ActiveMovementDestinations[BestIndex];
	return true;
}

bool AWLCampaign3DView::BuildMovementRouteForForce(
	const FString& ForceId,
	const FString& DestinationNodeId,
	TArray<FWLCampaign3DMovementNodeView>& OutRouteNodes,
	int32& OutEstimatedTurns) const
{
	OutRouteNodes.Reset();
	OutEstimatedTurns = 0;

	FWLCampaign3DForceView Force;
	if (!TryGetForceById(ForceId, Force) || !Force.bMovable)
	{
		return false;
	}

	const FString OriginNodeId = Force.MovementNodeId.IsEmpty() ? FindNearestMovementNodeId(Force) : Force.MovementNodeId;
	if (OriginNodeId.IsEmpty() || DestinationNodeId.IsEmpty() || OriginNodeId.Equals(DestinationNodeId, ESearchCase::IgnoreCase))
	{
		return false;
	}

	const FWLCampaign3DMovementNodeView* OriginNode = FindMovementNodeById(OriginNodeId);
	const FWLCampaign3DMovementNodeView* DestinationNode = FindMovementNodeById(DestinationNodeId);
	if (!OriginNode || !DestinationNode)
	{
		return false;
	}

	const auto NodeAllowedForForce = [&Force](const FWLCampaign3DMovementNodeView& Node)
	{
		if (Force.bAir)
		{
			return false;
		}
		if (Force.bNaval)
		{
			return Node.bPort;
		}
		return true;
	};

	if (!NodeAllowedForForce(*DestinationNode))
	{
		return false;
	}

	TArray<FString> Queue;
	TMap<FString, FString> CameFrom;
	Queue.Add(OriginNodeId);
	CameFrom.Add(OriginNodeId, TEXT(""));
	for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
	{
		const FString Current = Queue[Cursor];
		if (Current.Equals(DestinationNodeId, ESearchCase::IgnoreCase))
		{
			break;
		}

		const TArray<FString>* Neighbors = MovementAdjacency.Find(Current);
		if (!Neighbors)
		{
			continue;
		}
		for (const FString& NeighborId : *Neighbors)
		{
			if (CameFrom.Contains(NeighborId))
			{
				continue;
			}
			const FWLCampaign3DMovementNodeView* NeighborNode = FindMovementNodeById(NeighborId);
			if (!NeighborNode || !NodeAllowedForForce(*NeighborNode))
			{
				continue;
			}
			CameFrom.Add(NeighborId, Current);
			Queue.Add(NeighborId);
		}
	}

	if (!CameFrom.Contains(DestinationNodeId))
	{
		return false;
	}

	TArray<FString> ReverseNodeIds;
	FString Current = DestinationNodeId;
	while (!Current.IsEmpty())
	{
		ReverseNodeIds.Add(Current);
		const FString* Previous = CameFrom.Find(Current);
		Current = Previous ? *Previous : FString();
	}

	for (int32 Index = ReverseNodeIds.Num() - 1; Index >= 0; --Index)
	{
		if (const FWLCampaign3DMovementNodeView* Node = FindMovementNodeById(ReverseNodeIds[Index]))
		{
			OutRouteNodes.Add(*Node);
		}
	}

	OutEstimatedTurns = FMath::Max(1, OutRouteNodes.Num() - 1);
	return OutRouteNodes.Num() >= 2;
}

bool AWLCampaign3DView::UpdateForceMovementLocation(
	const FString& ForceId,
	const FString& DestinationNodeId,
	FWLCampaign3DForceView& OutForce)
{
	const FWLCampaign3DMovementNodeView* DestinationNode = FindMovementNodeById(DestinationNodeId);
	if (!DestinationNode)
	{
		return false;
	}

	TArray<FWLCampaign3DMovementNodeView> RouteNodes;
	int32 EstimatedTurns = 0;
	if (!BuildMovementRouteForForce(ForceId, DestinationNodeId, RouteNodes, EstimatedTurns))
	{
		return false;
	}

	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		FWLCampaign3DForceView& Force = ForceViews[Index];
		if (!Force.Id.Equals(ForceId, ESearchCase::IgnoreCase) || !Force.bMovable)
		{
			continue;
		}

		// Guarda la RUTA COMPLETA como orden de marcha y avanza SOLO la primera etapa este turno (el "cargador"
		// limita cuanto recorre). Los turnos [M] siguientes continuan via AdvanceArmyMovements: un destino
		// lejano tarda varios turnos, con animacion por la carretera. Ya NO se teletransporta al destino.
		Force.bHasMoveTarget = false;   // el boton Mover (ruta por nodos) anula un movimiento libre en curso
		Force.MovePathNodeIds.Reset();
		for (const FWLCampaign3DMovementNodeView& Node : RouteNodes)
		{
			Force.MovePathNodeIds.Add(Node.Id);
		}
		AdvanceForceAlongPath(Index);

		OutForce = Force;
		if (SelectedForceHighlightId.Equals(Force.Id, ESearchCase::IgnoreCase))
		{
			SetSelectedForceHighlight(Force.Id);
		}
		return true;
	}

	return false;
}

void AWLCampaign3DView::ShowMovementDestinationOptions(const FString& ForceId)
{
	ActiveMovementForceId = ForceId;
	PreviewMovementDestinationNodeId.Reset();
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->ClearAllMeshSections();
	}

	FWLCampaign3DForceView Force;
	TArray<FWLCampaign3DMovementNodeView> Destinations;
	if (TryGetForceById(ForceId, Force))
	{
		GetValidMovementDestinations(Force, Destinations);
	}
	RebuildMovementDestinationMarkers(Destinations);
}

void AWLCampaign3DView::SetMovementDestinationPreview(const FString& ForceId, const FString& DestinationNodeId)
{
	if (ForceId.IsEmpty() || DestinationNodeId.IsEmpty())
	{
		if (PreviewMovementDestinationNodeId.IsEmpty())
		{
			return;
		}
		if (MovementRoutePreviewMesh)
		{
			MovementRoutePreviewMesh->ClearAllMeshSections();
		}
		PreviewMovementDestinationNodeId.Reset();
		RebuildMovementDestinationMarkers(ActiveMovementDestinations);
		return;
	}

	if (PreviewMovementDestinationNodeId.Equals(DestinationNodeId, ESearchCase::IgnoreCase))
	{
		return;
	}

	TArray<FWLCampaign3DMovementNodeView> RouteNodes;
	int32 EstimatedTurns = 0;
	if (!BuildMovementRouteForForce(ForceId, DestinationNodeId, RouteNodes, EstimatedTurns))
	{
		return;
	}

	PreviewMovementDestinationNodeId = DestinationNodeId;
	RebuildMovementDestinationMarkers(ActiveMovementDestinations);
	RebuildMovementRoutePreview(RouteNodes);
}

void AWLCampaign3DView::ClearMovementPreview()
{
	ActiveMovementForceId.Reset();
	PreviewMovementDestinationNodeId.Reset();
	ActiveMovementDestinations.Reset();
	DestroyMovementDestinationMarkers();
	if (MovementRoutePreviewMesh)
	{
		MovementRoutePreviewMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetSelectedProvinceHighlight(const FString& ProvinceId)
{
	SelectedProvinceHighlightId = ProvinceId;
	SelectedCityHighlightId.Reset();
	SelectedForceHighlightId.Reset();
	RefreshMilitaryForceMarkerVisuals();
	if (TerritoryLayer)
	{
		TerritoryLayer->SetSelectedTerritory(ProvinceId);
	}

	for (const FWLCampaign3DProvinceView& Province : ProvinceViews)
	{
		if (Province.Id.Equals(ProvinceId, ESearchCase::IgnoreCase))
		{
			RebuildPointSelectionHighlight(Province.WorldLocation + FVector(0.f, 0.f, 820.f), 6200.f, FLinearColor(0.96f, 0.78f, 0.34f, 1.f));
			return;
		}
	}

	if (TerritoryLayer)
	{
		FWLCampaignTerritoryRegionView Territory;
		if (TerritoryLayer->GetRegionById(ProvinceId, Territory))
		{
			RebuildPointSelectionHighlight(Territory.WorldLocation + FVector(0.f, 0.f, 900.f), 5200.f, FLinearColor(0.96f, 0.78f, 0.34f, 1.f));
			return;
		}
	}

	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetSelectedCityHighlight(const FString& CityId)
{
	SelectedCityHighlightId = CityId;
	SelectedProvinceHighlightId.Reset();
	SelectedForceHighlightId.Reset();
	RefreshMilitaryForceMarkerVisuals();

	for (const FWLCampaign3DCityView& City : CityViews)
	{
		if (City.Id.Equals(CityId, ESearchCase::IgnoreCase))
		{
			if (TerritoryLayer && !City.TerritoryId.IsEmpty())
			{
				TerritoryLayer->SetSelectedTerritory(City.TerritoryId);
			}
			RebuildPointSelectionHighlight(City.WorldLocation + FVector(0.f, 0.f, 1240.f), City.bCapital ? 7600.f : 6200.f,
				City.bPort ? FLinearColor(0.46f, 0.82f, 0.86f, 1.f) : FLinearColor(0.96f, 0.78f, 0.34f, 1.f));
			return;
		}
	}

	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetSelectedForceHighlight(const FString& ForceId)
{
	SelectedForceHighlightId = ForceId;
	SelectedProvinceHighlightId.Reset();
	SelectedCityHighlightId.Reset();
	if (TerritoryLayer)
	{
		TerritoryLayer->SetSelectedTerritory(TEXT(""));
	}
	RefreshMilitaryForceMarkerVisuals();

	// NADA de aro/oreola en el suelo para fuerzas: el feedback de seleccion es el propio token (la unidad se
	// agranda y se tinta en RefreshMilitaryForceMarkerVisuals). El anillo dorado se veia mal sobre el tanque.
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

void AWLCampaign3DView::SetHoveredForceHighlight(const FString& ForceId)
{
	if (HoveredForceHighlightId.Equals(ForceId, ESearchCase::IgnoreCase))
	{
		return;
	}

	HoveredForceHighlightId = ForceId;
	RefreshMilitaryForceMarkerVisuals();
}

void AWLCampaign3DView::ClearSelectionHighlight()
{
	SelectedProvinceHighlightId.Reset();
	SelectedCityHighlightId.Reset();
	SelectedForceHighlightId.Reset();
	HoveredForceHighlightId.Reset();
	RefreshMilitaryForceMarkerVisuals();
	if (TerritoryLayer)
	{
		TerritoryLayer->SetSelectedTerritory(TEXT(""));
	}
	if (SelectionHighlightMesh)
	{
		SelectionHighlightMesh->ClearAllMeshSections();
	}
}

FBox2D AWLCampaign3DView::GetViewBounds2D() const
{
	return Bounds;
}

FBox2D AWLCampaign3DView::GetCameraBounds2D(float CameraHeight) const
{
	if (!OverviewBounds.bIsValid)
	{
		return Bounds;
	}

	if (!Bounds.bIsValid)
	{
		return OverviewBounds;
	}

	// Bounds (terreno de DETALLE, ProjectLonLat ~35600 u/grado) y OverviewBounds (mesh America
	// ESTRATEGICO, ProjectStrategicLonLat GeoScale=9000 u/grado) viven en ESPACIOS distintos (~4x).
	// Mezclarlos por altura encogia el limite del teatro: el sur quedaba en ~Santiago (-33.5) y el
	// este en ~lon -43 (cortaba el NE de Brasil). El teatro (rueda hasta 480k) usa SIEMPRE el
	// continente de detalle completo; solo la vista America (boton, altura > tope de rueda) usa el
	// box estrategico para centrar. Switch DURO, sin mezcla de espacios.
	if (CameraHeight < 560000.f)
	{
		return Bounds;
	}
	return OverviewBounds;
}

FVector AWLCampaign3DView::GetTheaterFocusPoint() const
{
	return ProjectLonLat(-69.3f, 7.05f) + FVector(0.f, 0.f, 2600.f);
}

FString AWLCampaign3DView::GetCurrentZoomLODLabel() const
{
	switch (CurrentZoomLOD)
	{
	case EWLCampaign3DZoomLOD::Close:
		return TEXT("Cercano");
	case EWLCampaign3DZoomLOD::Theater:
		return TEXT("Teatro");
	case EWLCampaign3DZoomLOD::Region:
		return TEXT("Region");
	case EWLCampaign3DZoomLOD::Global:
		return TEXT("America");
	default:
		return TEXT("Teatro");
	}
}
