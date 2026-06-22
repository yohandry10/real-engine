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
	bool CampaignPointInTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
	{
		const float Area = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
		const float S = ((A.Y - C.Y) * (P.X - C.X) + (C.X - A.X) * (P.Y - C.Y)) / Area;
		const float T = ((C.Y - B.Y) * (P.X - C.X) + (B.X - C.X) * (P.Y - C.Y)) / Area;
		const float U = 1.f - S - T;
		return S >= 0.f && T >= 0.f && U >= 0.f;
	}

	float SignedArea(const TArray<FVector2D>& Poly)
	{
		float Area = 0.f;
		for (int32 Index = 0; Index < Poly.Num(); ++Index)
		{
			const FVector2D& A = Poly[Index];
			const FVector2D& B = Poly[(Index + 1) % Poly.Num()];
			Area += (A.X * B.Y) - (B.X * A.Y);
		}
		return Area * 0.5f;
	}

	void TriangulateSimplePolygon(const TArray<FVector2D>& Contour, TArray<int32>& OutTris)
	{
		OutTris.Reset();
		const int32 Num = Contour.Num();
		if (Num < 3)
		{
			return;
		}

		TArray<int32> V;
		V.Reserve(Num);
		const bool bClockwise = SignedArea(Contour) < 0.f;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			V.Add(bClockwise ? (Num - 1 - Index) : Index);
		}

		int32 Guard = 0;
		while (V.Num() > 2 && Guard++ < Num * Num)
		{
			bool bClipped = false;
			for (int32 Index = 0; Index < V.Num(); ++Index)
			{
				const int32 Prev = V[(Index + V.Num() - 1) % V.Num()];
				const int32 Curr = V[Index];
				const int32 Next = V[(Index + 1) % V.Num()];
				const FVector2D& A = Contour[Prev];
				const FVector2D& B = Contour[Curr];
				const FVector2D& C = Contour[Next];

				const float Cross = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
				if (Cross <= 0.f)
				{
					continue;
				}

				bool bEar = true;
				for (int32 TestIndex : V)
				{
					if (TestIndex == Prev || TestIndex == Curr || TestIndex == Next)
					{
						continue;
					}
					if (CampaignPointInTriangle(A, B, C, Contour[TestIndex]))
					{
						bEar = false;
						break;
					}
				}

				if (bEar)
				{
					OutTris.Add(Prev);
					OutTris.Add(Curr);
					OutTris.Add(Next);
					V.RemoveAt(Index);
					bClipped = true;
					break;
				}
			}

			if (!bClipped)
			{
				break;
			}
		}

}
}

UWLDataRegistry* AWLCampaign3DView::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FVector AWLCampaign3DView::ProjectLonLat(float Lon, float Lat) const
{
	const float X = (Lat - TheaterCenterLonLat.Y) * GeoScale;
	const float Y = (Lon - TheaterCenterLonLat.X) * GeoScale;
	return FVector(X, Y, SampleTerrainHeight(Lon, Lat));
}

float AWLCampaign3DView::SampleTerrainHeight(float Lon, float Lat) const
{
	// Relieve continental (exagerado para que se lea en 3D al inclinar la camara).
	// Devuelve unidades de mundo: el terreno y todo lo que va encima (ciudades,
	// caminos, labels) usan esto via ProjectLonLat, asi que todo sube coherente.

	// Cresta de los Andes (oeste de Sudamerica) por latitud (mismos puntos de control
	// que el clasificador de biomas, para que montana-color y montana-relieve coincidan).
	auto AndesCrestLon = [](float L) -> float
	{
		const float Lats[] = { 11.0f, 9.0f, 7.0f, 5.0f, 2.5f, 0.0f, -2.5f, -5.5f, -8.0f, -11.0f, -14.0f, -17.0f, -20.0f, -23.0f, -27.0f, -33.0f, -40.0f, -47.0f, -53.0f, -56.0f };
		const float Lons[] = { -72.0f, -71.0f, -73.0f, -74.6f, -76.3f, -78.4f, -79.0f, -79.4f, -77.8f, -76.3f, -72.8f, -69.0f, -67.5f, -66.5f, -69.0f, -70.0f, -71.5f, -72.8f, -72.5f, -69.0f };
		const int32 N = 20;
		if (L >= Lats[0]) return Lons[0];
		if (L <= Lats[N - 1]) return Lons[N - 1];
		for (int32 i = 0; i < N - 1; ++i)
		{
			if (L <= Lats[i] && L >= Lats[i + 1])
			{
				const float T = (L - Lats[i + 1]) / (Lats[i] - Lats[i + 1]);
				return FMath::Lerp(Lons[i + 1], Lons[i], T);
			}
		}
		return Lons[N - 1];
	};

	float H = 0.f; // 0..1

	// Andes
	if (Lat < 13.f && Lat > -56.f)
	{
		const float DAndes = Lon - AndesCrestLon(Lat);
		const float Band = (Lat < -6.f && Lat > -28.f) ? 2.6f : 1.5f; // altiplano mas ancho
		H = FMath::Max(H, FMath::Exp(-FMath::Square(DAndes / Band)));
	}
	// Rocosas (oeste de EEUU/Canada)
	{
		const float BandX = FMath::Exp(-FMath::Square((Lon + 112.f) / 6.f));
		const float LatGate = FMath::Clamp((Lat - 31.f) / 12.f, 0.f, 1.f) * FMath::Clamp((64.f - Lat) / 12.f, 0.f, 1.f);
		H = FMath::Max(H, 0.82f * BandX * LatGate);
	}
	// Sierra Madre (Mexico)
	{
		const float BandX = FMath::Exp(-FMath::Square((Lon + 104.f) / 3.6f));
		const float LatGate = FMath::Exp(-FMath::Square((Lat - 23.f) / 7.f));
		H = FMath::Max(H, 0.58f * BandX * LatGate);
	}
	// Apalaches (este de EEUU), suaves
	{
		const float BandX = FMath::Exp(-FMath::Square((Lon + 80.f) / 3.f));
		const float LatGate = FMath::Exp(-FMath::Square((Lat - 38.f) / 7.f));
		H = FMath::Max(H, 0.30f * BandX * LatGate);
	}
	// Tierras altas de Brasil + escudo guayanes, suaves
	{
		const float Brazil = FMath::Exp(-FMath::Square((Lon + 47.f) / 7.f)) * FMath::Exp(-FMath::Square((Lat + 16.f) / 8.f));
		const float Guiana = FMath::Exp(-FMath::Square((Lon + 63.6f) / 3.2f)) * FMath::Exp(-FMath::Square((Lat - 6.6f) / 2.6f));
		H = FMath::Max(H, FMath::Max(0.26f * Brazil, 0.30f * Guiana));
	}

	// Cuenca de Maracaibo: el modelo de Andes (cresta unica) mete una montana donde en
	// realidad hay una DEPRESION baja (lago de Maracaibo + llanura del Zulia), entre la
	// Sierra de Perija (O) y la cordillera de Merida (SE). Hundimos el relieve aqui para
	// que el lago, la ciudad y la carretera queden en tierra baja (no sobre una cima).
	{
		// Cuenco de FONDO PLANO (no gaussiana): asi el lago, el estrecho y la ciudad de
		// Maracaibo quedan TODOS en tierra baja nivelada, y el relieve sube de nuevo a los
		// Andes fuera del radio. Centro ~(-71.5, 9.9); plano hasta R=0.75, sube hasta 1.45.
		const float BasinX = (Lon + 71.5f) / 1.05f;
		const float BasinY = (Lat - 9.9f) / 1.30f;
		const float BasinR = FMath::Sqrt(BasinX * BasinX + BasinY * BasinY);
		const float Basin = 1.f - FMath::SmoothStep(0.75f, 1.45f, BasinR);
		H *= (1.f - 0.93f * Basin);
	}

	// Ruido fino para textura del terreno.
	const float Noise = 0.045f * (FMath::Sin(Lon * 2.6f + Lat * 1.4f) + FMath::Cos(Lon * 1.2f - Lat * 2.8f));
	H = FMath::Clamp(H + Noise, 0.f, 1.15f);

	// Exageracion vertical reducida (antes 19000): los Andes salian como PICOS afilados y
	// las ciudades (Bogota, Bucaramanga, Quito...) quedaban "montadas" sobre cimas con la
	// carretera trepando raro. Mas bajo = relieve mas legible y caminos coherentes.
	return H * 12000.f;
}

FLinearColor AWLCampaign3DView::TerrainColor(EWLTerrainType Terrain, const FString& CountryIso) const
{
	if (Terrain == EWLTerrainType::Urban)
	{
		return FLinearColor(0.31f, 0.33f, 0.30f);
	}
	if (Terrain == EWLTerrainType::Coastal)
	{
		return FLinearColor(0.28f, 0.42f, 0.25f);
	}
	if (Terrain == EWLTerrainType::Jungle)
	{
		return FLinearColor(0.12f, 0.35f, 0.19f);
	}
	if (CountryIso.Equals(TEXT("CO"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.20f, 0.36f, 0.22f);
	}
	return FLinearColor(0.17f, 0.31f, 0.20f);
}

UMaterialInstanceDynamic* AWLCampaign3DView::MakeColorMaterial(const FLinearColor& Color)
{
	if (!BaseMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Mat->SetVectorParameterValue(TEXT("Base Color"), Color);
		Mat->SetVectorParameterValue(TEXT("DiffuseColor"), Color);
		Mat->SetVectorParameterValue(TEXT("Tint"), Color);
		Mat->SetVectorParameterValue(TEXT("Albedo"), Color);
	}
	return Mat;
}

void AWLCampaign3DView::BuildSea()
{
	FWLCampaignWaterBuildParams Params;
	Params.Center = ProjectLonLat(TheaterCenterLonLat.X, TheaterCenterLonLat.Y);
	Params.HalfSize = 3600000.f;
	Params.SeaZ = -2350.f;
	Params.TileCount = 60;
	Params.WaterMaterial = VertexColorMaterial;
	Params.DetailMaterial = VertexColorMaterial;

	FWLCampaignWaterBuilder::Build(SeaMesh, SeaDetailMesh, Params, [this](float Lon, float Lat)
	{
		return ProjectLonLat(Lon, Lat);
	});
}

void AWLCampaign3DView::BuildOverviewLayer()
{
	if (!OverviewMesh)
	{
		return;
	}

	FWLCampaignOverviewBuildParams Params;
	Params.Material = VertexColorMaterial;
	Params.LandZ = 9000.f;
	Params.BorderZ = 11800.f;
	Params.BorderWidth = 900.f;
	Params.CityZ = 14800.f;
	Params.CityMarkerSize = 0.f;

	TArray<FWLCampaignOverviewLabelSpec> OverviewLabelSpecs;
	FWLCampaignOverviewBuilder::Build(OverviewMesh, Params, OverviewLabelSpecs, [this](float Lon, float Lat)
	{
		return ProjectLonLat(Lon, Lat);
	});
	OverviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const TArray<FVector2D> OverviewCorners = {
		FVector2D(-172.f, 84.f),
		FVector2D(-10.f, 84.f),
		FVector2D(-10.f, -56.f),
		FVector2D(-172.f, -56.f)
	};
	for (const FVector2D& Corner : OverviewCorners)
	{
		const FVector P = ProjectLonLat(Corner.X, Corner.Y);
		OverviewBounds += FVector2D(P.X, P.Y);
	}

	for (const FWLCampaignOverviewLabelSpec& Label : OverviewLabelSpecs)
	{
		AddOverviewLabel(
			Label.Text,
			Label.Lon,
			Label.Lat,
			Label.ZOffset,
			Label.WorldSize,
			Label.Color,
			Label.bShowInGlobal,
			Label.bShowInRegion);
	}
}

void AWLCampaign3DView::BuildTerrain()
{
	FWLCampaignTerrainBuildParams Params;
	Params.RegionMinLon = RegionMinLon;
	Params.RegionMaxLon = RegionMaxLon;
	Params.RegionMinLat = RegionMinLat;
	Params.RegionMaxLat = RegionMaxLat;
	Params.TerrainMaterial = VertexColorMaterial;
	Params.BoundaryMaterial = VertexColorMaterial;
	FString CityVisualTestValue;
	Params.bCreateTerrainCollision = !FParse::Value(FCommandLine::Get(), TEXT("WLCityVisualTest="), CityVisualTestValue);

	FWLCampaignTerrainBuilder::Build(
		TerrainMesh,
		BoundaryMesh,
		Params,
		[this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		},
		Bounds);
}

void AWLCampaign3DView::AddTerrainPatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset)
{
	if (!TerrainMesh || LonLatPoints.Num() < 3)
	{
		return;
	}

	TArray<int32> Tris;
	TriangulateSimplePolygon(LonLatPoints, Tris);
	if (Tris.Num() < 3)
	{
		return;
	}

	TArray<FVector> Verts;
	TArray<FVector2D> UVs;
	Verts.Reserve(LonLatPoints.Num());
	UVs.Reserve(LonLatPoints.Num());
	for (const FVector2D& LonLat : LonLatPoints)
	{
		FVector P = ProjectLonLat(LonLat.X, LonLat.Y);
		P.Z += ZOffset;
		Verts.Add(P);
		UVs.Add(FVector2D((LonLat.X - RegionMinLon) / (RegionMaxLon - RegionMinLon), (LonLat.Y - RegionMinLat) / (RegionMaxLat - RegionMinLat)));
		Bounds += FVector2D(P.X, P.Y);
	}

	TArray<int32> FinalTris;
	FinalTris.Reserve(Tris.Num() * 2);
	for (int32 Index = 0; Index + 2 < Tris.Num(); Index += 3)
	{
		FinalTris.Add(Tris[Index]);
		FinalTris.Add(Tris[Index + 1]);
		FinalTris.Add(Tris[Index + 2]);
		FinalTris.Add(Tris[Index]);
		FinalTris.Add(Tris[Index + 2]);
		FinalTris.Add(Tris[Index + 1]);
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FColor> Colors;
	Colors.Init(FWLCampaignVisualStyle::ToVertexFColor(Color), Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	const int32 SectionIndex = TerrainMesh->GetNumSections();
	TerrainMesh->CreateMeshSection(SectionIndex, Verts, FinalTris, Normals, UVs, Colors, Tangents, true);
	if (VertexColorMaterial)
	{
		TerrainMesh->SetMaterial(SectionIndex, VertexColorMaterial);
	}
}

void AWLCampaign3DView::AddPolylineSegment(const FVector& Start, const FVector& End, const FLinearColor& Color, float RadiusScale)
{
	if (!BoundaryMesh)
	{
		return;
	}

	const FVector Direction = End - Start;
	const float Length = Direction.Size();
	if (Length < 1.f)
	{
		return;
	}

	const FVector FlatDirection(Direction.X, Direction.Y, 0.f);
	if (FlatDirection.SizeSquared() < 1.f)
	{
		return;
	}

	const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal()) * (RadiusScale * 360.f);
	TArray<FVector> Verts = {
		Start - Side,
		Start + Side,
		End + Side,
		End - Side
	};
	TArray<int32> Tris = { 0, 1, 2, 0, 2, 3 };
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs = { FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(1.f, 0.f) };
	TArray<FColor> Colors;
	Colors.Init(FWLCampaignVisualStyle::ToVertexFColor(Color), Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	const int32 SectionIndex = BoundaryMesh->GetNumSections();
	BoundaryMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (VertexColorMaterial)
	{
		BoundaryMesh->SetMaterial(SectionIndex, VertexColorMaterial);
	}
}

void AWLCampaign3DView::AddCoastline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale)
{
	if (LonLatPoints.Num() < 2)
	{
		return;
	}

	for (int32 Index = 0; Index < LonLatPoints.Num(); ++Index)
	{
		const FVector2D& A = LonLatPoints[Index];
		const FVector2D& B = LonLatPoints[(Index + 1) % LonLatPoints.Num()];
		const FVector Start = ProjectLonLat(A.X, A.Y) + FVector(0.f, 0.f, 320.f);
		const FVector End = ProjectLonLat(B.X, B.Y) + FVector(0.f, 0.f, 320.f);
		AddPolylineSegment(Start, End, Color, RadiusScale);
	}
}
