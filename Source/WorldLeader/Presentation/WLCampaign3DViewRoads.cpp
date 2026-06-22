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
	FVector ScaleToBounds(UStaticMesh* Mesh, float TargetX, float TargetY, float TargetZ)
	{
		if (!Mesh)
		{
			return FVector::OneVector;
		}

		const FVector Ext = Mesh->GetBounds().BoxExtent;
		return FVector(
			TargetX / FMath::Max(1.f, Ext.X * 2.f),
			TargetY / FMath::Max(1.f, Ext.Y * 2.f),
			TargetZ / FMath::Max(1.f, Ext.Z * 2.f));
	}

	FTransform MakeBottomAnchoredTransform(
		UStaticMesh* Mesh,
		const FVector& DesiredBottomCenter,
		const FRotator& Rotation,
		const FVector& Scale)
	{
		if (!Mesh)
		{
			return FTransform(Rotation, DesiredBottomCenter, Scale);
		}

		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector LocalBottomCenter(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		const FVector Location = DesiredBottomCenter - Rotation.RotateVector(LocalBottomCenter * Scale);
		return FTransform(Rotation, Location, Scale);
	}

	struct FVisualRoadStyle
	{
		float Width = 760.f;
		float ZOffset = 1210.f;
		float SegmentLength = 1850.f;
	};

	FVector2D RoadCatmullRom2D(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;
		return 0.5f * (
			(2.f * P1)
			+ (-P0 + P2) * T
			+ (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2
			+ (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
	}

	TArray<FVector2D> SmoothRoadLonLatPoints(const TArray<FVector2D>& Points, int32 Smoothness)
	{
		if (Points.Num() < 2)
		{
			return Points;
		}

		TArray<FVector2D> Result;
		const int32 Steps = FMath::Clamp(Smoothness, 3, 10);
		Result.Reserve((Points.Num() - 1) * Steps + 1);
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			const FVector2D& P0 = Points[Index > 0 ? Index - 1 : Index];
			const FVector2D& P1 = Points[Index];
			const FVector2D& P2 = Points[Index + 1];
			const FVector2D& P3 = Points[Index + 2 < Points.Num() ? Index + 2 : Index + 1];
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Result.Add(RoadCatmullRom2D(P0, P1, P2, P3, static_cast<float>(Step) / static_cast<float>(Steps)));
			}
		}
		Result.Add(Points.Last());
		return Result;
	}

	FVisualRoadStyle VisualRoadStyleFor(EWLCampaignRouteType Type)
	{
		FVisualRoadStyle Style;
		switch (Type)
		{
		// Anchos ensanchados: es una autopista por la que pasan tropas/tanques, no un
		// sendero. La carretera de asset (Venezuela) es ahora la unica via del teatro VE
		// (la cinta procedural sobre VE se removio para no duplicar). Segmentos mas
		// cortos => menos huecos al curvar al ser mas ancha.
		case EWLCampaignRouteType::Primary:
			Style.Width = 1600.f;
			Style.ZOffset = 1065.f;
			Style.SegmentLength = 1300.f;
			break;
		case EWLCampaignRouteType::Secondary:
			Style.Width = 1200.f;
			Style.ZOffset = 1045.f;
			Style.SegmentLength = 1300.f;
			break;
		case EWLCampaignRouteType::Rural:
			Style.Width = 860.f;
			Style.ZOffset = 980.f;
			Style.SegmentLength = 1500.f;
			break;
		case EWLCampaignRouteType::PortAccess:
			Style.Width = 1080.f;
			Style.ZOffset = 1075.f;
			Style.SegmentLength = 1150.f;
			break;
		case EWLCampaignRouteType::BorderCrossing:
			Style.Width = 1400.f;
			Style.ZOffset = 1085.f;
			Style.SegmentLength = 1200.f;
			break;
		default:
			break;
		}
		return Style;
	}

#include "Presentation/WLCampaign3DViewSouthAmericaRoadCorridors.inl"
}

void AWLCampaign3DView::BuildIntercityRoads()
{
	// Agrupa las ciudades por pais (derivando lon/lat de su posicion proyectada) y
	// las conecta con una red minima (arbol de expansion, Prim). CO/VE ya tienen
	// rutas curadas, asi que se omiten. Da caminos a todo el continente sin datos a mano.
	struct FRoadCity
	{
		float Lon = 0.f;
		float Lat = 0.f;
		bool bCapital = false;
	};

	// Incluimos TODOS los paises (tambien CO/VE) como endpoints para los cruces
	// fronterizos; la red INTERNA de CO/VE ya la dibujan las rutas curadas del teatro, asi
	// que a esos no les construimos MST interno (solo participan en cruces).
	TMap<FString, TArray<FRoadCity>> ByCountry;
	for (const FWLCampaign3DCityView& City : CityViews)
	{
		if (City.CountryIso.IsEmpty())
		{
			continue;
		}
		FRoadCity RC;
		RC.Lon = TheaterCenterLonLat.X + City.WorldLocation.Y / GeoScale;
		RC.Lat = TheaterCenterLonLat.Y + City.WorldLocation.X / GeoScale;
		RC.bCapital = City.bCapital;
		ByCountry.FindOrAdd(City.CountryIso).Add(RC);
	}

	// Tierra (poligonos nacionales reales): para no trazar cruces fronterizos sobre el mar.
	auto IsLand = [this](float L, float T) -> bool
	{
		for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
		{
			if (FWLCampaignRegionGeometry::PointInAnyLonLatRing(FVector2D(L, T), Country.Rings))
			{
				return true;
			}
		}
		return false;
	};
	// La recta entre dos ciudades fronterizas debe ir por tierra (muestreo interior).
	auto EdgeOnLand = [&IsLand](const FRoadCity& A, const FRoadCity& B) -> bool
	{
		const int32 Samples = 6;
		for (int32 S = 1; S < Samples; ++S)
		{
			const float T = static_cast<float>(S) / static_cast<float>(Samples);
			if (!IsLand(FMath::Lerp(A.Lon, B.Lon, T), FMath::Lerp(A.Lat, B.Lat, T)))
			{
				return false;
			}
		}
		return true;
	};
	auto IsTheaterCore = [](const FString& Iso) -> bool
	{
		return Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase) || Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase);
	};

	const float MaxEdgeDeg = 22.0f; // asegura continuidad interna en paises grandes (CA/US) sin afectar cruces internacionales
	TArray<FWLCampaignRouteSpec> Network;
	for (const TPair<FString, TArray<FRoadCity>>& Pair : ByCountry)
	{
		// CO/VE: red interna ya curada -> no MST aqui (si participan en cruces fronterizos).
		if (IsTheaterCore(Pair.Key))
		{
			continue;
		}
		const TArray<FRoadCity>& Cities = Pair.Value;
		const int32 N = Cities.Num();
		if (N < 2)
		{
			continue;
		}

		TSet<FString> CountryRoadKeys;
		auto MakeCountryRoadKey = [](int32 A, int32 B)
		{
			const int32 MinIndex = FMath::Min(A, B);
			const int32 MaxIndex = FMath::Max(A, B);
			return FString::Printf(TEXT("%d-%d"), MinIndex, MaxIndex);
		};
		auto AddCountryRoad = [&Network, &Pair, &Cities, &CountryRoadKeys, &MakeCountryRoadKey](int32 From, int32 To, EWLCampaignRouteType Type)
		{
			if (From < 0 || To < 0 || From == To)
			{
				return;
			}
			const FString EdgeKey = MakeCountryRoadKey(From, To);
			if (CountryRoadKeys.Contains(EdgeKey))
			{
				return;
			}
			CountryRoadKeys.Add(EdgeKey);

			const FRoadCity& A = Cities[From];
			const FRoadCity& B = Cities[To];
			FWLCampaignRouteSpec Spec;
			Spec.Name = Pair.Key + TEXT("-intercity");
			Spec.Type = Type;
			Spec.Points = { FVector2D(A.Lon, A.Lat), FVector2D(B.Lon, B.Lat) };
			Spec.Smoothness = 2;
			Spec.bShowJunctions = false;
			Network.Add(Spec);
		};

		// Prim: arbol de expansion minima sobre la distancia lon/lat.
		TArray<bool> InTree;   InTree.Init(false, N);
		TArray<float> BestDist; BestDist.Init(TNumericLimits<float>::Max(), N);
		TArray<int32> BestFrom; BestFrom.Init(-1, N);
		InTree[0] = true;
		for (int32 J = 1; J < N; ++J)
		{
			BestDist[J] = FVector2D::Distance(FVector2D(Cities[0].Lon, Cities[0].Lat), FVector2D(Cities[J].Lon, Cities[J].Lat));
			BestFrom[J] = 0;
		}

		for (int32 Added = 1; Added < N; ++Added)
		{
			int32 Next = -1;
			float NextDist = TNumericLimits<float>::Max();
			for (int32 J = 0; J < N; ++J)
			{
				if (!InTree[J] && BestDist[J] < NextDist)
				{
					NextDist = BestDist[J];
					Next = J;
				}
			}
			if (Next < 0)
			{
				break;
			}
			InTree[Next] = true;

			const int32 From = BestFrom[Next];
			if (From >= 0 && NextDist <= MaxEdgeDeg)
			{
				const FRoadCity& A = Cities[From];
				const FRoadCity& B = Cities[Next];
				AddCountryRoad(From, Next, (A.bCapital || B.bCapital) ? EWLCampaignRouteType::Primary : EWLCampaignRouteType::Secondary);
			}

			for (int32 J = 0; J < N; ++J)
			{
				if (!InTree[J])
				{
					const float D = FVector2D::Distance(FVector2D(Cities[Next].Lon, Cities[Next].Lat), FVector2D(Cities[J].Lon, Cities[J].Lat));
					if (D < BestDist[J])
					{
						BestDist[J] = D;
						BestFrom[J] = Next;
					}
				}
			}
		}

		const int32 LocalLinksPerCity = N >= 10 ? 2 : 1;
		const float LocalCapDeg = N >= 14 ? 8.0f : 6.0f;
		for (int32 I = 0; I < N; ++I)
		{
			TArray<TPair<float, int32>> Nearest;
			for (int32 J = 0; J < N; ++J)
			{
				if (I == J)
				{
					continue;
				}
				const float D = FVector2D::Distance(FVector2D(Cities[I].Lon, Cities[I].Lat), FVector2D(Cities[J].Lon, Cities[J].Lat));
				if (D <= LocalCapDeg)
				{
					Nearest.Add(TPair<float, int32>(D, J));
				}
			}
			Nearest.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
			{
				return A.Key < B.Key;
			});

			for (int32 Index = 0; Index < Nearest.Num() && Index < LocalLinksPerCity; ++Index)
			{
				AddCountryRoad(I, Nearest[Index].Value, EWLCampaignRouteType::Secondary);
			}
		}
	}

	// --- Cruces fronterizos: conecta cada par de paises por su par de ciudades mas
	//     cercano (tope de distancia), si la recta va por TIERRA. Da interconexion real
	//     (autopistas internacionales) sin trazar sobre el mar; el tope deja fuera huecos
	//     sin carretera real (p.ej. el tapon del Darien CO-Panama). CO-VE ya esta curado.
	{
		const float CrossBorderCap = 4.5f; // cubre pasos fronterizos largos de Amazonia/Andes
		TArray<FString> Keys;
		ByCountry.GetKeys(Keys);
		for (int32 ia = 0; ia < Keys.Num(); ++ia)
		{
			for (int32 ib = ia + 1; ib < Keys.Num(); ++ib)
			{
				if (IsTheaterCore(Keys[ia]) && IsTheaterCore(Keys[ib]))
				{
					continue; // par CO-VE: ya curado por la ruta de frontera del teatro
				}
				const TArray<FRoadCity>& CA = ByCountry[Keys[ia]];
				const TArray<FRoadCity>& CB = ByCountry[Keys[ib]];
				int32 BestA = -1;
				int32 BestB = -1;
				float BestPairDist = CrossBorderCap;
				for (int32 i = 0; i < CA.Num(); ++i)
				{
					for (int32 j = 0; j < CB.Num(); ++j)
					{
						const float D = FVector2D::Distance(
							FVector2D(CA[i].Lon, CA[i].Lat), FVector2D(CB[j].Lon, CB[j].Lat));
						if (D < BestPairDist)
						{
							BestPairDist = D;
							BestA = i;
							BestB = j;
						}
					}
				}
				if (BestA >= 0 && BestB >= 0 && EdgeOnLand(CA[BestA], CB[BestB]))
				{
					FWLCampaignRouteSpec Spec;
					Spec.Name = Keys[ia] + TEXT("-") + Keys[ib] + TEXT("-border");
					Spec.Type = EWLCampaignRouteType::Primary;
					Spec.Points = {
						FVector2D(CA[BestA].Lon, CA[BestA].Lat),
						FVector2D(CB[BestB].Lon, CB[BestB].Lat) };
					Spec.Smoothness = 4;
					Spec.bShowJunctions = false;
					Network.Add(Spec);
				}
			}
		}
	}

	AppendSouthAmericaRoadCorridors(Network);

	if (Network.Num() > 0)
	{
		FWLCampaignRouteBuilder::AppendRoutes(
			RoadMesh,
			VertexColorMaterial,
			Network,
			[this](float Lon, float Lat)
			{
				return ProjectLonLat(Lon, Lat);
			});
	}

	UE_LOG(
		LogWorldLeader,
		Log,
		TEXT("Campaign3D intercity road layer complete. Countries=%d Cities=%d GeneratedRoutes=%d."),
		ByCountry.Num(),
		CityViews.Num(),
		Network.Num());
}

void AWLCampaign3DView::BuildVenezuelaRoadAssetLayer()
{
	if (RoadAssetMeshes.Num() == 0)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("Campaign3D road asset layer skipped: Cartoon_City_Free road meshes not found."));
		return;
	}

	const TArray<FWLCampaignRouteSpec> Routes = {
		{
			TEXT("VE high-detail western mountain corridor"),
			EWLCampaignRouteType::Primary,
			{
				FVector2D(-72.23f, 7.77f), FVector2D(-72.05f, 8.72f), FVector2D(-71.86f, 9.54f),
				FVector2D(-71.60f, 10.60f), FVector2D(-70.42f, 10.36f), FVector2D(-69.32f, 10.07f),
				FVector2D(-68.00f, 10.20f), FVector2D(-66.90f, 10.50f)
			},
			8,
			true
		},
		{
			TEXT("VE high-detail coastal spine"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-68.00f, 10.20f), FVector2D(-67.58f, 10.28f), FVector2D(-66.90f, 10.50f),
				FVector2D(-65.72f, 10.34f), FVector2D(-64.70f, 10.10f), FVector2D(-64.63f, 10.21f)
			},
			8,
			true
		},
		{
			TEXT("VE high-detail Caracas to Orinoco trunk"),
			EWLCampaignRouteType::Secondary,
			{
				FVector2D(-66.90f, 10.50f), FVector2D(-66.66f, 9.82f), FVector2D(-66.42f, 9.18f),
				FVector2D(-66.05f, 8.18f), FVector2D(-64.40f, 8.05f), FVector2D(-62.60f, 8.30f)
			},
			7,
			true
		},
		{
			TEXT("VE high-detail llanos logistics road"),
			EWLCampaignRouteType::Rural,
			{
				FVector2D(-72.23f, 7.77f), FVector2D(-70.40f, 7.25f), FVector2D(-68.20f, 7.05f),
				FVector2D(-66.20f, 7.42f), FVector2D(-64.40f, 8.05f), FVector2D(-62.60f, 8.30f)
			},
			6,
			false
		},
		{
			TEXT("VE Maracaibo port access asset"),
			EWLCampaignRouteType::PortAccess,
			{ FVector2D(-71.60f, 10.60f), FVector2D(-71.70f, 10.82f), FVector2D(-71.82f, 11.02f) },
			5,
			true
		},
		{
			TEXT("VE Puerto La Cruz port access asset"),
			EWLCampaignRouteType::PortAccess,
			{ FVector2D(-64.70f, 10.10f), FVector2D(-64.65f, 10.23f), FVector2D(-64.58f, 10.38f) },
			5,
			true
		},
		{
			TEXT("VE Cucuta San Cristobal border asset"),
			EWLCampaignRouteType::BorderCrossing,
			{ FVector2D(-72.50f, 7.90f), FVector2D(-72.35f, 7.83f), FVector2D(-72.23f, 7.77f) },
			5,
			true
		}
	};

	int32 SegmentIndex = 0;
	for (const FWLCampaignRouteSpec& Route : Routes)
	{
		AddRoadAssetRoute(Route, SegmentIndex);
	}

	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D road asset layer complete: components=%d meshes=%d."), RoadAssetComponents.Num(), RoadAssetMeshes.Num());
}

void AWLCampaign3DView::AddRoadAssetRoute(const FWLCampaignRouteSpec& Spec, int32& InOutSegmentIndex)
{
	if (RoadAssetMeshes.Num() == 0 || Spec.Points.Num() < 2)
	{
		return;
	}

	const FVisualRoadStyle Style = VisualRoadStyleFor(Spec.Type);
	const TArray<FVector2D> Smoothed = SmoothRoadLonLatPoints(Spec.Points, Spec.Smoothness);
	for (int32 Index = 0; Index + 1 < Smoothed.Num(); ++Index)
	{
		const FVector2D LonLatA = Smoothed[Index];
		const FVector2D LonLatB = Smoothed[Index + 1];
		const FVector WorldA = ProjectLonLat(LonLatA.X, LonLatA.Y);
		const FVector WorldB = ProjectLonLat(LonLatB.X, LonLatB.Y);
		const float Distance = FVector::Dist2D(WorldA, WorldB);
		const int32 Pieces = FMath::Max(1, FMath::CeilToInt(Distance / FMath::Max(300.f, Style.SegmentLength)));
		for (int32 Piece = 0; Piece < Pieces; ++Piece)
		{
			const float T0 = static_cast<float>(Piece) / static_cast<float>(Pieces);
			const float T1 = static_cast<float>(Piece + 1) / static_cast<float>(Pieces);
			const FVector2D PieceLonLatA = FMath::Lerp(LonLatA, LonLatB, T0);
			const FVector2D PieceLonLatB = FMath::Lerp(LonLatA, LonLatB, T1);
			const FVector Start = ProjectLonLat(PieceLonLatA.X, PieceLonLatA.Y) + FVector(0.f, 0.f, Style.ZOffset);
			const FVector End = ProjectLonLat(PieceLonLatB.X, PieceLonLatB.Y) + FVector(0.f, 0.f, Style.ZOffset);
			const FVector Delta = End - Start;
			const float PieceLength = Delta.Size2D();
			if (PieceLength < 120.f)
			{
				continue;
			}

			// Use the straight tile for intercity roads. Mixing road-pack corner and
			// junction tiles on long routes reads as broken/dashed geometry at map scale.
			UStaticMesh* Mesh = RoadAssetMeshes[0];
			if (!Mesh)
			{
				continue;
			}

			UStaticMeshComponent* Road = NewObject<UStaticMeshComponent>(this);
			if (!Road)
			{
				continue;
			}
			Road->SetupAttachment(SceneRoot);
			Road->RegisterComponent();
			Road->SetMobility(EComponentMobility::Movable);
			Road->SetStaticMesh(Mesh);
			Road->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Road->SetCastShadow(false);

			const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
			const bool bMeshForwardIsY = MeshBounds.BoxExtent.Y > MeshBounds.BoxExtent.X * 1.15f;
			const FVector Mid = (Start + End) * 0.5f;
			const float RouteYaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			const FRotator Rotation(0.f, RouteYaw + (bMeshForwardIsY ? -90.f : 0.f), 0.f);
			const FVector Scale = bMeshForwardIsY
				? ScaleToBounds(Mesh, Style.Width, PieceLength * 1.22f, 72.f)
				: ScaleToBounds(Mesh, PieceLength * 1.22f, Style.Width, 72.f);
			Road->SetWorldTransform(MakeBottomAnchoredTransform(Mesh, Mid, Rotation, Scale));
			Road->SetVisibility(!IsHidden(), true);
			RoadAssetComponents.Add(Road);
			++InOutSegmentIndex;
		}
	}
}
