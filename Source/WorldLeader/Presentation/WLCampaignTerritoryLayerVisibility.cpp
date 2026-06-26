// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignTerritoryLayerComponent.h"

#include "Presentation/WLCampaignAmericaLowDetailData.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Presentation/WLCampaignTerritoryLayerPrivate.h"

using namespace WLCampaignTerritoryLayerPrivate;

void UWLCampaignTerritoryLayerComponent::RebuildResourceMarkers()
{
	if (!ResourceMarkerMesh)
	{
		return;
	}
	ResourceMarkerMesh->ClearAllMeshSections();
	ResourceMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResourceMarkerMesh->SetVisibility(false, true);
}

void UWLCampaignTerritoryLayerComponent::RebuildHighlightMesh(
	UProceduralMeshComponent* Mesh,
	const FString& RegionId,
	const FLinearColor& Color,
	float WidthWorld,
	float ZLift)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->ClearAllMeshSections();
	if (RegionId.IsEmpty())
	{
		return;
	}

	const FRegionRecord* Region = Regions.FindByPredicate([&](const FRegionRecord& Candidate)
	{
		return Candidate.View.Id == RegionId;
	});
	if (!Region || Region->WorldPolygon.Num() < 2)
	{
		return;
	}

	FMeshBuildBuffer Buffer;
	for (int32 Index = 0; Index < Region->WorldPolygon.Num(); ++Index)
	{
		AddRibbon(Buffer, Region->WorldPolygon[Index] + FVector(0.f, 0.f, ZLift),
			Region->WorldPolygon[(Index + 1) % Region->WorldPolygon.Num()] + FVector(0.f, 0.f, ZLift),
			WidthWorld, Color);
	}
	if (!Buffer.Verts.IsEmpty())
	{
		Mesh->CreateMeshSection(0, Buffer.Verts, Buffer.Tris, Buffer.Normals, Buffer.UVs, Buffer.Colors, Buffer.Tangents, false);
		if (VertexColorMaterial)
		{
			Mesh->SetMaterial(0, VertexColorMaterial);
		}
	}
}

void UWLCampaignTerritoryLayerComponent::SetPresentationActive(bool bActive)
{
	bLayerActive = bActive;
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::SetLayerToggles(bool bBorders, bool bProvinces, bool bLabels, bool bResources)
{
	bShowBorders = bBorders;
	bShowProvinces = bProvinces;
	bShowLabels = bLabels;
	bShowResources = bResources;
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::ApplyVisibility(float CameraHeight)
{
	LastCameraHeight = CameraHeight;
	const bool bGlobal = CameraHeight >= 360000.f;
	const bool bRegion = CameraHeight >= 185000.f && !bGlobal;
	const bool bDetailedTerritoryVisible = CameraHeight < 115000.f;

	const bool bNationalVisible = bLayerActive && bShowBorders && (bGlobal || bRegion);
	// Antes, al bajar de 115k se dibujaba la rejilla de TODAS las provincias (58 en
	// CO/VE) con sus nombres -> tapaba el mapa con una telarana de lineas. Se desactiva
	// la rejilla y los nombres de provincia: el mapa queda limpio. Las provincias
	// siguen siendo seleccionables (hit meshes) y se resaltan al pasar el cursor o al
	// seleccionarlas (hover/selected highlight, mas abajo).
	const bool bProvinceVisible = false;
	const bool bCountryLabelsVisible = bLayerActive && bShowLabels && (bGlobal || bRegion);
	const bool bProvinceLabelsVisible = false;
	// Resource diamonds were placeholder debug markers; they read as random yellow
	// squares over cities/sea at close zoom and do not belong in the accepted map view.
	const bool bResourcesVisible = false;

	if (NationalBorderMesh)
	{
		NationalBorderMesh->SetVisibility(bNationalVisible, true);
	}
	if (ProvinceBorderMesh)
	{
		ProvinceBorderMesh->SetVisibility(bProvinceVisible, true);
	}
	if (ResourceMarkerMesh)
	{
		ResourceMarkerMesh->SetVisibility(bResourcesVisible, true);
	}
	if (HoverHighlightMesh)
	{
		HoverHighlightMesh->SetVisibility(bLayerActive && !HoveredRegionId.IsEmpty() && (bShowProvinces || bGlobal || bRegion), true);
	}
	if (SelectedHighlightMesh)
	{
		SelectedHighlightMesh->SetVisibility(bLayerActive && !SelectedRegionId.IsEmpty() && (bShowProvinces || bGlobal || bRegion), true);
	}

	for (UTextRenderComponent* Label : CountryLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bCountryLabelsVisible, true);
		}
	}
	for (UTextRenderComponent* Label : ProvinceLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bProvinceLabelsVisible, true);
		}
	}

	for (FRegionRecord& Region : Regions)
	{
		if (!Region.HitComponent)
		{
			continue;
		}
		const bool bHitEnabled = Region.View.bSelectable && (Region.bCountry
			? (bLayerActive && (bGlobal || bRegion))
			: (bLayerActive && bShowProvinces && !bGlobal));
		Region.HitComponent->SetCollisionEnabled(bHitEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Region.HitComponent->SetVisibility(true, true);
		Region.HitComponent->SetHiddenInGame(false, true);
		Region.HitComponent->SetRenderInMainPass(false);
	}
}

void UWLCampaignTerritoryLayerComponent::SetHoveredTerritory(const FString& RegionId)
{
	if (HoveredRegionId == RegionId)
	{
		return;
	}
	HoveredRegionId = RegionId;
	RebuildHighlightMesh(HoverHighlightMesh, HoveredRegionId, FLinearColor(0.86f, 0.62f, 0.24f, 1.f), 1250.f, 620.f);
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::SetSelectedTerritory(const FString& RegionId)
{
	if (SelectedRegionId == RegionId)
	{
		return;
	}
	SelectedRegionId = RegionId;
	RebuildHighlightMesh(SelectedHighlightMesh, SelectedRegionId, FLinearColor(0.96f, 0.78f, 0.34f, 1.f), 1750.f, 920.f);
	ApplyVisibility(LastCameraHeight);
}
