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

namespace
{
	FString RouteKey(const FString& A, const FString& B)
	{
		return A < B ? FString::Printf(TEXT("%s|%s"), *A, *B) : FString::Printf(TEXT("%s|%s"), *B, *A);
	}

}

void AWLCampaign3DView::RebuildPointSelectionHighlight(const FVector& Location, float Radius, const FLinearColor& Color)
{
	if (!SelectionHighlightMesh)
	{
		return;
	}

	SelectionHighlightMesh->ClearAllMeshSections();
	const int32 Segments = 36;
	const float OuterRadius = FMath::Max(1200.f, Radius);
	const float InnerRadius = OuterRadius * 0.74f;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Verts.Reserve(Segments * 2);
	Normals.Reserve(Segments * 2);
	UVs.Reserve(Segments * 2);
	Colors.Reserve(Segments * 2);

	const FColor VertexColor = Color.ToFColor(false);
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float Angle = (static_cast<float>(Index) / static_cast<float>(Segments)) * 2.f * PI;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		Verts.Add(Location + Direction * OuterRadius);
		Verts.Add(Location + Direction * InnerRadius);
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(1.f, 0.f));
		UVs.Add(FVector2D(0.f, 0.f));
		Colors.Add(VertexColor);
		Colors.Add(VertexColor);
	}
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const int32 Next = (Index + 1) % Segments;
		const int32 A = Index * 2;
		const int32 B = A + 1;
		const int32 C = Next * 2;
		const int32 D = C + 1;
		Tris.Append({ A, C, B, B, C, D });
	}

	SelectionHighlightMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (VertexColorMaterial)
	{
		SelectionHighlightMesh->SetMaterial(0, VertexColorMaterial);
	}
	SelectionHighlightMesh->SetVisibility(!IsHidden(), true);
	SelectionHighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWLCampaign3DView::AddVegetationScatter(float MinLon, float MaxLon, float MinLat, float MaxLat, int32 Columns, int32 Rows, bool bDenseJungle)
{
	for (int32 Col = 0; Col < Columns; ++Col)
	{
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const float U = (static_cast<float>(Col) + 0.35f + 0.18f * FMath::Sin(Row * 1.7f)) / static_cast<float>(Columns);
			const float V = (static_cast<float>(Row) + 0.42f + 0.16f * FMath::Cos(Col * 1.3f)) / static_cast<float>(Rows);
			if (((Col * 11 + Row * 17) % (bDenseJungle ? 7 : 5)) == 0)
			{
				continue;
			}
			const float Lon = FMath::Lerp(MinLon, MaxLon, U);
			const float Lat = FMath::Lerp(MinLat, MaxLat, V);
			const FVector Location = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, bDenseJungle ? 760.f : 560.f);
			const float ScaleBase = (bDenseJungle ? 6.6f : 4.7f) * (0.82f + 0.26f * FMath::Frac(FMath::Sin((Col + 1) * 12.9898f + Row * 78.233f) * 43758.5453f));
			AddInstance(bDenseJungle ? TreeInstances : BrushInstances, Location, FRotator(0.f, 37.f * (Col + Row), 0.f), FVector(ScaleBase, ScaleBase, bDenseJungle ? 9.5f : 4.8f));
		}
	}
}

void AWLCampaign3DView::AddInstance(UInstancedStaticMeshComponent* Component, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (!Component || !Component->GetStaticMesh())
	{
		return;
	}

	Component->AddInstance(FTransform(Rotation, Location, Scale));
}

void AWLCampaign3DView::AddOverviewLabel(
	const FString& Text,
	float Lon,
	float Lat,
	float ZOffset,
	float WorldSize,
	const FColor& Color,
	bool bShowInGlobal,
	bool bShowInRegion)
{
	if (!SceneRoot)
	{
		return;
	}

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	if (!Label)
	{
		return;
	}

	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetWorldLocation(ProjectStrategicLonLat(Lon, Lat) + FVector(0.f, 0.f, ZOffset));
	Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(WorldSize);
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(Color);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverviewLabels.Add(Label);

	uint8 VisibilityMask = 0;
	if (bShowInGlobal)
	{
		VisibilityMask |= 1;
	}
	if (bShowInRegion)
	{
		VisibilityMask |= 2;
	}
	OverviewLabelVisibilityMasks.Add(VisibilityMask);
}

void AWLCampaign3DView::UpdateOverviewLabelVisibility(bool bStrategicVisible)
{
	for (int32 Index = 0; Index < OverviewLabels.Num(); ++Index)
	{
		UTextRenderComponent* Label = OverviewLabels[Index];
		if (!Label)
		{
			continue;
		}

		const uint8 VisibilityMask = OverviewLabelVisibilityMasks.IsValidIndex(Index)
			? OverviewLabelVisibilityMasks[Index]
			: 3;
		const bool bAllowedForZoom =
			(CurrentZoomLOD == EWLCampaign3DZoomLOD::Global && (VisibilityMask & 1) != 0)
			|| (CurrentZoomLOD == EWLCampaign3DZoomLOD::Region && (VisibilityMask & 2) != 0);
		Label->SetVisibility(bStrategicVisible && bAllowedForZoom, true);
	}
}

void AWLCampaign3DView::ConfigureInstancedComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, const FLinearColor& Color)
{
	if (!Component || !Mesh)
	{
		return;
	}

	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetMobility(EComponentMobility::Movable);
	if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(Color))
	{
		Component->SetMaterial(0, Mat);
	}
}

void AWLCampaign3DView::AddProvinceMarkers()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	TArray<FWLProvinceData> Provinces = Registry->GetAllProvinces();
	Provinces.Sort([](const FWLProvinceData& A, const FWLProvinceData& B)
	{
		return A.Id < B.Id;
	});

	for (const FWLProvinceData& Province : Provinces)
	{
		if (FWLCampaignRegionGeometry::IsRegionalProvince(Province))
		{
			AddProvinceMarker(Province);
		}
	}
}

void AWLCampaign3DView::AddProvinceMarker(const FWLProvinceData& Province)
{
	const FVector Location = ProjectLonLat(Province.MapLon, Province.MapLat) + FVector(0.f, 0.f, Province.bIsCapital ? 1050.f : 780.f);
	USphereComponent* Marker = NewObject<USphereComponent>(this);
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetWorldLocation(Location);
	Marker->SetSphereRadius(Province.bIsCapital ? 6200.f : 4800.f);
	Marker->SetVisibility(false, true);
	Marker->SetHiddenInGame(false, true);
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DProvince"));
	Marker->ComponentTags.Add(FName(*Province.Id));

	FWLCampaign3DProvinceView View;
	View.Id = Province.Id;
	View.Name = Province.Name;
	View.CountryIso = Province.CountryIso;
	View.Region = Province.Region;
	View.WorldLocation = Location;
	ProvinceMarkers.Add(Marker);
	ProvinceViews.Add(View);

	if (Province.bIsCapital)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetWorldLocation(Location + FVector(0.f, 0.f, 1500.f));
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(920.f);
		Label->SetText(FText::FromString(Province.Capital));
		Label->SetTextRenderColor(FColor(235, 210, 120));
		SettlementLabels.Add(Label);
	}
}

void AWLCampaign3DView::AddRoutes()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry || !RouteMesh)
	{
		return;
	}

	TSet<FString> Processed;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		if (!FWLCampaignRegionGeometry::IsRegionalProvince(Province))
		{
			continue;
		}

		for (const FString& NeighborId : Province.Neighbors)
		{
			FWLProvinceData Neighbor;
			if (!Registry->GetProvince(NeighborId, Neighbor) || !FWLCampaignRegionGeometry::IsRegionalProvince(Neighbor))
			{
				continue;
			}
			const FString Key = RouteKey(Province.Id, Neighbor.Id);
			if (Processed.Contains(Key))
			{
				continue;
			}
			Processed.Add(Key);
			AddRouteSegment(Province, Neighbor);
		}
	}
}

void AWLCampaign3DView::AddRouteSegment(const FWLProvinceData& A, const FWLProvinceData& B)
{
	const FVector Start = ProjectLonLat(A.MapLon, A.MapLat) + FVector(0.f, 0.f, 520.f);
	const FVector End = ProjectLonLat(B.MapLon, B.MapLat) + FVector(0.f, 0.f, 520.f);
	AddPolylineSegment(Start, End, FLinearColor(0.58f, 0.46f, 0.20f, 0.92f), 0.55f);
}
