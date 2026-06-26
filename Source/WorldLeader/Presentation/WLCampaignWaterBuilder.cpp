// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignWaterBuilder.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
	struct FWaterMeshBuffer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
	};

	FColor WaterVertexColor(const FLinearColor& Color)
	{
		return Color.ToFColor(false);
	}

	float WaterRippleHeight(float X, float Y)
	{
		(void)X;
		(void)Y;
		return 0.f;
	}

	FLinearColor WaterDepthColor(float U, float V)
	{
		(void)U;
		(void)V;
		return FLinearColor(0.018f, 0.250f, 0.305f);
	}

	void AddStrip(FWaterMeshBuffer& Buffer, const FVector& Start, const FVector& End, float Width, const FLinearColor& Color)
	{
		const FVector Direction = End - Start;
		const FVector Flat(Direction.X, Direction.Y, 0.f);
		if (Flat.SizeSquared() < 1.f)
		{
			return;
		}

		const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat.GetSafeNormal()) * (Width * 0.5f);
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Add(Start - Side);
		Buffer.Verts.Add(Start + Side);
		Buffer.Verts.Add(End + Side);
		Buffer.Verts.Add(End - Side);
		Buffer.UVs.Add(FVector2D(0.f, 0.f));
		Buffer.UVs.Add(FVector2D(0.f, 1.f));
		Buffer.UVs.Add(FVector2D(1.f, 1.f));
		Buffer.UVs.Add(FVector2D(1.f, 0.f));
		Buffer.Colors.Add(WaterVertexColor(Color));
		Buffer.Colors.Add(WaterVertexColor(Color));
		Buffer.Colors.Add(WaterVertexColor(Color));
		Buffer.Colors.Add(WaterVertexColor(Color));
		Buffer.Tris.Add(Base);
		Buffer.Tris.Add(Base + 1);
		Buffer.Tris.Add(Base + 2);
		Buffer.Tris.Add(Base);
		Buffer.Tris.Add(Base + 2);
		Buffer.Tris.Add(Base + 3);
	}

	void AddWorldPolyline(FWaterMeshBuffer& Buffer, const TArray<FVector>& Points, float Width, const FLinearColor& Color)
	{
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			AddStrip(Buffer, Points[Index], Points[Index + 1], Width, Color);
		}
	}

	void AddLonLatPolyline(
		FWaterMeshBuffer& Buffer,
		const TArray<FVector2D>& Points,
		float Width,
		float Z,
		const FLinearColor& Color,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		TArray<FVector> WorldPoints;
		WorldPoints.Reserve(Points.Num());
		for (const FVector2D& Point : Points)
		{
			FVector World = ProjectLonLat(Point.X, Point.Y);
			World.Z = Z;
			WorldPoints.Add(World);
		}
		AddWorldPolyline(Buffer, WorldPoints, Width, Color);
	}

	void CommitDetailSection(
		UProceduralMeshComponent* DetailMesh,
		const FWaterMeshBuffer& Buffer,
		UMaterialInterface* DetailMaterial)
	{
		if (!DetailMesh || Buffer.Verts.IsEmpty())
		{
			return;
		}

		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Buffer.Verts.Num());
		TArray<FProcMeshTangent> Tangents;
		const int32 SectionIndex = DetailMesh->GetNumSections();
		DetailMesh->CreateMeshSection(SectionIndex, Buffer.Verts, Buffer.Tris, Normals, Buffer.UVs, Buffer.Colors, Tangents, false);
		if (DetailMaterial)
		{
			DetailMesh->SetMaterial(SectionIndex, DetailMaterial);
		}
	}

	void BuildBaseSea(UProceduralMeshComponent* SeaMesh, const FWLCampaignWaterBuildParams& Params)
	{
		if (!SeaMesh)
		{
			return;
		}

		const int32 TileCount = FMath::Max(8, Params.TileCount);
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		Verts.Reserve((TileCount + 1) * (TileCount + 1));
		UVs.Reserve((TileCount + 1) * (TileCount + 1));
		Colors.Reserve((TileCount + 1) * (TileCount + 1));

		for (int32 Y = 0; Y <= TileCount; ++Y)
		{
			for (int32 X = 0; X <= TileCount; ++X)
			{
				const float U = static_cast<float>(X) / static_cast<float>(TileCount);
				const float V = static_cast<float>(Y) / static_cast<float>(TileCount);
				const float WorldX = FMath::Lerp(Params.Center.X - Params.HalfSize, Params.Center.X + Params.HalfSize, U);
				const float WorldY = FMath::Lerp(Params.Center.Y - Params.HalfSize, Params.Center.Y + Params.HalfSize, V);
				Verts.Add(FVector(WorldX, WorldY, Params.SeaZ + WaterRippleHeight(WorldX, WorldY)));
				UVs.Add(FVector2D(U * 8.f, V * 8.f));
				Colors.Add(WaterVertexColor(WaterDepthColor(U, V)));
			}
		}

		for (int32 Y = 0; Y < TileCount; ++Y)
		{
			for (int32 X = 0; X < TileCount; ++X)
			{
				const int32 Base = Y * (TileCount + 1) + X;
				Tris.Add(Base);
				Tris.Add(Base + TileCount + 1);
				Tris.Add(Base + 1);
				Tris.Add(Base + 1);
				Tris.Add(Base + TileCount + 1);
				Tris.Add(Base + TileCount + 2);
			}
		}

		TArray<FVector> Normals;
		Normals.Init(FVector::UpVector, Verts.Num());
		TArray<FProcMeshTangent> Tangents;
		SeaMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
		if (Params.WaterMaterial)
		{
			SeaMesh->SetMaterial(0, Params.WaterMaterial);
		}
		else if (Params.DetailMaterial)
		{
			SeaMesh->SetMaterial(0, Params.DetailMaterial);
		}
	}

	void BuildSeaDetails(
		UProceduralMeshComponent* DetailMesh,
		const FWLCampaignWaterBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		(void)Params;
		(void)ProjectLonLat;
		if (!DetailMesh)
		{
			return;
		}
		DetailMesh->ClearAllMeshSections();
		DetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DetailMesh->SetVisibility(false, true);
	}
}

void FWLCampaignWaterBuilder::Build(
	UProceduralMeshComponent* SeaMesh,
	UProceduralMeshComponent* DetailMesh,
	const FWLCampaignWaterBuildParams& Params,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	if (SeaMesh)
	{
		SeaMesh->ClearAllMeshSections();
		SeaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SeaMesh->SetCastShadow(false);
		SeaMesh->bCastDynamicShadow = false;
		SeaMesh->bCastStaticShadow = false;
		SeaMesh->SetReceivesDecals(false);
	}
	if (DetailMesh)
	{
		DetailMesh->ClearAllMeshSections();
		DetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DetailMesh->SetCastShadow(false);
		DetailMesh->bCastDynamicShadow = false;
		DetailMesh->bCastStaticShadow = false;
		DetailMesh->SetReceivesDecals(false);
	}

	BuildBaseSea(SeaMesh, Params);
	BuildSeaDetails(DetailMesh, Params, ProjectLonLat);
}
