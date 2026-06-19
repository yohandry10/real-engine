// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignOverviewBuilder.h"

#include "Presentation/WLCampaignAmericaLowDetailData.h"
#include "ProceduralMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WorldLeader.h"

namespace
{
	struct FMeshBuffers
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	FColor ToColor(const FLinearColor& Color)
	{
		return Color.ToFColor(false);
	}

	bool BoundsIntersect(float AMinLon, float AMaxLon, float AMinLat, float AMaxLat, float BMinLon, float BMaxLon, float BMinLat, float BMaxLat)
	{
		return AMinLon <= BMaxLon && AMaxLon >= BMinLon && AMinLat <= BMaxLat && AMaxLat >= BMinLat;
	}

	void GetRingBounds(const TArray<FVector2D>& Ring, float& MinLon, float& MaxLon, float& MinLat, float& MaxLat)
	{
		MinLon = TNumericLimits<float>::Max();
		MaxLon = -TNumericLimits<float>::Max();
		MinLat = TNumericLimits<float>::Max();
		MaxLat = -TNumericLimits<float>::Max();
		for (const FVector2D& P : Ring)
		{
			MinLon = FMath::Min(MinLon, P.X);
			MaxLon = FMath::Max(MaxLon, P.X);
			MinLat = FMath::Min(MinLat, P.Y);
			MaxLat = FMath::Max(MaxLat, P.Y);
		}
	}

	bool PointInRing(const FVector2D& P, const TArray<FVector2D>& Ring)
	{
		bool bInside = false;
		for (int32 Index = 0, Prev = Ring.Num() - 1; Index < Ring.Num(); Prev = Index++)
		{
			const FVector2D& A = Ring[Index];
			const FVector2D& B = Ring[Prev];
			const float Denom = B.Y - A.Y;
			if (FMath::Abs(Denom) < UE_SMALL_NUMBER)
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

	bool PointInTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
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
			bool bEarFound = false;
			for (int32 I = 0; I < V.Num(); ++I)
			{
				const int32 Prev = V[(I + V.Num() - 1) % V.Num()];
				const int32 Curr = V[I];
				const int32 Next = V[(I + 1) % V.Num()];
				const FVector2D& A = Contour[Prev];
				const FVector2D& B = Contour[Curr];
				const FVector2D& C = Contour[Next];
				const float Cross = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
				if (Cross <= UE_SMALL_NUMBER)
				{
					continue;
				}

				bool bContainsPoint = false;
				for (int32 OtherIndex : V)
				{
					if (OtherIndex == Prev || OtherIndex == Curr || OtherIndex == Next)
					{
						continue;
					}
					if (PointInTriangle(A, B, C, Contour[OtherIndex]))
					{
						bContainsPoint = true;
						break;
					}
				}
				if (bContainsPoint)
				{
					continue;
				}

				OutTris.Add(Prev);
				OutTris.Add(Curr);
				OutTris.Add(Next);
				V.RemoveAt(I);
				bEarFound = true;
				break;
			}

			if (!bEarFound)
			{
				break;
			}
		}
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
		if (Result.Num() > 2 && Result[0].Equals(Result.Last(), 0.0001f))
		{
			Result.Pop();
		}
		return Result;
	}

	bool IsSouthAmericaIso(const FString& Iso)
	{
		return Iso == TEXT("CO") || Iso == TEXT("VE") || Iso == TEXT("EC") || Iso == TEXT("PE")
			|| Iso == TEXT("BO") || Iso == TEXT("CL") || Iso == TEXT("AR") || Iso == TEXT("UY")
			|| Iso == TEXT("PY") || Iso == TEXT("BR") || Iso == TEXT("GY") || Iso == TEXT("SR")
			|| Iso == TEXT("GF");
	}

	TArray<FVector2D> SimplifyRingForCountry(const FString& Iso, const TArray<FVector2D>& Ring)
	{
		if (FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Iso) || Ring.Num() <= 96)
		{
			return Ring;
		}

		const int32 TargetPoints = IsSouthAmericaIso(Iso) ? 240 : 150;
		if (Ring.Num() <= TargetPoints)
		{
			return Ring;
		}

		const int32 Step = FMath::Max(1, FMath::CeilToInt(static_cast<float>(Ring.Num()) / static_cast<float>(TargetPoints)));
		TArray<FVector2D> Simplified;
		for (int32 Index = 0; Index < Ring.Num(); Index += Step)
		{
			Simplified.Add(Ring[Index]);
		}
		return Simplified.Num() >= 3 ? Simplified : Ring;
	}

	FLinearColor CountryFillColor(const FString& Iso, bool bSpecialTerritory)
	{
		if (FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Iso))
		{
			return FLinearColor(0.210f, 0.355f, 0.185f, 0.98f);
		}
		const uint32 Hash = GetTypeHash(Iso);
		const float Variant = static_cast<float>(Hash % 7) * 0.010f;
		if (bSpecialTerritory)
		{
			return FLinearColor(0.120f + Variant, 0.185f + Variant, 0.155f + Variant, 0.90f);
		}
		return FLinearColor(0.105f + Variant, 0.205f + Variant, 0.135f + Variant * 0.65f, 0.94f);
	}

	FVector OverviewPoint(const FVector2D& LonLat, float Z, TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		FVector Point = ProjectLonLat(LonLat.X, LonLat.Y);
		const float Relief = 115.f * FMath::Sin(LonLat.X * 0.19f + LonLat.Y * 0.21f);
		Point.Z = Z + Relief;
		return Point;
	}

	void AddQuad(FMeshBuffers& Buffer, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color)
	{
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({A, B, C, D});
		Buffer.UVs.Append({FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(1.f, 1.f), FVector2D(0.f, 1.f)});
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Buffer.Colors.Add(ToColor(Color));
		}
		Buffer.Tris.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		Buffer.Tris.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
	}

	void AddPolygon(
		FMeshBuffers& Buffer,
		const TArray<FVector2D>& Ring,
		const FLinearColor& Color,
		float Z,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		TArray<int32> LocalTris;
		TriangulateSimplePolygon(Ring, LocalTris);
		if (LocalTris.Num() < 3)
		{
			return;
		}

		const int32 Base = Buffer.Verts.Num();
		for (const FVector2D& Point : Ring)
		{
			Buffer.Verts.Add(OverviewPoint(Point, Z, ProjectLonLat));
			Buffer.UVs.Add(FVector2D((Point.X + 180.f) / 360.f, (Point.Y + 90.f) / 180.f));
			Buffer.Colors.Add(ToColor(Color));
		}
		for (int32 Tri : LocalTris)
		{
			Buffer.Tris.Add(Base + Tri);
		}
		for (int32 Index = 0; Index + 2 < LocalTris.Num(); Index += 3)
		{
			Buffer.Tris.Add(Base + LocalTris[Index]);
			Buffer.Tris.Add(Base + LocalTris[Index + 2]);
			Buffer.Tris.Add(Base + LocalTris[Index + 1]);
		}
	}

	void AddRasterizedPolygon(
		FMeshBuffers& Buffer,
		const TArray<FVector2D>& Ring,
		const FString& Iso,
		const FLinearColor& Color,
		float Z,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		if (Ring.Num() < 3)
		{
			return;
		}

		float MinLon, MaxLon, MinLat, MaxLat;
		GetRingBounds(Ring, MinLon, MaxLon, MinLat, MaxLat);
		const bool bCoreTheater = FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Iso);
		const float CellDegrees = bCoreTheater ? 0.20f : (IsSouthAmericaIso(Iso) ? 0.42f : 0.58f);
		for (float Lon = MinLon; Lon < MaxLon; Lon += CellDegrees)
		{
			for (float Lat = MinLat; Lat < MaxLat; Lat += CellDegrees)
			{
				const FVector2D Center(Lon + CellDegrees * 0.5f, Lat + CellDegrees * 0.5f);
				if (!PointInRing(Center, Ring))
				{
					continue;
				}

				const FVector A = OverviewPoint(FVector2D(Lon, Lat), Z, ProjectLonLat);
				const FVector B = OverviewPoint(FVector2D(Lon + CellDegrees, Lat), Z, ProjectLonLat);
				const FVector C = OverviewPoint(FVector2D(Lon + CellDegrees, Lat + CellDegrees), Z, ProjectLonLat);
				const FVector D = OverviewPoint(FVector2D(Lon, Lat + CellDegrees), Z, ProjectLonLat);
				AddQuad(Buffer, A, B, C, D, Color);
			}
		}
	}

	void AddOutline(
		FMeshBuffers& Buffer,
		const TArray<FVector2D>& Ring,
		const FLinearColor& Color,
		float Z,
		float Width,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		for (int32 Index = 0; Index < Ring.Num(); ++Index)
		{
			const FVector A = OverviewPoint(Ring[Index], Z, ProjectLonLat);
			const FVector B = OverviewPoint(Ring[(Index + 1) % Ring.Num()], Z, ProjectLonLat);
			const FVector FlatDirection(B.X - A.X, B.Y - A.Y, 0.f);
			if (FlatDirection.SizeSquared() < 1.f)
			{
				continue;
			}
			const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal()) * Width;
			AddQuad(Buffer, A - Side, A + Side, B + Side, B - Side, Color);
		}
	}

	void AddCityMarker(
		FMeshBuffers& Buffer,
		const FWLCampaignAmericaCitySpec& City,
		const FWLCampaignOverviewBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		const bool bCoreTheater = FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(City.CountryIso);
		if (!City.bMajor && !bCoreTheater)
		{
			return;
		}

		const FVector Center = OverviewPoint(FVector2D(City.Lon, City.Lat), Params.CityZ, ProjectLonLat);
		const float Size = Params.CityMarkerSize * (City.bMajor ? 1.18f : 0.70f);
		const FLinearColor Color = City.bMajor
			? FLinearColor(0.820f, 0.640f, 0.250f, 0.96f)
			: FLinearColor(0.610f, 0.665f, 0.520f, 0.74f);
		AddQuad(Buffer,
			Center + FVector(-Size, 0.f, 0.f),
			Center + FVector(0.f, -Size, 0.f),
			Center + FVector(Size, 0.f, 0.f),
			Center + FVector(0.f, Size, 0.f),
			Color);
	}

	void AddCountryLabels(const TArray<FWLCampaignAmericaCountrySpec>& Countries, TArray<FWLCampaignOverviewLabelSpec>& OutLabels)
	{
		for (const FWLCampaignAmericaCountrySpec& Country : Countries)
		{
			const bool bCoreTheater = FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Country.Iso);
			const bool bPriorityLabel = bCoreTheater
				|| Country.Iso == TEXT("CA") || Country.Iso == TEXT("US") || Country.Iso == TEXT("MX")
				|| Country.Iso == TEXT("BR") || Country.Iso == TEXT("AR") || Country.Iso == TEXT("CL")
				|| Country.Iso == TEXT("PE") || Country.Iso == TEXT("EC") || Country.Iso == TEXT("BO")
				|| Country.Iso == TEXT("PA") || Country.Iso == TEXT("CU") || Country.Iso == TEXT("JM")
				|| Country.Iso == TEXT("TT") || Country.Iso == TEXT("GY");
			if (!bPriorityLabel)
			{
				continue;
			}

			FWLCampaignOverviewLabelSpec Label;
			Label.Text = Country.DisplayName;
			Label.Lon = Country.LabelLon;
			Label.Lat = Country.LabelLat;
			Label.ZOffset = bCoreTheater ? 34000.f : 30000.f;
			Label.WorldSize = bCoreTheater ? 16500.f : (IsSouthAmericaIso(Country.Iso) ? 11200.f : 9800.f);
			Label.Color = bCoreTheater ? FColor(235, 214, 126) : FColor(168, 194, 174);
			OutLabels.Add(Label);
		}
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

	bool ProcessPolygon(
		const TArray<TSharedPtr<FJsonValue>>& Rings,
		const FString& Iso,
		const FWLCampaignAmericaCountrySpec& Config,
		const FWLCampaignOverviewBuildParams& Params,
		FMeshBuffers& Buffer,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		if (Rings.IsEmpty())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Outer = nullptr;
		if (!Rings[0].IsValid() || !Rings[0]->TryGetArray(Outer) || !Outer)
		{
			return false;
		}

		TArray<FVector2D> Ring = SimplifyRingForCountry(Iso, RingFromJson(*Outer));
		if (Ring.Num() < 3 || !ShouldUseRingForCountry(Iso, Ring))
		{
			return false;
		}

		const FLinearColor Fill = CountryFillColor(Iso, Config.bSpecialTerritory);
		const FLinearColor Border = FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Iso)
			? FLinearColor(0.760f, 0.620f, 0.250f, 0.92f)
			: FLinearColor(0.130f, 0.250f, 0.230f, 0.78f);
		AddRasterizedPolygon(Buffer, Ring, Iso, Fill, Params.LandZ, ProjectLonLat);
		AddOutline(Buffer, Ring, Border, Params.BorderZ, Params.BorderWidth, ProjectLonLat);
		return true;
	}

	TArray<FVector2D> MakePlaceholderRing(const FWLCampaignAmericaCountrySpec& Country)
	{
		const float RadiusLon = Country.bSpecialTerritory ? 0.28f : 0.62f;
		const float RadiusLat = Country.bSpecialTerritory ? 0.20f : 0.42f;
		return {
			FVector2D(Country.LabelLon - RadiusLon * 0.45f, Country.LabelLat - RadiusLat),
			FVector2D(Country.LabelLon + RadiusLon * 0.45f, Country.LabelLat - RadiusLat),
			FVector2D(Country.LabelLon + RadiusLon, Country.LabelLat - RadiusLat * 0.45f),
			FVector2D(Country.LabelLon + RadiusLon, Country.LabelLat + RadiusLat * 0.45f),
			FVector2D(Country.LabelLon + RadiusLon * 0.45f, Country.LabelLat + RadiusLat),
			FVector2D(Country.LabelLon - RadiusLon * 0.45f, Country.LabelLat + RadiusLat),
			FVector2D(Country.LabelLon - RadiusLon, Country.LabelLat + RadiusLat * 0.45f),
			FVector2D(Country.LabelLon - RadiusLon, Country.LabelLat - RadiusLat * 0.45f)
		};
	}

	void AddCountryPlaceholder(
		const FWLCampaignAmericaCountrySpec& Country,
		const FWLCampaignOverviewBuildParams& Params,
		FMeshBuffers& Buffer,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		const TArray<FVector2D> Ring = MakePlaceholderRing(Country);
		const FLinearColor Fill = CountryFillColor(Country.Iso, Country.bSpecialTerritory).Desaturate(0.18f);
		const FLinearColor Border = FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Country.Iso)
			? FLinearColor(0.760f, 0.620f, 0.250f, 0.92f)
			: FLinearColor(0.150f, 0.270f, 0.245f, 0.78f);
		AddPolygon(Buffer, Ring, Fill, Params.LandZ + 30.f, ProjectLonLat);
		AddOutline(Buffer, Ring, Border, Params.BorderZ + 30.f, Params.BorderWidth * 0.72f, ProjectLonLat);
	}

	void AddCountriesFromGeoJson(
		const FWLCampaignOverviewBuildParams& Params,
		const TArray<FWLCampaignAmericaCountrySpec>& Countries,
		FMeshBuffers& Buffer,
		TSet<FString>& OutRenderedIsos,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		TMap<FString, FWLCampaignAmericaCountrySpec> CountryMap;
		for (const FWLCampaignAmericaCountrySpec& Country : Countries)
		{
			CountryMap.Add(Country.Iso, Country);
		}

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

		for (const TSharedPtr<FJsonValue>& FeatureValue : *Features)
		{
			const TSharedPtr<FJsonObject>* Feature = nullptr;
			if (!FeatureValue.IsValid() || !FeatureValue->TryGetObject(Feature))
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

			const TSharedPtr<FJsonObject>* Geometry = nullptr;
			if (!(*Feature)->TryGetObjectField(TEXT("geometry"), Geometry))
			{
				continue;
			}

			FString GeometryType;
			(*Geometry)->TryGetStringField(TEXT("type"), GeometryType);
			const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
			if (!(*Geometry)->TryGetArrayField(TEXT("coordinates"), Coords))
			{
				continue;
			}

			if (GeometryType == TEXT("Polygon"))
			{
				if (ProcessPolygon(*Coords, Iso, *Config, Params, Buffer, ProjectLonLat))
				{
					OutRenderedIsos.Add(Iso);
				}
			}
			else if (GeometryType == TEXT("MultiPolygon"))
			{
				for (const TSharedPtr<FJsonValue>& PolyValue : *Coords)
				{
					const TArray<TSharedPtr<FJsonValue>>* Rings = nullptr;
					if (PolyValue.IsValid() && PolyValue->TryGetArray(Rings) && Rings)
					{
						if (ProcessPolygon(*Rings, Iso, *Config, Params, Buffer, ProjectLonLat))
						{
							OutRenderedIsos.Add(Iso);
						}
					}
				}
			}
		}
	}

	void AddMissingCountryPlaceholders(
		const TArray<FWLCampaignAmericaCountrySpec>& Countries,
		const TSet<FString>& RenderedIsos,
		const FWLCampaignOverviewBuildParams& Params,
		FMeshBuffers& Buffer,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		for (const FWLCampaignAmericaCountrySpec& Country : Countries)
		{
			if (!RenderedIsos.Contains(Country.Iso))
			{
				AddCountryPlaceholder(Country, Params, Buffer, ProjectLonLat);
			}
		}
	}
}

void FWLCampaignOverviewBuilder::Build(
	UProceduralMeshComponent* OverviewMesh,
	const FWLCampaignOverviewBuildParams& Params,
	TArray<FWLCampaignOverviewLabelSpec>& OutLabels,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	if (!OverviewMesh)
	{
		return;
	}

	OverviewMesh->ClearAllMeshSections();
	OutLabels.Reset();

	FWLCampaignAmericaLowDetailData AmericaData;
	if (!FWLCampaignAmericaLowDetailDataLoader::Load(AmericaData))
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CampaignOverview: no se pudo cargar AmericaLowDetail.json."));
		return;
	}

	FMeshBuffers Buffer;
	TSet<FString> RenderedIsos;
	AddCountriesFromGeoJson(Params, AmericaData.Countries, Buffer, RenderedIsos, ProjectLonLat);
	AddMissingCountryPlaceholders(AmericaData.Countries, RenderedIsos, Params, Buffer, ProjectLonLat);
	for (const FWLCampaignAmericaCitySpec& City : AmericaData.Cities)
	{
		AddCityMarker(Buffer, City, Params, ProjectLonLat);
	}
	AddCountryLabels(AmericaData.Countries, OutLabels);

	if (Buffer.Verts.IsEmpty() || Buffer.Tris.IsEmpty())
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CampaignOverview: sin vertices/tris para overview America. Countries=%d Cities=%d Rendered=%d."),
			AmericaData.Countries.Num(), AmericaData.Cities.Num(), RenderedIsos.Num());
		return;
	}

	Buffer.Normals.Init(FVector::UpVector, Buffer.Verts.Num());
	OverviewMesh->CreateMeshSection(0, Buffer.Verts, Buffer.Tris, Buffer.Normals, Buffer.UVs, Buffer.Colors, Buffer.Tangents, false);
	if (Params.Material)
	{
		OverviewMesh->SetMaterial(0, Params.Material);
	}
	UE_LOG(LogWorldLeader, Log, TEXT("CampaignOverview: America low-detail listo. Countries=%d Rendered=%d Cities=%d Verts=%d Tris=%d."),
		AmericaData.Countries.Num(), RenderedIsos.Num(), AmericaData.Cities.Num(), Buffer.Verts.Num(), Buffer.Tris.Num() / 3);
}
