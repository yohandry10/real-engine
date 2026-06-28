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
			// ScaleAudit: con GeoScale=9000 esto se leia como una via de 16-21 km.
			// Reducimos solo ancho visual; la polilinea y el drapeado no cambian.
			Style.SurfaceWidth = 820.f;
			Style.ShoulderWidth = 1060.f;
			Style.ZOffset = 990.f;
			Style.SurfaceColor = FLinearColor(0.180f, 0.185f, 0.195f); // asfalto
			Style.ShoulderColor = FLinearColor(0.300f, 0.285f, 0.240f); // grava/tierra
			Style.CenterWearColor = FLinearColor(0.800f, 0.580f, 0.055f); // linea central amarilla
			Style.CenterWearWidth = 46.f;
			break;
		case EWLCampaignRouteType::Secondary:
			Style.SurfaceWidth = 680.f;
			Style.ShoulderWidth = 880.f;
			Style.ZOffset = 945.f;
			Style.SurfaceColor = FLinearColor(0.195f, 0.200f, 0.210f);
			Style.ShoulderColor = FLinearColor(0.310f, 0.292f, 0.245f);
			Style.CenterWearColor = FLinearColor(0.780f, 0.560f, 0.055f);
			Style.CenterWearWidth = 38.f;
			break;
		case EWLCampaignRouteType::Rural:
			// Antes era tierra marron y se confundia ("¿es carretera o que?"). Ahora es
			// asfalto angosto con linea, igual que el resto: todo se lee como carretera.
			Style.SurfaceWidth = 520.f;
			Style.ShoulderWidth = 700.f;
			Style.ZOffset = 920.f;
			Style.SurfaceColor = FLinearColor(0.210f, 0.212f, 0.220f);
			Style.ShoulderColor = FLinearColor(0.320f, 0.300f, 0.250f);
			Style.CenterWearColor = FLinearColor(0.760f, 0.540f, 0.055f);
			Style.CenterWearWidth = 32.f;
			break;
		case EWLCampaignRouteType::PortAccess:
			Style.SurfaceWidth = 620.f;
			Style.ShoulderWidth = 820.f;
			Style.ZOffset = 950.f;
			Style.SurfaceColor = FLinearColor(0.190f, 0.195f, 0.205f);
			Style.ShoulderColor = FLinearColor(0.310f, 0.292f, 0.245f);
			Style.CenterWearColor = FLinearColor(0.780f, 0.560f, 0.055f);
			Style.CenterWearWidth = 34.f;
			break;
		case EWLCampaignRouteType::BorderCrossing:
			Style.SurfaceWidth = 740.f;
			Style.ShoulderWidth = 980.f;
			Style.ZOffset = 1005.f;
			Style.SurfaceColor = FLinearColor(0.180f, 0.185f, 0.195f);
			Style.ShoulderColor = FLinearColor(0.325f, 0.300f, 0.250f);
			Style.CenterWearColor = FLinearColor(0.800f, 0.580f, 0.055f);
			Style.CenterWearWidth = 42.f;
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

	// Subdivide los tramos largos para que la cinta SIGA el relieve de cerca. Sin esto, una
	// arista entre dos ciudades lejanas (costa -> selva cruzando los Andes) es una tira plana
	// que la cresta TAPA en el medio -> el camino se ve "cortado". Densificar lo evita.
	TArray<FVector2D> DensifyLonLat(const TArray<FVector2D>& Points, float MaxSegDeg)
	{
		if (Points.Num() < 2)
		{
			return Points;
		}
		TArray<FVector2D> Result;
		Result.Add(Points[0]);
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			const FVector2D A = Points[Index];
			const FVector2D B = Points[Index + 1];
			const int32 Sub = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / FMath::Max(0.02f, MaxSegDeg)));
			for (int32 S = 1; S <= Sub; ++S)
			{
				Result.Add(FMath::Lerp(A, B, static_cast<float>(S) / static_cast<float>(Sub)));
			}
		}
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

	// Cinta SIMPLE: cuadrilatero por segmento, sin extension/solape. El "overlap" que
	// habia metido el agente paralelo creaba quads superpuestos coplanares -> Z-fighting,
	// que se veia como pista "rota/segmentada". Tiras adyacentes comparten borde = continuo.
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

	// Recorte de carretera. La cinta se RECORTA donde no debe verse: dentro de las huellas de ciudad
	// (la via flota ~900u sobre el piso y la cruzaria) y sobre el AGUA (la via va a Z+990 y el mar a
	// -2350, asi que la ruta costera flotaba sobre el mar). Ambos los fija el view antes de construir.
	TArray<FVector> GRoadClipCircles;             // X=lon, Y=lat, Z=radio en grados
	TFunction<bool(float, float)> GRoadLandTest;  // true = (lon,lat) esta en tierra

	// Un punto se EXCLUYE (no se dibuja la via ahi) si cae dentro de una ciudad o sobre el mar.
	bool RoutePointExcluded(const FVector2D& P)
	{
		for (const FVector& C : GRoadClipCircles)
		{
			if (FVector2D::DistSquared(P, FVector2D(C.X, C.Y)) < C.Z * C.Z)
			{
				return true;
			}
		}
		if (GRoadLandTest && !GRoadLandTest(P.X, P.Y))
		{
			return true;
		}
		return false;
	}

	// Punto de transicion (biseccion) entre un punto incluido y uno excluido -> la via termina justo
	// en el borde (de la ciudad o de la costa).
	FVector2D RouteBoundaryPoint(const FVector2D& Included, const FVector2D& Excluded)
	{
		FVector2D Lo = Included, Hi = Excluded;
		for (int32 It = 0; It < 18; ++It)
		{
			const FVector2D Mid = (Lo + Hi) * 0.5f;
			if (RoutePointExcluded(Mid)) { Hi = Mid; } else { Lo = Mid; }
		}
		return (Lo + Hi) * 0.5f;
	}

	// Divide una polilinea (lon/lat) en sub-polilineas que quedan FUERA de ciudades y SOBRE TIERRA,
	// terminando justo en el borde (biseccion). Sin recortes activos, devuelve la polilinea tal cual.
	TArray<TArray<FVector2D>> ClipRoute(const TArray<FVector2D>& Pts)
	{
		TArray<TArray<FVector2D>> Out;
		if (Pts.Num() < 2)
		{
			return Out;
		}
		if (GRoadClipCircles.Num() == 0 && !GRoadLandTest)
		{
			Out.Add(Pts);
			return Out;
		}
		TArray<FVector2D> Cur;
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			const bool bExcluded = RoutePointExcluded(Pts[i]);
			if (!bExcluded)
			{
				if (Cur.Num() == 0 && i > 0 && RoutePointExcluded(Pts[i - 1]))
				{
					Cur.Add(RouteBoundaryPoint(Pts[i], Pts[i - 1]));
				}
				Cur.Add(Pts[i]);
			}
			else if (Cur.Num() > 0)
			{
				Cur.Add(RouteBoundaryPoint(Pts[i - 1], Pts[i]));
				if (Cur.Num() >= 2) { Out.Add(Cur); }
				Cur.Reset();
			}
		}
		if (Cur.Num() >= 2) { Out.Add(Cur); }
		return Out;
	}

	void AddRoute(FRouteMeshBuffer& Buffer, const FWLCampaignRouteSpec& Spec, TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		if (Spec.Points.Num() < 2)
		{
			return;
		}

		const FRouteStyle Style = StyleFor(Spec.Type);
		// Densificado a ~0.06 grados (~6.5 km) para que la cinta pegue al relieve y la
		// cordillera no "corte" los caminos que la cruzan (Quito<->Sto Domingo, Quito<->Amazonia,
		// costa<->selva en Peru, etc.). Antes 0.12 dejaba la cinta hundida bajo la cresta entre puntos.
		const TArray<FVector2D> Smoothed = DensifyLonLat(SmoothPoints(Spec.Points, Spec.Smoothness), 0.06f);
		// Recorta la cinta en cada huella de ciudad (la via llega al borde y para, no la cruza por
		// encima) y sobre el AGUA (no flota sobre el mar). Sin recortes, una sola sub-polilinea.
		for (const TArray<FVector2D>& Sub : ClipRoute(Smoothed))
		{
			if (Sub.Num() < 2)
			{
				continue;
			}
			const TArray<FVector> WorldPoints = ProjectRoutePoints(Sub, Style.ZOffset, ProjectLonLat);
			AddRouteLayer(Buffer, WorldPoints, Style.ShoulderWidth, -24.f, Style.ShoulderColor);
			AddRouteLayer(Buffer, WorldPoints, Style.SurfaceWidth, 0.f, Style.SurfaceColor);
			if (Style.bHasCenterWear && Style.CenterWearWidth > 1.f)
			{
				AddRouteLayer(Buffer, WorldPoints, Style.CenterWearWidth, 18.f, Style.CenterWearColor);
			}
		}

		if (Spec.bShowJunctions)
		{
			const float Radius = FMath::Max(Style.SurfaceWidth * 0.78f, 260.f);
			for (const FVector2D& Point : Spec.Points)
			{
				if (RoutePointExcluded(Point))
				{
					continue;  // el nodo caeria sobre la ciudad o el mar
				}
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

void FWLCampaignRouteBuilder::SetCityClipCircles(const TArray<FVector>& Circles)
{
	GRoadClipCircles = Circles;
}

void FWLCampaignRouteBuilder::SetRoadLandMask(TFunction<bool(float, float)> LandTest)
{
	GRoadLandTest = MoveTemp(LandTest);
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
		},
		// --- ECUADOR: red vial real, mismo pipeline curado que CO/VE (conecta las 10 ciudades
		//     por corredores reales). El cruce CO-EC (Ipiales-Tulcan) ya esta arriba; el cruce
		//     EC-PE (Loja-Macara / La Tina) lo genera el paso automatico de fronteras.
		// Troncal de la Sierra (Panamericana E35): Tulcan -> Ibarra -> Quito -> Latacunga ->
		// Ambato -> Riobamba -> Azogues -> Cuenca -> Loja. Eje andino, pasa POR las ciudades.
		{
			TEXT("EC Panamericana Sierra E35"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-77.72f, 0.81f), FVector2D(-78.12f, 0.35f), FVector2D(-78.14f, 0.04f),
				FVector2D(-78.45f, -0.15f), FVector2D(-78.52f, -0.22f), FVector2D(-78.57f, -0.51f),
				FVector2D(-78.62f, -0.93f), FVector2D(-78.62f, -1.24f), FVector2D(-78.65f, -1.67f),
				FVector2D(-78.84f, -2.20f), FVector2D(-78.85f, -2.74f), FVector2D(-79.00f, -2.90f),
				FVector2D(-79.15f, -3.45f), FVector2D(-79.20f, -3.99f)
			},
			6,
			false
		},
		// Aloag - Santo Domingo (E20): conecta Quito con la costa noroccidental. Linea oeste
		// monotona (sin la V a Aloag) para cruzar la cordillera occidental de un solo paso y
		// que la cinta no se hunda bajo la cresta (se veia "rota" entre Quito y Sto Domingo).
		{
			TEXT("EC Quito Santo Domingo E20"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-78.52f, -0.22f), FVector2D(-78.72f, -0.26f), FVector2D(-78.95f, -0.27f), FVector2D(-79.17f, -0.25f) },
			5,
			false
		},
		// Troncal de la Costa (E25): Santo Domingo -> Quevedo -> Babahoyo -> Guayaquil.
		{
			TEXT("EC Troncal Costa E25 north"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-79.17f, -0.25f), FVector2D(-79.32f, -0.75f), FVector2D(-79.47f, -1.03f),
				FVector2D(-79.53f, -1.80f), FVector2D(-79.78f, -2.05f), FVector2D(-79.90f, -2.19f)
			},
			6,
			false
		},
		// E25 sur: Guayaquil -> Naranjal -> Machala (litoral de El Oro).
		{
			TEXT("EC Guayaquil Machala E25 south"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-79.90f, -2.19f), FVector2D(-79.78f, -2.45f), FVector2D(-79.62f, -2.67f), FVector2D(-79.83f, -3.05f), FVector2D(-79.97f, -3.26f) },
			5,
			false
		},
		// E40 sierra-costa: Guayaquil -> Molleturo (paso del Cajas) -> Cuenca.
		{
			TEXT("EC Guayaquil Cuenca E40"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-79.90f, -2.19f), FVector2D(-79.62f, -2.50f), FVector2D(-79.40f, -2.78f), FVector2D(-79.18f, -2.88f), FVector2D(-79.00f, -2.90f) },
			5,
			false
		},
		// E30/E15: Santo Domingo -> Chone -> Portoviejo -> Manta (Manabi).
		{
			TEXT("EC Santo Domingo Manta E30"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-79.17f, -0.25f), FVector2D(-79.55f, -0.45f), FVector2D(-80.09f, -0.70f), FVector2D(-80.45f, -1.05f), FVector2D(-80.72f, -0.95f) },
			5,
			false
		},
		// E15 litoral: Manta -> Jipijapa -> Guayaquil.
		{
			TEXT("EC Manta Guayaquil coast E15"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-80.72f, -0.95f), FVector2D(-80.58f, -1.35f), FVector2D(-80.23f, -1.75f), FVector2D(-80.00f, -2.05f), FVector2D(-79.90f, -2.19f) },
			5,
			false
		},
		// E30 central: Riobamba -> Pallatanga -> Bucay -> Guayaquil (rama sierra-costa central).
		{
			TEXT("EC Riobamba Guayaquil E30"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-78.65f, -1.67f), FVector2D(-78.96f, -1.99f), FVector2D(-79.13f, -2.19f), FVector2D(-79.50f, -2.20f), FVector2D(-79.90f, -2.19f) },
			5,
			false
		},
		// E50/E35 sur: Machala -> Pasaje -> Saraguro -> Loja (cierra el anillo costa-sierra sur).
		{
			TEXT("EC Machala Loja E50"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-79.97f, -3.26f), FVector2D(-79.80f, -3.33f), FVector2D(-79.45f, -3.55f), FVector2D(-79.24f, -3.70f), FVector2D(-79.20f, -3.99f) },
			5,
			false
		},
		// Troncal Amazonica (E45/E20): Quito -> Papallacta -> Baeza -> Lumbaqui -> Nueva Loja
		// (Lago Agrio). Conecta la ciudad amazonica que quedaba aislada (sin via).
		{
			TEXT("EC Quito Nueva Loja Amazonia"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-78.52f, -0.22f), FVector2D(-78.14f, -0.37f), FVector2D(-77.82f, -0.40f), FVector2D(-77.30f, -0.05f), FVector2D(-76.89f, 0.09f) },
			5,
			false
		},
		// Panamericana sur (E25): Machala -> Arenillas -> Huaquillas (frontera con Peru). El cruce
		// Huaquillas-Tumbes va curado abajo (EC y PE ya son paises curados).
		{
			TEXT("EC Machala Huaquillas border"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-79.97f, -3.26f), FVector2D(-80.06f, -3.45f), FVector2D(-80.23f, -3.48f) },
			4,
			false
		},
		// ====================== PERU ======================
		// Panamericana (costa): Tumbes -> Piura -> Chiclayo -> Trujillo -> Chimbote -> Lima -> Ica
		// -> Nazca -> Camana -> Arequipa -> Moquegua -> Tacna. Eje costero, pasa POR las ciudades.
		{
			TEXT("PE Panamericana coast"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-80.45f, -3.57f), FVector2D(-80.63f, -5.19f), FVector2D(-79.84f, -6.77f),
				FVector2D(-79.03f, -8.11f), FVector2D(-78.59f, -9.08f), FVector2D(-77.63f, -11.10f),
				FVector2D(-77.04f, -12.05f), FVector2D(-76.20f, -13.42f), FVector2D(-75.73f, -14.07f),
				FVector2D(-74.94f, -14.83f), FVector2D(-73.40f, -15.70f), FVector2D(-72.71f, -16.62f),
				FVector2D(-71.54f, -16.41f), FVector2D(-70.93f, -17.19f), FVector2D(-70.25f, -18.01f)
			},
			6,
			false
		},
		// Carretera Central: Lima -> La Oroya -> Jauja -> Huancayo.
		{
			TEXT("PE Carretera Central Huancayo"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-77.04f, -12.05f), FVector2D(-76.40f, -11.72f), FVector2D(-75.90f, -11.52f), FVector2D(-75.50f, -11.78f), FVector2D(-75.21f, -12.07f) },
			5,
			false
		},
		// Sierra sur: Arequipa -> Juliaca -> Puno (altiplano del Titicaca).
		{
			TEXT("PE Arequipa Juliaca Puno"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-71.54f, -16.41f), FVector2D(-71.00f, -15.90f), FVector2D(-70.13f, -15.49f), FVector2D(-70.02f, -15.84f) },
			5,
			false
		},
		// Sierra (Ruta 3S): Juliaca -> Sicuani -> Cusco.
		{
			TEXT("PE Juliaca Cusco"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-70.13f, -15.49f), FVector2D(-70.85f, -14.60f), FVector2D(-71.97f, -13.53f) },
			5,
			false
		},
		// Interoceanica Sur: Cusco -> Puerto Maldonado (Amazonia sur, hacia Bolivia/Brasil).
		{
			TEXT("PE Interoceanica Cusco Maldonado"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-71.97f, -13.53f), FVector2D(-71.20f, -13.20f), FVector2D(-70.20f, -12.80f), FVector2D(-69.18f, -12.59f) },
			5,
			false
		},
		// Cusco -> Abancay -> Puquio -> Nazca: enlaza la sierra sur con la Panamericana (en el punto Nazca).
		{
			TEXT("PE Cusco Abancay Nazca"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-71.97f, -13.53f), FVector2D(-72.88f, -13.63f), FVector2D(-73.80f, -14.30f), FVector2D(-74.94f, -14.83f) },
			5,
			false
		},
		// Selva central: Huancayo -> Cerro de Pasco -> Huanuco -> Tingo Maria -> Pucallpa.
		{
			TEXT("PE Huancayo Pucallpa"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-75.21f, -12.07f), FVector2D(-76.26f, -10.68f), FVector2D(-76.24f, -9.93f), FVector2D(-75.99f, -9.29f), FVector2D(-74.55f, -8.38f) },
			5,
			false
		},
		// IIRSA Norte: Iquitos -> Yurimaguas -> Tarapoto -> Moyobamba -> Bagua -> Chiclayo. Conecta
		// Iquitos (real fluvial) con la costa por el corredor norte para que no quede aislada.
		{
			TEXT("PE IIRSA Norte Iquitos Chiclayo"),
			EWLCampaignRouteType::Rural,
			{
				FVector2D(-73.25f, -3.75f), FVector2D(-75.20f, -5.10f), FVector2D(-76.11f, -5.90f),
				FVector2D(-76.37f, -6.49f), FVector2D(-76.97f, -6.03f), FVector2D(-78.00f, -6.00f),
				FVector2D(-78.53f, -5.66f), FVector2D(-79.84f, -6.77f)
			},
			5,
			false
		},
		// ====================== CHILE ======================
		// Panamericana Ruta 5 Norte: Arica -> Iquique -> Antofagasta -> Copiapo -> La Serena -> Santiago.
		{
			TEXT("CL Ruta 5 north"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-70.31f, -18.48f), FVector2D(-70.13f, -20.22f), FVector2D(-70.05f, -21.40f),
				FVector2D(-70.40f, -23.65f), FVector2D(-70.40f, -25.40f), FVector2D(-70.33f, -27.37f),
				FVector2D(-71.00f, -28.80f), FVector2D(-71.25f, -29.90f), FVector2D(-71.50f, -31.90f),
				FVector2D(-70.95f, -32.85f), FVector2D(-70.65f, -33.45f)
			},
			6,
			false
		},
		// Spur al puerto de Valparaiso.
		{
			TEXT("CL Santiago Valparaiso"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-70.65f, -33.45f), FVector2D(-71.20f, -33.20f), FVector2D(-71.62f, -33.05f) },
			4,
			false
		},
		// Panamericana Ruta 5 Sur: Santiago -> Talca -> Chillan -> Concepcion -> Temuco -> Valdivia -> Puerto Montt.
		{
			TEXT("CL Ruta 5 south"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-70.65f, -33.45f), FVector2D(-71.00f, -34.20f), FVector2D(-71.67f, -35.43f),
				FVector2D(-72.10f, -36.61f), FVector2D(-73.05f, -36.83f), FVector2D(-72.59f, -38.74f),
				FVector2D(-73.25f, -39.81f), FVector2D(-73.10f, -40.60f), FVector2D(-72.94f, -41.47f)
			},
			6,
			false
		},
		// Carretera Austral / Patagonia: Puerto Montt -> Coyhaique -> Puerto Natales -> Punta Arenas.
		// Trazada por el interior (mainland, dipping a la Patagonia argentina donde no hay tierra
		// chilena continua) para no caer en los fiordos; la mascara deja pasar cualquier tierra.
		{
			TEXT("CL Austral Patagonia"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-72.94f, -41.47f), FVector2D(-72.40f, -43.50f), FVector2D(-72.07f, -45.57f),
				FVector2D(-71.80f, -48.50f), FVector2D(-72.00f, -50.80f), FVector2D(-72.51f, -51.73f),
				FVector2D(-70.92f, -53.16f)
			},
			5,
			false
		},
		// ============ CRUCES FRONTERIZOS CURADOS (entre paises ya curados) ============
		// EC<->PE costero: Huaquillas -> Tumbes (Panamericana, Aguas Verdes).
		{
			TEXT("EC-PE Huaquillas Tumbes crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-80.23f, -3.48f), FVector2D(-80.34f, -3.50f), FVector2D(-80.45f, -3.57f) },
			4,
			false
		},
		// PE<->CL: Tacna -> Arica (Panamericana, hito de la Concordia).
		{
			TEXT("PE-CL Tacna Arica crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-70.25f, -18.01f), FVector2D(-70.28f, -18.25f), FVector2D(-70.31f, -18.48f) },
			4,
			false
		},
		// ====================== ARGENTINA ======================
		// RN9/RN34 NO: Buenos Aires -> Rosario -> Cordoba -> Tucuman -> Salta -> La Quiaca (a Bolivia).
		{
			TEXT("AR RN9 northwest spine"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-58.38f, -34.60f), FVector2D(-60.64f, -32.95f), FVector2D(-64.18f, -31.42f),
				FVector2D(-64.80f, -29.00f), FVector2D(-65.22f, -26.82f), FVector2D(-65.41f, -24.79f),
				FVector2D(-65.50f, -23.60f), FVector2D(-65.59f, -22.10f)
			},
			6,
			false
		},
		// RN7 Cuyo: Cordoba -> Rio Cuarto -> San Luis -> Mendoza (hacia Chile).
		{
			TEXT("AR RN7 Cuyo"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-64.18f, -31.42f), FVector2D(-64.35f, -33.13f), FVector2D(-66.34f, -33.30f), FVector2D(-68.84f, -32.89f) },
			5,
			false
		},
		// RN3 Atlantica/Patagonia: Buenos Aires -> Bahia Blanca -> Comodoro -> Rio Gallegos -> Ushuaia.
		{
			TEXT("AR RN3 Patagonia"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-58.38f, -34.60f), FVector2D(-62.27f, -38.72f), FVector2D(-65.10f, -43.30f),
				FVector2D(-67.48f, -45.86f), FVector2D(-68.50f, -49.00f), FVector2D(-69.22f, -51.62f),
				FVector2D(-68.80f, -53.00f), FVector2D(-68.30f, -54.80f)
			},
			6,
			false
		},
		// Spur a Mar del Plata.
		{
			TEXT("AR Mar del Plata spur"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-58.38f, -34.60f), FVector2D(-57.80f, -37.00f), FVector2D(-57.55f, -38.00f) },
			4,
			false
		},
		// Mesopotamia (RN12/14): Buenos Aires -> Concordia -> Posadas -> Puerto Iguazu.
		{
			TEXT("AR Mesopotamia"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-58.38f, -34.60f), FVector2D(-58.00f, -31.40f), FVector2D(-55.90f, -27.37f), FVector2D(-54.58f, -25.60f) },
			5,
			false
		},
		// RN40 Andina: Mendoza -> Neuquen -> Bariloche.
		{
			TEXT("AR RN40 Andean"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-68.84f, -32.89f), FVector2D(-68.06f, -38.95f), FVector2D(-71.31f, -41.13f) },
			5,
			false
		},
		// ====================== URUGUAY ======================
		// Costa (Interbalnearia): Colonia -> Montevideo -> Punta del Este -> Chuy.
		{
			TEXT("UY coast Interbalnearia"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-57.84f, -34.47f), FVector2D(-56.16f, -34.90f), FVector2D(-54.95f, -34.97f), FVector2D(-53.90f, -34.00f), FVector2D(-53.46f, -33.69f) },
			5,
			false
		},
		// Ruta 5 interior: Montevideo -> Rivera (a Brasil).
		{
			TEXT("UY Ruta 5 Rivera"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-56.16f, -34.90f), FVector2D(-56.00f, -32.40f), FVector2D(-55.55f, -30.90f) },
			5,
			false
		},
		// Ruta 3 NO: Montevideo -> Fray Bentos -> Salto -> Artigas.
		{
			TEXT("UY Ruta 3 northwest"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-56.16f, -34.90f), FVector2D(-57.00f, -34.00f), FVector2D(-58.30f, -33.12f), FVector2D(-57.96f, -31.38f), FVector2D(-56.47f, -30.40f) },
			5,
			false
		},
		// Ruta 8: Montevideo -> Melo.
		{
			TEXT("UY Ruta 8 Melo"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-56.16f, -34.90f), FVector2D(-55.20f, -33.70f), FVector2D(-54.18f, -32.37f) },
			4,
			false
		},
		// ====================== BRASIL ======================
		// Litoral Atlantico (BR-101/116): Belem -> Sao Luis -> Fortaleza -> Natal -> Recife -> Salvador
		// -> Vitoria -> Rio -> Sao Paulo -> Curitiba -> Florianopolis -> Porto Alegre.
		{
			TEXT("BR Atlantic coast"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-48.50f, -1.46f), FVector2D(-44.30f, -2.53f), FVector2D(-38.54f, -3.73f),
				FVector2D(-35.21f, -5.79f), FVector2D(-34.88f, -8.05f), FVector2D(-38.51f, -12.97f),
				FVector2D(-40.34f, -20.32f), FVector2D(-43.20f, -22.91f), FVector2D(-46.63f, -23.55f),
				FVector2D(-49.27f, -25.43f), FVector2D(-48.55f, -27.59f), FVector2D(-51.23f, -30.03f)
			},
			7,
			false
		},
		// Interior SE: Sao Paulo -> Belo Horizonte -> Brasilia -> Goiania.
		{
			TEXT("BR interior southeast"),
			EWLCampaignRouteType::Primary,
			{ FVector2D(-46.63f, -23.55f), FVector2D(-43.94f, -19.92f), FVector2D(-47.88f, -15.79f), FVector2D(-49.25f, -16.68f) },
			5,
			false
		},
		// NE interior (BR-020/226): Brasilia -> Teresina.
		{
			TEXT("BR Brasilia Teresina"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-47.88f, -15.79f), FVector2D(-46.00f, -11.00f), FVector2D(-44.00f, -7.00f), FVector2D(-42.80f, -5.09f) },
			5,
			false
		},
		// Belem-Brasilia (BR-153): corredor interior norte-sur.
		{
			TEXT("BR Belem Brasilia"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-48.50f, -1.46f), FVector2D(-48.00f, -7.00f), FVector2D(-48.50f, -11.00f), FVector2D(-47.88f, -15.79f) },
			5,
			false
		},
		// BR-364 Centro-Oeste/Amazonia: Sao Paulo -> Campo Grande -> Cuiaba -> Porto Velho -> Rio Branco -> Brasileia.
		{
			TEXT("BR BR-364 west"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-46.63f, -23.55f), FVector2D(-54.62f, -20.45f), FVector2D(-56.10f, -15.60f),
				FVector2D(-63.90f, -8.76f), FVector2D(-67.81f, -9.97f), FVector2D(-68.75f, -11.01f)
			},
			6,
			false
		},
		// Spur a Corumba (Pantanal, a Bolivia).
		{
			TEXT("BR Corumba spur"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-54.62f, -20.45f), FVector2D(-56.00f, -19.50f), FVector2D(-57.65f, -19.01f) },
			4,
			false
		},
		// Amazonia Norte (BR-319/174): Porto Velho -> Manaus -> Boa Vista -> Pacaraima (a Venezuela).
		{
			TEXT("BR Amazon north"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-63.90f, -8.76f), FVector2D(-60.02f, -3.10f), FVector2D(-60.30f, 0.00f), FVector2D(-60.67f, 2.82f), FVector2D(-61.15f, 4.48f) },
			6,
			false
		},
		// Amazonia Oeste: Manaus -> Tabatinga (frontera tripartita; real fluvial).
		{
			TEXT("BR Manaus Tabatinga"),
			EWLCampaignRouteType::Rural,
			{ FVector2D(-60.02f, -3.10f), FVector2D(-65.00f, -4.00f), FVector2D(-69.94f, -4.25f) },
			4,
			false
		},
		// Amapa: Belem -> Macapa -> Oiapoque (norte, a la Guayana Francesa).
		{
			TEXT("BR Amapa north"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-48.50f, -1.46f), FVector2D(-51.06f, 0.03f), FVector2D(-51.50f, 2.00f), FVector2D(-51.83f, 3.84f) },
			5,
			false
		},
		// BR-163: Cuiaba -> Santarem (corredor de la soja).
		{
			TEXT("BR BR-163 Santarem"),
			EWLCampaignRouteType::Rural,
			{ FVector2D(-56.10f, -15.60f), FVector2D(-55.00f, -8.00f), FVector2D(-54.70f, -2.44f) },
			5,
			false
		},
		// Sur (BR-277): Curitiba -> Foz do Iguacu.
		{
			TEXT("BR Curitiba Foz"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-49.27f, -25.43f), FVector2D(-52.00f, -25.50f), FVector2D(-54.59f, -25.54f) },
			4,
			false
		},
		// Sur (BR-290): Porto Alegre -> Uruguaiana (a Argentina).
		{
			TEXT("BR Porto Alegre Uruguaiana"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-51.23f, -30.03f), FVector2D(-54.00f, -29.80f), FVector2D(-57.09f, -29.75f) },
			4,
			false
		},
		// Sur (BR-158): Porto Alegre -> Santana do Livramento (a Uruguay/Rivera).
		{
			TEXT("BR Porto Alegre Santana"),
			EWLCampaignRouteType::Secondary,
			{ FVector2D(-51.23f, -30.03f), FVector2D(-53.00f, -30.50f), FVector2D(-55.53f, -30.89f) },
			4,
			false
		},
		// ============ CRUCES FRONTERIZOS CURADOS (cono sur, entre paises curados) ============
		// CL<->AR: Santiago -> Mendoza (Cristo Redentor / Los Libertadores).
		{
			TEXT("CL-AR Santiago Mendoza crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-70.65f, -33.45f), FVector2D(-69.80f, -33.00f), FVector2D(-68.84f, -32.89f) },
			4,
			false
		},
		// CL<->AR sur: Puerto Montt -> Bariloche (paso Cardenal Samore).
		{
			TEXT("CL-AR Puerto Montt Bariloche crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-72.94f, -41.47f), FVector2D(-72.10f, -41.30f), FVector2D(-71.31f, -41.13f) },
			4,
			false
		},
		// AR<->UY: Buenos Aires -> Fray Bentos (puente Gualeguaychu, por tierra del lado argentino).
		{
			TEXT("AR-UY Buenos Aires Fray Bentos crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-58.38f, -34.60f), FVector2D(-58.60f, -33.50f), FVector2D(-58.30f, -33.12f) },
			4,
			false
		},
		// AR<->BR: Puerto Iguazu -> Foz do Iguacu (Triple Frontera).
		{
			TEXT("AR-BR Iguazu crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-54.58f, -25.60f), FVector2D(-54.59f, -25.54f) },
			3,
			false
		},
		// UY<->BR: Rivera -> Santana do Livramento (frontera seca, ciudades gemelas).
		{
			TEXT("UY-BR Rivera Santana crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-55.55f, -30.90f), FVector2D(-55.53f, -30.89f) },
			3,
			false
		},
		// PE<->BR: Puerto Maldonado -> Brasileia (Interoceanica, Inapari/Assis Brasil).
		{
			TEXT("PE-BR Maldonado Brasileia crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-69.18f, -12.59f), FVector2D(-68.95f, -11.80f), FVector2D(-68.75f, -11.01f) },
			4,
			false
		},
		// CO<->BR: Leticia -> Tabatinga (frontera amazonica, ciudades gemelas).
		{
			TEXT("CO-BR Leticia Tabatinga crossing"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-69.94f, -4.22f), FVector2D(-69.94f, -4.25f) },
			3,
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
