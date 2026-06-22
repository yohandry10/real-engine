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

UWLCampaignTerritoryLayerComponent::UWLCampaignTerritoryLayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWLCampaignTerritoryLayerComponent::BuildLayer(
	USceneComponent* Parent,
	UMaterialInterface* InVertexColorMaterial,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	ClearLayer();
	VertexColorMaterial = InVertexColorMaterial;
	if (!Parent)
	{
		return;
	}

	NationalBorderMesh = NewProceduralMesh(Parent, TEXT("TerritoryNationalBorders"), false);
	ProvinceBorderMesh = NewProceduralMesh(Parent, TEXT("TerritoryProvinceBorders"), false);
	ResourceMarkerMesh = NewProceduralMesh(Parent, TEXT("TerritoryResourceMarkers"), false);
	HoverHighlightMesh = NewProceduralMesh(Parent, TEXT("TerritoryHoverHighlight"), false);
	SelectedHighlightMesh = NewProceduralMesh(Parent, TEXT("TerritorySelectedHighlight"), false);

	BuildCountryBordersAndHitProxies(Parent, ProjectLonLat);
	BuildProvinceRegions(Parent, ProjectLonLat);
	RebuildResourceMarkers();
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::ClearLayer()
{
	for (UProceduralMeshComponent* Mesh : OwnedMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	for (UTextRenderComponent* Label : CountryLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	for (UTextRenderComponent* Label : ProvinceLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}

	OwnedMeshes.Reset();
	HitMeshes.Reset();
	CountryLabels.Reset();
	ProvinceLabels.Reset();
	Regions.Reset();
	HitComponentToRegion.Reset();
	NationalBorderMesh = nullptr;
	ProvinceBorderMesh = nullptr;
	ResourceMarkerMesh = nullptr;
	HoverHighlightMesh = nullptr;
	SelectedHighlightMesh = nullptr;
	HoveredRegionId.Reset();
	SelectedRegionId.Reset();
}

UProceduralMeshComponent* UWLCampaignTerritoryLayerComponent::NewProceduralMesh(
	USceneComponent* Parent,
	const FName& Name,
	bool bCollision)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Parent)
	{
		return nullptr;
	}

	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner, Name);
	if (!Mesh)
	{
		return nullptr;
	}

	Mesh->SetupAttachment(Parent);
	Mesh->RegisterComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, bCollision ? ECR_Block : ECR_Ignore);
	if (bCollision)
	{
		Mesh->SetRenderInMainPass(false);
	}
	if (VertexColorMaterial)
	{
		Mesh->SetMaterial(0, VertexColorMaterial);
	}
	OwnedMeshes.Add(Mesh);
	return Mesh;
}
