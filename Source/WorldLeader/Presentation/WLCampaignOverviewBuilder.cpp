// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignOverviewBuilder.h"

#include "ProceduralMeshComponent.h"

namespace
{
	struct FOverviewLandmass
	{
		FString Name;
		TArray<FVector2D> Points;
		FLinearColor Fill;
		FLinearColor Border;
	};

	FColor ToColor(const FLinearColor& Color)
	{
		return Color.ToFColor(false);
	}

	FVector OverviewPoint(
		const FVector2D& LonLat,
		float Z,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		FVector Point = ProjectLonLat(LonLat.X, LonLat.Y);
		const float Relief = 150.f * FMath::Sin(LonLat.X * 0.17f + LonLat.Y * 0.23f);
		Point.Z = Z + Relief;
		return Point;
	}

	void AddQuad(
		TArray<FVector>& Verts,
		TArray<int32>& Tris,
		TArray<FVector2D>& UVs,
		TArray<FColor>& Colors,
		const FVector& A,
		const FVector& B,
		const FVector& C,
		const FVector& D,
		const FLinearColor& Color)
	{
		const int32 Base = Verts.Num();
		Verts.Add(A);
		Verts.Add(B);
		Verts.Add(C);
		Verts.Add(D);
		UVs.Add(FVector2D(0.f, 0.f));
		UVs.Add(FVector2D(1.f, 0.f));
		UVs.Add(FVector2D(1.f, 1.f));
		UVs.Add(FVector2D(0.f, 1.f));
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Colors.Add(ToColor(Color));
		}
		Tris.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		Tris.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
	}

	void AddFilledLandmass(
		const FOverviewLandmass& Landmass,
		const FWLCampaignOverviewBuildParams& Params,
		TArray<FVector>& Verts,
		TArray<int32>& Tris,
		TArray<FVector2D>& UVs,
		TArray<FColor>& Colors,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		if (Landmass.Points.Num() < 3)
		{
			return;
		}

		FVector2D CenterLonLat = FVector2D::ZeroVector;
		for (const FVector2D& Point : Landmass.Points)
		{
			CenterLonLat += Point;
		}
		CenterLonLat /= static_cast<float>(Landmass.Points.Num());

		const int32 CenterIndex = Verts.Num();
		Verts.Add(OverviewPoint(CenterLonLat, Params.LandZ, ProjectLonLat));
		UVs.Add(FVector2D(0.5f, 0.5f));
		Colors.Add(ToColor(Landmass.Fill));

		for (const FVector2D& Point : Landmass.Points)
		{
			Verts.Add(OverviewPoint(Point, Params.LandZ, ProjectLonLat));
			UVs.Add(FVector2D((Point.X + 180.f) / 360.f, (Point.Y + 90.f) / 180.f));
			Colors.Add(ToColor(Landmass.Fill));
		}

		const int32 Count = Landmass.Points.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 A = CenterIndex;
			const int32 B = CenterIndex + 1 + Index;
			const int32 C = CenterIndex + 1 + ((Index + 1) % Count);
			Tris.Append({A, B, C, A, C, B});
		}
	}

	void AddOutline(
		const FOverviewLandmass& Landmass,
		const FWLCampaignOverviewBuildParams& Params,
		TArray<FVector>& Verts,
		TArray<int32>& Tris,
		TArray<FVector2D>& UVs,
		TArray<FColor>& Colors,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		if (Landmass.Points.Num() < 2)
		{
			return;
		}

		const int32 Count = Landmass.Points.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector A = OverviewPoint(Landmass.Points[Index], Params.BorderZ, ProjectLonLat);
			const FVector B = OverviewPoint(Landmass.Points[(Index + 1) % Count], Params.BorderZ, ProjectLonLat);
			const FVector FlatDirection(B.X - A.X, B.Y - A.Y, 0.f);
			if (FlatDirection.SizeSquared() < 1.f)
			{
				continue;
			}

			const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal()) * Params.BorderWidth;
			AddQuad(Verts, Tris, UVs, Colors, A - Side, A + Side, B + Side, B - Side, Landmass.Border);
		}
	}

	TArray<FOverviewLandmass> MakeLandmasses()
	{
		const FLinearColor RemoteFill(0.070f, 0.128f, 0.106f, 0.92f);
		const FLinearColor RemoteBorder(0.120f, 0.225f, 0.215f, 0.78f);
		const FLinearColor ActiveFill(0.120f, 0.205f, 0.130f, 0.96f);
		const FLinearColor ActiveBorder(0.780f, 0.650f, 0.315f, 0.90f);

		TArray<FOverviewLandmass> Landmasses;
		Landmasses.Add({
			TEXT("North America"),
			{
				FVector2D(-139.f, 56.f), FVector2D(-126.f, 69.f), FVector2D(-96.f, 63.f),
				FVector2D(-72.f, 51.f), FVector2D(-61.f, 30.f), FVector2D(-80.f, 18.f),
				FVector2D(-99.f, 22.f), FVector2D(-118.f, 31.f), FVector2D(-132.f, 45.f)
			},
			RemoteFill,
			RemoteBorder
		});
		Landmasses.Add({
			TEXT("Central America"),
			{
				FVector2D(-96.f, 19.f), FVector2D(-88.f, 18.f), FVector2D(-82.f, 12.f),
				FVector2D(-77.f, 8.f), FVector2D(-79.f, 6.f), FVector2D(-87.f, 8.f),
				FVector2D(-94.f, 14.f)
			},
			RemoteFill * 1.08f,
			RemoteBorder
		});
		Landmasses.Add({
			TEXT("South America"),
			{
				FVector2D(-81.f, 12.f), FVector2D(-70.f, 13.f), FVector2D(-58.f, 8.f),
				FVector2D(-47.f, -2.f), FVector2D(-39.f, -15.f), FVector2D(-53.f, -35.f),
				FVector2D(-67.f, -55.f), FVector2D(-76.f, -35.f), FVector2D(-81.f, -12.f)
			},
			RemoteFill * 1.14f,
			RemoteBorder
		});
		Landmasses.Add({
			TEXT("Caribbean"),
			{
				FVector2D(-84.f, 23.f), FVector2D(-74.f, 23.f), FVector2D(-65.f, 20.f),
				FVector2D(-61.f, 14.f), FVector2D(-70.f, 12.f), FVector2D(-80.f, 17.f)
			},
			FLinearColor(0.080f, 0.150f, 0.140f, 0.82f),
			FLinearColor(0.165f, 0.285f, 0.290f, 0.70f)
		});
		Landmasses.Add({
			TEXT("Active Theater"),
			{
				FVector2D(-80.f, -1.5f), FVector2D(-58.5f, -1.5f),
				FVector2D(-58.5f, 14.4f), FVector2D(-80.f, 14.4f)
			},
			ActiveFill,
			ActiveBorder
		});
		return Landmasses;
	}
}

void FWLCampaignOverviewBuilder::Build(
	UProceduralMeshComponent* OverviewMesh,
	const FWLCampaignOverviewBuildParams& Params,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	if (!OverviewMesh)
	{
		return;
	}

	OverviewMesh->ClearAllMeshSections();

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	for (const FOverviewLandmass& Landmass : MakeLandmasses())
	{
		AddFilledLandmass(Landmass, Params, Verts, Tris, UVs, Colors, ProjectLonLat);
		AddOutline(Landmass, Params, Verts, Tris, UVs, Colors, ProjectLonLat);
	}

	if (Verts.IsEmpty() || Tris.IsEmpty())
	{
		return;
	}

	Normals.Init(FVector::UpVector, Verts.Num());
	OverviewMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (Params.Material)
	{
		OverviewMesh->SetMaterial(0, Params.Material);
	}
}
