// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignTerrainBuilder.h"

#include "Presentation/WLCampaignRegionGeometry.h"
#include "Presentation/WLCampaignVisualStyle.h"
#include "ProceduralMeshComponent.h"

namespace
{
	struct FTerrainSectionBuffer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
	};

	void BuildTerrainNormals(const TArray<FVector>& Verts, const TArray<int32>& Tris, TArray<FVector>& OutNormals)
	{
		OutNormals.Reset();
		OutNormals.SetNumZeroed(Verts.Num());
		for (int32 Index = 0; Index + 2 < Tris.Num(); Index += 3)
		{
			const int32 A = Tris[Index];
			const int32 B = Tris[Index + 1];
			const int32 C = Tris[Index + 2];
			if (!Verts.IsValidIndex(A) || !Verts.IsValidIndex(B) || !Verts.IsValidIndex(C))
			{
				continue;
			}
			FVector Normal = FVector::CrossProduct(Verts[B] - Verts[A], Verts[C] - Verts[A]).GetSafeNormal();
			if (Normal.Z < 0.f)
			{
				Normal *= -1.f;
			}
			OutNormals[A] += Normal;
			OutNormals[B] += Normal;
			OutNormals[C] += Normal;
		}
		for (FVector& Normal : OutNormals)
		{
			Normal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		}
	}

	void AddCountryTerrain(
		UProceduralMeshComponent* TerrainMesh,
		const TArray<TArray<FVector2D>>& Rings,
		const FLinearColor& Color,
		bool bCoreCountry,
		const FWLCampaignTerrainBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat,
		FBox2D& Bounds)
	{
		if (!TerrainMesh || Rings.IsEmpty())
		{
			return;
		}

		TStaticArray<FTerrainSectionBuffer, static_cast<int32>(EWLVisualBiome::Count)> Buffers;
		// Paises "teatro" enormes (Brasil, a futuro EEUU/Argentina) usan celdas mas
		// gruesas para no disparar el conteo de vertices; los chicos conservan el detalle.
		float CoreCell = 0.040f;
		{
			float SpanMinLon = 1.0e9f, SpanMaxLon = -1.0e9f, SpanMinLat = 1.0e9f, SpanMaxLat = -1.0e9f;
			for (const TArray<FVector2D>& Ring : Rings)
			{
				for (const FVector2D& P : Ring)
				{
					SpanMinLon = FMath::Min(SpanMinLon, P.X);
					SpanMaxLon = FMath::Max(SpanMaxLon, P.X);
					SpanMinLat = FMath::Min(SpanMinLat, P.Y);
					SpanMaxLat = FMath::Max(SpanMaxLat, P.Y);
				}
			}
			const float Span = FMath::Max(SpanMaxLon - SpanMinLon, SpanMaxLat - SpanMinLat);
			if (Span > 25.f)
			{
				CoreCell = 0.10f;
			}
		}
		const float CellDegrees = bCoreCountry ? CoreCell : 0.16f;

		for (const TArray<FVector2D>& Ring : Rings)
		{
			if (Ring.Num() < 3)
			{
				continue;
			}

			float RingMinLon, RingMaxLon, RingMinLat, RingMaxLat;
			FWLCampaignRegionGeometry::GetLonLatBounds(Ring, RingMinLon, RingMaxLon, RingMinLat, RingMaxLat);
			const float MinLon = FMath::Max(RingMinLon, Params.RegionMinLon - 2.2f);
			const float MaxLon = FMath::Min(RingMaxLon, Params.RegionMaxLon + 2.2f);
			const float MinLat = FMath::Max(RingMinLat, Params.RegionMinLat - 2.2f);
			const float MaxLat = FMath::Min(RingMaxLat, Params.RegionMaxLat + 2.2f);

			for (float Lon = MinLon; Lon < MaxLon; Lon += CellDegrees)
			{
				for (float Lat = MinLat; Lat < MaxLat; Lat += CellDegrees)
				{
					const FVector2D Center(Lon + CellDegrees * 0.5f, Lat + CellDegrees * 0.5f);
					if (!FWLCampaignRegionGeometry::PointInLonLatRing(Center, Ring))
					{
						continue;
					}

					const EWLVisualBiome Biome = FWLCampaignVisualStyle::ClassifyVisualBiome(Center.X, Center.Y, bCoreCountry);
					FTerrainSectionBuffer& Buffer = Buffers[static_cast<int32>(Biome)];
					const float ZOffset = FWLCampaignVisualStyle::VisualBiomeZOffset(Biome, bCoreCountry);
					const int32 Base = Buffer.Verts.Num();
					FVector A = ProjectLonLat(Lon, Lat);
					FVector B = ProjectLonLat(Lon + CellDegrees, Lat);
					FVector C = ProjectLonLat(Lon + CellDegrees, Lat + CellDegrees);
					FVector D = ProjectLonLat(Lon, Lat + CellDegrees);
					A.Z += ZOffset;
					B.Z += ZOffset;
					C.Z += ZOffset;
					D.Z += ZOffset;

					Buffer.Verts.Add(A);
					Buffer.Verts.Add(B);
					Buffer.Verts.Add(C);
					Buffer.Verts.Add(D);
					Buffer.UVs.Add(FVector2D(0.f, 0.f));
					Buffer.UVs.Add(FVector2D(1.f, 0.f));
					Buffer.UVs.Add(FVector2D(1.f, 1.f));
					Buffer.UVs.Add(FVector2D(0.f, 1.f));
					// Para paises "teatro" pintamos por bioma (geografia); el contexto
					// conserva su tono plano de pais.
					const FLinearColor CellBaseColor = bCoreCountry
						? FWLCampaignVisualStyle::VisualBiomeColor(Biome)
						: Color;
					Buffer.Colors.Add(FWLCampaignVisualStyle::ToVertexFColor(FWLCampaignVisualStyle::ShadeTerrainVertex(CellBaseColor, Lon, Lat, A.Z)));
					Buffer.Colors.Add(FWLCampaignVisualStyle::ToVertexFColor(FWLCampaignVisualStyle::ShadeTerrainVertex(CellBaseColor, Lon + CellDegrees, Lat, B.Z)));
					Buffer.Colors.Add(FWLCampaignVisualStyle::ToVertexFColor(FWLCampaignVisualStyle::ShadeTerrainVertex(CellBaseColor, Lon + CellDegrees, Lat + CellDegrees, C.Z)));
					Buffer.Colors.Add(FWLCampaignVisualStyle::ToVertexFColor(FWLCampaignVisualStyle::ShadeTerrainVertex(CellBaseColor, Lon, Lat + CellDegrees, D.Z)));
					Buffer.Tris.Add(Base);
					Buffer.Tris.Add(Base + 2);
					Buffer.Tris.Add(Base + 1);
					Buffer.Tris.Add(Base);
					Buffer.Tris.Add(Base + 3);
					Buffer.Tris.Add(Base + 2);
					Bounds += FVector2D(A.X, A.Y);
					Bounds += FVector2D(C.X, C.Y);
				}
			}
		}

		for (int32 Section = 0; Section < Buffers.Num(); ++Section)
		{
			const FTerrainSectionBuffer& Buffer = Buffers[Section];
			if (Buffer.Verts.IsEmpty() || Buffer.Tris.IsEmpty())
			{
				continue;
			}

			TArray<FVector> Normals;
			BuildTerrainNormals(Buffer.Verts, Buffer.Tris, Normals);
			TArray<FProcMeshTangent> Tangents;
			const int32 SectionIndex = TerrainMesh->GetNumSections();
			TerrainMesh->CreateMeshSection(SectionIndex, Buffer.Verts, Buffer.Tris, Normals, Buffer.UVs, Buffer.Colors, Tangents, true);
			if (Params.TerrainMaterial)
			{
				TerrainMesh->SetMaterial(SectionIndex, Params.TerrainMaterial);
			}
		}
	}

	void AddBoundaryRibbon(
		UProceduralMeshComponent* BoundaryMesh,
		const TArray<FVector2D>& LonLatPoints,
		const FLinearColor& Color,
		float WidthWorld,
		float ZOffset,
		const FWLCampaignTerrainBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		if (!BoundaryMesh || LonLatPoints.Num() < 2)
		{
			return;
		}

		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		Verts.Reserve(LonLatPoints.Num() * 4);
		Tris.Reserve(LonLatPoints.Num() * 6);

		const float MinLon = Params.RegionMinLon - 2.5f;
		const float MaxLon = Params.RegionMaxLon + 2.5f;
		const float MinLat = Params.RegionMinLat - 2.5f;
		const float MaxLat = Params.RegionMaxLat + 2.5f;

		for (int32 Index = 0; Index < LonLatPoints.Num(); ++Index)
		{
			const FVector2D& A = LonLatPoints[Index];
			const FVector2D& B = LonLatPoints[(Index + 1) % LonLatPoints.Num()];
			const float SegmentMinLon = FMath::Min(A.X, B.X);
			const float SegmentMaxLon = FMath::Max(A.X, B.X);
			const float SegmentMinLat = FMath::Min(A.Y, B.Y);
			const float SegmentMaxLat = FMath::Max(A.Y, B.Y);
			if (!FWLCampaignRegionGeometry::LonLatBoundsIntersect(SegmentMinLon, SegmentMaxLon, SegmentMinLat, SegmentMaxLat, MinLon, MaxLon, MinLat, MaxLat))
			{
				continue;
			}

			const FVector StartCenter = ProjectLonLat(A.X, A.Y) + FVector(0.f, 0.f, ZOffset);
			const FVector EndCenter = ProjectLonLat(B.X, B.Y) + FVector(0.f, 0.f, ZOffset);
			const FVector Direction = EndCenter - StartCenter;
			const FVector FlatDirection(Direction.X, Direction.Y, 0.f);
			if (FlatDirection.SizeSquared() < 1.f)
			{
				continue;
			}

			const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal()) * (WidthWorld * 0.5f);
			const int32 Base = Verts.Num();
			Verts.Add(StartCenter - Side);
			Verts.Add(StartCenter + Side);
			Verts.Add(EndCenter + Side);
			Verts.Add(EndCenter - Side);
			UVs.Add(FVector2D(0.f, 0.f));
			UVs.Add(FVector2D(0.f, 1.f));
			UVs.Add(FVector2D(1.f, 1.f));
			UVs.Add(FVector2D(1.f, 0.f));
			Tris.Add(Base);
			Tris.Add(Base + 1);
			Tris.Add(Base + 2);
			Tris.Add(Base);
			Tris.Add(Base + 2);
			Tris.Add(Base + 3);
		}

		if (Verts.IsEmpty())
		{
			return;
		}

		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Verts.Num());
		TArray<FColor> Colors;
		Colors.Init(FWLCampaignVisualStyle::ToVertexFColor(Color), Verts.Num());
		TArray<FProcMeshTangent> Tangents;
		const int32 SectionIndex = BoundaryMesh->GetNumSections();
		BoundaryMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors, Tangents, false);
		if (Params.BoundaryMaterial)
		{
			BoundaryMesh->SetMaterial(SectionIndex, Params.BoundaryMaterial);
		}
	}
}

bool FWLCampaignTerrainBuilder::Build(
	UProceduralMeshComponent* TerrainMesh,
	UProceduralMeshComponent* BoundaryMesh,
	const FWLCampaignTerrainBuildParams& Params,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat,
	FBox2D& Bounds)
{
	if (!TerrainMesh)
	{
		return false;
	}

	TArray<FWLRegionalCountryGeometry> Countries;
	if (!FWLCampaignRegionGeometry::LoadCountries(
		Params.RegionMinLon - 3.f,
		Params.RegionMaxLon + 3.f,
		Params.RegionMinLat - 3.f,
		Params.RegionMaxLat + 3.f,
		Countries))
	{
		return false;
	}

	for (const FWLRegionalCountryGeometry& Country : Countries)
	{
		AddCountryTerrain(TerrainMesh, Country.Rings, Country.Color, Country.bCoreCountry, Params, ProjectLonLat, Bounds);
		for (const TArray<FVector2D>& Ring : Country.Rings)
		{
			const FLinearColor Outline = Country.bCoreCountry
				? FLinearColor(0.78f, 0.77f, 0.64f, 0.88f)
				: FLinearColor(0.16f, 0.28f, 0.24f, 0.70f);
			AddBoundaryRibbon(
				BoundaryMesh,
				Ring,
				Outline,
				Country.bCoreCountry ? 260.f : 180.f,
				Country.bCoreCountry ? 2300.f : 920.f,
				Params,
				ProjectLonLat);
		}
	}

	return true;
}
