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

	bool OverviewBoundsIntersect(float AMinLon, float AMaxLon, float AMinLat, float AMaxLat, float BMinLon, float BMaxLon, float BMinLat, float BMaxLat)
	{
		return AMinLon <= BMaxLon && AMaxLon >= BMinLon && AMinLat <= BMaxLat && AMaxLat >= BMinLat;
	}

	void GetOverviewRingBounds(const TArray<FVector2D>& Ring, float& MinLon, float& MaxLon, float& MinLat, float& MaxLat)
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

	float OverviewSignedArea(const TArray<FVector2D>& Poly)
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

	bool OverviewPointInTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
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

	void TriangulateOverviewSimplePolygon(const TArray<FVector2D>& Contour, TArray<int32>& OutTris)
	{
		OutTris.Reset();
		const int32 Num = Contour.Num();
		if (Num < 3)
		{
			return;
		}

		TArray<int32> V;
		V.Reserve(Num);
		const bool bClockwise = OverviewSignedArea(Contour) < 0.f;
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
					if (OverviewPointInTriangle(A, B, C, Contour[OtherIndex]))
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

	TArray<FVector2D> OverviewRingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring)
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

	bool FieldEquals(const FString& Value, const TCHAR* Expected)
	{
		return Value.Equals(Expected, ESearchCase::IgnoreCase);
	}

	bool FieldContains(const FString& Value, const TCHAR* Token)
	{
		return Value.Contains(Token, ESearchCase::IgnoreCase);
	}

	FLinearColor CountryFillColor(const FWLCampaignAmericaCountrySpec& Country)
	{
		if (FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Country.Iso))
		{
			return FLinearColor(0.210f, 0.350f, 0.170f, 0.98f);
		}
		if (Country.bSpecialTerritory || FieldContains(Country.PrimaryBiome, TEXT("arctic")))
		{
			return FLinearColor(0.118f, 0.190f, 0.185f, 0.92f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("cold")))
		{
			return FLinearColor(0.095f, 0.185f, 0.145f, 0.94f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("industrial")))
		{
			return FLinearColor(0.105f, 0.175f, 0.130f, 0.95f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("arid")) || FieldContains(Country.PrimaryBiome, TEXT("altiplano")))
		{
			return FLinearColor(0.205f, 0.220f, 0.125f, 0.95f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("island")))
		{
			return FLinearColor(0.135f, 0.245f, 0.165f, 0.95f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("amazon")))
		{
			return FLinearColor(0.075f, 0.245f, 0.105f, 0.96f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("andes")))
		{
			return FLinearColor(0.150f, 0.235f, 0.130f, 0.95f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("pampas")) || FieldContains(Country.PrimaryBiome, TEXT("temperate")))
		{
			return FLinearColor(0.150f, 0.225f, 0.135f, 0.95f);
		}
		if (FieldContains(Country.PrimaryBiome, TEXT("chaco")))
		{
			return FLinearColor(0.165f, 0.185f, 0.105f, 0.94f);
		}
		return FLinearColor(0.105f, 0.205f, 0.135f, 0.94f);
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
		TriangulateOverviewSimplePolygon(Ring, LocalTris);
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
		GetOverviewRingBounds(Ring, MinLon, MaxLon, MinLat, MaxLat);
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

	void AddPolylineRibbon(
		FMeshBuffers& Buffer,
		const TArray<FVector2D>& Points,
		const FLinearColor& Color,
		float Z,
		float Width,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			const FVector A = OverviewPoint(Points[Index], Z, ProjectLonLat);
			const FVector B = OverviewPoint(Points[Index + 1], Z, ProjectLonLat);
			const FVector Direction(B.X - A.X, B.Y - A.Y, 0.f);
			if (Direction.SizeSquared() < 1.f)
			{
				continue;
			}
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction.GetSafeNormal()) * Width;
			AddQuad(Buffer, A - Side, A + Side, B + Side, B - Side, Color);
		}
	}

	void AddTriangle(FMeshBuffers& Buffer, const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color)
	{
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({A, B, C});
		Buffer.UVs.Append({FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(0.5f, 1.f)});
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Buffer.Colors.Add(ToColor(Color));
		}
		Buffer.Tris.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 1});
	}

	void AddMarkerDiamond(FMeshBuffers& Buffer, const FVector& Center, float Size, const FLinearColor& Color)
	{
		AddQuad(Buffer,
			Center + FVector(-Size, 0.f, 0.f),
			Center + FVector(0.f, -Size, 0.f),
			Center + FVector(Size, 0.f, 0.f),
			Center + FVector(0.f, Size, 0.f),
			Color);
	}

	void AddMarkerSquare(FMeshBuffers& Buffer, const FVector& Center, float Size, const FLinearColor& Color)
	{
		AddQuad(Buffer,
			Center + FVector(-Size, -Size, 0.f),
			Center + FVector(Size, -Size, 0.f),
			Center + FVector(Size, Size, 0.f),
			Center + FVector(-Size, Size, 0.f),
			Color);
	}

	void AddCityMarkerShape(FMeshBuffers& Buffer, const FWLCampaignAmericaCitySpec& City, const FVector& Center, float Size)
	{
		const FVector RaisedCenter = Center + FVector(0.f, 0.f, 70.f);
		const FLinearColor DarkStroke(0.030f, 0.048f, 0.043f, 0.98f);

		if (FieldEquals(City.MarkerType, TEXT("capital")))
		{
			AddMarkerDiamond(Buffer, Center, Size * 1.34f, DarkStroke);
			AddMarkerDiamond(Buffer, RaisedCenter, Size * 0.94f, FLinearColor(0.900f, 0.690f, 0.250f, 0.98f));
			AddMarkerSquare(Buffer, RaisedCenter + FVector(0.f, 0.f, 50.f), Size * 0.28f, FLinearColor(0.960f, 0.880f, 0.610f, 1.f));
			return;
		}
		if (FieldEquals(City.MarkerType, TEXT("metropolis")))
		{
			AddMarkerSquare(Buffer, Center, Size * 1.06f, DarkStroke);
			AddMarkerSquare(Buffer, RaisedCenter, Size * 0.72f, FLinearColor(0.720f, 0.635f, 0.405f, 0.96f));
			AddMarkerDiamond(Buffer, RaisedCenter + FVector(0.f, 0.f, 50.f), Size * 0.42f, FLinearColor(0.945f, 0.800f, 0.420f, 0.98f));
			return;
		}
		if (FieldEquals(City.MarkerType, TEXT("port")) || City.bPort)
		{
			AddMarkerDiamond(Buffer, Center, Size * 1.02f, DarkStroke);
			AddTriangle(Buffer,
				RaisedCenter + FVector(-Size * 0.78f, Size * 0.54f, 0.f),
				RaisedCenter + FVector(Size * 0.78f, Size * 0.54f, 0.f),
				RaisedCenter + FVector(0.f, -Size * 0.86f, 0.f),
				FLinearColor(0.235f, 0.540f, 0.600f, 0.98f));
			AddQuad(Buffer,
				RaisedCenter + FVector(-Size * 0.86f, Size * 0.76f, 55.f),
				RaisedCenter + FVector(Size * 0.86f, Size * 0.76f, 55.f),
				RaisedCenter + FVector(Size * 0.86f, Size * 0.98f, 55.f),
				RaisedCenter + FVector(-Size * 0.86f, Size * 0.98f, 55.f),
				FLinearColor(0.860f, 0.770f, 0.520f, 0.95f));
			return;
		}
		if (FieldEquals(City.MarkerType, TEXT("border")))
		{
			AddMarkerDiamond(Buffer, Center, Size * 0.94f, DarkStroke);
			AddMarkerDiamond(Buffer, RaisedCenter, Size * 0.62f, FLinearColor(0.865f, 0.530f, 0.245f, 0.96f));
			return;
		}
		if (FieldEquals(City.MarkerType, TEXT("island")))
		{
			AddMarkerDiamond(Buffer, Center, Size * 0.82f, FLinearColor(0.042f, 0.070f, 0.062f, 0.94f));
			AddMarkerDiamond(Buffer, RaisedCenter, Size * 0.52f, FLinearColor(0.715f, 0.790f, 0.585f, 0.94f));
			return;
		}

		AddMarkerDiamond(Buffer, Center, Size * 0.78f, DarkStroke);
		AddMarkerDiamond(Buffer, RaisedCenter, Size * 0.48f, FLinearColor(0.590f, 0.665f, 0.510f, 0.88f));
	}

	bool CityVisibleAtGlobal(const FWLCampaignAmericaCitySpec& City)
	{
		return FieldEquals(City.VisibleFromZoom, TEXT("global"));
	}

	float CityMarkerScale(const FWLCampaignAmericaCitySpec& City)
	{
		if (FieldEquals(City.MarkerType, TEXT("capital")))
		{
			return City.bAdministrable ? 1.36f : 1.18f;
		}
		if (FieldEquals(City.MarkerType, TEXT("metropolis")))
		{
			return 1.06f;
		}
		if (FieldEquals(City.MarkerType, TEXT("port")) || City.bPort)
		{
			return 0.90f;
		}
		if (FieldEquals(City.MarkerType, TEXT("border")))
		{
			return 0.78f;
		}
		if (FieldEquals(City.MarkerType, TEXT("island")))
		{
			return 0.70f;
		}
		return 0.62f;
	}

	void AddCityMarker(
		FMeshBuffers& GlobalMarkerBuffer,
		FMeshBuffers& RegionalMarkerBuffer,
		const FWLCampaignAmericaCitySpec& City,
		const FWLCampaignOverviewBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		const FVector Center = OverviewPoint(FVector2D(City.Lon, City.Lat), Params.CityZ, ProjectLonLat);
		const float Size = Params.CityMarkerSize * CityMarkerScale(City);
		AddCityMarkerShape(CityVisibleAtGlobal(City) ? GlobalMarkerBuffer : RegionalMarkerBuffer, City, Center, Size);
	}

	void AddRegionalVisualOverlays(
		FMeshBuffers& Buffer,
		const FWLCampaignOverviewBuildParams& Params,
		TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
	{
		const float PatchZ = Params.LandZ + 620.f;
		AddPolylineRibbon(Buffer, {
			FVector2D(-91.5f, 15.2f), FVector2D(-88.0f, 14.5f), FVector2D(-85.5f, 13.0f),
			FVector2D(-83.8f, 10.2f), FVector2D(-80.5f, 8.8f)
		}, FLinearColor(0.145f, 0.205f, 0.105f, 0.82f), PatchZ + 260.f, 760.f, ProjectLonLat);
		AddPolylineRibbon(Buffer, {
			FVector2D(-82.5f, 23.1f), FVector2D(-77.5f, 20.8f), FVector2D(-72.0f, 19.0f),
			FVector2D(-66.3f, 18.2f), FVector2D(-61.1f, 14.1f), FVector2D(-61.6f, 10.7f)
		}, FLinearColor(0.070f, 0.390f, 0.405f, 0.54f), PatchZ + 180.f, 150.f, ProjectLonLat);
		AddPolylineRibbon(Buffer, {
			FVector2D(-76.0f, 8.5f), FVector2D(-78.5f, -1.5f), FVector2D(-76.0f, -10.0f),
			FVector2D(-70.2f, -18.0f), FVector2D(-70.5f, -32.0f), FVector2D(-71.2f, -45.0f)
		}, FLinearColor(0.205f, 0.180f, 0.120f, 0.88f), PatchZ + 430.f, 1250.f, ProjectLonLat);
		AddPolygon(Buffer, {
			FVector2D(-74.f, 6.f), FVector2D(-61.f, 8.f), FVector2D(-48.f, 3.f),
			FVector2D(-45.f, -8.f), FVector2D(-54.f, -17.f), FVector2D(-67.f, -13.f),
			FVector2D(-75.f, -4.f)
		}, FLinearColor(0.045f, 0.185f, 0.075f, 0.72f), PatchZ + 90.f, ProjectLonLat);
		AddPolygon(Buffer, {
			FVector2D(-66.f, -24.f), FVector2D(-55.f, -24.f), FVector2D(-51.f, -33.f),
			FVector2D(-59.f, -38.f), FVector2D(-67.f, -34.f)
		}, FLinearColor(0.125f, 0.165f, 0.100f, 0.70f), PatchZ + 70.f, ProjectLonLat);
		AddPolylineRibbon(Buffer, {
			FVector2D(-72.f, -40.f), FVector2D(-68.f, -45.f), FVector2D(-67.f, -51.f)
		}, FLinearColor(0.085f, 0.130f, 0.125f, 0.78f), PatchZ + 120.f, 1050.f, ProjectLonLat);
		AddPolylineRibbon(Buffer, {
			FVector2D(-49.f, -1.f), FVector2D(-39.f, -12.f), FVector2D(-43.f, -23.f),
			FVector2D(-48.f, -30.f)
		}, FLinearColor(0.080f, 0.190f, 0.100f, 0.74f), PatchZ + 220.f, 980.f, ProjectLonLat);
	}

	void AddCountryLabels(const TArray<FWLCampaignAmericaCountrySpec>& Countries, TArray<FWLCampaignOverviewLabelSpec>& OutLabels)
	{
		for (const FWLCampaignAmericaCountrySpec& Country : Countries)
		{
			const bool bCoreTheater = FWLCampaignAmericaLowDetailDataLoader::IsCoreTheaterIso(Country.Iso);
			const bool bGlobalPriority = bCoreTheater
				|| Country.Iso == TEXT("CA") || Country.Iso == TEXT("US") || Country.Iso == TEXT("MX")
				|| Country.Iso == TEXT("BR") || Country.Iso == TEXT("AR") || Country.Iso == TEXT("CL")
				|| Country.Iso == TEXT("PE") || Country.Iso == TEXT("EC") || Country.Iso == TEXT("BO")
				|| Country.Iso == TEXT("PA") || Country.Iso == TEXT("CU") || Country.Iso == TEXT("JM")
				|| Country.Iso == TEXT("TT") || Country.Iso == TEXT("GY");

			if (bGlobalPriority)
			{
				FWLCampaignOverviewLabelSpec GlobalLabel;
				GlobalLabel.Text = Country.DisplayName;
				GlobalLabel.Lon = Country.LabelLon;
				GlobalLabel.Lat = Country.LabelLat;
				GlobalLabel.ZOffset = bCoreTheater ? 36000.f : 32000.f;
				GlobalLabel.WorldSize = bCoreTheater ? 22000.f : (IsSouthAmericaIso(Country.Iso) ? 16000.f : 14500.f);
				GlobalLabel.Color = bCoreTheater ? FColor(235, 214, 126) : FColor(186, 210, 184);
				GlobalLabel.bShowInGlobal = true;
				GlobalLabel.bShowInRegion = false;
				OutLabels.Add(GlobalLabel);
			}

			FWLCampaignOverviewLabelSpec RegionLabel;
			RegionLabel.Text = Country.DisplayName;
			RegionLabel.Lon = Country.LabelLon;
			RegionLabel.Lat = Country.LabelLat;
			RegionLabel.ZOffset = bCoreTheater ? 34200.f : 31200.f;
			RegionLabel.WorldSize = bCoreTheater
				? 9800.f
				: (bGlobalPriority ? (IsSouthAmericaIso(Country.Iso) ? 7200.f : 6500.f) : 3600.f);
			if (FieldEquals(Country.ContinentalRegion, TEXT("Caribbean")) && !bGlobalPriority)
			{
				RegionLabel.WorldSize = 2600.f;
				RegionLabel.ZOffset = 32200.f;
			}
			RegionLabel.Color = bCoreTheater ? FColor(230, 210, 130) : FColor(170, 194, 166);
			RegionLabel.bShowInGlobal = false;
			RegionLabel.bShowInRegion = true;
			OutLabels.Add(RegionLabel);
		}
	}

	void AddCityLabels(const TArray<FWLCampaignAmericaCitySpec>& Cities, TArray<FWLCampaignOverviewLabelSpec>& OutLabels)
	{
		for (const FWLCampaignAmericaCitySpec& City : Cities)
		{
			const bool bGlobal = CityVisibleAtGlobal(City);
			const bool bRegionalLabel = City.bCapital || City.bMajor || City.bPort || FieldEquals(City.MarkerType, TEXT("border"));
			if (!bGlobal && !bRegionalLabel)
			{
				continue;
			}
			if (FieldEquals(City.Region, TEXT("Caribbean"))
				&& !bGlobal
				&& !City.CountryIso.Equals(TEXT("PA"), ESearchCase::IgnoreCase)
				&& !City.CountryIso.Equals(TEXT("CU"), ESearchCase::IgnoreCase)
				&& !City.CountryIso.Equals(TEXT("DO"), ESearchCase::IgnoreCase)
				&& !City.CountryIso.Equals(TEXT("JM"), ESearchCase::IgnoreCase)
				&& !City.CountryIso.Equals(TEXT("PR"), ESearchCase::IgnoreCase)
				&& !City.CountryIso.Equals(TEXT("TT"), ESearchCase::IgnoreCase)
				&& !City.CountryIso.Equals(TEXT("BS"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			if (bGlobal)
			{
				FWLCampaignOverviewLabelSpec GlobalLabel;
				GlobalLabel.Text = City.Name;
				GlobalLabel.Lon = City.Lon;
				GlobalLabel.Lat = City.Lat;
				GlobalLabel.ZOffset = City.bCapital ? 40200.f : 38200.f;
				GlobalLabel.WorldSize = City.bCapital ? 7600.f : 6500.f;
				GlobalLabel.Color = City.bCapital ? FColor(224, 205, 130) : FColor(205, 214, 190);
				GlobalLabel.bShowInGlobal = true;
				GlobalLabel.bShowInRegion = false;
				OutLabels.Add(GlobalLabel);
			}

			FWLCampaignOverviewLabelSpec RegionLabel;
			RegionLabel.Text = City.Name;
			RegionLabel.Lon = City.Lon;
			RegionLabel.Lat = City.Lat;
			RegionLabel.ZOffset = City.bCapital ? 38200.f : 36200.f;
			RegionLabel.WorldSize = City.bCapital ? 3300.f : (City.bMajor ? 3000.f : 2500.f);
			RegionLabel.Color = City.bCapital
				? FColor(224, 205, 130)
				: (City.bPort ? FColor(160, 215, 218) : FColor(205, 214, 190));
			RegionLabel.bShowInGlobal = false;
			RegionLabel.bShowInRegion = true;
			OutLabels.Add(RegionLabel);
		}
	}

	bool ShouldUseOverviewRingForCountry(const FString& Iso, const TArray<FVector2D>& Ring)
	{
		float MinLon, MaxLon, MinLat, MaxLat;
		GetOverviewRingBounds(Ring, MinLon, MaxLon, MinLat, MaxLat);
		if (Iso == TEXT("GF"))
		{
			return OverviewBoundsIntersect(MinLon, MaxLon, MinLat, MaxLat, -55.5f, -50.5f, 1.0f, 6.5f);
		}
		if (MaxLon > -10.f || MinLon < -172.f)
		{
			return false;
		}
		return OverviewBoundsIntersect(MinLon, MaxLon, MinLat, MaxLat, -172.f, -10.f, -56.5f, 84.5f);
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

		TArray<FVector2D> Ring = SimplifyRingForCountry(Iso, OverviewRingFromJson(*Outer));
		if (Ring.Num() < 3 || !ShouldUseOverviewRingForCountry(Iso, Ring))
		{
			return false;
		}

		const FLinearColor Fill = CountryFillColor(Config);
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
		const FLinearColor Fill = CountryFillColor(Country).Desaturate(0.18f);
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

	void CreateMeshSectionIfNotEmpty(
		UProceduralMeshComponent* Mesh,
		int32 SectionIndex,
		FMeshBuffers& Buffer,
		UMaterialInterface* Material)
	{
		if (!Mesh || Buffer.Verts.IsEmpty() || Buffer.Tris.IsEmpty())
		{
			return;
		}

		Buffer.Normals.Init(FVector::UpVector, Buffer.Verts.Num());
		Mesh->CreateMeshSection(SectionIndex, Buffer.Verts, Buffer.Tris, Buffer.Normals, Buffer.UVs, Buffer.Colors, Buffer.Tangents, false);
		if (Material)
		{
			Mesh->SetMaterial(SectionIndex, Material);
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

	FMeshBuffers LandBuffer;
	FMeshBuffers GlobalMarkerBuffer;
	FMeshBuffers RegionalMarkerBuffer;
	TSet<FString> RenderedIsos;
	AddCountriesFromGeoJson(Params, AmericaData.Countries, LandBuffer, RenderedIsos, ProjectLonLat);
	AddMissingCountryPlaceholders(AmericaData.Countries, RenderedIsos, Params, LandBuffer, ProjectLonLat);
	AddRegionalVisualOverlays(LandBuffer, Params, ProjectLonLat);
	for (const FWLCampaignAmericaCitySpec& City : AmericaData.Cities)
	{
		AddCityMarker(GlobalMarkerBuffer, RegionalMarkerBuffer, City, Params, ProjectLonLat);
	}
	AddCountryLabels(AmericaData.Countries, OutLabels);
	AddCityLabels(AmericaData.Cities, OutLabels);

	if (LandBuffer.Verts.IsEmpty() || LandBuffer.Tris.IsEmpty())
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("CampaignOverview: sin vertices/tris para overview America. Countries=%d Cities=%d Rendered=%d."),
			AmericaData.Countries.Num(), AmericaData.Cities.Num(), RenderedIsos.Num());
		return;
	}

	CreateMeshSectionIfNotEmpty(OverviewMesh, 0, LandBuffer, Params.Material);
	CreateMeshSectionIfNotEmpty(OverviewMesh, 1, GlobalMarkerBuffer, Params.Material);
	CreateMeshSectionIfNotEmpty(OverviewMesh, 2, RegionalMarkerBuffer, Params.Material);
	UE_LOG(LogWorldLeader, Log, TEXT("CampaignOverview: America low-detail listo. Countries=%d Rendered=%d Cities=%d LandVerts=%d GlobalMarkers=%d RegionalMarkers=%d."),
		AmericaData.Countries.Num(),
		RenderedIsos.Num(),
		AmericaData.Cities.Num(),
		LandBuffer.Verts.Num(),
		GlobalMarkerBuffer.Verts.Num(),
		RegionalMarkerBuffer.Verts.Num());
}
