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
