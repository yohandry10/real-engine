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

	// Hash determinista 0..1 (file-local; el GridHash01 de Visual.cpp es de otra unidad de traduccion).
	float NatureHash01(int32 X, int32 Y, int32 Salt)
	{
		const float Seed = static_cast<float>((X + 101) * 12.9898 + (Y + 211) * 78.233 + Salt * 37.719);
		return FMath::Frac(FMath::Abs(FMath::Sin(Seed) * 43758.5453f));
	}

	// Que asset de naturaleza corresponde a un bioma (con variedad por hash). Andes = coniferas con
	// pico/roca/loma ocasional; costa = palmeras; jungla/llanos = hoja ancha.
	EWLNatureKind PickNatureKind(EWLVisualBiome Biome, float Hash)
	{
		switch (Biome)
		{
		case EWLVisualBiome::Mountain:
			if (Hash < 0.12f) { return EWLNatureKind::Peak; }   // cumbre nevada (acento)
			if (Hash < 0.24f) { return EWLNatureKind::Rock; }   // peñasco
			if (Hash < 0.30f) { return EWLNatureKind::Mount; }  // loma verde (acento)
			return EWLNatureKind::Conifer;                      // bosque andino (dominante)
		case EWLVisualBiome::Coast:
			return (Hash < 0.72f) ? EWLNatureKind::Palm : EWLNatureKind::Broadleaf;
		case EWLVisualBiome::Jungle:
			return (Hash < 0.82f) ? EWLNatureKind::Broadleaf : EWLNatureKind::Palm;
		default: // Llanos / UrbanInfluence / Context
			return EWLNatureKind::Broadleaf;
		}
	}

	// Altura objetivo (unidades de mundo) por tipo -> escala = objetivo / altura del mesh (conserva
	// proporciones). Grandes para LEER sobre el mapa continental (los conos viejos ~800u eran puntitos).
	float NatureTargetHeight(EWLNatureKind Kind)
	{
		switch (Kind)
		{
		case EWLNatureKind::Conifer:   return 1500.f;
		case EWLNatureKind::Broadleaf: return 1450.f;
		case EWLNatureKind::Palm:      return 1700.f;
		case EWLNatureKind::Rock:      return 950.f;
		case EWLNatureKind::Peak:      return 2700.f; // crag andino con nieve, lee como cumbre
		case EWLNatureKind::Mount:     return 2400.f; // loma verde redondeada (acento)
		default:                       return 1400.f;
		}
	}

	// Probabilidad de que una celda (rejilla ~0.06°) reciba un asset. Bosque denso en montaña/jungla,
	// rala en llanos. Con celda 0.06° (~540u) y canopy ~700-900u, esto da masas de bosque que se leen.
	float NatureKeepFraction(EWLVisualBiome Biome)
	{
		switch (Biome)
		{
		case EWLVisualBiome::Jungle:   return 0.60f;
		case EWLVisualBiome::Mountain: return 0.58f;
		case EWLVisualBiome::Coast:    return 0.34f;
		case EWLVisualBiome::Llanos:   return 0.17f;
		default:                       return 0.26f;
		}
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

// Esparce vegetacion/relieve por BIOMA cubriendo TODOS los paises core (CO/VE), igual que el terreno:
// recorre cada anillo de pais core con una rejilla densa (~0.06°) y en cada celda con tierra muestrea
// ClassifyVisualBiome -> elige el asset (conifera en Andes, palmera en costa, hoja ancha en
// jungla/llanos, pico/roca en la cresta) y ancla la base con el MISMO VisualBiomeZOffset que el
// terreno. Esto cubre la cordillera entera (sin huecos de rectangulos) y solo pisa tierra core.
void AWLCampaign3DView::AddVegetationScatter()
{
	if (SettlementLandGeometry.Num() == 0)
	{
		return;
	}

	const float CellDeg = 0.06f;   // ~540 u entre celdas -> bosque que se lee a zoom Teatro/Cercano.
	const float Jitter = 0.45f;    // rompe la rejilla (no quedan en lineas perfectas).
	int32 Placed = 0;

	for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
	{
		if (!Country.bCoreCountry) // solo CO/VE: el resto es "context" (terreno plano, sin biomas reales).
		{
			continue;
		}

		for (const TArray<FVector2D>& Ring : Country.Rings)
		{
			if (Ring.Num() < 3)
			{
				continue;
			}

			float MinLon, MaxLon, MinLat, MaxLat;
			FWLCampaignRegionGeometry::GetLonLatBounds(Ring, MinLon, MaxLon, MinLat, MaxLat);

			for (float GridLon = MinLon; GridLon < MaxLon; GridLon += CellDeg)
			{
				for (float GridLat = MinLat; GridLat < MaxLat; GridLat += CellDeg)
				{
					const int32 HX = FMath::RoundToInt(GridLon * 1000.f);
					const int32 HY = FMath::RoundToInt(GridLat * 1000.f);
					const float Lon = GridLon + CellDeg * (0.5f + (NatureHash01(HX, HY, 1) - 0.5f) * Jitter);
					const float Lat = GridLat + CellDeg * (0.5f + (NatureHash01(HX, HY, 2) - 0.5f) * Jitter);

					// Mascara de tierra exacta: solo dentro del anillo del pais (nunca sobre el mar).
					if (!FWLCampaignRegionGeometry::PointInLonLatRing(FVector2D(Lon, Lat), Ring))
					{
						continue;
					}

					const EWLVisualBiome Biome = FWLCampaignVisualStyle::ClassifyVisualBiome(Lon, Lat, true);

					// Densidad por bioma.
					if (NatureHash01(HX, HY, 17) > NatureKeepFraction(Biome))
					{
						continue;
					}

					// No plantar dentro de la huella de una ciudad (su modelo ya la ocupa). Radio mayor
					// para capitales (huella mas ancha). Alrededor SI hay bosque -> ciudad en el bosque.
					bool bNearCity = false;
					for (const FWLCampaign3DCityView& City : CityViews)
					{
						const float R = City.bCapital ? 0.42f : 0.22f;
						if (FMath::Abs(City.Lon - Lon) < R && FMath::Abs(City.Lat - Lat) < R)
						{
							bNearCity = true;
							break;
						}
					}
					if (bNearCity)
					{
						continue;
					}

					const float KindHash = NatureHash01(HX, HY, 3);
					const int32 KindIndex = static_cast<int32>(PickNatureKind(Biome, KindHash));
					if (!NatureInstances.IsValidIndex(KindIndex) || !NatureInstances[KindIndex] || !NatureInstances[KindIndex]->GetStaticMesh())
					{
						continue;
					}

					// Ancla la BASE (z=0) a la superficie visible = ProjectLonLat.Z + offset del bioma.
					const float ZOffset = FWLCampaignVisualStyle::VisualBiomeZOffset(Biome, true);
					const FVector Location = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, ZOffset);

					// Escala uniforme = altura objetivo / altura del mesh (conserva proporciones) + jitter.
					const float MeshHeight = FMath::Max(1.f, static_cast<float>(NatureInstances[KindIndex]->GetStaticMesh()->GetBoundingBox().GetSize().Z));
					const float UniformScale = (NatureTargetHeight(static_cast<EWLNatureKind>(KindIndex)) / MeshHeight) * (0.80f + 0.45f * NatureHash01(HX, HY, 11));
					const float Yaw = 360.f * NatureHash01(HX, HY, 23);

					AddInstance(NatureInstances[KindIndex], Location, FRotator(0.f, Yaw, 0.f), FVector(UniformScale));
					++Placed;
				}
			}
		}
	}

	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D vegetation scatter: %d instances over core countries."), Placed);
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
