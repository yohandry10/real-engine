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
		// Carretera ancha tipo autopista (pasan tropas/tanques): asfalto OSCURO + linea
		// central AMARILLA + arcenes claros (de grava), como la referencia. Colores en
		// lineal bajos = asfalto oscuro real (antes 0.31 se veia gris palido). La cinta es
		// continua, asi que la calzada se lee de corrido (sin "saltos").
		case EWLCampaignRouteType::Primary:
			Style.SurfaceWidth = 1320.f;   // ~2-3 carriles
			Style.ShoulderWidth = 1720.f;  // asfalto + arcenes
			Style.ZOffset = 990.f;
			Style.SurfaceColor = FLinearColor(0.058f, 0.060f, 0.066f); // asfalto
			Style.ShoulderColor = FLinearColor(0.205f, 0.180f, 0.138f); // grava/tierra
			Style.CenterWearColor = FLinearColor(0.860f, 0.640f, 0.060f); // linea central amarilla
			Style.CenterWearWidth = 70.f;
			break;
		case EWLCampaignRouteType::Secondary:
			Style.SurfaceWidth = 1060.f;
			Style.ShoulderWidth = 1380.f;
			Style.ZOffset = 945.f;
			Style.SurfaceColor = FLinearColor(0.066f, 0.068f, 0.074f);
			Style.ShoulderColor = FLinearColor(0.190f, 0.166f, 0.126f);
			Style.CenterWearColor = FLinearColor(0.840f, 0.620f, 0.060f);
			Style.CenterWearWidth = 56.f;
			break;
		case EWLCampaignRouteType::Rural:
			// Antes era tierra marron y se confundia ("¿es carretera o que?"). Ahora es
			// asfalto angosto con linea, igual que el resto: todo se lee como carretera.
			Style.SurfaceWidth = 860.f;
			Style.ShoulderWidth = 1140.f;
			Style.ZOffset = 920.f;
			Style.SurfaceColor = FLinearColor(0.070f, 0.072f, 0.078f);
			Style.ShoulderColor = FLinearColor(0.185f, 0.162f, 0.124f);
			Style.CenterWearColor = FLinearColor(0.820f, 0.600f, 0.060f);
			Style.CenterWearWidth = 46.f;
			break;
		case EWLCampaignRouteType::PortAccess:
			Style.SurfaceWidth = 980.f;
			Style.ShoulderWidth = 1280.f;
			Style.ZOffset = 950.f;
			Style.SurfaceColor = FLinearColor(0.060f, 0.062f, 0.068f);
			Style.ShoulderColor = FLinearColor(0.195f, 0.172f, 0.132f);
			Style.CenterWearColor = FLinearColor(0.840f, 0.620f, 0.060f);
			Style.CenterWearWidth = 50.f;
			break;
		case EWLCampaignRouteType::BorderCrossing:
			Style.SurfaceWidth = 1180.f;
			Style.ShoulderWidth = 1540.f;
			Style.ZOffset = 1005.f;
			Style.SurfaceColor = FLinearColor(0.058f, 0.060f, 0.066f);
			Style.ShoulderColor = FLinearColor(0.225f, 0.190f, 0.138f);
			Style.CenterWearColor = FLinearColor(0.860f, 0.640f, 0.060f);
			Style.CenterWearWidth = 64.f;
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

	float RouteJoinOverlap(float Width)
	{
		return FMath::Clamp(Width * 0.34f, 120.f, 620.f);
	}

	float RouteCapRadius(float Width)
	{
		return FMath::Clamp(Width * 0.52f, 120.f, 900.f);
	}

	void AddStrip(
		FRouteMeshBuffer& Buffer,
		const FVector& Start,
		const FVector& End,
		float Width,
		float LongitudinalOverlap,
		const FLinearColor& Color)
	{
		const FVector Direction = End - Start;
		const FVector Flat(Direction.X, Direction.Y, 0.f);
		if (Flat.SizeSquared() < 1.f)
		{
			return;
		}

		const FVector Forward = Flat.GetSafeNormal();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward) * (Width * 0.5f);
		const FVector ExtendedStart = Start - Forward * LongitudinalOverlap;
		const FVector ExtendedEnd = End + Forward * LongitudinalOverlap;
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({ ExtendedStart - Side, ExtendedStart + Side, ExtendedEnd + Side, ExtendedEnd - Side });
		Buffer.Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Buffer.Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector });
		Buffer.UVs.Append({ FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(1.f, 0.f) });
		const FColor VertexColor = RouteVertexColor(Color);
		Buffer.Colors.Append({ VertexColor, VertexColor, VertexColor, VertexColor });
	}

	void AddRouteLayer(FRouteMeshBuffer& Buffer, const TArray<FVector>& Points, float Width, float ZLift, const FLinearColor& Color)
	{
		const float Overlap = RouteJoinOverlap(Width);
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			AddStrip(
				Buffer,
				Points[Index] + FVector(0.f, 0.f, ZLift),
				Points[Index + 1] + FVector(0.f, 0.f, ZLift),
				Width,
				Overlap,
				Color);
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

	void AddRouteLayerCaps(
		FRouteMeshBuffer& Buffer,
		const TArray<FVector2D>& ControlPoints,
		float Width,
		float ZOffset,
		float ZLift,
		const FLinearColor& Color,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		const float Radius = RouteCapRadius(Width);
		for (const FVector2D& Point : ControlPoints)
		{
			const FVector Center = ProjectLonLat(Point.X, Point.Y) + FVector(0.f, 0.f, ZOffset + ZLift + 2.f);
			AddNode(Buffer, Center, Radius, Color);
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
		AddRouteLayerCaps(Buffer, Spec.Points, Style.ShoulderWidth, Style.ZOffset, -24.f, Style.ShoulderColor, ProjectLonLat);
		AddRouteLayer(Buffer, WorldPoints, Style.SurfaceWidth, 0.f, Style.SurfaceColor);
		AddRouteLayerCaps(Buffer, Spec.Points, Style.SurfaceWidth, Style.ZOffset, 0.f, Style.SurfaceColor, ProjectLonLat);
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

	// Red del teatro = UNA cinta continua para CO y VE (antes VE usaba tiles de asset que
	// se veian "saltados"). Rutas curadas sobre TIERRA, conectando las ciudades. Se
	// omiten spurs de puerto que entraban al agua (p.ej. Maracaibo hacia el Golfo).
	FRouteMeshBuffer Buffer;
	const TArray<FWLCampaignRouteSpec> Routes = {
		// --- COLOMBIA: corredores reales (Panamericana + Caribe). Sin nodos-blob. ---
		// Eje andino (Troncal/Panamericana -> frontera EC): Cucuta -> Pamplona ->
		// Bucaramanga -> Tunja -> Bogota -> Ibague -> Armenia -> Cali -> Popayan ->
		// Pasto -> Ipiales. Pasa POR las ciudades (antes Bucaramanga quedaba sin via).
		{
			TEXT("CO Andean Panamericana spine"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-72.50f, 7.90f), FVector2D(-72.65f, 7.38f), FVector2D(-73.12f, 7.12f),
				FVector2D(-73.36f, 5.53f), FVector2D(-74.10f, 4.60f), FVector2D(-75.23f, 4.44f),
				FVector2D(-75.68f, 4.53f), FVector2D(-76.53f, 3.45f), FVector2D(-76.61f, 2.44f),
				FVector2D(-77.28f, 1.21f), FVector2D(-77.64f, 0.83f)
			},
			6,
			false
		},
		// Costa caribena: Maicao -> Riohacha -> Santa Marta -> Barranquilla -> Cartagena.
		{
			TEXT("CO Caribbean coast road"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-72.24f, 11.38f), FVector2D(-72.90f, 11.50f), FVector2D(-74.20f, 11.24f),
				FVector2D(-74.80f, 10.98f), FVector2D(-75.50f, 10.40f)
			},
			6,
			false
		},
		// Conector Caribe -> Andes: Maicao/Valledupar -> Bucaramanga.
		{
			TEXT("CO Caribbean to Andes connector"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-72.24f, 11.38f), FVector2D(-73.25f, 10.46f), FVector2D(-73.40f, 8.30f),
				FVector2D(-73.12f, 7.12f)
			},
			5,
			false
		},
		// Noroccidente: Cartagena -> Sincelejo -> Monteria -> Medellin -> Pereira -> eje.
		{
			TEXT("CO northwest to spine"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-75.50f, 10.40f), FVector2D(-75.40f, 9.30f), FVector2D(-75.88f, 8.75f),
				FVector2D(-75.58f, 6.25f), FVector2D(-75.69f, 4.81f), FVector2D(-75.68f, 4.53f)
			},
			6,
			false
		},
		// --- CRUCES FRONTERIZOS reales ---
		// VE<->CO andino: San Cristobal -> San Antonio del Tachira -> Cucuta (Simon Bolivar).
		{
			TEXT("VE-CO San Antonio Cucuta crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-72.23f, 7.77f), FVector2D(-72.44f, 7.82f), FVector2D(-72.50f, 7.90f) },
			4,
			false
		},
		// VE<->CO caribeno: Maracaibo -> Paraguachon -> Maicao (por la Guajira, tierra).
		{
			TEXT("VE-CO Maracaibo Maicao crossing"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-71.82f, 10.58f), FVector2D(-71.95f, 11.05f), FVector2D(-72.05f, 11.28f),
				FVector2D(-72.24f, 11.38f)
			},
			6,
			false
		},
		// CO<->EC andino: Ipiales -> Rumichaca -> Tulcan.
		{
			TEXT("CO-EC Ipiales Tulcan crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-77.64f, 0.83f), FVector2D(-77.68f, 0.82f), FVector2D(-77.72f, 0.81f) },
			3,
			false
		},
		// --- COLOMBIA relleno: llanos, sur, Pacifico, cafetero (nada queda colgado) ---
		// Llanos: Bogota -> Villavicencio -> Yopal -> Arauca.
		{
			TEXT("CO llanos road"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-74.10f, 4.60f), FVector2D(-73.63f, 4.15f), FVector2D(-72.39f, 5.34f), FVector2D(-70.76f, 7.08f) },
			6,
			false
		},
		// Orinoquia (tierra): Villavicencio -> San Jose -> Inirida -> Puerto Carreno.
		{
			TEXT("CO Orinoquia track"),
			EWLCampaignRouteType::Rural,
			{
				FVector2D(-73.63f, 4.15f), FVector2D(-72.64f, 2.57f), FVector2D(-70.20f, 3.40f),
				FVector2D(-67.92f, 3.87f), FVector2D(-67.49f, 6.19f)
			},
			5,
			false
		},
		// Sur andino: Bogota -> Neiva -> Pitalito -> Mocoa -> Pasto (empalma al eje).
		{
			TEXT("CO southern Andes"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-74.10f, 4.60f), FVector2D(-75.28f, 2.93f), FVector2D(-76.05f, 1.86f), FVector2D(-76.65f, 1.15f), FVector2D(-77.28f, 1.21f) },
			6,
			false
		},
		// Caqueta: Neiva -> Florencia.
		{
			TEXT("CO Caqueta road"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-75.28f, 2.93f), FVector2D(-75.40f, 2.20f), FVector2D(-75.61f, 1.61f) },
			4,
			false
		},
		{
			TEXT("CO Guaviare Caqueta connector"),
			EWLCampaignRouteType::Rural,
			{ FVector2D(-75.61f, 1.61f), FVector2D(-74.20f, 1.75f), FVector2D(-72.64f, 2.57f) },
			4,
			false
		},
		{
			TEXT("CO Putumayo Amazonas frontier track"),
			EWLCampaignRouteType::Rural,
			{ FVector2D(-76.65f, 1.15f), FVector2D(-76.50f, 0.51f), FVector2D(-73.85f, -1.20f), FVector2D(-69.94f, -4.22f) },
			5,
			false
		},
		// Pacifico: Cali -> Buenaventura.
		{
			TEXT("CO Buenaventura access"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-76.53f, 3.45f), FVector2D(-76.80f, 3.70f), FVector2D(-77.07f, 3.88f) },
			4,
			false
		},
		// Pasto -> Tumaco (Pacifico sur).
		{
			TEXT("CO Tumaco access"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-77.28f, 1.21f), FVector2D(-78.05f, 1.50f), FVector2D(-78.79f, 1.79f) },
			4,
			false
		},
		// Medellin -> Quibdo (Choco).
		{
			TEXT("CO Quibdo access"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-75.58f, 6.25f), FVector2D(-76.10f, 5.95f), FVector2D(-76.65f, 5.69f) },
			4,
			false
		},
		// Eje cafetero: Medellin -> Manizales -> Pereira.
		{
			TEXT("CO coffee axis"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-75.58f, 6.25f), FVector2D(-75.51f, 5.07f), FVector2D(-75.69f, 4.81f) },
			5,
			false
		},
		// Cruce CO<->VE oriental: Arauca -> Guasdualito -> Barinas.
		{
			TEXT("CO-VE Arauca crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-70.76f, 7.08f), FVector2D(-70.80f, 7.24f), FVector2D(-70.21f, 8.62f) },
			5,
			false
		},
		// Cruce CO<->VE Orinoco: Puerto Carreno -> Puerto Ayacucho.
		{
			TEXT("CO-VE Orinoco crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-67.49f, 6.19f), FVector2D(-67.55f, 5.90f), FVector2D(-67.62f, 5.66f) },
			4,
			false
		},
		// Guyana al continente: Lethem -> Boa Vista (refuerza el cruce GY-BR).
		{
			TEXT("GY-BR Lethem Boa Vista"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-59.80f, 3.38f), FVector2D(-60.20f, 3.10f), FVector2D(-60.67f, 2.82f) },
			4,
			false
		},
		{
			TEXT("GY interior road"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-58.16f, 6.80f), FVector2D(-58.30f, 6.00f), FVector2D(-58.62f, 6.41f), FVector2D(-59.80f, 3.38f) },
			5,
			false
		},
		{
			TEXT("GY-SR Corentyne crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-58.16f, 6.80f), FVector2D(-57.15f, 5.88f), FVector2D(-56.97f, 5.95f) },
			4,
			false
		},
		// BR-174: Boa Vista -> Manaus (conecta el cluster norte hacia el interior).
		{
			TEXT("BR-174 Boa Vista Manaus"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-60.67f, 2.82f), FVector2D(-60.20f, 0.00f), FVector2D(-60.02f, -3.10f) },
			6,
			false
		},
		// --- VENEZUELA (todo sobre tierra; sin cruzar lago/mar) ---
		// Troncal central: San Cristobal -> (por el SUR del lago de Maracaibo) ->
		// Barquisimeto -> Valencia -> Maracay -> Caracas. Evita el lago a proposito.
		{
			TEXT("VE central trunk"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-72.23f, 7.77f), FVector2D(-71.30f, 8.55f), FVector2D(-70.40f, 9.05f),
				FVector2D(-69.70f, 9.62f), FVector2D(-69.32f, 10.07f), FVector2D(-68.00f, 10.20f),
				FVector2D(-67.60f, 10.25f), FVector2D(-66.90f, 10.50f)
			},
			8,
			false
		},
		// Ramal a Maracaibo por la RIBERA OESTE del lago (tierra; no cruza el agua).
		{
			TEXT("VE Maracaibo west-bank spur"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-72.23f, 7.77f), FVector2D(-72.45f, 8.55f), FVector2D(-72.30f, 9.40f),
				FVector2D(-72.05f, 10.15f), FVector2D(-71.82f, 10.58f)
			},
			7,
			false
		},
		// Costa oriental: Caracas -> Puerto La Cruz, ligeramente tierra adentro para no
		// meterse al mar (antes el tramo final caia sobre agua).
		{
			TEXT("VE eastern coast road"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-66.90f, 10.50f), FVector2D(-65.70f, 10.18f), FVector2D(-64.95f, 10.05f),
				FVector2D(-64.70f, 10.10f)
			},
			7,
			false
		},
		{
			TEXT("VE Caracas to Orinoco trunk"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-66.90f, 10.50f), FVector2D(-66.66f, 9.82f), FVector2D(-66.42f, 9.18f),
				FVector2D(-66.05f, 8.18f), FVector2D(-64.40f, 8.05f), FVector2D(-62.60f, 8.30f)
			},
			7,
			false
		},
		{
			TEXT("VE llanos logistics road"),
			EWLCampaignRouteType::Rural,
			{
				FVector2D(-72.23f, 7.77f), FVector2D(-70.40f, 7.25f), FVector2D(-68.20f, 7.05f),
				FVector2D(-66.20f, 7.42f), FVector2D(-64.40f, 8.05f), FVector2D(-62.60f, 8.30f)
			},
			6,
			false
		},
		{
			TEXT("VE Cucuta San Cristobal border"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-72.50f, 7.90f), FVector2D(-72.35f, 7.83f), FVector2D(-72.23f, 7.77f) },
			5,
			false
		},
		// Trasandina (Andes): San Cristobal -> Merida -> Valera -> Barquisimeto.
		{
			TEXT("VE Trasandina"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-72.23f, 7.77f), FVector2D(-71.14f, 8.60f), FVector2D(-70.60f, 9.32f), FVector2D(-69.35f, 10.07f) },
			6,
			false
		},
		// Piedemonte llanero (Troncal 5): San Cristobal -> Barinas -> Guanare -> Acarigua -> Barquisimeto.
		{
			TEXT("VE llanos piedmont"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-72.23f, 7.77f), FVector2D(-70.21f, 8.62f), FVector2D(-69.75f, 9.04f), FVector2D(-69.20f, 9.55f), FVector2D(-69.35f, 10.07f) },
			6,
			false
		},
		// Costa de Falcon: Valencia -> Puerto Cabello -> Coro -> Punto Fijo.
		{
			TEXT("VE Falcon coast"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-68.00f, 10.17f), FVector2D(-68.01f, 10.47f), FVector2D(-69.10f, 11.00f), FVector2D(-69.67f, 11.41f), FVector2D(-70.20f, 11.70f) },
			6,
			false
		},
		// Llano central (Troncal 19): Barinas -> San Fernando -> Valle de la Pascua -> El Tigre -> Ciudad Bolivar.
		{
			TEXT("VE central llanos"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-70.21f, 8.62f), FVector2D(-67.47f, 7.90f), FVector2D(-66.00f, 9.21f), FVector2D(-64.25f, 8.89f), FVector2D(-63.55f, 8.13f) },
			6,
			false
		},
		// Apure -> Amazonas: San Fernando -> Puerto Ayacucho.
		{
			TEXT("VE Apure to Amazonas"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-67.47f, 7.90f), FVector2D(-67.55f, 6.70f), FVector2D(-67.62f, 5.66f) },
			5,
			false
		},
		// Caracas -> Valle de la Pascua (conecta la costa con el llano central).
		{
			TEXT("VE Caracas to central llanos"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-66.90f, 10.49f), FVector2D(-66.50f, 9.90f), FVector2D(-66.00f, 9.21f) },
			5,
			false
		},
		// Oriente: Puerto La Cruz -> Maturin -> Ciudad Guayana.
		{
			TEXT("VE Maturin axis"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-64.63f, 10.21f), FVector2D(-63.18f, 9.75f), FVector2D(-62.60f, 8.30f) },
			6,
			false
		},
		// Costa oriental corta: Puerto La Cruz -> Cumana.
		{
			TEXT("VE Cumana coast"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-64.63f, 10.21f), FVector2D(-64.40f, 10.32f), FVector2D(-64.18f, 10.45f) },
			4,
			false
		},
		// Guayana hacia Brasil (Troncal 10): Ciudad Bolivar -> Ciudad Guayana -> Upata -> Tumeremo -> Santa Elena.
		{
			TEXT("VE Guayana to Brazil"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-63.55f, 8.13f), FVector2D(-62.60f, 8.30f), FVector2D(-62.40f, 8.02f), FVector2D(-61.50f, 7.30f), FVector2D(-61.30f, 6.00f), FVector2D(-61.11f, 4.60f) },
			7,
			false
		},
		{
			TEXT("VE-GY Guayana border trail"),
			EWLCampaignRouteType::Rural,
			{ FVector2D(-61.50f, 7.30f), FVector2D(-60.70f, 7.55f), FVector2D(-59.78f, 8.20f), FVector2D(-58.62f, 6.41f) },
			5,
			false
		},
		// Cruce VE<->BR: Santa Elena de Uairen -> Pacaraima -> Boa Vista.
		{
			TEXT("VE-BR Santa Elena Boa Vista crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-61.11f, 4.60f), FVector2D(-61.13f, 4.48f), FVector2D(-60.85f, 3.60f), FVector2D(-60.67f, 2.82f) },
			6,
			false
		}
	};

	for (const FWLCampaignRouteSpec& Route : Routes)
	{
		AddRoute(Buffer, Route, ProjectLonLat);
	}

	AddBridge(Buffer, FVector2D(-74.93f, 8.05f), FVector2D(-74.80f, 8.42f), ProjectLonLat);
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
