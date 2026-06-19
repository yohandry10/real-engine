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
		return Color.ToFColor(true);
	}

	float WaterRippleHeight(float X, float Y)
	{
		return FMath::Sin(X * 0.000034f + Y * 0.000019f) * 115.f
			+ FMath::Cos(X * 0.000013f - Y * 0.000031f) * 72.f
			+ FMath::Sin((X + Y) * 0.000011f) * 48.f;
	}

	FLinearColor WaterDepthColor(float U, float V)
	{
		const FLinearColor Deep(0.000f, 0.010f, 0.024f);
		const FLinearColor Mid(0.003f, 0.052f, 0.078f);
		const FLinearColor Shelf(0.010f, 0.088f, 0.106f);
		const float WaveNoise = FMath::Sin(U * PI * 10.0f + V * PI * 2.4f) * 0.10f
			+ FMath::Cos(U * PI * 3.8f - V * PI * 8.0f) * 0.08f
			+ FMath::Sin((U + V) * PI * 7.0f) * 0.06f;
		const float BroadGradient = FMath::Clamp(0.12f + V * 0.26f + WaveNoise, 0.f, 1.f);
		const float ShelfMask = FMath::Clamp(0.12f + FMath::Sin((U + V) * PI * 2.0f) * 0.08f + (1.f - V) * 0.10f, 0.f, 0.32f);
		return FMath::Lerp(FMath::Lerp(Deep, Mid, BroadGradient), Shelf, ShelfMask);
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
		if (!DetailMesh)
		{
			return;
		}

		FWaterMeshBuffer Swell;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			const float T = static_cast<float>(Index) / 11.f;
			const float StartX = FMath::Lerp(Params.Center.X - Params.HalfSize * 0.95f, Params.Center.X + Params.HalfSize * 0.55f, T);
			const float StartY = Params.Center.Y - Params.HalfSize * (0.82f - T * 0.18f);
			TArray<FVector> Points;
			for (int32 P = 0; P < 5; ++P)
			{
				const float Step = static_cast<float>(P) / 4.f;
				const float X = StartX + Step * Params.HalfSize * 1.10f;
				const float Y = StartY + FMath::Sin(Step * PI * 1.4f + Index * 0.7f) * 12000.f + Step * Params.HalfSize * 0.18f;
				Points.Add(FVector(X, Y, Params.SeaZ + 760.f + Index * 14.f));
			}
			const FLinearColor SwellColor = (Index % 3 == 0)
				? FLinearColor(0.060f, 0.185f, 0.205f)
				: FLinearColor(0.010f, 0.055f, 0.078f);
			AddWorldPolyline(Swell, Points, 860.f + Index * 22.f, SwellColor);
		}
		CommitDetailSection(DetailMesh, Swell, Params.DetailMaterial);

		FWaterMeshBuffer DepthBands;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float T = static_cast<float>(Index) / 4.f;
			const float OffsetY = FMath::Lerp(-0.58f, -0.14f, T) * Params.HalfSize;
			TArray<FVector> Points;
			for (int32 P = 0; P < 7; ++P)
			{
				const float Step = static_cast<float>(P) / 6.f;
				const float X = FMath::Lerp(Params.Center.X - Params.HalfSize * 0.96f, Params.Center.X + Params.HalfSize * 0.82f, Step);
				const float Y = Params.Center.Y + OffsetY + FMath::Sin(Step * PI * 2.0f + Index * 0.65f) * 18500.f;
				Points.Add(FVector(X, Y, Params.SeaZ + 690.f + Index * 18.f));
			}
			AddWorldPolyline(DepthBands, Points, 7200.f + Index * 1400.f, Index % 2 == 0
				? FLinearColor(0.000f, 0.026f, 0.045f)
				: FLinearColor(0.016f, 0.082f, 0.095f));
		}
		CommitDetailSection(DetailMesh, DepthBands, Params.DetailMaterial);

		FWaterMeshBuffer Glints;
		for (int32 Index = 0; Index < 18; ++Index)
		{
			const float U = static_cast<float>(Index % 6) / 5.f;
			const float V = static_cast<float>(Index / 6) / 2.f;
			const FVector Start(
				FMath::Lerp(Params.Center.X - Params.HalfSize * 0.86f, Params.Center.X + Params.HalfSize * 0.40f, U),
				FMath::Lerp(Params.Center.Y - Params.HalfSize * 0.78f, Params.Center.Y - Params.HalfSize * 0.24f, V),
				Params.SeaZ + 1450.f + Index * 2.f);
			const FVector End = Start + FVector(18000.f + Index * 450.f, 4300.f * FMath::Sin(Index * 0.9f), 0.f);
			AddStrip(Glints, Start, End, 280.f, FLinearColor(0.090f, 0.220f, 0.225f));
		}
		CommitDetailSection(DetailMesh, Glints, Params.DetailMaterial);

		FWaterMeshBuffer Coast;
		AddLonLatPolyline(Coast,
			{ FVector2D(-76.7f, 10.2f), FVector2D(-75.5f, 10.7f), FVector2D(-74.7f, 11.05f), FVector2D(-72.8f, 11.55f), FVector2D(-71.6f, 10.95f), FVector2D(-68.5f, 10.75f), FVector2D(-66.7f, 10.55f), FVector2D(-64.4f, 10.25f) },
			1160.f,
			Params.SeaZ + 1120.f,
			FLinearColor(0.105f, 0.235f, 0.215f),
			ProjectLonLat);
		AddLonLatPolyline(Coast,
			{ FVector2D(-75.6f, 12.6f), FVector2D(-72.1f, 12.0f), FVector2D(-69.2f, 11.85f), FVector2D(-66.0f, 11.65f), FVector2D(-63.6f, 10.95f) },
			520.f,
			Params.SeaZ + 1260.f,
			FLinearColor(0.035f, 0.135f, 0.160f),
			ProjectLonLat);
		CommitDetailSection(DetailMesh, Coast, Params.DetailMaterial);

		FWaterMeshBuffer CaribbeanGlints;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			const float Lon = FMath::Lerp(-76.8f, -66.0f, static_cast<float>(Index % 6) / 5.f);
			const float Lat = 11.55f + static_cast<float>(Index / 6) * 0.55f + FMath::Sin(Index * 0.8f) * 0.08f;
			const FVector Start = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, 1350.f + Index * 5.f);
			const FVector End = ProjectLonLat(Lon + 0.45f, Lat + 0.05f) + FVector(0.f, 0.f, 1350.f + Index * 5.f);
			AddStrip(CaribbeanGlints, Start, End, 360.f, FLinearColor(0.050f, 0.150f, 0.155f));
		}
		CommitDetailSection(DetailMesh, CaribbeanGlints, Params.DetailMaterial);

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
	}
	if (DetailMesh)
	{
		DetailMesh->ClearAllMeshSections();
		DetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	BuildBaseSea(SeaMesh, Params);
	BuildSeaDetails(DetailMesh, Params, ProjectLonLat);
}
