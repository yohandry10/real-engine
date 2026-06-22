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
		if (Params.CityMarkerSize <= 0.f)
		{
			return;
		}

		const FVector Center = OverviewPoint(FVector2D(City.Lon, City.Lat), Params.CityZ, ProjectLonLat);
		const float Size = Params.CityMarkerSize * CityMarkerScale(City);
		AddCityMarkerShape(CityVisibleAtGlobal(City) ? GlobalMarkerBuffer : RegionalMarkerBuffer, City, Center, Size);
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
			const bool bCaribbean = FieldEquals(Country.ContinentalRegion, TEXT("Caribbean"));

			// Estandar de etiquetas: los nombres de PAIS solo se muestran en el zoom
			// lejano (Global / "America"). Al acercarse (Region) se ocultan y aparecen
			// las ciudades, para que nunca se encimen pais + ciudad.
			FWLCampaignOverviewLabelSpec Label;
			Label.Text = Country.DisplayName;
			Label.Lon = Country.LabelLon;
			Label.Lat = Country.LabelLat;
			Label.ZOffset = bCoreTheater ? 36000.f : 32000.f;
			Label.WorldSize = bCoreTheater
				? 22000.f
				: (bGlobalPriority
					? (IsSouthAmericaIso(Country.Iso) ? 16000.f : 14500.f)
					: (bCaribbean ? 8000.f : 11000.f));
			Label.Color = bCoreTheater ? FColor(235, 214, 126) : FColor(190, 212, 188);
			Label.bShowInGlobal = true;
			Label.bShowInRegion = false;
			OutLabels.Add(Label);
		}
	}

	void AddCityLabels(const TArray<FWLCampaignAmericaCitySpec>& Cities, TArray<FWLCampaignOverviewLabelSpec>& OutLabels)
	{
		for (const FWLCampaignAmericaCitySpec& City : Cities)
		{
			const bool bRegionalLabel = City.bCapital || City.bMajor || City.bPort || FieldEquals(City.MarkerType, TEXT("border"));
			if (!bRegionalLabel)
			{
				continue;
			}
			if (FieldEquals(City.Region, TEXT("Caribbean"))
				&& !City.bCapital
				&& !City.bMajor
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

			// Las CIUDADES solo aparecen al acercarse (Region); en Global se ven paises.
			// Mas grandes y con mas contraste para leerse bien sobre el terreno verde.
			FWLCampaignOverviewLabelSpec Label;
			Label.Text = City.Name;
			Label.Lon = City.Lon;
			Label.Lat = City.Lat;
			Label.ZOffset = City.bCapital ? 38600.f : 36600.f;
			Label.WorldSize = City.bCapital ? 4800.f : (City.bMajor ? 4200.f : 3800.f);
			Label.Color = City.bCapital
				? FColor(245, 224, 142)
				: (City.bPort ? FColor(150, 228, 234) : FColor(238, 242, 234));
			// Las ciudades del overview se ocultan en ambos zooms lejanos: los nombres de
			// ciudad solo aparecen al acercar (capa de teatro). En zoom lejano, solo paises.
			Label.bShowInGlobal = false;
			Label.bShowInRegion = false;
			OutLabels.Add(Label);
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
