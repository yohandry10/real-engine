// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignTerritoryLayerComponent.h"

#include "Presentation/WLCampaignAmericaLowDetailData.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FMeshBuildBuffer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	FColor ToFColor(const FLinearColor& Color)
	{
		return Color.ToFColor(false);
	}

	void AddRibbon(FMeshBuildBuffer& Buffer, const FVector& A, const FVector& B, float WidthWorld, const FLinearColor& Color)
	{
		const FVector Direction = B - A;
		const FVector Flat(Direction.X, Direction.Y, 0.f);
		if (Flat.SizeSquared() < 1.f)
		{
			return;
		}

		const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat.GetSafeNormal()) * (WidthWorld * 0.5f);
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({ A - Side, A + Side, B + Side, B - Side });
		Buffer.Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Buffer.Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector });
		Buffer.UVs.Append({ FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(1.f, 0.f) });
		const FColor VertexColor = ToFColor(Color);
		Buffer.Colors.Append({ VertexColor, VertexColor, VertexColor, VertexColor });
	}

	float PolygonArea(const TArray<FVector2D>& Poly)
	{
		float Area = 0.f;
		for (int32 Index = 0; Index < Poly.Num(); ++Index)
		{
			const FVector2D& A = Poly[Index];
			const FVector2D& B = Poly[(Index + 1) % Poly.Num()];
			Area += A.X * B.Y - B.X * A.Y;
		}
		return Area * 0.5f;
	}

	bool PointInTriangle2D(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
	{
		const float Area = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
		if (FMath::Abs(Area) < UE_SMALL_NUMBER)
		{
			return false;
		}
		const float S = ((A.Y - C.Y) * (P.X - C.X) + (C.X - A.X) * (P.Y - C.Y)) / Area;
		const float T = ((C.Y - B.Y) * (P.X - C.X) + (B.X - C.X) * (P.Y - C.Y)) / Area;
		const float U = 1.f - S - T;
		return S >= 0.f && T >= 0.f && U >= 0.f;
	}

	void TerritoryLayerTriangulateSimplePolygon(const TArray<FVector2D>& Contour, TArray<int32>& OutTris)
	{
		OutTris.Reset();
		if (Contour.Num() < 3)
		{
			return;
		}

		TArray<int32> V;
		const bool bClockwise = PolygonArea(Contour) < 0.f;
		for (int32 Index = 0; Index < Contour.Num(); ++Index)
		{
			V.Add(bClockwise ? (Contour.Num() - 1 - Index) : Index);
		}

		int32 Guard = 0;
		while (V.Num() > 2 && Guard++ < Contour.Num() * Contour.Num())
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
				if (((B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X)) <= 0.f)
				{
					continue;
				}

				bool bEar = true;
				for (const int32 TestIndex : V)
				{
					if (TestIndex != Prev && TestIndex != Curr && TestIndex != Next
						&& PointInTriangle2D(A, B, C, Contour[TestIndex]))
					{
						bEar = false;
						break;
					}
				}

				if (bEar)
				{
					OutTris.Append({ Prev, Curr, Next });
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

	FVector2D PolygonCenter(const TArray<FVector2D>& Poly)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& P : Poly)
		{
			Sum += P;
		}
		return Poly.IsEmpty() ? FVector2D::ZeroVector : Sum / static_cast<float>(Poly.Num());
	}

	TArray<FVector2D> SimplifyRing(const TArray<FVector2D>& Ring, int32 TargetPoints)
	{
		if (Ring.Num() <= TargetPoints)
		{
			return Ring;
		}

		TArray<FVector2D> Result;
		const int32 Step = FMath::Max(1, Ring.Num() / TargetPoints);
		for (int32 Index = 0; Index < Ring.Num(); Index += Step)
		{
			Result.Add(Ring[Index]);
		}
		return Result;
	}

	TArray<FVector2D> RingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring)
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

	void GetRingBounds(const TArray<FVector2D>& Ring, float& MinLon, float& MaxLon, float& MinLat, float& MaxLat)
	{
		MinLon = TNumericLimits<float>::Max();
		MaxLon = TNumericLimits<float>::Lowest();
		MinLat = TNumericLimits<float>::Max();
		MaxLat = TNumericLimits<float>::Lowest();
		for (const FVector2D& P : Ring)
		{
			MinLon = FMath::Min(MinLon, P.X);
			MaxLon = FMath::Max(MaxLon, P.X);
			MinLat = FMath::Min(MinLat, P.Y);
			MaxLat = FMath::Max(MaxLat, P.Y);
		}
	}

	bool BoundsIntersect(float AMinLon, float AMaxLon, float AMinLat, float AMaxLat, float BMinLon, float BMaxLon, float BMinLat, float BMaxLat)
	{
		return AMinLon <= BMaxLon && AMaxLon >= BMinLon && AMinLat <= BMaxLat && AMaxLat >= BMinLat;
	}

	bool PointInWorldPolygonXY(const TArray<FVector>& Polygon, const FVector& Point)
	{
		bool bInside = false;
		const int32 Count = Polygon.Num();
		if (Count < 3)
		{
			return false;
		}

		for (int32 Index = 0, Previous = Count - 1; Index < Count; Previous = Index++)
		{
			const FVector& A = Polygon[Index];
			const FVector& B = Polygon[Previous];
			const bool bCrossesY = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (!bCrossesY)
			{
				continue;
			}

			const float Denominator = B.Y - A.Y;
			if (FMath::Abs(Denominator) < UE_SMALL_NUMBER)
			{
				continue;
			}

			const float CrossX = ((B.X - A.X) * (Point.Y - A.Y) / Denominator) + A.X;
			if (Point.X < CrossX)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	bool WorldPolygonBoundsContainsXY(const TArray<FVector>& Polygon, const FVector& Point, float Padding)
	{
		if (Polygon.IsEmpty())
		{
			return false;
		}

		float MinX = TNumericLimits<float>::Max();
		float MaxX = TNumericLimits<float>::Lowest();
		float MinY = TNumericLimits<float>::Max();
		float MaxY = TNumericLimits<float>::Lowest();
		for (const FVector& Vertex : Polygon)
		{
			MinX = FMath::Min(MinX, Vertex.X);
			MaxX = FMath::Max(MaxX, Vertex.X);
			MinY = FMath::Min(MinY, Vertex.Y);
			MaxY = FMath::Max(MaxY, Vertex.Y);
		}

		return Point.X >= MinX - Padding && Point.X <= MaxX + Padding
			&& Point.Y >= MinY - Padding && Point.Y <= MaxY + Padding;
	}

	float WorldPolygonBoundsAreaXY(const TArray<FVector>& Polygon)
	{
		if (Polygon.IsEmpty())
		{
			return TNumericLimits<float>::Max();
		}

		float MinX = TNumericLimits<float>::Max();
		float MaxX = TNumericLimits<float>::Lowest();
		float MinY = TNumericLimits<float>::Max();
		float MaxY = TNumericLimits<float>::Lowest();
		for (const FVector& Vertex : Polygon)
		{
			MinX = FMath::Min(MinX, Vertex.X);
			MaxX = FMath::Max(MaxX, Vertex.X);
			MinY = FMath::Min(MinY, Vertex.Y);
			MaxY = FMath::Max(MaxY, Vertex.Y);
		}
		return FMath::Max(1.f, MaxX - MinX) * FMath::Max(1.f, MaxY - MinY);
	}

	bool ShouldUseRingForCountry(const FString& Iso, const TArray<FVector2D>& Ring)
	{
		float MinLon, MaxLon, MinLat, MaxLat;
		GetRingBounds(Ring, MinLon, MaxLon, MinLat, MaxLat);
		if (Iso == TEXT("GF"))
		{
			return BoundsIntersect(MinLon, MaxLon, MinLat, MaxLat, -55.5f, -50.5f, 1.0f, 6.5f);
		}
		if (MaxLon > -10.f || MinLon < -172.f)
		{
			return false;
		}
		return BoundsIntersect(MinLon, MaxLon, MinLat, MaxLat, -172.f, -10.f, -56.5f, 84.5f);
	}

	TMap<FString, FWLCampaignAmericaCountrySpec> MakeAmericaCountryMap()
	{
		FWLCampaignAmericaLowDetailData Data;
		TMap<FString, FWLCampaignAmericaCountrySpec> CountryMap;
		if (!FWLCampaignAmericaLowDetailDataLoader::Load(Data))
		{
			return CountryMap;
		}

		for (const FWLCampaignAmericaCountrySpec& Country : Data.Countries)
		{
			CountryMap.Add(Country.Iso, Country);
		}
		return CountryMap;
	}

	TArray<FVector2D> StylizedBoundsPolygon(float MinLon, float MaxLon, float MinLat, float MaxLat)
	{
		const float DLon = FMath::Max((MaxLon - MinLon) * 0.13f, 0.03f);
		const float DLat = FMath::Max((MaxLat - MinLat) * 0.13f, 0.03f);
		return {
			FVector2D(MinLon + DLon, MinLat),
			FVector2D(MaxLon - DLon, MinLat),
			FVector2D(MaxLon, MinLat + DLat),
			FVector2D(MaxLon, MaxLat - DLat),
			FVector2D(MaxLon - DLon, MaxLat),
			FVector2D(MinLon + DLon, MaxLat),
			FVector2D(MinLon, MaxLat - DLat),
			FVector2D(MinLon, MinLat + DLat)
		};
	}

	FString JoinJsonStringArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Obj->TryGetArrayField(FieldName, Values) || !Values)
		{
			return TEXT("");
		}

		TArray<FString> Parts;
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
			{
				Parts.Add(Text);
			}
		}
		return FString::Join(Parts, TEXT(", "));
	}
}

UWLCampaignTerritoryLayerComponent::UWLCampaignTerritoryLayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWLCampaignTerritoryLayerComponent::BuildLayer(
	USceneComponent* Parent,
	UMaterialInterface* InVertexColorMaterial,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	ClearLayer();
	VertexColorMaterial = InVertexColorMaterial;
	if (!Parent)
	{
		return;
	}

	NationalBorderMesh = NewProceduralMesh(Parent, TEXT("TerritoryNationalBorders"), false);
	ProvinceBorderMesh = NewProceduralMesh(Parent, TEXT("TerritoryProvinceBorders"), false);
	ResourceMarkerMesh = NewProceduralMesh(Parent, TEXT("TerritoryResourceMarkers"), false);
	HoverHighlightMesh = NewProceduralMesh(Parent, TEXT("TerritoryHoverHighlight"), false);
	SelectedHighlightMesh = NewProceduralMesh(Parent, TEXT("TerritorySelectedHighlight"), false);

	BuildCountryBordersAndHitProxies(Parent, ProjectLonLat);
	BuildProvinceRegions(Parent, ProjectLonLat);
	RebuildResourceMarkers();
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::ClearLayer()
{
	for (UProceduralMeshComponent* Mesh : OwnedMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	for (UTextRenderComponent* Label : CountryLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	for (UTextRenderComponent* Label : ProvinceLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}

	OwnedMeshes.Reset();
	HitMeshes.Reset();
	CountryLabels.Reset();
	ProvinceLabels.Reset();
	Regions.Reset();
	HitComponentToRegion.Reset();
	NationalBorderMesh = nullptr;
	ProvinceBorderMesh = nullptr;
	ResourceMarkerMesh = nullptr;
	HoverHighlightMesh = nullptr;
	SelectedHighlightMesh = nullptr;
	HoveredRegionId.Reset();
	SelectedRegionId.Reset();
}

UProceduralMeshComponent* UWLCampaignTerritoryLayerComponent::NewProceduralMesh(
	USceneComponent* Parent,
	const FName& Name,
	bool bCollision)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Parent)
	{
		return nullptr;
	}

	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner, Name);
	if (!Mesh)
	{
		return nullptr;
	}

	Mesh->SetupAttachment(Parent);
	Mesh->RegisterComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, bCollision ? ECR_Block : ECR_Ignore);
	if (bCollision)
	{
		Mesh->SetRenderInMainPass(false);
	}
	if (VertexColorMaterial)
	{
		Mesh->SetMaterial(0, VertexColorMaterial);
	}
	OwnedMeshes.Add(Mesh);
	return Mesh;
}

void UWLCampaignTerritoryLayerComponent::BuildCountryBordersAndHitProxies(
	USceneComponent* Parent,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Geo") / TEXT("Countries.geojson");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
	if (!Root->TryGetArrayField(TEXT("features"), Features))
	{
		return;
	}

	const TMap<FString, FWLCampaignAmericaCountrySpec> CountryMap = MakeAmericaCountryMap();
	if (CountryMap.Num() == 0)
	{
		return;
	}

	FMeshBuildBuffer BorderBuffer;
	for (const TSharedPtr<FJsonValue>& FeatVal : *Features)
	{
		const TSharedPtr<FJsonObject>* Feature = nullptr;
		if (!FeatVal.IsValid() || !FeatVal->TryGetObject(Feature))
		{
			continue;
		}

		const TSharedPtr<FJsonObject>* Props = nullptr;
		if (!(*Feature)->TryGetObjectField(TEXT("properties"), Props))
		{
			continue;
		}

		FString Iso;
		(*Props)->TryGetStringField(TEXT("ISO_A2"), Iso);
		FString Admin;
		(*Props)->TryGetStringField(TEXT("ADMIN"), Admin);
		if (Admin == TEXT("France"))
		{
			Iso = TEXT("GF");
		}

		const FWLCampaignAmericaCountrySpec* Config = CountryMap.Find(Iso);
		if (!Config || Config->GeoAdmin != Admin)
		{
			continue;
		}

		const FString Name = Config->DisplayName.IsEmpty() ? Config->GeoAdmin : Config->DisplayName;

		const TSharedPtr<FJsonObject>* Geometry = nullptr;
		if (!(*Feature)->TryGetObjectField(TEXT("geometry"), Geometry))
		{
			continue;
		}

		FString GeomType;
		(*Geometry)->TryGetStringField(TEXT("type"), GeomType);
		const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
		if (!(*Geometry)->TryGetArrayField(TEXT("coordinates"), Coords))
		{
			continue;
		}

		FRegionRecord Region;
		Region.bCountry = true;
		Region.View.Id = FString::Printf(TEXT("COUNTRY-%s"), *Iso);
		Region.View.Name = Name;
		Region.View.CountryIso = Iso;
		Region.View.ProvinceType = TEXT("country");
		Region.View.OwnerCountry = Iso;
		Region.View.ControllerCountry = Iso;
		Region.View.MainCity = TEXT("Capital marker");
		Region.View.DetailLevel = TEXT("low");
		Region.View.TerrainType = TEXT("National overview");
		Region.View.BiomeType = TEXT("America low-detail");
		Region.View.bIsCountry = true;
		Region.View.bSelectable = true;

		UProceduralMeshComponent* HitMesh = NewProceduralMesh(Parent, FName(*FString::Printf(TEXT("CountryHit_%s"), *Iso)), true);
		if (HitMesh)
		{
			HitMesh->SetVisibility(true, true);
			HitMesh->SetHiddenInGame(false, true);
			HitMesh->SetRenderInMainPass(false);
		}

		auto ProcessPolygon = [&](const TArray<TSharedPtr<FJsonValue>>& Rings)
		{
			const TArray<TSharedPtr<FJsonValue>>* Outer = nullptr;
			if (Rings.IsEmpty() || !Rings[0]->TryGetArray(Outer) || !Outer)
			{
				return;
			}

			TArray<FVector2D> Ring = SimplifyRing(RingFromJson(*Outer), 92);
			if (Ring.Num() < 3 || !ShouldUseRingForCountry(Iso, Ring))
			{
				return;
			}

			TArray<FVector> WorldRing;
			WorldRing.Reserve(Ring.Num());
			for (const FVector2D& LonLat : Ring)
			{
				WorldRing.Add(ProjectLonLat(LonLat.X, LonLat.Y) + FVector(0.f, 0.f, 1550.f));
			}

			for (int32 Index = 0; Index < WorldRing.Num(); ++Index)
			{
				AddRibbon(BorderBuffer, WorldRing[Index], WorldRing[(Index + 1) % WorldRing.Num()], 720.f, FLinearColor(0.48f, 0.44f, 0.31f, 1.f));
			}

			if (HitMesh)
			{
				TArray<int32> Tris;
				TerritoryLayerTriangulateSimplePolygon(Ring, Tris);
				TArray<FVector> Verts;
				TArray<FVector2D> UVs;
				TArray<FColor> Colors;
				for (const FVector2D& LonLat : Ring)
				{
					Verts.Add(ProjectLonLat(LonLat.X, LonLat.Y) + FVector(0.f, 0.f, 2400.f));
					UVs.Add(FVector2D::ZeroVector);
					Colors.Add(ToFColor(FLinearColor(0.f, 0.f, 0.f, 0.f)));
				}
				TArray<FVector> Normals;
				Normals.Init(FVector::UpVector, Verts.Num());
				const int32 Section = HitMesh->GetNumSections();
				HitMesh->CreateMeshSection(Section, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
			}

			if (Region.WorldPolygon.Num() < WorldRing.Num())
			{
				Region.LonLatPolygon = Ring;
				Region.WorldPolygon = MoveTemp(WorldRing);
				const FVector2D Center = PolygonCenter(Ring);
				Region.View.WorldLocation = ProjectLonLat(Center.X, Center.Y) + FVector(0.f, 0.f, 5200.f);
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
				const TArray<TSharedPtr<FJsonValue>>* Poly = nullptr;
				if (PolyVal.IsValid() && PolyVal->TryGetArray(Poly) && Poly)
				{
					ProcessPolygon(*Poly);
				}
			}
		}

		if (!Region.WorldPolygon.IsEmpty())
		{
			Region.HitComponent = HitMesh;
			const int32 RegionIndex = Regions.Add(MoveTemp(Region));
			if (HitMesh)
			{
				HitComponentToRegion.Add(HitMesh, RegionIndex);
				HitMeshes.Add(HitMesh);
			}

			// Los paises "teatro" ya tienen su etiqueta (correctamente orientada) en la
			// vista; aqui solo etiquetamos los de contexto para no duplicar.
			if (!FWLCampaignRegionGeometry::IsTheaterIso(Iso))
			{
				if (UTextRenderComponent* Label = NewObject<UTextRenderComponent>(GetOwner()))
				{
					Label->SetupAttachment(Parent);
					Label->RegisterComponent();
					Label->SetWorldLocation(Regions[RegionIndex].View.WorldLocation);
					Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
					Label->SetHorizontalAlignment(EHTA_Center);
					Label->SetWorldSize((Iso == TEXT("US") || Iso == TEXT("BR") || Iso == TEXT("CA")) ? 5200.f : 2850.f);
					Label->SetText(FText::FromString(Name));
					Label->SetTextRenderColor(FColor(120, 142, 138));
					Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					CountryLabels.Add(Label);
				}
			}
		}
	}

	if (NationalBorderMesh && !BorderBuffer.Verts.IsEmpty())
	{
		NationalBorderMesh->CreateMeshSection(0, BorderBuffer.Verts, BorderBuffer.Tris, BorderBuffer.Normals,
			BorderBuffer.UVs, BorderBuffer.Colors, BorderBuffer.Tangents, false);
		if (VertexColorMaterial)
		{
			NationalBorderMesh->SetMaterial(0, VertexColorMaterial);
		}
	}
}

void UWLCampaignTerritoryLayerComponent::BuildProvinceRegions(
	USceneComponent* Parent,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign3D") / TEXT("TerritoryRegions.json");
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Entries;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Entries))
	{
		return;
	}

	FMeshBuildBuffer ProvinceBorderBuffer;
	for (const TSharedPtr<FJsonValue>& EntryValue : Entries)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!EntryValue.IsValid() || !EntryValue->TryGetObject(ObjPtr) || !ObjPtr)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FRegionRecord Region;
		Obj->TryGetStringField(TEXT("id"), Region.View.Id);
		Obj->TryGetStringField(TEXT("name"), Region.View.Name);
		Obj->TryGetStringField(TEXT("country_iso"), Region.View.CountryIso);
		Obj->TryGetStringField(TEXT("province_type"), Region.View.ProvinceType);
		Obj->TryGetStringField(TEXT("owner_country"), Region.View.OwnerCountry);
		Obj->TryGetStringField(TEXT("controller_country"), Region.View.ControllerCountry);
		Obj->TryGetStringField(TEXT("main_city"), Region.View.MainCity);
		Obj->TryGetStringField(TEXT("terrain_type"), Region.View.TerrainType);
		Obj->TryGetStringField(TEXT("biome_type"), Region.View.BiomeType);
		Obj->TryGetStringField(TEXT("detail_level"), Region.View.DetailLevel);
		Region.View.ResourcesText = JoinJsonStringArray(Obj, TEXT("resources"));
		Region.View.bSelectable = true;
		Obj->TryGetBoolField(TEXT("selectable"), Region.View.bSelectable);
		Obj->TryGetBoolField(TEXT("is_occupied"), Region.View.bIsOccupied);
		Obj->TryGetBoolField(TEXT("is_disputed"), Region.View.bIsDisputed);
		Obj->TryGetBoolField(TEXT("is_capital_region"), Region.View.bIsCapitalRegion);
		Obj->TryGetBoolField(TEXT("is_border_region"), Region.View.bIsBorderRegion);
		Obj->TryGetBoolField(TEXT("is_coastal"), Region.View.bIsCoastal);
		Obj->TryGetBoolField(TEXT("is_resource_rich"), Region.View.bIsResourceRich);
		int32 TmpInt = 0;
		if (Obj->TryGetNumberField(TEXT("strategic_value"), TmpInt)) Region.View.StrategicValue = TmpInt;
		if (Obj->TryGetNumberField(TEXT("population_placeholder"), TmpInt)) Region.View.PopulationPlaceholder = TmpInt;
		if (Obj->TryGetNumberField(TEXT("infrastructure_placeholder"), TmpInt)) Region.View.InfrastructurePlaceholder = TmpInt;
		if (Obj->TryGetNumberField(TEXT("public_order_placeholder"), TmpInt)) Region.View.PublicOrderPlaceholder = TmpInt;
		if (Region.View.OwnerCountry.IsEmpty()) Region.View.OwnerCountry = Region.View.CountryIso;
		if (Region.View.ControllerCountry.IsEmpty()) Region.View.ControllerCountry = Region.View.OwnerCountry;

		const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
		if (Obj->TryGetArrayField(TEXT("points"), Points) && Points)
		{
			Region.LonLatPolygon = RingFromJson(*Points);
		}
		else
		{
			const TArray<TSharedPtr<FJsonValue>>* Bounds = nullptr;
			if (Obj->TryGetArrayField(TEXT("bounds"), Bounds) && Bounds && Bounds->Num() >= 4)
			{
				Region.LonLatPolygon = StylizedBoundsPolygon(
					static_cast<float>((*Bounds)[0]->AsNumber()),
					static_cast<float>((*Bounds)[1]->AsNumber()),
					static_cast<float>((*Bounds)[2]->AsNumber()),
					static_cast<float>((*Bounds)[3]->AsNumber()));
			}
		}

		if (Region.LonLatPolygon.Num() < 3 || Region.View.Id.IsEmpty())
		{
			continue;
		}

		const float ZLift = Region.View.bIsCapitalRegion ? 3350.f : 3000.f;
		for (const FVector2D& LonLat : Region.LonLatPolygon)
		{
			Region.WorldPolygon.Add(ProjectLonLat(LonLat.X, LonLat.Y) + FVector(0.f, 0.f, ZLift));
		}
		for (int32 Index = 0; Index < Region.WorldPolygon.Num(); ++Index)
		{
			const bool bBorderRegion = Region.View.bIsBorderRegion || Region.View.CountryIso == TEXT("CO") || Region.View.CountryIso == TEXT("VE");
			const FLinearColor Color = bBorderRegion
				? FLinearColor(0.50f, 0.50f, 0.42f, 1.f)
				: FLinearColor(0.34f, 0.38f, 0.35f, 1.f);
			AddRibbon(ProvinceBorderBuffer, Region.WorldPolygon[Index], Region.WorldPolygon[(Index + 1) % Region.WorldPolygon.Num()],
				Region.View.bIsCapitalRegion ? 620.f : 390.f, Color);
		}

		UProceduralMeshComponent* HitMesh = NewProceduralMesh(Parent, FName(*FString::Printf(TEXT("ProvinceHit_%s"), *Region.View.Id)), true);
		if (HitMesh)
		{
			TArray<int32> Tris;
			TerritoryLayerTriangulateSimplePolygon(Region.LonLatPolygon, Tris);
			TArray<FVector> Normals;
			Normals.Init(FVector::UpVector, Region.WorldPolygon.Num());
			TArray<FVector2D> UVs;
			TArray<FColor> Colors;
			for (int32 Index = 0; Index < Region.WorldPolygon.Num(); ++Index)
			{
				UVs.Add(FVector2D::ZeroVector);
				Colors.Add(ToFColor(FLinearColor(0.f, 0.f, 0.f, 0.f)));
			}
			HitMesh->CreateMeshSection(0, Region.WorldPolygon, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
			HitMesh->SetVisibility(true, true);
			HitMesh->SetHiddenInGame(false, true);
			HitMesh->SetRenderInMainPass(false);
			Region.HitComponent = HitMesh;
		}

		const FVector2D CenterLonLat = PolygonCenter(Region.LonLatPolygon);
		Region.View.WorldLocation = ProjectLonLat(CenterLonLat.X, CenterLonLat.Y) + FVector(0.f, 0.f, ZLift + 1450.f);
		const int32 RegionIndex = Regions.Add(MoveTemp(Region));
		if (HitMesh)
		{
			HitMeshes.Add(HitMesh);
			HitComponentToRegion.Add(HitMesh, RegionIndex);
		}

		if (UTextRenderComponent* Label = NewObject<UTextRenderComponent>(GetOwner()))
		{
			Label->SetupAttachment(Parent);
			Label->RegisterComponent();
			Label->SetWorldLocation(Regions[RegionIndex].View.WorldLocation);
			Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
			Label->SetHorizontalAlignment(EHTA_Center);
			Label->SetWorldSize(Regions[RegionIndex].View.bIsCapitalRegion ? 860.f : 640.f);
			Label->SetText(FText::FromString(Regions[RegionIndex].View.Name));
			Label->SetTextRenderColor(Regions[RegionIndex].View.CountryIso == TEXT("CO")
				? FColor(220, 205, 132)
				: FColor(224, 166, 126));
			Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ProvinceLabels.Add(Label);
		}
	}

	if (ProvinceBorderMesh && !ProvinceBorderBuffer.Verts.IsEmpty())
	{
		ProvinceBorderMesh->CreateMeshSection(0, ProvinceBorderBuffer.Verts, ProvinceBorderBuffer.Tris,
			ProvinceBorderBuffer.Normals, ProvinceBorderBuffer.UVs, ProvinceBorderBuffer.Colors,
			ProvinceBorderBuffer.Tangents, false);
		if (VertexColorMaterial)
		{
			ProvinceBorderMesh->SetMaterial(0, VertexColorMaterial);
		}
	}
}

void UWLCampaignTerritoryLayerComponent::RebuildResourceMarkers()
{
	if (!ResourceMarkerMesh)
	{
		return;
	}
	ResourceMarkerMesh->ClearAllMeshSections();

	FMeshBuildBuffer Buffer;
	for (const FRegionRecord& Region : Regions)
	{
		if (Region.bCountry || !Region.View.bIsResourceRich || Region.View.ResourcesText.IsEmpty())
		{
			continue;
		}
		const FVector Center = Region.View.WorldLocation + FVector(0.f, 0.f, 900.f);
		const float Size = Region.View.bIsCapitalRegion ? 1050.f : 760.f;
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({
			Center + FVector(Size, 0.f, 0.f),
			Center + FVector(0.f, Size, 0.f),
			Center + FVector(-Size, 0.f, 0.f),
			Center + FVector(0.f, -Size, 0.f)
		});
		Buffer.Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Buffer.Normals.Append({ FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector });
		Buffer.UVs.Append({ FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector });
		const FColor Color = ToFColor(FLinearColor(0.76f, 0.58f, 0.20f, 1.f));
		Buffer.Colors.Append({ Color, Color, Color, Color });
	}

	if (!Buffer.Verts.IsEmpty())
	{
		ResourceMarkerMesh->CreateMeshSection(0, Buffer.Verts, Buffer.Tris, Buffer.Normals,
			Buffer.UVs, Buffer.Colors, Buffer.Tangents, false);
		if (VertexColorMaterial)
		{
			ResourceMarkerMesh->SetMaterial(0, VertexColorMaterial);
		}
	}
}

void UWLCampaignTerritoryLayerComponent::RebuildHighlightMesh(
	UProceduralMeshComponent* Mesh,
	const FString& RegionId,
	const FLinearColor& Color,
	float WidthWorld,
	float ZLift)
{
	if (!Mesh)
	{
		return;
	}
	Mesh->ClearAllMeshSections();
	if (RegionId.IsEmpty())
	{
		return;
	}

	const FRegionRecord* Region = Regions.FindByPredicate([&](const FRegionRecord& Candidate)
	{
		return Candidate.View.Id == RegionId;
	});
	if (!Region || Region->WorldPolygon.Num() < 2)
	{
		return;
	}

	FMeshBuildBuffer Buffer;
	for (int32 Index = 0; Index < Region->WorldPolygon.Num(); ++Index)
	{
		AddRibbon(Buffer, Region->WorldPolygon[Index] + FVector(0.f, 0.f, ZLift),
			Region->WorldPolygon[(Index + 1) % Region->WorldPolygon.Num()] + FVector(0.f, 0.f, ZLift),
			WidthWorld, Color);
	}
	if (!Buffer.Verts.IsEmpty())
	{
		Mesh->CreateMeshSection(0, Buffer.Verts, Buffer.Tris, Buffer.Normals, Buffer.UVs, Buffer.Colors, Buffer.Tangents, false);
		if (VertexColorMaterial)
		{
			Mesh->SetMaterial(0, VertexColorMaterial);
		}
	}
}

void UWLCampaignTerritoryLayerComponent::SetPresentationActive(bool bActive)
{
	bLayerActive = bActive;
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::SetLayerToggles(bool bBorders, bool bProvinces, bool bLabels, bool bResources)
{
	bShowBorders = bBorders;
	bShowProvinces = bProvinces;
	bShowLabels = bLabels;
	bShowResources = bResources;
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::ApplyVisibility(float CameraHeight)
{
	LastCameraHeight = CameraHeight;
	const bool bGlobal = CameraHeight >= 360000.f;
	const bool bRegion = CameraHeight >= 185000.f && !bGlobal;
	const bool bDetailedTerritoryVisible = CameraHeight < 115000.f;

	const bool bNationalVisible = bLayerActive && bShowBorders && bGlobal;
	const bool bProvinceVisible = bLayerActive && bShowBorders && bShowProvinces && bDetailedTerritoryVisible;
	const bool bCountryLabelsVisible = bLayerActive && bShowLabels && bGlobal;
	const bool bProvinceLabelsVisible = bLayerActive && bShowLabels && bShowProvinces && bDetailedTerritoryVisible;
	const bool bResourcesVisible = bLayerActive && bShowResources && bShowProvinces && bDetailedTerritoryVisible;

	if (NationalBorderMesh)
	{
		NationalBorderMesh->SetVisibility(bNationalVisible, true);
	}
	if (ProvinceBorderMesh)
	{
		ProvinceBorderMesh->SetVisibility(bProvinceVisible, true);
	}
	if (ResourceMarkerMesh)
	{
		ResourceMarkerMesh->SetVisibility(bResourcesVisible, true);
	}
	if (HoverHighlightMesh)
	{
		HoverHighlightMesh->SetVisibility(bLayerActive && !HoveredRegionId.IsEmpty() && (bShowProvinces || bGlobal || bRegion), true);
	}
	if (SelectedHighlightMesh)
	{
		SelectedHighlightMesh->SetVisibility(bLayerActive && !SelectedRegionId.IsEmpty() && (bShowProvinces || bGlobal || bRegion), true);
	}

	for (UTextRenderComponent* Label : CountryLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bCountryLabelsVisible, true);
		}
	}
	for (UTextRenderComponent* Label : ProvinceLabels)
	{
		if (Label)
		{
			Label->SetVisibility(bProvinceLabelsVisible, true);
		}
	}

	for (FRegionRecord& Region : Regions)
	{
		if (!Region.HitComponent)
		{
			continue;
		}
		const bool bHitEnabled = Region.bCountry
			? (bLayerActive && (bGlobal || bRegion))
			: (bLayerActive && bShowProvinces && !bGlobal);
		Region.HitComponent->SetCollisionEnabled(bHitEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Region.HitComponent->SetVisibility(true, true);
		Region.HitComponent->SetHiddenInGame(false, true);
		Region.HitComponent->SetRenderInMainPass(false);
	}
}

void UWLCampaignTerritoryLayerComponent::SetHoveredTerritory(const FString& RegionId)
{
	if (HoveredRegionId == RegionId)
	{
		return;
	}
	HoveredRegionId = RegionId;
	RebuildHighlightMesh(HoverHighlightMesh, HoveredRegionId, FLinearColor(0.86f, 0.62f, 0.24f, 1.f), 1250.f, 620.f);
	ApplyVisibility(LastCameraHeight);
}

void UWLCampaignTerritoryLayerComponent::SetSelectedTerritory(const FString& RegionId)
{
	if (SelectedRegionId == RegionId)
	{
		return;
	}
	SelectedRegionId = RegionId;
	RebuildHighlightMesh(SelectedHighlightMesh, SelectedRegionId, FLinearColor(0.96f, 0.78f, 0.34f, 1.f), 1750.f, 920.f);
	ApplyVisibility(LastCameraHeight);
}

bool UWLCampaignTerritoryLayerComponent::TryGetTerritoryForComponent(
	const UPrimitiveComponent* Component,
	FWLCampaignTerritoryRegionView& OutRegion) const
{
	if (!Component)
	{
		return false;
	}

	const int32* RegionIndex = HitComponentToRegion.Find(Component);
	if (!RegionIndex || !Regions.IsValidIndex(*RegionIndex))
	{
		return false;
	}

	OutRegion = Regions[*RegionIndex].View;
	return OutRegion.bSelectable;
}

bool UWLCampaignTerritoryLayerComponent::TryGetTerritoryAtWorldLocation(
	const FVector& WorldLocation,
	bool bRequireProvince,
	FWLCampaignTerritoryRegionView& OutRegion) const
{
	for (const FRegionRecord& Region : Regions)
	{
		if (Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		if (PointInWorldPolygonXY(Region.WorldPolygon, WorldLocation))
		{
			OutRegion = Region.View;
			return true;
		}
	}

	const FRegionRecord* BoundsMatchedProvince = nullptr;
	float BoundsMatchedArea = TNumericLimits<float>::Max();
	for (const FRegionRecord& Region : Regions)
	{
		if (Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		if (!WorldPolygonBoundsContainsXY(Region.WorldPolygon, WorldLocation, 1500.f))
		{
			continue;
		}
		const float Area = WorldPolygonBoundsAreaXY(Region.WorldPolygon);
		if (Area < BoundsMatchedArea)
		{
			BoundsMatchedArea = Area;
			BoundsMatchedProvince = &Region;
		}
	}
	if (BoundsMatchedProvince)
	{
		OutRegion = BoundsMatchedProvince->View;
		return true;
	}

	const FRegionRecord* NearestProvince = nullptr;
	float NearestDistanceSq = FMath::Square(260000.f);
	for (const FRegionRecord& Region : Regions)
	{
		if (Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared2D(Region.View.WorldLocation, WorldLocation);
		if (DistanceSq < NearestDistanceSq)
		{
			NearestDistanceSq = DistanceSq;
			NearestProvince = &Region;
		}
	}
	if (NearestProvince)
	{
		OutRegion = NearestProvince->View;
		return true;
	}

	if (bRequireProvince)
	{
		return false;
	}

	for (const FRegionRecord& Region : Regions)
	{
		if (!Region.bCountry || !Region.View.bSelectable)
		{
			continue;
		}
		if (PointInWorldPolygonXY(Region.WorldPolygon, WorldLocation))
		{
			OutRegion = Region.View;
			return true;
		}
	}
	return false;
}

bool UWLCampaignTerritoryLayerComponent::GetRegionById(const FString& RegionId, FWLCampaignTerritoryRegionView& OutRegion) const
{
	const FRegionRecord* Region = Regions.FindByPredicate([&](const FRegionRecord& Candidate)
	{
		return Candidate.View.Id == RegionId;
	});
	if (!Region)
	{
		return false;
	}
	OutRegion = Region->View;
	return true;
}
