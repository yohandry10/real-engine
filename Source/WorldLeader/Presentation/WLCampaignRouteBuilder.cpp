// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignRouteBuilder.h"

#include "ProceduralMeshComponent.h"

namespace
{
	struct FRouteMeshBuffer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	struct FRouteStyle
	{
		float SurfaceWidth = 520.f;
		float ShoulderWidth = 760.f;
		float ZOffset = 880.f;
		FLinearColor SurfaceColor = FLinearColor(0.36f, 0.335f, 0.285f);
		FLinearColor ShoulderColor = FLinearColor(0.210f, 0.195f, 0.160f);
		FLinearColor CenterWearColor = FLinearColor(0.230f, 0.215f, 0.180f);
		float CenterWearWidth = 90.f;
		bool bHasCenterWear = true;
	};

	FColor RouteVertexColor(const FLinearColor& Color)
	{
		return Color.ToFColor(false);
	}

	FRouteStyle StyleFor(EWLCampaignRouteType Type)
	{
		FRouteStyle Style;
		switch (Type)
		{
		case EWLCampaignRouteType::Primary:
			Style.SurfaceWidth = 640.f;
			Style.ShoulderWidth = 1120.f;
			Style.ZOffset = 990.f;
			Style.SurfaceColor = FLinearColor(0.310f, 0.292f, 0.240f);
			Style.ShoulderColor = FLinearColor(0.135f, 0.122f, 0.092f);
			Style.CenterWearColor = FLinearColor(0.205f, 0.190f, 0.155f);
			Style.CenterWearWidth = 115.f;
			break;
		case EWLCampaignRouteType::Secondary:
			Style.SurfaceWidth = 430.f;
			Style.ShoulderWidth = 760.f;
			Style.ZOffset = 900.f;
			Style.SurfaceColor = FLinearColor(0.245f, 0.225f, 0.175f);
			Style.ShoulderColor = FLinearColor(0.120f, 0.105f, 0.075f);
			Style.CenterWearColor = FLinearColor(0.165f, 0.150f, 0.115f);
			Style.CenterWearWidth = 72.f;
			break;
		case EWLCampaignRouteType::Rural:
			Style.SurfaceWidth = 280.f;
			Style.ShoulderWidth = 500.f;
			Style.ZOffset = 760.f;
			Style.SurfaceColor = FLinearColor(0.235f, 0.165f, 0.090f);
			Style.ShoulderColor = FLinearColor(0.105f, 0.070f, 0.040f);
			Style.CenterWearColor = FLinearColor(0.155f, 0.110f, 0.060f);
			Style.CenterWearWidth = 0.f;
			Style.bHasCenterWear = false;
			break;
		case EWLCampaignRouteType::PortAccess:
			Style.SurfaceWidth = 500.f;
			Style.ShoulderWidth = 860.f;
			Style.ZOffset = 950.f;
			Style.SurfaceColor = FLinearColor(0.280f, 0.270f, 0.215f);
			Style.ShoulderColor = FLinearColor(0.115f, 0.105f, 0.080f);
			Style.CenterWearColor = FLinearColor(0.180f, 0.170f, 0.135f);
			Style.CenterWearWidth = 84.f;
			break;
		case EWLCampaignRouteType::BorderCrossing:
			Style.SurfaceWidth = 560.f;
			Style.ShoulderWidth = 940.f;
			Style.ZOffset = 1005.f;
			Style.SurfaceColor = FLinearColor(0.330f, 0.250f, 0.145f);
			Style.ShoulderColor = FLinearColor(0.150f, 0.100f, 0.052f);
			Style.CenterWearColor = FLinearColor(0.430f, 0.310f, 0.130f);
			Style.CenterWearWidth = 110.f;
			break;
		default:
			break;
		}
		return Style;
	}

	FVector2D CatmullRom2D(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return 0.5f * (
			(2.f * P1)
			+ (-P0 + P2) * T
			+ (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2
			+ (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
	}

	TArray<FVector2D> SmoothPoints(const TArray<FVector2D>& Points, int32 Smoothness)
	{
		if (Points.Num() < 2)
		{
			return Points;
		}

		TArray<FVector2D> Result;
		const int32 Steps = FMath::Clamp(Smoothness, 2, 9);
		Result.Reserve((Points.Num() - 1) * Steps + 1);
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			const FVector2D& P0 = Points[Index > 0 ? Index - 1 : Index];
			const FVector2D& P1 = Points[Index];
			const FVector2D& P2 = Points[Index + 1];
			const FVector2D& P3 = Points[Index + 2 < Points.Num() ? Index + 2 : Index + 1];
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Result.Add(CatmullRom2D(P0, P1, P2, P3, static_cast<float>(Step) / static_cast<float>(Steps)));
			}
		}
		Result.Add(Points.Last());
		return Result;
	}

	TArray<FVector> ProjectRoutePoints(
		const TArray<FVector2D>& LonLatPoints,
		float ZOffset,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		TArray<FVector> WorldPoints;
		WorldPoints.Reserve(LonLatPoints.Num());
		for (const FVector2D& Point : LonLatPoints)
		{
			FVector World = ProjectLonLat(Point.X, Point.Y);
			World.Z += ZOffset;
			WorldPoints.Add(World);
		}
		return WorldPoints;
	}

	void AddStrip(FRouteMeshBuffer& Buffer, const FVector& Start, const FVector& End, float Width, const FLinearColor& Color)
	{
		const FVector Direction = End - Start;
		const FVector Flat(Direction.X, Direction.Y, 0.f);
		if (Flat.SizeSquared() < 1.f)
		{
			return;
		}

		const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat.GetSafeNormal()) * (Width * 0.5f);
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({ Start - Side, Start + Side, End + Side, End - Side });
		Buffer.Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Buffer.Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector });
		Buffer.UVs.Append({ FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(1.f, 0.f) });
		const FColor VertexColor = RouteVertexColor(Color);
		Buffer.Colors.Append({ VertexColor, VertexColor, VertexColor, VertexColor });
	}

	void AddRouteLayer(FRouteMeshBuffer& Buffer, const TArray<FVector>& Points, float Width, float ZLift, const FLinearColor& Color)
	{
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			AddStrip(Buffer, Points[Index] + FVector(0.f, 0.f, ZLift), Points[Index + 1] + FVector(0.f, 0.f, ZLift), Width, Color);
		}
	}

	void AddNode(FRouteMeshBuffer& Buffer, const FVector& Center, float Radius, const FLinearColor& Color)
	{
		const int32 Sides = 10;
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Add(Center);
		Buffer.Normals.Add(FVector::UpVector);
		Buffer.UVs.Add(FVector2D(0.5f, 0.5f));
		Buffer.Colors.Add(RouteVertexColor(Color));
		for (int32 Index = 0; Index < Sides; ++Index)
		{
			const float Angle = static_cast<float>(Index) / static_cast<float>(Sides) * 2.f * PI;
			Buffer.Verts.Add(Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f));
			Buffer.Normals.Add(FVector::UpVector);
			Buffer.UVs.Add(FVector2D(FMath::Cos(Angle) * 0.5f + 0.5f, FMath::Sin(Angle) * 0.5f + 0.5f));
			Buffer.Colors.Add(RouteVertexColor(Color));
		}
		for (int32 Index = 0; Index < Sides; ++Index)
		{
			Buffer.Tris.Add(Base);
			Buffer.Tris.Add(Base + 1 + Index);
			Buffer.Tris.Add(Base + 1 + ((Index + 1) % Sides));
		}
	}

	void AddFlatBox(
		FRouteMeshBuffer& Buffer,
		const FVector& Center,
		const FRotator& Rotation,
		const FVector& Extent,
		const FLinearColor& Color)
	{
		const FVector X = Rotation.RotateVector(FVector(Extent.X, 0.f, 0.f));
		const FVector Y = Rotation.RotateVector(FVector(0.f, Extent.Y, 0.f));
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({ Center - X - Y, Center + X - Y, Center + X + Y, Center - X + Y });
		Buffer.Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Buffer.Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector });
		Buffer.UVs.Append({ FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(1.f, 1.f), FVector2D(0.f, 1.f) });
		const FColor VertexColor = RouteVertexColor(Color);
		Buffer.Colors.Append({ VertexColor, VertexColor, VertexColor, VertexColor });
	}

	void AddRoute(FRouteMeshBuffer& Buffer, const FWLCampaignRouteSpec& Spec, TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		if (Spec.Points.Num() < 2)
		{
			return;
		}

		const FRouteStyle Style = StyleFor(Spec.Type);
		const TArray<FVector2D> Smoothed = SmoothPoints(Spec.Points, Spec.Smoothness);
		const TArray<FVector> WorldPoints = ProjectRoutePoints(Smoothed, Style.ZOffset, ProjectLonLat);
		AddRouteLayer(Buffer, WorldPoints, Style.ShoulderWidth, -24.f, Style.ShoulderColor);
		AddRouteLayer(Buffer, WorldPoints, Style.SurfaceWidth, 0.f, Style.SurfaceColor);
		if (Style.bHasCenterWear && Style.CenterWearWidth > 1.f)
		{
			AddRouteLayer(Buffer, WorldPoints, Style.CenterWearWidth, 18.f, Style.CenterWearColor);
		}

		if (Spec.bShowJunctions)
		{
			const float Radius = FMath::Max(Style.SurfaceWidth * 0.78f, 260.f);
			for (const FVector2D& Point : Spec.Points)
			{
				const FVector Center = ProjectLonLat(Point.X, Point.Y) + FVector(0.f, 0.f, Style.ZOffset + 30.f);
				AddNode(Buffer, Center, Radius, Style.SurfaceColor);
			}
		}
	}

	void AddBridge(FRouteMeshBuffer& Buffer, const FVector2D& A, const FVector2D& B, TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		const FVector Start = ProjectLonLat(A.X, A.Y) + FVector(0.f, 0.f, 1120.f);
		const FVector End = ProjectLonLat(B.X, B.Y) + FVector(0.f, 0.f, 1120.f);
		const FVector Mid = (Start + End) * 0.5f;
		const FVector Direction = End - Start;
		const FRotator Rotation(0.f, FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)), 0.f);
		AddFlatBox(Buffer, Mid, Rotation, FVector(Direction.Size2D() * 0.56f, 520.f, 0.f), FLinearColor(0.280f, 0.275f, 0.245f));
		AddFlatBox(Buffer, Mid + Rotation.RotateVector(FVector(0.f, 620.f, 36.f)), Rotation, FVector(Direction.Size2D() * 0.54f, 50.f, 0.f), FLinearColor(0.130f, 0.125f, 0.110f));
		AddFlatBox(Buffer, Mid + Rotation.RotateVector(FVector(0.f, -620.f, 36.f)), Rotation, FVector(Direction.Size2D() * 0.54f, 50.f, 0.f), FLinearColor(0.130f, 0.125f, 0.110f));
	}

	void AddCheckpoint(FRouteMeshBuffer& Buffer, const FVector2D& CenterLonLat, float YawDegrees, TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		const FVector Center = ProjectLonLat(CenterLonLat.X, CenterLonLat.Y) + FVector(0.f, 0.f, 1240.f);
		const FRotator Rotation(0.f, YawDegrees, 0.f);
		AddFlatBox(Buffer, Center, Rotation, FVector(1100.f, 780.f, 0.f), FLinearColor(0.235f, 0.190f, 0.105f));
		AddFlatBox(Buffer, Center + Rotation.RotateVector(FVector(-620.f, 0.f, 28.f)), Rotation, FVector(170.f, 460.f, 0.f), FLinearColor(0.070f, 0.075f, 0.070f));
		AddFlatBox(Buffer, Center + Rotation.RotateVector(FVector(620.f, 0.f, 28.f)), Rotation, FVector(170.f, 460.f, 0.f), FLinearColor(0.070f, 0.075f, 0.070f));
		AddFlatBox(Buffer, Center + Rotation.RotateVector(FVector(0.f, 0.f, 42.f)), Rotation, FVector(820.f, 72.f, 0.f), FLinearColor(0.700f, 0.520f, 0.180f));
	}

	void CommitRoutes(UProceduralMeshComponent* RoadMesh, UMaterialInterface* RoadMaterial, const FRouteMeshBuffer& Buffer)
	{
		if (!RoadMesh || Buffer.Verts.IsEmpty())
		{
			return;
		}

		const int32 SectionIndex = RoadMesh->GetNumSections();
		RoadMesh->CreateMeshSection(
			SectionIndex,
			Buffer.Verts,
			Buffer.Tris,
			Buffer.Normals,
			Buffer.UVs,
			Buffer.Colors,
			Buffer.Tangents,
			false);
		if (RoadMaterial)
		{
			RoadMesh->SetMaterial(SectionIndex, RoadMaterial);
		}
	}
}

void FWLCampaignRouteBuilder::BuildDefaultTheaterRoutes(
	UProceduralMeshComponent* RoadMesh,
	UMaterialInterface* RoadMaterial,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	if (!RoadMesh)
	{
		return;
	}

	FRouteMeshBuffer Buffer;
	const TArray<FWLCampaignRouteSpec> Routes = {
		{
			TEXT("Andes to Venezuela strategic corridor"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-74.10f, 4.60f), FVector2D(-73.82f, 5.55f), FVector2D(-73.42f, 6.58f),
				FVector2D(-72.96f, 7.35f), FVector2D(-72.50f, 7.90f), FVector2D(-72.05f, 8.72f),
				FVector2D(-71.60f, 10.60f), FVector2D(-69.70f, 10.36f), FVector2D(-68.00f, 10.20f),
				FVector2D(-66.90f, 10.50f), FVector2D(-65.72f, 10.34f), FVector2D(-64.70f, 10.10f)
			},
			6,
			true
		},
		{
			TEXT("Colombian capital to Caribbean ports"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-74.10f, 4.60f), FVector2D(-74.70f, 5.45f), FVector2D(-75.58f, 6.25f),
				FVector2D(-75.77f, 7.95f), FVector2D(-75.55f, 9.18f), FVector2D(-75.50f, 10.40f),
				FVector2D(-74.80f, 10.98f), FVector2D(-72.90f, 11.50f)
			},
			6,
			true
		},
		{
			TEXT("Venezuelan coastal spine"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-71.60f, 10.60f), FVector2D(-70.42f, 10.36f), FVector2D(-68.00f, 10.20f),
				FVector2D(-66.90f, 10.50f), FVector2D(-65.72f, 10.34f), FVector2D(-64.70f, 10.10f)
			},
			6,
			true
		},
		{
			TEXT("Caracas to Orinoco interior"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-66.90f, 10.50f), FVector2D(-66.42f, 9.18f), FVector2D(-66.05f, 8.18f),
				FVector2D(-64.40f, 8.05f), FVector2D(-62.60f, 8.30f)
			},
			5,
			true
		},
		{
			TEXT("Llanos logistics track"),
			EWLCampaignRouteType::Rural,
			{
				FVector2D(-74.10f, 4.60f), FVector2D(-72.80f, 5.20f), FVector2D(-70.60f, 5.55f),
				FVector2D(-68.50f, 5.20f), FVector2D(-66.80f, 5.80f), FVector2D(-64.50f, 7.05f),
				FVector2D(-62.60f, 8.30f)
			},
			4,
			false
		},
		{
			TEXT("Magdalena interior secondary"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-74.10f, 4.60f), FVector2D(-74.55f, 6.15f), FVector2D(-74.78f, 8.25f),
				FVector2D(-74.80f, 10.98f)
			},
			5,
			true
		},
		{
			TEXT("Cartagena port access"),
			EWLCampaignRouteType::PortAccess,
			{ FVector2D(-75.50f, 10.40f), FVector2D(-75.64f, 10.58f), FVector2D(-75.78f, 10.78f) },
			4,
			true
		},
		{
			TEXT("Barranquilla port access"),
			EWLCampaignRouteType::PortAccess,
			{ FVector2D(-74.80f, 10.98f), FVector2D(-74.78f, 11.12f), FVector2D(-74.74f, 11.28f) },
			4,
			true
		},
		{
			TEXT("Maracaibo port and oil access"),
			EWLCampaignRouteType::PortAccess,
			{ FVector2D(-71.60f, 10.60f), FVector2D(-71.70f, 10.82f), FVector2D(-71.82f, 11.02f) },
			4,
			true
		},
		{
			TEXT("Barcelona port access"),
			EWLCampaignRouteType::PortAccess,
			{ FVector2D(-64.70f, 10.10f), FVector2D(-64.65f, 10.23f), FVector2D(-64.58f, 10.38f) },
			4,
			true
		},
		{
			TEXT("Cucuta border crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-72.78f, 7.72f), FVector2D(-72.50f, 7.90f), FVector2D(-72.18f, 8.22f) },
			5,
			true
		}
	};

	for (const FWLCampaignRouteSpec& Route : Routes)
	{
		AddRoute(Buffer, Route, ProjectLonLat);
	}

	AddBridge(Buffer, FVector2D(-72.54f, 7.96f), FVector2D(-72.25f, 8.18f), ProjectLonLat);
	AddBridge(Buffer, FVector2D(-74.93f, 8.05f), FVector2D(-74.80f, 8.42f), ProjectLonLat);
	AddCheckpoint(Buffer, FVector2D(-72.38f, 8.06f), 34.f, ProjectLonLat);
	CommitRoutes(RoadMesh, RoadMaterial, Buffer);
}

void FWLCampaignRouteBuilder::AppendRoutes(
	UProceduralMeshComponent* RoadMesh,
	UMaterialInterface* RoadMaterial,
	const TArray<FWLCampaignRouteSpec>& Routes,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	if (!RoadMesh || Routes.Num() == 0)
	{
		return;
	}

	FRouteMeshBuffer Buffer;
	for (const FWLCampaignRouteSpec& Route : Routes)
	{
		AddRoute(Buffer, Route, ProjectLonLat);
	}
	CommitRoutes(RoadMesh, RoadMaterial, Buffer);
}
