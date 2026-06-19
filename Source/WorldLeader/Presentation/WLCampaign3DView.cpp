// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

#include "Campaign/WLDataRegistry.h"
#include "Presentation/WLCampaignRouteBuilder.h"
#include "Presentation/WLCampaignSettlementBuilder.h"
#include "Presentation/WLCampaignWaterBuilder.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FWLRegionalCountryGeometry
	{
		FString Iso;
		FString Name;
		TArray<TArray<FVector2D>> Rings;
		FLinearColor Color = FLinearColor::White;
		bool bCoreCountry = false;
	};

	FString RouteKey(const FString& A, const FString& B)
	{
		return A < B ? FString::Printf(TEXT("%s|%s"), *A, *B) : FString::Printf(TEXT("%s|%s"), *B, *A);
	}

	bool IsRegionalProvince(const FWLProvinceData& Province)
	{
		return Province.CountryIso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
			|| Province.CountryIso.Equals(TEXT("VE"), ESearchCase::IgnoreCase);
	}

	bool IsCampaignTheaterIso(const FString& Iso)
	{
		return Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase);
	}

	bool IsCampaignContextIso(const FString& Iso)
	{
		return IsCampaignTheaterIso(Iso)
			|| Iso.Equals(TEXT("PA"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("EC"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("PE"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("BR"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("GY"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("SR"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("TT"), ESearchCase::IgnoreCase);
	}

	enum class EWLVisualBiome : uint8
	{
		Context = 0,
		Coast,
		Jungle,
		Llanos,
		Mountain,
		UrbanInfluence,
		Count
	};

	EWLVisualBiome ClassifyVisualBiome(float Lon, float Lat, bool bCoreCountry)
	{
		if (!bCoreCountry)
		{
			return EWLVisualBiome::Context;
		}
		const bool bAndes = Lon < -71.4f && Lon > -77.2f && Lat < 11.6f;
		const bool bCaribbeanCoast = Lat > 9.8f;
		const bool bGuajiraDry = Lon < -71.0f && Lon > -74.3f && Lat > 10.1f;
		const bool bAmazonOrGuiana = Lat < 6.4f || (Lon > -67.0f && Lat < 8.4f);
		const bool bLlanos = Lon > -72.6f && Lon < -64.0f && Lat >= 6.1f && Lat < 9.9f;
		const bool bUrbanCorridor = (Lon < -73.2f && Lon > -76.2f && Lat > 4.1f && Lat < 7.0f)
			|| (Lon > -69.0f && Lon < -66.1f && Lat > 9.7f && Lat < 10.8f);

		if (bUrbanCorridor)
		{
			return EWLVisualBiome::UrbanInfluence;
		}
		if (bAndes)
		{
			return EWLVisualBiome::Mountain;
		}
		if (bGuajiraDry || bCaribbeanCoast)
		{
			return EWLVisualBiome::Coast;
		}
		if (bLlanos)
		{
			return EWLVisualBiome::Llanos;
		}
		if (bAmazonOrGuiana)
		{
			return EWLVisualBiome::Jungle;
		}
		return EWLVisualBiome::Jungle;
	}

	FLinearColor VisualBiomeColor(EWLVisualBiome Biome)
	{
		switch (Biome)
		{
		case EWLVisualBiome::Coast:
			return FLinearColor(0.50f, 0.40f, 0.20f);
		case EWLVisualBiome::Jungle:
			return FLinearColor(0.025f, 0.230f, 0.070f);
		case EWLVisualBiome::Llanos:
			return FLinearColor(0.285f, 0.300f, 0.105f);
		case EWLVisualBiome::Mountain:
			return FLinearColor(0.385f, 0.330f, 0.215f);
		case EWLVisualBiome::UrbanInfluence:
			return FLinearColor(0.300f, 0.285f, 0.205f);
		default:
			return FLinearColor(0.050f, 0.105f, 0.070f);
		}
	}

	FLinearColor ShadeTerrainVertex(const FLinearColor& Base, float Lon, float Lat, float Height)
	{
		const float Relief = FMath::Clamp(Height / 14500.f, 0.f, 1.f);
		const float Noise = 0.07f * FMath::Sin(Lon * 4.7f + Lat * 1.3f) + 0.05f * FMath::Cos(Lon * 2.1f - Lat * 3.4f);
		const float Light = FMath::Clamp(0.82f + Relief * 0.42f + Noise, 0.55f, 1.38f);
		FLinearColor Color(Base.R * Light, Base.G * Light, Base.B * Light, 1.f);
		Color += FLinearColor(Relief * 0.13f, Relief * 0.105f, Relief * 0.065f, 0.f);
		Color.R = FMath::Clamp(Color.R, 0.f, 1.f);
		Color.G = FMath::Clamp(Color.G, 0.f, 1.f);
		Color.B = FMath::Clamp(Color.B, 0.f, 1.f);
		return Color;
	}

	FColor ToVertexFColor(const FLinearColor& Color)
	{
		return Color.ToFColor(true);
	}

	float VisualBiomeZOffset(EWLVisualBiome Biome, bool bCoreCountry)
	{
		if (!bCoreCountry)
		{
			return -120.f;
		}
		switch (Biome)
		{
		case EWLVisualBiome::Mountain:
			return 560.f;
		case EWLVisualBiome::Coast:
			return 95.f;
		case EWLVisualBiome::Llanos:
			return 135.f;
		case EWLVisualBiome::UrbanInfluence:
			return 210.f;
		default:
			return 165.f;
		}
	}

	FLinearColor CountryTerrainColor(const FString& Iso)
	{
		if (Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(0.075f, 0.155f, 0.085f);
		}
		if (Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(0.090f, 0.150f, 0.080f);
		}
		return FLinearColor(0.038f, 0.075f, 0.055f);
	}

	bool LonLatBoundsIntersect(
		float AMinLon, float AMaxLon, float AMinLat, float AMaxLat,
		float BMinLon, float BMaxLon, float BMinLat, float BMaxLat)
	{
		return AMinLon <= BMaxLon && AMaxLon >= BMinLon && AMinLat <= BMaxLat && AMaxLat >= BMinLat;
	}

	void GetLonLatBounds(const TArray<FVector2D>& Ring, float& OutMinLon, float& OutMaxLon, float& OutMinLat, float& OutMaxLat)
	{
		OutMinLon = TNumericLimits<float>::Max();
		OutMaxLon = TNumericLimits<float>::Lowest();
		OutMinLat = TNumericLimits<float>::Max();
		OutMaxLat = TNumericLimits<float>::Lowest();
		for (const FVector2D& P : Ring)
		{
			OutMinLon = FMath::Min(OutMinLon, P.X);
			OutMaxLon = FMath::Max(OutMaxLon, P.X);
			OutMinLat = FMath::Min(OutMinLat, P.Y);
			OutMaxLat = FMath::Max(OutMaxLat, P.Y);
		}
	}

	bool PointInLonLatRing(const FVector2D& P, const TArray<FVector2D>& Ring)
	{
		bool bInside = false;
		for (int32 Index = 0, Prev = Ring.Num() - 1; Index < Ring.Num(); Prev = Index++)
		{
			const FVector2D& A = Ring[Index];
			const FVector2D& B = Ring[Prev];
			const float Denom = B.Y - A.Y;
			if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const bool bCrosses = ((A.Y > P.Y) != (B.Y > P.Y))
				&& (P.X < (B.X - A.X) * (P.Y - A.Y) / Denom + A.X);
			if (bCrosses)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	bool PointInAnyLonLatRing(const FVector2D& P, const TArray<TArray<FVector2D>>& Rings)
	{
		for (const TArray<FVector2D>& Ring : Rings)
		{
			if (Ring.Num() >= 3 && PointInLonLatRing(P, Ring))
			{
				return true;
			}
		}
		return false;
	}

	TArray<FVector2D> LonLatRingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring)
	{
		TArray<FVector2D> Result;
		Result.Reserve(Ring.Num());
		for (const TSharedPtr<FJsonValue>& CoordVal : Ring)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
			if (CoordVal.IsValid() && CoordVal->TryGetArray(Pair) && Pair && Pair->Num() >= 2)
			{
				Result.Add(FVector2D(static_cast<float>((*Pair)[0]->AsNumber()), static_cast<float>((*Pair)[1]->AsNumber())));
			}
		}
		return Result;
	}

	bool RingIntersectsCampaignRegion(const TArray<FVector2D>& Ring, float MinLon, float MaxLon, float MinLat, float MaxLat)
	{
		if (Ring.Num() < 3)
		{
			return false;
		}

		float RingMinLon, RingMaxLon, RingMinLat, RingMaxLat;
		GetLonLatBounds(Ring, RingMinLon, RingMaxLon, RingMinLat, RingMaxLat);
		return LonLatBoundsIntersect(RingMinLon, RingMaxLon, RingMinLat, RingMaxLat, MinLon, MaxLon, MinLat, MaxLat);
	}

	bool LoadRegionalCountryGeometry(float RegionMinLon, float RegionMaxLon, float RegionMinLat, float RegionMaxLat, TArray<FWLRegionalCountryGeometry>& OutCountries)
	{
		OutCountries.Reset();

		const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Geo") / TEXT("Countries.geojson");
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *Path))
		{
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
		if (!Root->TryGetArrayField(TEXT("features"), Features))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& FeatVal : *Features)
		{
			const TSharedPtr<FJsonObject>* FeatObj = nullptr;
			if (!FeatVal.IsValid() || !FeatVal->TryGetObject(FeatObj))
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* PropsObj = nullptr;
			if (!(*FeatObj)->TryGetObjectField(TEXT("properties"), PropsObj))
			{
				continue;
			}

			FString Iso;
			(*PropsObj)->TryGetStringField(TEXT("ISO_A2"), Iso);
			if (!IsCampaignContextIso(Iso))
			{
				continue;
			}

			FString Name;
			(*PropsObj)->TryGetStringField(TEXT("ADMIN"), Name);

			const TSharedPtr<FJsonObject>* GeomObj = nullptr;
			if (!(*FeatObj)->TryGetObjectField(TEXT("geometry"), GeomObj))
			{
				continue;
			}

			FString GeomType;
			(*GeomObj)->TryGetStringField(TEXT("type"), GeomType);
			const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
			if (!(*GeomObj)->TryGetArrayField(TEXT("coordinates"), Coords))
			{
				continue;
			}

			FWLRegionalCountryGeometry Country;
			Country.Iso = Iso;
			Country.Name = Name;
			Country.bCoreCountry = IsCampaignTheaterIso(Iso);
			Country.Color = CountryTerrainColor(Iso);

			auto ProcessPolygon = [&](const TArray<TSharedPtr<FJsonValue>>& Rings)
			{
				if (Rings.Num() == 0)
				{
					return;
				}
				const TArray<TSharedPtr<FJsonValue>>* Outer = nullptr;
				if (!Rings[0]->TryGetArray(Outer) || !Outer)
				{
					return;
				}

				TArray<FVector2D> Ring = LonLatRingFromJson(*Outer);
				if (RingIntersectsCampaignRegion(Ring, RegionMinLon, RegionMaxLon, RegionMinLat, RegionMaxLat))
				{
					Country.Rings.Add(MoveTemp(Ring));
				}
			};

			if (GeomType == TEXT("Polygon"))
			{
				ProcessPolygon(*Coords);
			}
			else if (GeomType == TEXT("MultiPolygon"))
			{
				for (const TSharedPtr<FJsonValue>& PolyVal : *Coords)
				{
					const TArray<TSharedPtr<FJsonValue>>* PolyRings = nullptr;
					if (PolyVal.IsValid() && PolyVal->TryGetArray(PolyRings) && PolyRings)
					{
						ProcessPolygon(*PolyRings);
					}
				}
			}

			if (!Country.Rings.IsEmpty())
			{
				OutCountries.Add(MoveTemp(Country));
			}
		}

		OutCountries.Sort([](const FWLRegionalCountryGeometry& A, const FWLRegionalCountryGeometry& B)
		{
			if (A.bCoreCountry != B.bCoreCountry)
			{
				return !A.bCoreCountry;
			}
			return A.Iso < B.Iso;
		});

		return !OutCountries.IsEmpty();
	}

	void BuildNormals(const TArray<FVector>& Verts, const TArray<int32>& Tris, TArray<FVector>& OutNormals)
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

AWLCampaign3DView::AWLCampaign3DView()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	SeaMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SeaMesh"));
	SeaMesh->SetupAttachment(SceneRoot);
	SeaDetailMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SeaDetailMesh"));
	SeaDetailMesh->SetupAttachment(SceneRoot);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	TerrainMesh->SetupAttachment(SceneRoot);

	BoundaryMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BoundaryMesh"));
	BoundaryMesh->SetupAttachment(SceneRoot);

	RoadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadMesh"));
	RoadMesh->SetupAttachment(SceneRoot);

	SettlementMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SettlementMesh"));
	SettlementMesh->SetupAttachment(SceneRoot);

	SettlementBlockInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SettlementBlockInstances"));
	SettlementBlockInstances->SetupAttachment(SceneRoot);
	SettlementTowerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SettlementTowerInstances"));
	SettlementTowerInstances->SetupAttachment(SceneRoot);
	TreeInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeInstances"));
	TreeInstances->SetupAttachment(SceneRoot);
	BrushInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BrushInstances"));
	BrushInstances->SetupAttachment(SceneRoot);
	PortInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PortInstances"));
	PortInstances->SetupAttachment(SceneRoot);
	ArmyMarkerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArmyMarkerInstances"));
	ArmyMarkerInstances->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexMatFinder(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexMatFinder.Succeeded())
	{
		VertexColorMaterial = VertexMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CityMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (CityMeshFinder.Succeeded())
	{
		CityMesh = CityMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RouteMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (RouteMeshFinder.Succeeded())
	{
		RouteMesh = RouteMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		CubeMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		ConeMesh = ConeMeshFinder.Object;
	}

	ConfigureInstancedComponent(SettlementBlockInstances, CubeMesh, FLinearColor(0.34f, 0.305f, 0.235f));
	ConfigureInstancedComponent(SettlementTowerInstances, CubeMesh, FLinearColor(0.56f, 0.50f, 0.355f));
	ConfigureInstancedComponent(TreeInstances, ConeMesh ? ConeMesh : CityMesh, FLinearColor(0.010f, 0.135f, 0.032f));
	ConfigureInstancedComponent(BrushInstances, ConeMesh ? ConeMesh : CityMesh, FLinearColor(0.125f, 0.170f, 0.070f));
	ConfigureInstancedComponent(PortInstances, CubeMesh, FLinearColor(0.27f, 0.225f, 0.150f));
	ConfigureInstancedComponent(ArmyMarkerInstances, ConeMesh ? ConeMesh : CityMesh, FLinearColor(0.86f, 0.68f, 0.22f));
}

void AWLCampaign3DView::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoBuildOnBeginPlay && !bHasBuiltView)
	{
		BuildView(ActivePlayerNationIso);
	}
}

void AWLCampaign3DView::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPresentationActors();
	Super::EndPlay(EndPlayReason);
}

void AWLCampaign3DView::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!SeaDetailMesh || IsHidden())
	{
		return;
	}

	WaterAnimationTime += DeltaSeconds;
	const FVector Drift(
		FMath::Sin(WaterAnimationTime * 0.17f) * 520.f,
		FMath::Cos(WaterAnimationTime * 0.13f) * 760.f,
		FMath::Sin(WaterAnimationTime * 0.31f) * 42.f);
	SeaDetailMesh->SetRelativeLocation(Drift);
}

void AWLCampaign3DView::BuildView(const FString& PlayerNationIso)
{
	ActivePlayerNationIso = PlayerNationIso.TrimStartAndEnd().ToUpper();
	bHasBuiltView = true;
	Bounds = FBox2D(ForceInit);
	ProvinceViews.Reset();

	if (SeaMesh)
	{
		SeaMesh->ClearAllMeshSections();
	}
	if (SeaDetailMesh)
	{
		SeaDetailMesh->ClearAllMeshSections();
	}
	if (TerrainMesh)
	{
		TerrainMesh->ClearAllMeshSections();
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->ClearAllMeshSections();
	}
	if (RoadMesh)
	{
		RoadMesh->ClearAllMeshSections();
	}
	if (SettlementMesh)
	{
		SettlementMesh->ClearAllMeshSections();
	}
	for (UStaticMeshComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	ProvinceMarkers.Reset();
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	VisualComponents.Reset();
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->DestroyComponent();
		}
	}
	RouteSegments.Reset();
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	Labels.Reset();
	for (UInstancedStaticMeshComponent* Instanced : {
		SettlementBlockInstances,
		SettlementTowerInstances,
		TreeInstances,
		BrushInstances,
		PortInstances,
		ArmyMarkerInstances
	})
	{
		if (Instanced)
		{
			Instanced->ClearInstances();
		}
	}

	BuildSea();
	BuildTerrain();
	BuildCampaignVisualLayer();
	SetupLightingAndCamera();
	SetPresentationActive(true, true);
}

void AWLCampaign3DView::SetPresentationActive(bool bActive, bool bSetCamera)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	SetActorTickEnabled(bActive);
	SetComponentSetActive(bActive);

	if (ViewDirectionalLight)
	{
		ViewDirectionalLight->SetActorHiddenInGame(!bActive);
	}
	if (ViewSkyLight)
	{
		ViewSkyLight->SetActorHiddenInGame(!bActive);
	}
	if (ViewFog)
	{
		ViewFog->SetActorHiddenInGame(!bActive);
	}
	if (bActive && bSetCamera && ViewCamera)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->SetViewTargetWithBlend(ViewCamera, 0.35f);
		}
	}
}

bool AWLCampaign3DView::TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLCampaign3DProvinceView& OutProvince) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ProvinceMarkers.Num() && Index < ProvinceViews.Num(); ++Index)
	{
		if (ProvinceMarkers[Index] == Component)
		{
			OutProvince = ProvinceViews[Index];
			return true;
		}
	}
	return false;
}

FBox2D AWLCampaign3DView::GetViewBounds2D() const
{
	return Bounds;
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
	const float ColombianCentralRidge = FMath::Exp(-FMath::Square((Lon + 74.4f + (Lat - 6.0f) * 0.10f) * 0.82f))
		* FMath::Exp(-FMath::Square((Lat - 6.0f) * 0.18f));
	const float ColombianEasternRidge = FMath::Exp(-FMath::Square((Lon + 72.5f + (Lat - 7.0f) * 0.07f) * 0.88f))
		* FMath::Exp(-FMath::Square((Lat - 7.3f) * 0.20f));
	const float VenezuelanCoastalRange = FMath::Exp(-FMath::Square((Lat - 10.45f) * 1.85f))
		* FMath::Exp(-FMath::Square((Lon + 67.7f) * 0.28f));
	const float GuianaShield = FMath::Exp(-FMath::Square((Lon + 63.6f) * 0.34f))
		* FMath::Exp(-FMath::Square((Lat - 6.6f) * 0.26f));
	const float CoastalTerrace = FMath::Clamp((Lat - 1.2f) / 11.5f, 0.f, 1.f) * 0.05f;
	const float ReliefNoise = (FMath::Sin(Lon * 2.6f + Lat * 1.4f) + FMath::Cos(Lon * 1.2f - Lat * 2.8f)) * 0.035f;
	const float Height = ColombianCentralRidge * 0.92f
		+ ColombianEasternRidge * 0.68f
		+ VenezuelanCoastalRange * 0.45f
		+ GuianaShield * 0.36f
		+ CoastalTerrace
		+ ReliefNoise;
	return FMath::Max(0.f, Height * 9800.f);
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
	Params.HalfSize = 760000.f;
	Params.SeaZ = -2350.f;
	Params.TileCount = 52;
	Params.WaterMaterial = VertexColorMaterial;
	Params.DetailMaterial = VertexColorMaterial;

	FWLCampaignWaterBuilder::Build(SeaMesh, SeaDetailMesh, Params, [this](float Lon, float Lat)
	{
		return ProjectLonLat(Lon, Lat);
	});
}

void AWLCampaign3DView::BuildTerrain()
{
	if (!TerrainMesh)
	{
		return;
	}

	TArray<FWLRegionalCountryGeometry> Countries;
	if (!LoadRegionalCountryGeometry(RegionMinLon - 3.f, RegionMaxLon + 3.f, RegionMinLat - 3.f, RegionMaxLat + 3.f, Countries))
	{
		return;
	}

	for (const FWLRegionalCountryGeometry& Country : Countries)
	{
		AddCountryTerrain(Country.Rings, Country.Color, Country.bCoreCountry);
	}
}

void AWLCampaign3DView::AddCountryTerrain(const TArray<TArray<FVector2D>>& Rings, const FLinearColor& Color, bool bCoreCountry)
{
	if (!TerrainMesh || Rings.IsEmpty())
	{
		return;
	}

	struct FSectionBuffer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
	};
	TStaticArray<FSectionBuffer, static_cast<int32>(EWLVisualBiome::Count)> Buffers;
	const float CellDegrees = bCoreCountry ? 0.075f : 0.18f;

	for (const TArray<FVector2D>& Ring : Rings)
	{
		if (Ring.Num() < 3)
		{
			continue;
		}

		float RingMinLon, RingMaxLon, RingMinLat, RingMaxLat;
		GetLonLatBounds(Ring, RingMinLon, RingMaxLon, RingMinLat, RingMaxLat);
		const float MinLon = FMath::Max(RingMinLon, RegionMinLon - 2.2f);
		const float MaxLon = FMath::Min(RingMaxLon, RegionMaxLon + 2.2f);
		const float MinLat = FMath::Max(RingMinLat, RegionMinLat - 2.2f);
		const float MaxLat = FMath::Min(RingMaxLat, RegionMaxLat + 2.2f);

		for (float Lon = MinLon; Lon < MaxLon; Lon += CellDegrees)
		{
			for (float Lat = MinLat; Lat < MaxLat; Lat += CellDegrees)
			{
				const FVector2D Center(Lon + CellDegrees * 0.5f, Lat + CellDegrees * 0.5f);
				if (!PointInLonLatRing(Center, Ring))
				{
					continue;
				}

				const EWLVisualBiome Biome = ClassifyVisualBiome(Center.X, Center.Y, bCoreCountry);
				FSectionBuffer& Buffer = Buffers[static_cast<int32>(Biome)];
				const float ZOffset = VisualBiomeZOffset(Biome, bCoreCountry);
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
				const FLinearColor CellBaseColor = bCoreCountry ? VisualBiomeColor(Biome) : Color;
				Buffer.Colors.Add(ToVertexFColor(ShadeTerrainVertex(CellBaseColor, Lon, Lat, A.Z)));
				Buffer.Colors.Add(ToVertexFColor(ShadeTerrainVertex(CellBaseColor, Lon + CellDegrees, Lat, B.Z)));
				Buffer.Colors.Add(ToVertexFColor(ShadeTerrainVertex(CellBaseColor, Lon + CellDegrees, Lat + CellDegrees, C.Z)));
				Buffer.Colors.Add(ToVertexFColor(ShadeTerrainVertex(CellBaseColor, Lon, Lat + CellDegrees, D.Z)));
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
		const FSectionBuffer& Buffer = Buffers[Section];
		if (Buffer.Verts.IsEmpty() || Buffer.Tris.IsEmpty())
		{
			continue;
		}

		TArray<FVector> Normals;
		BuildNormals(Buffer.Verts, Buffer.Tris, Normals);
		TArray<FProcMeshTangent> Tangents;
		const int32 SectionIndex = TerrainMesh->GetNumSections();
		TerrainMesh->CreateMeshSection(SectionIndex, Buffer.Verts, Buffer.Tris, Normals, Buffer.UVs, Buffer.Colors, Tangents, true);
		if (VertexColorMaterial)
		{
			TerrainMesh->SetMaterial(SectionIndex, VertexColorMaterial);
		}
	}
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
	Colors.Init(ToVertexFColor(Color), Verts.Num());
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
	Colors.Init(ToVertexFColor(Color), Verts.Num());
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

void AWLCampaign3DView::AddBoundaryRibbon(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float WidthWorld, float ZOffset)
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

	const float MinLon = RegionMinLon - 2.5f;
	const float MaxLon = RegionMaxLon + 2.5f;
	const float MinLat = RegionMinLat - 2.5f;
	const float MaxLat = RegionMaxLat + 2.5f;

	for (int32 Index = 0; Index < LonLatPoints.Num(); ++Index)
	{
		const FVector2D& A = LonLatPoints[Index];
		const FVector2D& B = LonLatPoints[(Index + 1) % LonLatPoints.Num()];
		const float SegmentMinLon = FMath::Min(A.X, B.X);
		const float SegmentMaxLon = FMath::Max(A.X, B.X);
		const float SegmentMinLat = FMath::Min(A.Y, B.Y);
		const float SegmentMaxLat = FMath::Max(A.Y, B.Y);
		if (!LonLatBoundsIntersect(SegmentMinLon, SegmentMaxLon, SegmentMinLat, SegmentMaxLat, MinLon, MaxLon, MinLat, MaxLat))
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
	Colors.Init(ToVertexFColor(Color), Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	const int32 SectionIndex = BoundaryMesh->GetNumSections();
	BoundaryMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (VertexColorMaterial)
	{
		BoundaryMesh->SetMaterial(SectionIndex, VertexColorMaterial);
	}
}

void AWLCampaign3DView::BuildCampaignVisualLayer()
{
	FWLCampaignRouteBuilder::BuildDefaultTheaterRoutes(
		RoadMesh,
		VertexColorMaterial,
		[this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		});

	AddSettlementCluster(TEXT("Bogota"), -74.1f, 4.6f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("Medellin"), -75.58f, 6.25f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("Cartagena"), -75.5f, 10.4f, EWLCampaignSettlementType::Port, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("Barranquilla"), -74.8f, 10.98f, EWLCampaignSettlementType::Port, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("Riohacha"), -72.9f, 11.5f, EWLCampaignSettlementType::Port, FLinearColor(0.66f, 0.62f, 0.46f));
	AddSettlementCluster(TEXT("Cucuta"), -72.5f, 7.9f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("Maracaibo"), -71.6f, 10.6f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("Caracas"), -66.9f, 10.5f, EWLCampaignSettlementType::Capital, FLinearColor(0.92f, 0.58f, 0.34f));
	AddSettlementCluster(TEXT("Valencia"), -68.0f, 10.2f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.78f, 0.52f, 0.34f));
	AddSettlementCluster(TEXT("Barcelona"), -64.7f, 10.1f, EWLCampaignSettlementType::Port, FLinearColor(0.78f, 0.52f, 0.34f));
	AddSettlementCluster(TEXT("Guayana"), -62.6f, 8.3f, EWLCampaignSettlementType::Industrial, FLinearColor(0.62f, 0.54f, 0.36f));

	AddVegetationScatter(-75.5f, -70.0f, 0.8f, 6.6f, 7, 6, true);
	AddVegetationScatter(-68.0f, -62.2f, 3.4f, 7.4f, 7, 5, true);
	AddVegetationScatter(-70.5f, -64.8f, 7.2f, 9.5f, 6, 4, false);
	AddVegetationScatter(-76.8f, -73.5f, 8.5f, 11.8f, 4, 3, false);

	AddInstance(ArmyMarkerInstances, ProjectLonLat(-72.2f, 10.2f) + FVector(0.f, 0.f, 1700.f), FRotator(0.f, 25.f, 0.f), FVector(9.f, 9.f, 18.f));
	AddInstance(ArmyMarkerInstances, ProjectLonLat(-73.4f, 7.8f) + FVector(0.f, 0.f, 1650.f), FRotator(0.f, -18.f, 0.f), FVector(8.f, 8.f, 16.f));
}

void AWLCampaign3DView::AddPathPolyline(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float RadiusScale, float ZOffset)
{
	if (LonLatPoints.Num() < 2)
	{
		return;
	}

	for (int32 Index = 0; Index + 1 < LonLatPoints.Num(); ++Index)
	{
		const FVector2D& A = LonLatPoints[Index];
		const FVector2D& B = LonLatPoints[Index + 1];
		AddPolylineSegment(ProjectLonLat(A.X, A.Y) + FVector(0.f, 0.f, ZOffset), ProjectLonLat(B.X, B.Y) + FVector(0.f, 0.f, ZOffset), Color, RadiusScale);
	}
}

void AWLCampaign3DView::AddBiomePatch(const TArray<FVector2D>& LonLatPoints, const FLinearColor& Color, float ZOffset)
{
	AddTerrainPatch(LonLatPoints, Color, ZOffset);
}

void AWLCampaign3DView::AddSettlementCluster(
	const FString& Name,
	float Lon,
	float Lat,
	EWLCampaignSettlementType Type,
	const FLinearColor& AccentColor)
{
	FWLCampaignSettlementSpec Spec;
	Spec.Name = Name;
	Spec.Lon = Lon;
	Spec.Lat = Lat;
	Spec.Type = Type;
	Spec.AccentColor = AccentColor;
	FWLCampaignSettlementBuilder::AddSettlement(
		SettlementMesh,
		VertexColorMaterial,
		Spec,
		[this](float InLon, float InLat)
		{
			return ProjectLonLat(InLon, InLat);
		});

	const bool bCapital = Type == EWLCampaignSettlementType::Capital;
	const bool bSmall = Type == EWLCampaignSettlementType::Frontier;
	const FVector Base = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, bCapital ? 2450.f : 2050.f);
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetWorldLocation(Base + FVector(0.f, 0.f, bCapital ? 2350.f : 1720.f));
	Label->SetWorldRotation(FRotator(62.f, 0.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(bCapital ? 720.f : (bSmall ? 430.f : 540.f));
	Label->SetText(FText::FromString(Name));
	Label->SetTextRenderColor(bCapital ? FColor(230, 210, 140) : FColor(195, 205, 190));
	Labels.Add(Label);
}

void AWLCampaign3DView::AddVegetationScatter(float MinLon, float MaxLon, float MinLat, float MaxLat, int32 Columns, int32 Rows, bool bDenseJungle)
{
	for (int32 Col = 0; Col < Columns; ++Col)
	{
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const float U = (static_cast<float>(Col) + 0.35f + 0.18f * FMath::Sin(Row * 1.7f)) / static_cast<float>(Columns);
			const float V = (static_cast<float>(Row) + 0.42f + 0.16f * FMath::Cos(Col * 1.3f)) / static_cast<float>(Rows);
			if (((Col * 11 + Row * 17) % (bDenseJungle ? 7 : 5)) == 0)
			{
				continue;
			}
			const float Lon = FMath::Lerp(MinLon, MaxLon, U);
			const float Lat = FMath::Lerp(MinLat, MaxLat, V);
			const FVector Location = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, bDenseJungle ? 760.f : 560.f);
			const float ScaleBase = (bDenseJungle ? 6.6f : 4.7f) * (0.82f + 0.26f * FMath::Frac(FMath::Sin((Col + 1) * 12.9898f + Row * 78.233f) * 43758.5453f));
			AddInstance(bDenseJungle ? TreeInstances : BrushInstances, Location, FRotator(0.f, 37.f * (Col + Row), 0.f), FVector(ScaleBase, ScaleBase, bDenseJungle ? 9.5f : 4.8f));
		}
	}
}

void AWLCampaign3DView::AddInstance(UInstancedStaticMeshComponent* Component, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (!Component || !Component->GetStaticMesh())
	{
		return;
	}

	Component->AddInstance(FTransform(Rotation, Location, Scale));
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
		if (IsRegionalProvince(Province))
		{
			AddProvinceMarker(Province);
		}
	}
}

void AWLCampaign3DView::AddProvinceMarker(const FWLProvinceData& Province)
{
	if (!CityMesh)
	{
		return;
	}

	const FVector Location = ProjectLonLat(Province.MapLon, Province.MapLat) + FVector(0.f, 0.f, Province.bIsCapital ? 1050.f : 780.f);
	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetStaticMesh(CityMesh);
	Marker->SetWorldLocation(Location);
	Marker->SetWorldScale3D(Province.bIsCapital ? FVector(11.f) : FVector(7.f));
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DProvince"));
	Marker->ComponentTags.Add(FName(*Province.Id));
	if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(Province.bIsCapital
		? FLinearColor(0.95f, 0.72f, 0.22f)
		: TerrainColor(Province.Terrain, Province.CountryIso).Desaturate(0.15f) + FLinearColor(0.22f, 0.20f, 0.08f)))
	{
		Marker->SetMaterial(0, Mat);
	}

	FWLCampaign3DProvinceView View;
	View.Id = Province.Id;
	View.Name = Province.Name;
	View.CountryIso = Province.CountryIso;
	View.Region = Province.Region;
	View.WorldLocation = Location;
	ProvinceMarkers.Add(Marker);
	ProvinceViews.Add(View);

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetWorldLocation(Location + FVector(0.f, 0.f, 1500.f));
	Label->SetWorldRotation(FRotator(63.f, 0.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(Province.bIsCapital ? 1050.f : 780.f);
	Label->SetText(FText::FromString(Province.Capital));
	Label->SetTextRenderColor(Province.bIsCapital ? FColor(235, 210, 120) : FColor(210, 220, 220));
	Labels.Add(Label);
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
		if (!IsRegionalProvince(Province))
		{
			continue;
		}

		for (const FString& NeighborId : Province.Neighbors)
		{
			FWLProvinceData Neighbor;
			if (!Registry->GetProvince(NeighborId, Neighbor) || !IsRegionalProvince(Neighbor))
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

void AWLCampaign3DView::SetupLightingAndCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DestroyPresentationActors();

	ViewDirectionalLight = World->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(),
		FVector(-12000.f, -16000.f, 45000.f),
		FRotator(-52.f, -38.f, 0.f));
	ViewSkyLight = World->SpawnActor<ASkyLight>(
		ASkyLight::StaticClass(),
		FVector(0.f, 0.f, 26000.f),
		FRotator::ZeroRotator);

	const FVector Center = ProjectLonLat(-69.3f, 7.05f) + FVector(0.f, 0.f, 2600.f);
	ViewFog = World->SpawnActor<AExponentialHeightFog>(
		AExponentialHeightFog::StaticClass(),
		Center + FVector(0.f, 0.f, 1200.f),
		FRotator::ZeroRotator);
	if (ViewFog && ViewFog->GetComponent())
	{
		ViewFog->GetComponent()->SetFogDensity(0.00034f);
		ViewFog->GetComponent()->SetFogHeightFalloff(0.060f);
		ViewFog->GetComponent()->SetFogMaxOpacity(0.14f);
		ViewFog->GetComponent()->SetStartDistance(118000.f);
		ViewFog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.010f, 0.018f, 0.019f));
	}

	const FVector CameraLocation = Center + FVector(-76000.f, -118000.f, 69000.f);
	ViewCamera = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		CameraLocation,
		(Center - CameraLocation).Rotation());
	if (ViewCamera && ViewCamera->GetCameraComponent())
	{
		ViewCamera->GetCameraComponent()->SetFieldOfView(43.f);
	}
}

void AWLCampaign3DView::DestroyPresentationActors()
{
	if (ViewCamera)
	{
		ViewCamera->Destroy();
		ViewCamera = nullptr;
	}
	if (ViewDirectionalLight)
	{
		ViewDirectionalLight->Destroy();
		ViewDirectionalLight = nullptr;
	}
	if (ViewSkyLight)
	{
		ViewSkyLight->Destroy();
		ViewSkyLight = nullptr;
	}
	if (ViewFog)
	{
		ViewFog->Destroy();
		ViewFog = nullptr;
	}
}

void AWLCampaign3DView::SetComponentSetActive(bool bActive)
{
	if (SeaMesh)
	{
		SeaMesh->SetVisibility(bActive, true);
		SeaMesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (SeaDetailMesh)
	{
		SeaDetailMesh->SetVisibility(bActive, true);
		SeaDetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (TerrainMesh)
	{
		TerrainMesh->SetVisibility(bActive, true);
		TerrainMesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (BoundaryMesh)
	{
		BoundaryMesh->SetVisibility(bActive, true);
		BoundaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (RoadMesh)
	{
		RoadMesh->SetVisibility(bActive, true);
		RoadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (SettlementMesh)
	{
		SettlementMesh->SetVisibility(bActive, true);
		SettlementMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	for (UStaticMeshComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(bActive, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Component : VisualComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bActive, true);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UInstancedStaticMeshComponent* Instanced : {
		SettlementBlockInstances,
		SettlementTowerInstances,
		TreeInstances,
		BrushInstances,
		PortInstances,
		ArmyMarkerInstances
	})
	{
		if (Instanced)
		{
			Instanced->SetVisibility(bActive, true);
			Instanced->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Route : RouteSegments)
	{
		if (Route)
		{
			Route->SetVisibility(bActive, true);
		}
	}
	for (UTextRenderComponent* Label : Labels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
}
