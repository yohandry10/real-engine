// Copyright World Leader project. See ROADMAP.md.

#include "Map/WLWorldMap.h"
#include "Campaign/WLDataRegistry.h"
#include "WorldLeader.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Test punto-en-triangulo (para ear clipping).
	bool PointInTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
	{
		const double ax = C.X - B.X, ay = C.Y - B.Y;
		const double bx = A.X - C.X, by = A.Y - C.Y;
		const double cx = B.X - A.X, cy = B.Y - A.Y;
		const double apx = P.X - A.X, apy = P.Y - A.Y;
		const double bpx = P.X - B.X, bpy = P.Y - B.Y;
		const double cpx = P.X - C.X, cpy = P.Y - C.Y;
		const double aCROSSbp = ax * bpy - ay * bpx;
		const double cCROSSap = cx * apy - cy * apx;
		const double bCROSScp = bx * cpy - by * cpx;
		return (aCROSSbp >= 0.0) && (bCROSScp >= 0.0) && (cCROSSap >= 0.0);
	}

	// Triangulacion por ear clipping de un poligono simple (Eberly). Produce
	// triangulos con winding consistente (CCW). Ignora agujeros (anillo exterior).
	void TriangulatePolygon(const TArray<FVector2D>& Contour, TArray<int32>& OutTris)
	{
		const int32 N = Contour.Num();
		if (N < 3) return;

		TArray<int32> V;
		V.SetNum(N);

		double Area = 0.0;
		for (int32 p = N - 1, q = 0; q < N; p = q++)
		{
			Area += (double)Contour[p].X * Contour[q].Y - (double)Contour[q].X * Contour[p].Y;
		}
		if (Area > 0.0) { for (int32 v = 0; v < N; v++) V[v] = v; }
		else            { for (int32 v = 0; v < N; v++) V[v] = (N - 1) - v; }

		int32 nv = N;
		int32 Count = 2 * nv;
		for (int32 v = nv - 1; nv > 2; )
		{
			if ((Count--) <= 0) return; // poligono degenerado o auto-intersectante

			int32 u = v;   if (nv <= u) u = 0;
			v = u + 1;     if (nv <= v) v = 0;
			int32 w = v + 1; if (nv <= w) w = 0;

			const FVector2D& A = Contour[V[u]];
			const FVector2D& B = Contour[V[v]];
			const FVector2D& C = Contour[V[w]];

			if (((B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X)) >= 0.0)
			{
				bool bEar = true;
				for (int32 p = 0; p < nv; p++)
				{
					if (p == u || p == v || p == w) continue;
					if (PointInTriangle(A, B, C, Contour[V[p]])) { bEar = false; break; }
				}
				if (bEar)
				{
					OutTris.Add(V[u]); OutTris.Add(V[v]); OutTris.Add(V[w]);
					for (int32 s = v, t = v + 1; t < nv; s++, t++) V[s] = V[t];
					nv--;
					Count = 2 * nv;
				}
			}
		}
	}

	double GetSignedPolygonArea(const TArray<FVector2D>& Poly)
	{
		double Area = 0.0;
		for (int32 p = Poly.Num() - 1, q = 0; q < Poly.Num(); p = q++)
		{
			Area += (double)Poly[p].X * Poly[q].Y - (double)Poly[q].X * Poly[p].Y;
		}
		return Area * 0.5;
	}

	FVector2D GetPolygonCenter(const TArray<FVector2D>& Poly)
	{
		if (Poly.IsEmpty())
		{
			return FVector2D::ZeroVector;
		}

		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& P : Poly)
		{
			Sum += P;
		}
		return Sum / Poly.Num();
	}
}

AWLWorldMap::AWLWorldMap()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MapMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MapMesh"));
	MapMesh->SetupAttachment(SceneRoot);

	ContinentFilters = { TEXT("North America"), TEXT("South America") };

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (MarkerMeshFinder.Succeeded())
	{
		MarkerMesh = MarkerMeshFinder.Object;
	}
}

void AWLWorldMap::BeginPlay()
{
	Super::BeginPlay();
	BuildMap();
}

void AWLWorldMap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyWorldActors();
	Super::EndPlay(EndPlayReason);
}

UWLDataRegistry* AWLWorldMap::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FBox2D AWLWorldMap::GetMapBounds2D() const
{
	return FBox2D(BoundsMin, BoundsMax);
}

bool AWLWorldMap::TryGetCountryForComponent(const UPrimitiveComponent* Component, FWLMapCountryView& OutCountry) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < CountryMarkers.Num() && Index < Countries.Num(); ++Index)
	{
		if (CountryMarkers[Index] == Component)
		{
			OutCountry = Countries[Index];
			return true;
		}
	}

	return false;
}

bool AWLWorldMap::TryGetProvinceForComponent(const UPrimitiveComponent* Component, FWLMapProvinceView& OutProvince) const
{
	if (!Component)
	{
		return false;
	}

	for (int32 Index = 0; Index < ProvinceMarkers.Num() && Index < Provinces.Num(); ++Index)
	{
		if (ProvinceMarkers[Index] == Component)
		{
			OutProvince = Provinces[Index];
			return true;
		}
	}

	return false;
}

void AWLWorldMap::SetPresentationActive(bool bActive, bool bSetCamera)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	if (MapMesh)
	{
		MapMesh->SetVisibility(bActive, true);
		MapMesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	for (UTextRenderComponent* Label : CountryLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bActive, true);
		}
	}
	for (UStaticMeshComponent* Marker : CountryMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(bActive, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	for (UStaticMeshComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->SetVisibility(bActive, true);
			Marker->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	if (MapDirectionalLight)
	{
		MapDirectionalLight->SetActorHiddenInGame(!bActive);
	}
	if (MapSkyLight)
	{
		MapSkyLight->SetActorHiddenInGame(!bActive);
	}
	if (bActive && bSetCamera && MapCamera)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->SetViewTargetWithBlend(MapCamera, 0.35f);
		}
	}
}

FVector2D AWLWorldMap::ProjectLonLat(double Lon, double Lat) const
{
	// X = norte (latitud), Y = este (longitud): mapa con el norte hacia arriba.
	return FVector2D(static_cast<float>(Lat * GeoScale), static_cast<float>(Lon * GeoScale));
}

bool AWLWorldMap::ShouldRenderContinent(const FString& Continent) const
{
	if (ContinentFilters.IsEmpty())
	{
		return true;
	}

	for (const FString& Filter : ContinentFilters)
	{
		if (Continent.Equals(Filter, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

TArray<FVector2D> AWLWorldMap::RingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring) const
{
	TArray<FVector2D> Poly;
	Poly.Reserve(Ring.Num());
	for (const TSharedPtr<FJsonValue>& CoordVal : Ring)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (CoordVal.IsValid() && CoordVal->TryGetArray(Pair) && Pair && Pair->Num() >= 2)
		{
			const double Lon = (*Pair)[0]->AsNumber();
			const double Lat = (*Pair)[1]->AsNumber();
			Poly.Add(ProjectLonLat(Lon, Lat));
		}
	}
	return Poly;
}

void AWLWorldMap::AddSection(const TArray<FVector2D>& Poly, const FLinearColor& Color)
{
	if (Poly.Num() < 3 || !MapMesh) return;

	TArray<int32> Tris;
	TriangulatePolygon(Poly, Tris);
	if (Tris.Num() < 3) return;

	TArray<FVector> Verts;
	Verts.Reserve(Poly.Num());
	for (const FVector2D& P : Poly)
	{
		Verts.Add(FVector(P.X, P.Y, 0.f));
		BoundsMin.X = FMath::Min(BoundsMin.X, P.X);
		BoundsMin.Y = FMath::Min(BoundsMin.Y, P.Y);
		BoundsMax.X = FMath::Max(BoundsMax.X, P.X);
		BoundsMax.Y = FMath::Max(BoundsMax.Y, P.Y);
	}

	// Doble cara: cada triangulo y su reverso, para que se vea desde arriba
	// sin depender del winding.
	TArray<int32> Final;
	Final.Reserve(Tris.Num() * 2);
	for (int32 i = 0; i + 2 < Tris.Num(); i += 3)
	{
		Final.Add(Tris[i]); Final.Add(Tris[i + 1]); Final.Add(Tris[i + 2]);
		Final.Add(Tris[i]); Final.Add(Tris[i + 2]); Final.Add(Tris[i + 1]);
	}

	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs;
	TArray<FColor> VColors;
	TArray<FProcMeshTangent> Tangents;

	MapMesh->CreateMeshSection(SectionIndex, Verts, Final, Normals, UVs, VColors, Tangents, false);

	if (BaseMaterial)
	{
		if (UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			MapMesh->SetMaterial(SectionIndex, Mat);
		}
	}

	++SectionIndex;
}

void AWLWorldMap::AddLabel(const FString& Text, const FVector2D& Pos)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetRelativeLocation(FVector(Pos.X, Pos.Y, 50.f));
	// Cenital: el pitch tumba el texto y el roll de 180 lo orienta hacia el
	// norte de pantalla para la camara superior (-90 pitch).
	Label->SetRelativeRotation(FRotator(90.f, 0.f, 180.f));
	Label->SetRelativeScale3D(FVector::OneVector);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(CountryLabelWorldSize);
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(FColor::White);
	CountryLabels.Add(Label);
}

void AWLWorldMap::AddCountryMarker(const FWLMapCountryView& Country, const FLinearColor& Color)
{
	if (!MarkerMesh)
	{
		return;
	}

	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetStaticMesh(MarkerMesh);
	Marker->SetRelativeLocation(Country.WorldLocation + FVector(0.f, 0.f, 160.f));
	Marker->SetRelativeScale3D(MarkerScale);
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCountryMarker"));
	Marker->ComponentTags.Add(FName(*Country.Iso));

	if (BaseMaterial)
	{
		if (UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this))
		{
			const FLinearColor MarkerColor = Country.bHasLargeLabel
				? Color.Desaturate(0.35f)
				: FLinearColor(1.0f, 0.82f, 0.18f);
			Mat->SetVectorParameterValue(TEXT("Color"), MarkerColor);
			Marker->SetMaterial(0, Mat);
		}
	}

	CountryMarkers.Add(Marker);
	Countries.Add(Country);
}

void AWLWorldMap::AddProvinceMarker(const FWLProvinceData& Province)
{
	if (!MarkerMesh || Province.Id.IsEmpty())
	{
		return;
	}
	if (!RenderedCountryIsos.Contains(Province.CountryIso))
	{
		return;
	}
	if (FMath::IsNearlyZero(Province.MapLat) && FMath::IsNearlyZero(Province.MapLon))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("GeoMap: provincia %s sin coordenadas validas."), *Province.Id);
		return;
	}

	const FVector2D Projected = ProjectLonLat(Province.MapLon, Province.MapLat);
	const FVector2D BoundsPadding(GeoScale * 2.f, GeoScale * 2.f);
	if (Projected.X < BoundsMin.X - BoundsPadding.X || Projected.X > BoundsMax.X + BoundsPadding.X
		|| Projected.Y < BoundsMin.Y - BoundsPadding.Y || Projected.Y > BoundsMax.Y + BoundsPadding.Y)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("GeoMap: provincia %s fuera de los limites del mapa renderizado."), *Province.Id);
		return;
	}

	FWLMapProvinceView ProvinceView;
	ProvinceView.Id = Province.Id;
	ProvinceView.Name = Province.Name;
	ProvinceView.CountryIso = Province.CountryIso;
	ProvinceView.Region = Province.Region;
	ProvinceView.WorldLocation = FVector(Projected.X, Projected.Y, 280.f);

	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetStaticMesh(MarkerMesh);
	Marker->SetRelativeLocation(ProvinceView.WorldLocation);
	Marker->SetRelativeScale3D(ProvinceMarkerScale);
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->ComponentTags.Add(TEXT("WorldLeaderProvinceMarker"));
	Marker->ComponentTags.Add(FName(*Province.Id));

	if (BaseMaterial)
	{
		if (UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.98f, 0.72f, 0.2f));
			Marker->SetMaterial(0, Mat);
		}
	}

	ProvinceMarkers.Add(Marker);
	Provinces.Add(ProvinceView);
}

void AWLWorldMap::AddProvinceMarkers()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		AddProvinceMarker(Province);
	}
}

void AWLWorldMap::BuildMap()
{
	if (MapMesh)
	{
		MapMesh->ClearAllMeshSections();
	}
	for (UTextRenderComponent* Label : CountryLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	CountryLabels.Reset();
	for (UStaticMeshComponent* Marker : CountryMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	CountryMarkers.Reset();
	Countries.Reset();
	RenderedCountryIsos.Reset();
	for (UStaticMeshComponent* Marker : ProvinceMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	ProvinceMarkers.Reset();
	Provinces.Reset();
	SectionIndex = 0;
	BoundsMin = FVector2D(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	BoundsMax = FVector2D(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());

	const UWLDataRegistry* Registry = GetRegistry();

	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Geo") / TEXT("Countries.geojson");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		UE_LOG(LogWorldLeader, Error, TEXT("GeoMap: no se pudo leer %s"), *Path);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogWorldLeader, Error, TEXT("GeoMap: GeoJSON invalido en %s"), *Path);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
	if (!Root->TryGetArrayField(TEXT("features"), Features))
	{
		return;
	}

	int32 CountryCount = 0;
	for (const TSharedPtr<FJsonValue>& FeatVal : *Features)
	{
		const TSharedPtr<FJsonObject>* FeatObj = nullptr;
		if (!FeatVal->TryGetObject(FeatObj)) continue;

		const TSharedPtr<FJsonObject>* PropsObj = nullptr;
		if (!(*FeatObj)->TryGetObjectField(TEXT("properties"), PropsObj)) continue;

		FString Continent;
		(*PropsObj)->TryGetStringField(TEXT("CONTINENT"), Continent);
		if (!ShouldRenderContinent(Continent)) continue;

		FString Iso, Name;
		(*PropsObj)->TryGetStringField(TEXT("ISO_A2"), Iso);
		(*PropsObj)->TryGetStringField(TEXT("ADMIN"), Name);

		// Color: nacion jugable -> su color; resto -> tierra neutral.
		FLinearColor Color(0.28f, 0.40f, 0.26f);
		FWLNationData Nation;
		if (Registry && Registry->GetNation(Iso, Nation))
		{
			Color = Nation.MapColor;
		}

		const TSharedPtr<FJsonObject>* GeomObj = nullptr;
		if (!(*FeatObj)->TryGetObjectField(TEXT("geometry"), GeomObj)) continue;

		FString GeomType;
		(*GeomObj)->TryGetStringField(TEXT("type"), GeomType);
		const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
		if (!(*GeomObj)->TryGetArrayField(TEXT("coordinates"), Coords)) continue;

		FVector2D LabelPos(0.f, 0.f);
		double LargestArea = 0.0;
		double TotalArea = 0.0;

		auto ProcessPolygon = [&](const TArray<TSharedPtr<FJsonValue>>& Rings)
		{
			if (Rings.Num() == 0) return;
			const TArray<TSharedPtr<FJsonValue>>* Outer = nullptr;
			if (!Rings[0]->TryGetArray(Outer) || !Outer) return;

			TArray<FVector2D> Poly = RingFromJson(*Outer);
			AddSection(Poly, Color);

			const double Area = FMath::Abs(GetSignedPolygonArea(Poly));
			TotalArea += Area;
			if (Area > LargestArea)
			{
				LargestArea = Area;
				LabelPos = GetPolygonCenter(Poly);
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
				if (PolyVal->TryGetArray(PolyRings) && PolyRings)
				{
					ProcessPolygon(*PolyRings);
				}
			}
		}

		if (LargestArea > 0.0)
		{
			FWLMapCountryView Country;
			Country.Iso = Iso;
			Country.Name = Name;
			Country.Continent = Continent;
			Country.WorldLocation = FVector(LabelPos.X, LabelPos.Y, 50.f);
			Country.ProjectedArea = static_cast<float>(TotalArea);
			Country.bHasLargeLabel = Country.ProjectedArea >= CountryLabelAreaThreshold;

			if (Country.bHasLargeLabel)
			{
				AddLabel(Name, LabelPos);
			}
			AddCountryMarker(Country, Color);
			RenderedCountryIsos.Add(Iso);
		}
		++CountryCount;
	}

	FString Scope = TEXT("World");
	if (!ContinentFilters.IsEmpty())
	{
		Scope = FString::Join(ContinentFilters, TEXT("+"));
	}
	UE_LOG(LogWorldLeader, Log, TEXT("GeoMap: %d paises (%s) renderizados en %d secciones."),
		CountryCount, *Scope, SectionIndex);

	AddProvinceMarkers();
	SetupLightingAndCamera();
}

void AWLWorldMap::SetupLightingAndCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DestroyWorldActors();

	MapDirectionalLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
		FVector(0.f, 0.f, 50000.f), FRotator(-60.f, 30.f, 0.f));
	MapSkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(),
		FVector(0.f, 0.f, 50000.f), FRotator::ZeroRotator);

	if (BoundsMax.X < BoundsMin.X)
	{
		return; // no se renderizo nada
	}

	FVector2D Center = ProjectLonLat(InitialCameraLonLat.X, InitialCameraLonLat.Y);
	Center.X = FMath::Clamp(Center.X, BoundsMin.X, BoundsMax.X);
	Center.Y = FMath::Clamp(Center.Y, BoundsMin.Y, BoundsMax.Y);
	const FVector2D Span = BoundsMax - BoundsMin;
	const float Height = InitialCameraHeight > 0.f
		? InitialCameraHeight
		: FMath::Max(FMath::Max(Span.X, Span.Y) * 0.7f, 10000.f);

	MapCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
		FVector(Center.X, Center.Y, Height), FRotator(-90.f, 0.f, 0.f));
	if (MapCamera && bActivateCameraOnBuild)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(MapCamera, 0.4f);
		}
	}
}

void AWLWorldMap::DestroyWorldActors()
{
	if (MapCamera)
	{
		MapCamera->Destroy();
		MapCamera = nullptr;
	}
	if (MapDirectionalLight)
	{
		MapDirectionalLight->Destroy();
		MapDirectionalLight = nullptr;
	}
	if (MapSkyLight)
	{
		MapSkyLight->Destroy();
		MapSkyLight = nullptr;
	}
}
