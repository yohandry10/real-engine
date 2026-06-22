// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"
#include "Presentation/WLCampaignAmericaLowDetailData.h"
#include "ProceduralMeshComponent.h"
#include "Dom/JsonObject.h"

namespace WLCampaignTerritoryLayerPrivate
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

	inline FColor ToFColor(const FLinearColor& Color)
	{
		return Color.ToFColor(false);
	}

	inline void AddRibbon(FMeshBuildBuffer& Buffer, const FVector& A, const FVector& B, float WidthWorld, const FLinearColor& Color)
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

	inline float PolygonArea(const TArray<FVector2D>& Poly)
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

	inline bool PointInTriangle2D(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& P)
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

	inline void TerritoryLayerTriangulateSimplePolygon(const TArray<FVector2D>& Contour, TArray<int32>& OutTris)
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

	inline FVector2D PolygonCenter(const TArray<FVector2D>& Poly)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& P : Poly)
		{
			Sum += P;
		}
		return Poly.IsEmpty() ? FVector2D::ZeroVector : Sum / static_cast<float>(Poly.Num());
	}

	inline TArray<FVector2D> SimplifyRing(const TArray<FVector2D>& Ring, int32 TargetPoints)
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

	inline TArray<FVector2D> RingFromJson(const TArray<TSharedPtr<FJsonValue>>& Ring)
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

	inline void GetRingBounds(const TArray<FVector2D>& Ring, float& MinLon, float& MaxLon, float& MinLat, float& MaxLat)
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

	inline bool BoundsIntersect(float AMinLon, float AMaxLon, float AMinLat, float AMaxLat, float BMinLon, float BMaxLon, float BMinLat, float BMaxLat)
	{
		return AMinLon <= BMaxLon && AMaxLon >= BMinLon && AMinLat <= BMaxLat && AMaxLat >= BMinLat;
	}

	inline bool PointInWorldPolygonXY(const TArray<FVector>& Polygon, const FVector& Point)
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

	inline bool WorldPolygonBoundsContainsXY(const TArray<FVector>& Polygon, const FVector& Point, float Padding)
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

	inline float WorldPolygonBoundsAreaXY(const TArray<FVector>& Polygon)
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

	inline bool ShouldUseRingForCountry(const FString& Iso, const TArray<FVector2D>& Ring)
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

	inline TMap<FString, FWLCampaignAmericaCountrySpec> MakeAmericaCountryMap()
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

	inline TArray<FVector2D> StylizedBoundsPolygon(float MinLon, float MaxLon, float MinLat, float MaxLat)
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

	inline FString JoinJsonStringArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName)
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
