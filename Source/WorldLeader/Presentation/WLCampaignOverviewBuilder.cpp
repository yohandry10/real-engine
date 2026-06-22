// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignOverviewBuilder.h"

#include "Presentation/WLCampaignAmericaLowDetailData.h"
#include "ProceduralMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WorldLeader.h"

namespace
{
#include "Presentation/WLCampaignOverviewBuilderGeometry.inl"
#include "Presentation/WLCampaignOverviewBuilderContent.inl"
}

void FWLCampaignOverviewBuilder::Build(
	UProceduralMeshComponent* OverviewMesh,
	const FWLCampaignOverviewBuildParams& Params,
	TArray<FWLCampaignOverviewLabelSpec>& OutLabels,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	if (!OverviewMesh)
	{
		return;
	}

	OverviewMesh->ClearAllMeshSections();
	OutLabels.Reset();

	FWLCampaignAmericaLowDetailData AmericaData;
	if (!FWLCampaignAmericaLowDetailDataLoader::Load(AmericaData))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CampaignOverview: no se pudo cargar AmericaLowDetail.json."));
		return;
	}

	FMeshBuffers LandBuffer;
	FMeshBuffers GlobalMarkerBuffer;
	FMeshBuffers RegionalMarkerBuffer;
	TSet<FString> RenderedIsos;
	AddCountriesFromGeoJson(Params, AmericaData.Countries, LandBuffer, RenderedIsos, ProjectLonLat);
	AddMissingCountryPlaceholders(AmericaData.Countries, RenderedIsos, Params, LandBuffer, ProjectLonLat);
	for (const FWLCampaignAmericaCitySpec& City : AmericaData.Cities)
	{
		AddCityMarker(GlobalMarkerBuffer, RegionalMarkerBuffer, City, Params, ProjectLonLat);
	}
	AddCountryLabels(AmericaData.Countries, OutLabels);
	AddCityLabels(AmericaData.Cities, OutLabels);

	if (LandBuffer.Verts.IsEmpty() || LandBuffer.Tris.IsEmpty())
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CampaignOverview: sin vertices/tris para overview America. Countries=%d Cities=%d Rendered=%d."),
			AmericaData.Countries.Num(), AmericaData.Cities.Num(), RenderedIsos.Num());
		return;
	}

	CreateMeshSectionIfNotEmpty(OverviewMesh, 0, LandBuffer, Params.Material);
	CreateMeshSectionIfNotEmpty(OverviewMesh, 1, GlobalMarkerBuffer, Params.Material);
	CreateMeshSectionIfNotEmpty(OverviewMesh, 2, RegionalMarkerBuffer, Params.Material);
	UE_LOG(LogWorldLeader, Log, TEXT("CampaignOverview: America low-detail listo. Countries=%d Rendered=%d Cities=%d LandVerts=%d GlobalMarkers=%d RegionalMarkers=%d."),
		AmericaData.Countries.Num(),
		RenderedIsos.Num(),
		AmericaData.Cities.Num(),
		LandBuffer.Verts.Num(),
		GlobalMarkerBuffer.Verts.Num(),
		RegionalMarkerBuffer.Verts.Num());
}
