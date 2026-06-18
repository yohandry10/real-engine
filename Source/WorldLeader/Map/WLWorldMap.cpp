// Copyright World Leader project. See ROADMAP.md.

#include "Map/WLWorldMap.h"
#include "Campaign/WLDataRegistry.h"
#include "WorldLeader.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
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
}

AWLWorldMap::AWLWorldMap()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MapMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MapMesh"));
	MapMesh->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}
}

void AWLWorldMap::BeginPlay()
{
	Super::BeginPlay();
	BuildMap();
}

UWLDataRegistry* AWLWorldMap::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

FVector2D AWLWorldMap::ProjectLonLat(double Lon, double Lat) const
{
	// X = norte (latitud), Y = este (longitud): mapa con el norte hacia arriba.
	return FVector2D(static_cast<float>(Lat * GeoScale), static_cast<float>(Lon * GeoScale));
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
	Label->SetWorldSize(3000.f);
	Label->SetText(FText::FromString(Text));
	Label->SetTextRenderColor(FColor::White);
}

void AWLWorldMap::BuildMap()
{
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
		if (Continent != ContinentFilter) continue;

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
		int32 LargestRing = 0;

		auto ProcessPolygon = [&](const TArray<TSharedPtr<FJsonValue>>& Rings)
		{
			if (Rings.Num() == 0) return;
			const TArray<TSharedPtr<FJsonValue>>* Outer = nullptr;
			if (!Rings[0]->TryGetArray(Outer) || !Outer) return;

			TArray<FVector2D> Poly = RingFromJson(*Outer);
			AddSection(Poly, Color);

			if (Poly.Num() > LargestRing)
			{
				LargestRing = Poly.Num();
				FVector2D Sum(0.f, 0.f);
				for (const FVector2D& P : Poly) Sum += P;
				LabelPos = Sum / Poly.Num();
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

		if (LargestRing > 0)
		{
			AddLabel(Name, LabelPos);
		}
		++CountryCount;
	}

	UE_LOG(LogWorldLeader, Log, TEXT("GeoMap: %d paises (%s) renderizados en %d secciones."),
		CountryCount, *ContinentFilter, SectionIndex);

	SetupLightingAndCamera();
}

void AWLWorldMap::SetupLightingAndCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
		FVector(0.f, 0.f, 50000.f), FRotator(-60.f, 30.f, 0.f));
	World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(),
		FVector(0.f, 0.f, 50000.f), FRotator::ZeroRotator);

	if (BoundsMax.X < BoundsMin.X)
	{
		return; // no se renderizo nada
	}

	const FVector2D Center = (BoundsMin + BoundsMax) * 0.5f;
	const FVector2D Span = BoundsMax - BoundsMin;
	const float Height = FMath::Max(FMath::Max(Span.X, Span.Y) * 0.7f, 10000.f);

	ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
		FVector(Center.X, Center.Y, Height), FRotator(-90.f, 0.f, 0.f));
	if (Camera)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(Camera, 0.4f);
		}
	}
}
