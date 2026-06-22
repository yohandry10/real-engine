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
#include "Presentation/WLCampaignTerritoryLayerPrivate.h"

using namespace WLCampaignTerritoryLayerPrivate;

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

			// El contorno de pais lo dibuja el BoundaryMesh del terreno (anillo completo,
			// recortado a la region -> linea unica y limpia). Aqui ya no trazamos un borde
			// nacional para no duplicar la linea ni generar segmentos que cruzan el oceano
			// (el anillo simplificado + segmento de cierre los producia).

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
