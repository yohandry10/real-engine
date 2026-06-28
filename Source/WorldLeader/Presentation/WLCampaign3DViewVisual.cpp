// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

#include "WorldLeader.h"
#include "Campaign/WLDataRegistry.h"
#include "Presentation/WLCampaignOverviewBuilder.h"
#include "Presentation/WLCampaignRegionGeometry.h"
#include "Presentation/WLCampaignRouteBuilder.h"
#include "Presentation/WLCampaignSettlementBuilder.h"
#include "Presentation/WLCampaignTerrainBuilder.h"
#include "Presentation/WLCampaignVisualStyle.h"
#include "Presentation/WLCampaignWaterBuilder.h"
#include "Presentation/WLCampaign3DViewRoadDetail.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ConstructorHelpers.h"
#include "UnrealClient.h"

namespace
{
	float GridHash01(int32 X, int32 Y, int32 Salt)
	{
		const float Seed = static_cast<float>((X + 101) * 12.9898 + (Y + 211) * 78.233 + Salt * 37.719);
		return FMath::Frac(FMath::Abs(FMath::Sin(Seed) * 43758.5453f));
	}

	FString SettlementTypeToPanelText(EWLCampaignSettlementType Type)
	{
		switch (Type)
		{
		case EWLCampaignSettlementType::Capital: return TEXT("capital");
		case EWLCampaignSettlementType::LargeCity: return TEXT("large city");
		case EWLCampaignSettlementType::Port: return TEXT("port");
		case EWLCampaignSettlementType::Frontier: return TEXT("border city");
		case EWLCampaignSettlementType::Industrial: return TEXT("industrial city");
		default: return TEXT("settlement");
		}
	}

	FString SettlementStrategicRole(EWLCampaignSettlementType Type)
	{
		switch (Type)
		{
		case EWLCampaignSettlementType::Capital: return TEXT("capital command hub");
		case EWLCampaignSettlementType::LargeCity: return TEXT("urban economic hub");
		case EWLCampaignSettlementType::Port: return TEXT("port and coastal logistics");
		case EWLCampaignSettlementType::Frontier: return TEXT("border control point");
		case EWLCampaignSettlementType::Industrial: return TEXT("industry and energy hub");
		default: return TEXT("regional settlement");
		}
	}


	constexpr float BorderOutpostMinCityClearanceKm = 14.f;
	constexpr float BorderOutpostDesiredRoadOffsetKm = 12.f;

	float LonLatDistanceKm(const FVector2D& A, const FVector2D& B)
	{
		const float AvgLatRad = FMath::DegreesToRadians((A.Y + B.Y) * 0.5f);
		const float KmNorth = (A.Y - B.Y) * 111.32f;
		const float KmEast = (A.X - B.X) * 111.32f * FMath::Cos(AvgLatRad);
		return FMath::Sqrt(KmNorth * KmNorth + KmEast * KmEast);
	}

	FVector2D WorldToLonLatForCampaign(
		const FVector& World,
		const FVector2D& TheaterCenterLonLat,
		float DetailWorldUnitsPerKm)
	{
		const float SafeUnitsPerKm = FMath::Max(1.f, DetailWorldUnitsPerKm);
		const float OriginLatRad = FMath::DegreesToRadians(TheaterCenterLonLat.Y);
		const float CosLat = FMath::Max(0.05f, FMath::Cos(OriginLatRad));
		const float Lat = TheaterCenterLonLat.Y + World.X / (111.32f * SafeUnitsPerKm);
		const float Lon = TheaterCenterLonLat.X + World.Y / (111.32f * CosLat * SafeUnitsPerKm);
		return FVector2D(Lon, Lat);
	}

	struct FBorderOutpostMeshBuffer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	FColor OutpostColor(const FLinearColor& Base, float Shade)
	{
		FLinearColor C(Base.R * Shade, Base.G * Shade, Base.B * Shade, Base.A);
		C.R = FMath::Clamp(C.R, 0.f, 1.f);
		C.G = FMath::Clamp(C.G, 0.f, 1.f);
		C.B = FMath::Clamp(C.B, 0.f, 1.f);
		return C.ToFColor(false);
	}

	void AddOutpostFace(
		FBorderOutpostMeshBuffer& Buffer,
		const FVector& A,
		const FVector& B,
		const FVector& C,
		const FVector& D,
		const FVector& Normal,
		const FColor& Color)
	{
		const int32 Base = Buffer.Verts.Num();
		Buffer.Verts.Append({ A, B, C, D });
		Buffer.Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Buffer.Normals.Append({ Normal, Normal, Normal, Normal });
		Buffer.UVs.Append({ FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(1.f, 1.f), FVector2D(0.f, 1.f) });
		Buffer.Colors.Append({ Color, Color, Color, Color });
		Buffer.Tangents.Append({ FProcMeshTangent(1.f, 0.f, 0.f), FProcMeshTangent(1.f, 0.f, 0.f), FProcMeshTangent(1.f, 0.f, 0.f), FProcMeshTangent(1.f, 0.f, 0.f) });
	}

	void AddOutpostBox(
		FBorderOutpostMeshBuffer& Buffer,
		const FVector& BottomCenter,
		const FRotator& Rotation,
		const FVector& Size,
		const FLinearColor& Color)
	{
		const FVector X = Rotation.RotateVector(FVector::ForwardVector);
		const FVector Y = Rotation.RotateVector(FVector::RightVector);
		const FVector Z = FVector::UpVector;
		const float HX = Size.X * 0.5f;
		const float HY = Size.Y * 0.5f;
		const float HZ = Size.Z;
		const FVector P000 = BottomCenter - X * HX - Y * HY;
		const FVector P100 = BottomCenter + X * HX - Y * HY;
		const FVector P110 = BottomCenter + X * HX + Y * HY;
		const FVector P010 = BottomCenter - X * HX + Y * HY;
		const FVector P001 = P000 + Z * HZ;
		const FVector P101 = P100 + Z * HZ;
		const FVector P111 = P110 + Z * HZ;
		const FVector P011 = P010 + Z * HZ;

		AddOutpostFace(Buffer, P001, P101, P111, P011, Z, OutpostColor(Color, 1.12f));
		AddOutpostFace(Buffer, P000, P010, P110, P100, -Z, OutpostColor(Color, 0.58f));
		AddOutpostFace(Buffer, P100, P101, P111, P110, X, OutpostColor(Color, 0.92f));
		AddOutpostFace(Buffer, P010, P011, P001, P000, -X, OutpostColor(Color, 0.74f));
		AddOutpostFace(Buffer, P110, P111, P011, P010, Y, OutpostColor(Color, 1.00f));
		AddOutpostFace(Buffer, P000, P001, P101, P100, -Y, OutpostColor(Color, 0.80f));
	}

	void CommitOutpostSection(
		UProceduralMeshComponent* Mesh,
		UMaterialInterface* Material,
		const FBorderOutpostMeshBuffer& Buffer)
	{
		if (!Mesh || Buffer.Verts.IsEmpty())
		{
			return;
		}
		const int32 SectionIndex = Mesh->GetNumSections();
		Mesh->CreateMeshSection(
			SectionIndex,
			Buffer.Verts,
			Buffer.Tris,
			Buffer.Normals,
			Buffer.UVs,
			Buffer.Colors,
			Buffer.Tangents,
			false);
		if (Material)
		{
			Mesh->SetMaterial(SectionIndex, Material);
		}
	}
}

#include "Presentation/WLCampaign3DViewSouthAmericaSettlements.inl"
#include "Presentation/WLCampaign3DViewTheaterSettlements.inl"

void AWLCampaign3DView::BuildCampaignVisualLayer()
{
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D visual layer start."));

	// Geometria de tierra para empujar las ciudades costeras hacia el interior
	// (evita que los marcadores queden sobre la costa/el mar).
	SettlementLandGeometry.Reset();
	FWLCampaignRegionGeometry::LoadCountries(
		RegionMinLon - 3.f, RegionMaxLon + 3.f, RegionMinLat - 3.f, RegionMaxLat + 3.f,
		SettlementLandGeometry);
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D visual layer phase: land geometry=%d."), SettlementLandGeometry.Num());

	// Lago de Maracaibo: cuerpo ovalado + estrecho al norte que conecta con el Golfo
	// (como en la realidad). Z bajo (ras de la cuenca hundida) para que sea agua sobre el
	// terreno, NO una lamina flotante que tapaba la ciudad. Maracaibo va en la ribera oeste.
	// La pre-pasada de flat pads NO construye biomas/carreteras/etc (eso necesita el terreno ya
	// mallado y solo debe ocurrir una vez). Solo registra los pads via AddSettlementCluster.
	if (!bCollectFlatPadsOnly)
	{
	AddBiomePatch({
		FVector2D(-71.62f, 10.85f), // boca del estrecho (NO, abre al golfo)
		FVector2D(-71.66f, 10.55f), // estrecho O
		FVector2D(-71.85f, 10.20f), // ribera O alta
		FVector2D(-71.95f, 9.70f),  // ribera O media
		FVector2D(-71.85f, 9.20f),  // ribera O baja
		FVector2D(-71.55f, 8.92f),  // sur O
		FVector2D(-71.20f, 8.95f),  // sur E
		FVector2D(-71.02f, 9.35f),  // ribera E baja
		FVector2D(-71.00f, 9.95f),  // ribera E media
		FVector2D(-71.14f, 10.35f), // ribera E alta
		FVector2D(-71.34f, 10.55f), // estrecho E
		FVector2D(-71.45f, 10.85f)  // boca del estrecho (NE)
	}, FLinearColor(0.020f, 0.430f, 0.520f, 0.92f), 420.f);

	// Dos mallas de carretera: una continua para teatro (130k/210k, sin ciudades visibles) y
	// otra recortada para cercano (90k, con ciudades visibles para que la ruta no atraviese edificios).
	// Mascara de tierra: la cinta NO se dibuja sobre el mar (la ruta costera flotaba sobre el agua a
	// Z+990). Si no hay geometria de tierra cargada, no recorta (devuelve true).
	FWLCampaignRouteBuilder::SetRoadLandMask([this](float Lon, float Lat) -> bool
	{
		if (SettlementLandGeometry.Num() == 0)
		{
			return true;
		}
		for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
		{
			if (FWLCampaignRegionGeometry::PointInAnyLonLatRing(FVector2D(Lon, Lat), Country.Rings))
			{
				return true;
			}
		}
		return false;
	});

	FWLCampaignRouteBuilder::SetCityClipCircles({});
	FWLCampaignRouteBuilder::BuildDefaultTheaterRoutes(
		RoadMesh,
		VertexColorMaterial,
		[this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		});

	{
		TArray<FVector> RoadClipCircles;
		RoadClipCircles.Reserve(CityFlatPads.Num());
		for (const FCityFlatPad& Pad : CityFlatPads)
		{
			RoadClipCircles.Add(FVector(Pad.Lon, Pad.Lat, Pad.FlatRadiusDeg * 0.72f));
		}
		FWLCampaignRouteBuilder::SetCityClipCircles(RoadClipCircles);
	}
	if (UProceduralMeshComponent* RoadDetailMesh = WLRoadDetail::FindRoadDetailMesh(this))
	{
		FWLCampaignRouteBuilder::BuildDefaultTheaterRoutes(
			RoadDetailMesh,
			VertexColorMaterial,
			[this](float Lon, float Lat)
			{
				return ProjectLonLat(Lon, Lat);
			});
	}
	}

	const auto AddTheaterCountryLabel = [this](const FString& Text, float Lon, float Lat, const FColor& Color, float WorldSize)
	{
		if (bCollectFlatPadsOnly)
		{
			return;
		}
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		if (!Label)
		{
			return;
		}
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetWorldLocation(ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, 11800.f));
		Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(WorldSize);
		Label->SetText(FText::FromString(Text));
		Label->SetTextRenderColor(Color);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Labels.Add(Label);
		CountryLabelBaseSizes.Add(WorldSize);
	};

	AddTheaterCountryLabel(TEXT("COLOMBIA"), -74.2f, 5.7f, FColor(232, 206, 126), 3400.f);
	AddTheaterCountryLabel(TEXT("VENEZUELA"), -66.4f, 7.8f, FColor(232, 206, 126), 3400.f);

	BuildTheaterSettlementLayer();

	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D visual layer phase: CO/VE settlements ready. Cities=%d."), CityViews.Num());

	// Lote 1 (Andes norte) - Ecuador. Mismo pipeline que CO/VE; CO/VE quedan intactos.
	AddTheaterCountryLabel(TEXT("ECUADOR"), -78.3f, -1.45f, FColor(232, 206, 126), 2400.f);
	AddSettlementCluster(TEXT("EC-TULCAN"), TEXT("Tulcan"), TEXT("EC"), TEXT("EC-CAR"), TEXT("Carchi"), -77.72f, 0.81f, EWLCampaignSettlementType::Frontier, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-IBARRA"), TEXT("Ibarra"), TEXT("EC"), TEXT("EC-IMB"), TEXT("Imbabura"), -78.12f, 0.35f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-QUITO"), TEXT("Quito"), TEXT("EC"), TEXT("EC-PIC"), TEXT("Pichincha"), -78.52f, -0.22f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("EC-GUAYAQUIL"), TEXT("Guayaquil"), TEXT("EC"), TEXT("EC-GUA"), TEXT("Guayas"), -79.90f, -2.19f, EWLCampaignSettlementType::Port, FLinearColor(0.76f, 0.66f, 0.44f));
	AddSettlementCluster(TEXT("EC-CUENCA"), TEXT("Cuenca"), TEXT("EC"), TEXT("EC-AZU"), TEXT("Azuay"), -79.00f, -2.90f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-SANTO-DOMINGO"), TEXT("Santo Domingo"), TEXT("EC"), TEXT("EC-SDT"), TEXT("Santo Domingo"), -79.17f, -0.25f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-MANTA"), TEXT("Manta"), TEXT("EC"), TEXT("EC-MAN"), TEXT("Manabi"), -80.72f, -0.95f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("EC-MACHALA"), TEXT("Machala"), TEXT("EC"), TEXT("EC-ELO"), TEXT("El Oro"), -79.97f, -3.26f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("EC-AMBATO"), TEXT("Ambato"), TEXT("EC"), TEXT("EC-TUN"), TEXT("Tungurahua"), -78.62f, -1.24f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("EC-LOJA"), TEXT("Loja"), TEXT("EC"), TEXT("EC-LOJ"), TEXT("Loja"), -79.20f, -3.99f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	// Lote 1 (Andes norte) - Peru.
	AddTheaterCountryLabel(TEXT("PERU"), -75.5f, -10.0f, FColor(232, 206, 126), 4200.f);
	AddSettlementCluster(TEXT("PE-LIMA"), TEXT("Lima"), TEXT("PE"), TEXT("PE-LIM"), TEXT("Lima"), -77.04f, -12.05f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("PE-TRUJILLO"), TEXT("Trujillo"), TEXT("PE"), TEXT("PE-LAL"), TEXT("La Libertad"), -79.03f, -8.11f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("PE-CHICLAYO"), TEXT("Chiclayo"), TEXT("PE"), TEXT("PE-LAM"), TEXT("Lambayeque"), -79.84f, -6.77f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("PE-PIURA"), TEXT("Piura"), TEXT("PE"), TEXT("PE-PIU"), TEXT("Piura"), -80.63f, -5.19f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("PE-AREQUIPA"), TEXT("Arequipa"), TEXT("PE"), TEXT("PE-ARE"), TEXT("Arequipa"), -71.54f, -16.41f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("PE-CUSCO"), TEXT("Cusco"), TEXT("PE"), TEXT("PE-CUS"), TEXT("Cusco"), -71.97f, -13.53f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("PE-IQUITOS"), TEXT("Iquitos"), TEXT("PE"), TEXT("PE-LOR"), TEXT("Loreto"), -73.25f, -3.75f, EWLCampaignSettlementType::Port, FLinearColor(0.66f, 0.62f, 0.46f));
	AddSettlementCluster(TEXT("PE-PUNO"), TEXT("Puno"), TEXT("PE"), TEXT("PE-PUN"), TEXT("Puno"), -70.02f, -15.84f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	// Lote 1 (Andes norte) - Bolivia. Mediterranea (sin puertos).
	AddTheaterCountryLabel(TEXT("BOLIVIA"), -65.0f, -17.4f, FColor(232, 206, 126), 3600.f);
	AddSettlementCluster(TEXT("BO-LA-PAZ"), TEXT("La Paz"), TEXT("BO"), TEXT("BO-LPZ"), TEXT("La Paz"), -68.15f, -16.50f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("BO-SANTA-CRUZ"), TEXT("Santa Cruz"), TEXT("BO"), TEXT("BO-SCZ"), TEXT("Santa Cruz"), -63.18f, -17.78f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BO-COCHABAMBA"), TEXT("Cochabamba"), TEXT("BO"), TEXT("BO-CBB"), TEXT("Cochabamba"), -66.16f, -17.39f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BO-SUCRE"), TEXT("Sucre"), TEXT("BO"), TEXT("BO-CHU"), TEXT("Chuquisaca"), -65.26f, -19.03f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BO-POTOSI"), TEXT("Potosi"), TEXT("BO"), TEXT("BO-PTS"), TEXT("Potosi"), -65.75f, -19.58f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BO-ORURO"), TEXT("Oruro"), TEXT("BO"), TEXT("BO-ORU"), TEXT("Oruro"), -67.10f, -17.98f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BO-TARIJA"), TEXT("Tarija"), TEXT("BO"), TEXT("BO-TJA"), TEXT("Tarija"), -64.73f, -21.53f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	// Lote 2 - Brasil.
	AddTheaterCountryLabel(TEXT("BRASIL"), -50.0f, -10.0f, FColor(232, 206, 126), 6000.f);
	AddSettlementCluster(TEXT("BR-BRASILIA"), TEXT("Brasilia"), TEXT("BR"), TEXT("BR-DF"), TEXT("Distrito Federal"), -47.88f, -15.79f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("BR-SAO-PAULO"), TEXT("Sao Paulo"), TEXT("BR"), TEXT("BR-SP"), TEXT("Sao Paulo"), -46.63f, -23.55f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BR-RIO"), TEXT("Rio de Janeiro"), TEXT("BR"), TEXT("BR-RJ"), TEXT("Rio de Janeiro"), -43.20f, -22.91f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-BELO-HORIZONTE"), TEXT("Belo Horizonte"), TEXT("BR"), TEXT("BR-MG"), TEXT("Minas Gerais"), -43.94f, -19.92f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("BR-SALVADOR"), TEXT("Salvador"), TEXT("BR"), TEXT("BR-BA"), TEXT("Bahia"), -38.51f, -12.97f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-FORTALEZA"), TEXT("Fortaleza"), TEXT("BR"), TEXT("BR-CE"), TEXT("Ceara"), -38.54f, -3.73f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-RECIFE"), TEXT("Recife"), TEXT("BR"), TEXT("BR-PE"), TEXT("Pernambuco"), -34.88f, -8.05f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-MANAUS"), TEXT("Manaus"), TEXT("BR"), TEXT("BR-AM"), TEXT("Amazonas"), -60.02f, -3.10f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("BR-BELEM"), TEXT("Belem"), TEXT("BR"), TEXT("BR-PA"), TEXT("Para"), -48.50f, -1.46f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-PORTO-ALEGRE"), TEXT("Porto Alegre"), TEXT("BR"), TEXT("BR-RS"), TEXT("Rio Grande do Sul"), -51.23f, -30.03f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("BR-CURITIBA"), TEXT("Curitiba"), TEXT("BR"), TEXT("BR-PR"), TEXT("Parana"), -49.27f, -25.43f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));

	// Lote 3 - Cono Sur (Argentina, Chile, Uruguay, Paraguay).
	AddTheaterCountryLabel(TEXT("ARGENTINA"), -65.0f, -38.0f, FColor(232, 206, 126), 6000.f);
	AddSettlementCluster(TEXT("AR-BUENOS-AIRES"), TEXT("Buenos Aires"), TEXT("AR"), TEXT("AR-BA"), TEXT("Buenos Aires"), -58.38f, -34.60f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("AR-CORDOBA"), TEXT("Cordoba"), TEXT("AR"), TEXT("AR-CB"), TEXT("Cordoba"), -64.18f, -31.42f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("AR-ROSARIO"), TEXT("Rosario"), TEXT("AR"), TEXT("AR-SF"), TEXT("Santa Fe"), -60.64f, -32.95f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("AR-MENDOZA"), TEXT("Mendoza"), TEXT("AR"), TEXT("AR-MZ"), TEXT("Mendoza"), -68.84f, -32.89f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("AR-MAR-DEL-PLATA"), TEXT("Mar del Plata"), TEXT("AR"), TEXT("AR-BAP"), TEXT("Buenos Aires"), -57.55f, -38.00f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("AR-TUCUMAN"), TEXT("Tucuman"), TEXT("AR"), TEXT("AR-TM"), TEXT("Tucuman"), -65.22f, -26.82f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("AR-COMODORO"), TEXT("Comodoro Rivadavia"), TEXT("AR"), TEXT("AR-CT"), TEXT("Chubut"), -67.48f, -45.86f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("AR-USHUAIA"), TEXT("Ushuaia"), TEXT("AR"), TEXT("AR-TF"), TEXT("Tierra del Fuego"), -68.30f, -54.80f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	AddTheaterCountryLabel(TEXT("CHILE"), -72.0f, -38.5f, FColor(232, 206, 126), 2800.f);
	AddSettlementCluster(TEXT("CL-SANTIAGO"), TEXT("Santiago"), TEXT("CL"), TEXT("CL-RM"), TEXT("Region Metropolitana"), -70.65f, -33.45f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CL-VALPARAISO"), TEXT("Valparaiso"), TEXT("CL"), TEXT("CL-VS"), TEXT("Valparaiso"), -71.62f, -33.05f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CL-CONCEPCION"), TEXT("Concepcion"), TEXT("CL"), TEXT("CL-BI"), TEXT("Biobio"), -73.05f, -36.83f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CL-ANTOFAGASTA"), TEXT("Antofagasta"), TEXT("CL"), TEXT("CL-AN"), TEXT("Antofagasta"), -70.40f, -23.65f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("CL-PUERTO-MONTT"), TEXT("Puerto Montt"), TEXT("CL"), TEXT("CL-LL"), TEXT("Los Lagos"), -72.94f, -41.47f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CL-PUNTA-ARENAS"), TEXT("Punta Arenas"), TEXT("CL"), TEXT("CL-MA"), TEXT("Magallanes"), -70.92f, -53.16f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));

	AddTheaterCountryLabel(TEXT("URUGUAY"), -56.0f, -33.0f, FColor(232, 206, 126), 2200.f);
	AddSettlementCluster(TEXT("UY-MONTEVIDEO"), TEXT("Montevideo"), TEXT("UY"), TEXT("UY-MO"), TEXT("Montevideo"), -56.16f, -34.90f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("UY-SALTO"), TEXT("Salto"), TEXT("UY"), TEXT("UY-SA"), TEXT("Salto"), -57.96f, -31.38f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("UY-PUNTA-DEL-ESTE"), TEXT("Punta del Este"), TEXT("UY"), TEXT("UY-MA"), TEXT("Maldonado"), -54.95f, -34.97f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	AddTheaterCountryLabel(TEXT("PARAGUAY"), -58.0f, -23.5f, FColor(232, 206, 126), 2800.f);
	AddSettlementCluster(TEXT("PY-ASUNCION"), TEXT("Asuncion"), TEXT("PY"), TEXT("PY-AS"), TEXT("Central"), -57.64f, -25.30f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("PY-CIUDAD-DEL-ESTE"), TEXT("Ciudad del Este"), TEXT("PY"), TEXT("PY-AP"), TEXT("Alto Parana"), -54.61f, -25.51f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("PY-ENCARNACION"), TEXT("Encarnacion"), TEXT("PY"), TEXT("PY-IT"), TEXT("Itapua"), -55.87f, -27.33f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));

	// Escudo guayanes - Guyana y Surinam.
	AddTheaterCountryLabel(TEXT("GUYANA"), -58.9f, 4.8f, FColor(232, 206, 126), 2000.f);
	AddSettlementCluster(TEXT("GY-GEORGETOWN"), TEXT("Georgetown"), TEXT("GY"), TEXT("GY-DE"), TEXT("Demerara-Mahaica"), -58.16f, 6.80f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("GY-LINDEN"), TEXT("Linden"), TEXT("GY"), TEXT("GY-UD"), TEXT("Upper Demerara"), -58.30f, 6.00f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));

	AddTheaterCountryLabel(TEXT("SURINAM"), -55.9f, 4.0f, FColor(232, 206, 126), 2000.f);
	AddSettlementCluster(TEXT("SR-PARAMARIBO"), TEXT("Paramaribo"), TEXT("SR"), TEXT("SR-PM"), TEXT("Paramaribo"), -55.17f, 5.87f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("SR-NICKERIE"), TEXT("Nieuw Nickerie"), TEXT("SR"), TEXT("SR-NI"), TEXT("Nickerie"), -56.97f, 5.95f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Lote 4 - Mexico.
	AddTheaterCountryLabel(TEXT("MEXICO"), -102.0f, 23.5f, FColor(232, 206, 126), 6000.f);
	AddSettlementCluster(TEXT("MX-CIUDAD-DE-MEXICO"), TEXT("Ciudad de Mexico"), TEXT("MX"), TEXT("MX-CMX"), TEXT("Ciudad de Mexico"), -99.13f, 19.43f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("MX-GUADALAJARA"), TEXT("Guadalajara"), TEXT("MX"), TEXT("MX-JAL"), TEXT("Jalisco"), -103.35f, 20.67f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("MX-MONTERREY"), TEXT("Monterrey"), TEXT("MX"), TEXT("MX-NLE"), TEXT("Nuevo Leon"), -100.31f, 25.67f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("MX-TIJUANA"), TEXT("Tijuana"), TEXT("MX"), TEXT("MX-BCN"), TEXT("Baja California"), -117.04f, 32.51f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("MX-CANCUN"), TEXT("Cancun"), TEXT("MX"), TEXT("MX-ROO"), TEXT("Quintana Roo"), -86.85f, 21.16f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("MX-VERACRUZ"), TEXT("Veracruz"), TEXT("MX"), TEXT("MX-VER"), TEXT("Veracruz"), -96.13f, 19.17f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("MX-MERIDA"), TEXT("Merida"), TEXT("MX"), TEXT("MX-YUC"), TEXT("Yucatan"), -89.59f, 20.97f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("MX-CIUDAD-JUAREZ"), TEXT("Ciudad Juarez"), TEXT("MX"), TEXT("MX-CHH"), TEXT("Chihuahua"), -106.49f, 31.74f, EWLCampaignSettlementType::Frontier, FLinearColor(0.90f, 0.72f, 0.34f));
	AddSettlementCluster(TEXT("MX-ACAPULCO"), TEXT("Acapulco"), TEXT("MX"), TEXT("MX-GRO"), TEXT("Guerrero"), -99.82f, 16.85f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Lote 5 - Caribe (Antillas Mayores + Bahamas + Trinidad).
	AddTheaterCountryLabel(TEXT("CUBA"), -79.2f, 21.9f, FColor(232, 206, 126), 2600.f);
	AddSettlementCluster(TEXT("CU-HABANA"), TEXT("La Habana"), TEXT("CU"), TEXT("CU-HAB"), TEXT("La Habana"), -82.38f, 23.13f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CU-SANTIAGO"), TEXT("Santiago de Cuba"), TEXT("CU"), TEXT("CU-SCU"), TEXT("Santiago"), -75.82f, 20.02f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));

	AddTheaterCountryLabel(TEXT("BAHAMAS"), -77.0f, 24.7f, FColor(232, 206, 126), 1700.f);
	AddSettlementCluster(TEXT("BS-NASSAU"), TEXT("Nassau"), TEXT("BS"), TEXT("BS-NP"), TEXT("New Providence"), -77.35f, 25.06f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("BS-FREEPORT"), TEXT("Freeport"), TEXT("BS"), TEXT("BS-FP"), TEXT("Grand Bahama"), -78.70f, 26.53f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	AddTheaterCountryLabel(TEXT("JAMAICA"), -77.3f, 17.6f, FColor(232, 206, 126), 1500.f);
	AddSettlementCluster(TEXT("JM-KINGSTON"), TEXT("Kingston"), TEXT("JM"), TEXT("JM-KIN"), TEXT("Kingston"), -76.79f, 17.97f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("JM-MONTEGO-BAY"), TEXT("Montego Bay"), TEXT("JM"), TEXT("JM-STJ"), TEXT("Saint James"), -77.89f, 18.48f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	AddTheaterCountryLabel(TEXT("HAITI"), -72.6f, 19.4f, FColor(232, 206, 126), 1500.f);
	AddSettlementCluster(TEXT("HT-PORT-AU-PRINCE"), TEXT("Puerto Principe"), TEXT("HT"), TEXT("HT-OU"), TEXT("Ouest"), -72.33f, 18.54f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("HT-CAP-HAITIEN"), TEXT("Cap-Haitien"), TEXT("HT"), TEXT("HT-ND"), TEXT("Nord"), -72.20f, 19.76f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	AddTheaterCountryLabel(TEXT("R. DOMINICANA"), -70.2f, 19.3f, FColor(232, 206, 126), 1500.f);
	AddSettlementCluster(TEXT("DO-SANTO-DOMINGO"), TEXT("Santo Domingo"), TEXT("DO"), TEXT("DO-DN"), TEXT("Distrito Nacional"), -69.93f, 18.47f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("DO-SANTIAGO"), TEXT("Santiago"), TEXT("DO"), TEXT("DO-ST"), TEXT("Santiago"), -70.70f, 19.45f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));

	AddTheaterCountryLabel(TEXT("PUERTO RICO"), -66.4f, 18.0f, FColor(232, 206, 126), 1400.f);
	AddSettlementCluster(TEXT("PR-SAN-JUAN"), TEXT("San Juan"), TEXT("PR"), TEXT("PR-SJ"), TEXT("San Juan"), -66.11f, 18.47f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("PR-PONCE"), TEXT("Ponce"), TEXT("PR"), TEXT("PR-PO"), TEXT("Ponce"), -66.61f, 18.01f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	AddTheaterCountryLabel(TEXT("TRINIDAD"), -61.3f, 10.4f, FColor(232, 206, 126), 1400.f);
	AddSettlementCluster(TEXT("TT-PORT-OF-SPAIN"), TEXT("Puerto Espana"), TEXT("TT"), TEXT("TT-POS"), TEXT("Puerto Espana"), -61.51f, 10.65f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("TT-SAN-FERNANDO"), TEXT("San Fernando"), TEXT("TT"), TEXT("TT-SFO"), TEXT("San Fernando"), -61.47f, 10.28f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));

	// Caribe menor: al menos dos nodos por pais para que el generador cree una via interna.
	AddSettlementCluster(TEXT("BB-BRIDGETOWN"), TEXT("Bridgetown"), TEXT("BB"), TEXT("BB-SM"), TEXT("Saint Michael"), -59.62f, 13.10f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("BB-SPEIGHTSTOWN"), TEXT("Speightstown"), TEXT("BB"), TEXT("BB-SP"), TEXT("Saint Peter"), -59.64f, 13.25f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("DM-ROSEAU"), TEXT("Roseau"), TEXT("DM"), TEXT("DM-SG"), TEXT("Saint George"), -61.39f, 15.30f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("DM-PORTSMOUTH"), TEXT("Portsmouth"), TEXT("DM"), TEXT("DM-SP"), TEXT("Saint John"), -61.46f, 15.57f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("LC-CASTRIES"), TEXT("Castries"), TEXT("LC"), TEXT("LC-CA"), TEXT("Castries"), -61.01f, 14.01f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("LC-VIEUX-FORT"), TEXT("Vieux Fort"), TEXT("LC"), TEXT("LC-VF"), TEXT("Vieux Fort"), -60.95f, 13.73f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("VC-KINGSTOWN"), TEXT("Kingstown"), TEXT("VC"), TEXT("VC-SG"), TEXT("Saint George"), -61.23f, 13.16f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("VC-GEORGETOWN"), TEXT("Georgetown"), TEXT("VC"), TEXT("VC-CH"), TEXT("Charlotte"), -61.12f, 13.28f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("GD-ST-GEORGES"), TEXT("St Georges"), TEXT("GD"), TEXT("GD-SG"), TEXT("Saint George"), -61.75f, 12.06f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("GD-GRENVILLE"), TEXT("Grenville"), TEXT("GD"), TEXT("GD-SA"), TEXT("Saint Andrew"), -61.62f, 12.12f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("AG-ST-JOHNS"), TEXT("St Johns"), TEXT("AG"), TEXT("AG-SJ"), TEXT("Saint John"), -61.85f, 17.13f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("AG-ENGLISH-HARBOUR"), TEXT("English Harbour"), TEXT("AG"), TEXT("AG-PH"), TEXT("Saint Paul"), -61.76f, 17.01f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("KN-BASSETERRE"), TEXT("Basseterre"), TEXT("KN"), TEXT("KN-SG"), TEXT("Saint George Basseterre"), -62.72f, 17.30f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("KN-CHARLESTOWN"), TEXT("Charlestown"), TEXT("KN"), TEXT("KN-NE"), TEXT("Nevis"), -62.62f, 17.14f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Lote 6 - Estados Unidos.
	AddTheaterCountryLabel(TEXT("ESTADOS UNIDOS"), -98.0f, 39.5f, FColor(232, 206, 126), 7000.f);
	AddSettlementCluster(TEXT("US-WASHINGTON"), TEXT("Washington D.C."), TEXT("US"), TEXT("US-DC"), TEXT("District of Columbia"), -77.04f, 38.91f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("US-NEW-YORK"), TEXT("Nueva York"), TEXT("US"), TEXT("US-NY"), TEXT("New York"), -74.01f, 40.71f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("US-LOS-ANGELES"), TEXT("Los Angeles"), TEXT("US"), TEXT("US-CA"), TEXT("California"), -118.24f, 34.05f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-CHICAGO"), TEXT("Chicago"), TEXT("US"), TEXT("US-IL"), TEXT("Illinois"), -87.63f, 41.88f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("US-HOUSTON"), TEXT("Houston"), TEXT("US"), TEXT("US-TX"), TEXT("Texas"), -95.37f, 29.76f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("US-MIAMI"), TEXT("Miami"), TEXT("US"), TEXT("US-FL"), TEXT("Florida"), -80.19f, 25.76f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("US-SAN-FRANCISCO"), TEXT("San Francisco"), TEXT("US"), TEXT("US-NCA"), TEXT("California"), -122.42f, 37.77f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-SEATTLE"), TEXT("Seattle"), TEXT("US"), TEXT("US-WA"), TEXT("Washington"), -122.33f, 47.61f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("US-DALLAS"), TEXT("Dallas"), TEXT("US"), TEXT("US-NTX"), TEXT("Texas"), -96.80f, 32.78f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-DENVER"), TEXT("Denver"), TEXT("US"), TEXT("US-CO"), TEXT("Colorado"), -104.99f, 39.74f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-ATLANTA"), TEXT("Atlanta"), TEXT("US"), TEXT("US-GA"), TEXT("Georgia"), -84.39f, 33.75f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("US-BOSTON"), TEXT("Boston"), TEXT("US"), TEXT("US-MA"), TEXT("Massachusetts"), -71.06f, 42.36f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Canada.
	AddTheaterCountryLabel(TEXT("CANADA"), -100.0f, 56.0f, FColor(232, 206, 126), 7000.f);
	AddSettlementCluster(TEXT("CA-OTTAWA"), TEXT("Ottawa"), TEXT("CA"), TEXT("CA-ON"), TEXT("Ontario"), -75.70f, 45.42f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CA-TORONTO"), TEXT("Toronto"), TEXT("CA"), TEXT("CA-ONT"), TEXT("Ontario"), -79.38f, 43.65f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddSettlementCluster(TEXT("CA-MONTREAL"), TEXT("Montreal"), TEXT("CA"), TEXT("CA-QC"), TEXT("Quebec"), -73.57f, 45.50f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-VANCOUVER"), TEXT("Vancouver"), TEXT("CA"), TEXT("CA-BC"), TEXT("British Columbia"), -123.12f, 49.28f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("CA-CALGARY"), TEXT("Calgary"), TEXT("CA"), TEXT("CA-AB"), TEXT("Alberta"), -114.07f, 51.05f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-EDMONTON"), TEXT("Edmonton"), TEXT("CA"), TEXT("CA-ABN"), TEXT("Alberta"), -113.49f, 53.55f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-WINNIPEG"), TEXT("Winnipeg"), TEXT("CA"), TEXT("CA-MB"), TEXT("Manitoba"), -97.14f, 49.90f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddSettlementCluster(TEXT("CA-HALIFAX"), TEXT("Halifax"), TEXT("CA"), TEXT("CA-NS"), TEXT("Nova Scotia"), -63.58f, 44.65f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddSettlementCluster(TEXT("GL-NUUK"), TEXT("Nuuk"), TEXT("GL"), TEXT("GL-SM"), TEXT("Sermersooq"), -51.72f, 64.18f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("GL-SISIMIUT"), TEXT("Sisimiut"), TEXT("GL"), TEXT("GL-QE"), TEXT("Qeqqata"), -53.67f, 66.94f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	// Lote 4 (resto) - Centroamerica a core.
	AddTheaterCountryLabel(TEXT("GUATEMALA"), -90.4f, 15.5f, FColor(232, 206, 126), 1900.f);
	AddSettlementCluster(TEXT("GT-CIUDAD"), TEXT("Ciudad de Guatemala"), TEXT("GT"), TEXT("GT-GU"), TEXT("Guatemala"), -90.51f, 14.63f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("GT-QUETZALTENANGO"), TEXT("Quetzaltenango"), TEXT("GT"), TEXT("GT-QU"), TEXT("Quetzaltenango"), -91.52f, 14.84f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddTheaterCountryLabel(TEXT("BELICE"), -88.7f, 17.2f, FColor(232, 206, 126), 1300.f);
	AddSettlementCluster(TEXT("BZ-BELMOPAN"), TEXT("Belmopan"), TEXT("BZ"), TEXT("BZ-CY"), TEXT("Cayo"), -88.77f, 17.25f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("BZ-BELIZE-CITY"), TEXT("Belize City"), TEXT("BZ"), TEXT("BZ-BZ"), TEXT("Belize"), -88.20f, 17.50f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddTheaterCountryLabel(TEXT("HONDURAS"), -86.9f, 14.8f, FColor(232, 206, 126), 1900.f);
	AddSettlementCluster(TEXT("HN-TEGUCIGALPA"), TEXT("Tegucigalpa"), TEXT("HN"), TEXT("HN-FM"), TEXT("Francisco Morazan"), -87.21f, 14.10f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("HN-SAN-PEDRO-SULA"), TEXT("San Pedro Sula"), TEXT("HN"), TEXT("HN-CR"), TEXT("Cortes"), -88.03f, 15.50f, EWLCampaignSettlementType::Industrial, FLinearColor(0.84f, 0.55f, 0.32f));
	AddTheaterCountryLabel(TEXT("EL SALVADOR"), -88.9f, 13.6f, FColor(232, 206, 126), 1400.f);
	AddSettlementCluster(TEXT("SV-SAN-SALVADOR"), TEXT("San Salvador"), TEXT("SV"), TEXT("SV-SS"), TEXT("San Salvador"), -89.22f, 13.69f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("SV-SANTA-ANA"), TEXT("Santa Ana"), TEXT("SV"), TEXT("SV-SA"), TEXT("Santa Ana"), -89.56f, 13.99f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddTheaterCountryLabel(TEXT("NICARAGUA"), -85.2f, 12.9f, FColor(232, 206, 126), 1900.f);
	AddSettlementCluster(TEXT("NI-MANAGUA"), TEXT("Managua"), TEXT("NI"), TEXT("NI-MN"), TEXT("Managua"), -86.27f, 12.13f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("NI-LEON"), TEXT("Leon"), TEXT("NI"), TEXT("NI-LE"), TEXT("Leon"), -86.88f, 12.44f, EWLCampaignSettlementType::LargeCity, FLinearColor(0.74f, 0.64f, 0.42f));
	AddTheaterCountryLabel(TEXT("COSTA RICA"), -84.2f, 9.9f, FColor(232, 206, 126), 1600.f);
	AddSettlementCluster(TEXT("CR-SAN-JOSE"), TEXT("San Jose"), TEXT("CR"), TEXT("CR-SJ"), TEXT("San Jose"), -84.09f, 9.93f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("CR-LIMON"), TEXT("Limon"), TEXT("CR"), TEXT("CR-LI"), TEXT("Limon"), -83.03f, 9.99f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));
	AddTheaterCountryLabel(TEXT("PANAMA"), -80.4f, 8.6f, FColor(232, 206, 126), 1800.f);
	AddSettlementCluster(TEXT("PA-CIUDAD"), TEXT("Ciudad de Panama"), TEXT("PA"), TEXT("PA-PA"), TEXT("Panama"), -79.52f, 8.98f, EWLCampaignSettlementType::Capital, FLinearColor(0.96f, 0.74f, 0.28f));
	AddSettlementCluster(TEXT("PA-COLON"), TEXT("Colon"), TEXT("PA"), TEXT("PA-CO"), TEXT("Colon"), -79.90f, 9.36f, EWLCampaignSettlementType::Port, FLinearColor(0.74f, 0.64f, 0.44f));

	BuildSouthAmericaFrontierSettlementLayer();
	BuildBorderOutpostLayer();

	if (!bCollectFlatPadsOnly)
	{
		FWLCampaignRouteBuilder::SetCityClipCircles({});
		BuildIntercityRoads();
		BuildMovementNodesAndEdges();
		AddMilitaryForceMarkers();
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D visual layer complete. Cities=%d MovementNodes=%d Forces=%d."), CityViews.Num(), MovementNodes.Num(), ForceViews.Num());
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

FVector2D AWLCampaign3DView::NudgeSettlementToLand(float Lon, float Lat) const
{
	if (SettlementLandGeometry.Num() == 0)
	{
		return FVector2D(Lon, Lat);
	}

	auto IsLand = [this](float L, float T) -> bool
	{
		for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
		{
			if (FWLCampaignRegionGeometry::PointInAnyLonLatRing(FVector2D(L, T), Country.Rings))
			{
				return true;
			}
		}
		return false;
	};

	// Muestreamos 8 direcciones; si la ciudad esta junto a la costa, parte de las
	// muestras caen en el mar y el vector resultante apunta tierra adentro.
	const float Probe = 0.45f;
	static const FVector2D Dirs[8] = {
		FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f),
		FVector2D(0.707f, 0.707f), FVector2D(-0.707f, 0.707f), FVector2D(0.707f, -0.707f), FVector2D(-0.707f, -0.707f)
	};
	FVector2D LandDir(0.f, 0.f);
	int32 LandHits = 0;
	for (const FVector2D& Dir : Dirs)
	{
		if (IsLand(Lon + Dir.X * Probe, Lat + Dir.Y * Probe))
		{
			LandDir += Dir;
			++LandHits;
		}
	}

	// Interior (rodeada de tierra), isla diminuta (<=1 muestra) o sin datos: no mover.
	if (LandHits <= 1 || LandHits >= 7 || LandDir.IsNearlyZero())
	{
		return FVector2D(Lon, Lat);
	}

	LandDir.Normalize();
	const float Step = 0.38f;
	return FVector2D(Lon + LandDir.X * Step, Lat + LandDir.Y * Step);
}

FVector2D AWLCampaign3DView::NudgePortSettlementFootprintToLand(float Lon, float Lat, float TargetWidthWorldUnits) const
{
	if (SettlementLandGeometry.Num() == 0)
	{
		return FVector2D(Lon, Lat);
	}

	auto IsLandLonLat = [this](const FVector2D& LonLat) -> bool
	{
		for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
		{
			if (FWLCampaignRegionGeometry::PointInAnyLonLatRing(LonLat, Country.Rings))
			{
				return true;
			}
		}
		return false;
	};
	auto IsLandWorld = [this, &IsLandLonLat](const FVector& World) -> bool
	{
		return IsLandLonLat(WorldToLonLatForCampaign(World, TheaterCenterLonLat, DetailWorldUnitsPerKm));
	};

	const FVector OriginalProjected = ProjectLonLat(Lon, Lat);
	const FVector OriginalWorld(OriginalProjected.X, OriginalProjected.Y, 0.f);
	// The city is the protected object. Ports may move into the water, but the full city footprint
	// needs land around it so the shoreline/port never visually cuts the city block.
	const float Half = TargetWidthWorldUnits * 0.5f + 1850.f;
	TArray<FVector2D> FootprintChecks;
	constexpr int32 FootprintGridRadius = 4;
	FootprintChecks.Reserve((FootprintGridRadius * 2 + 1) * (FootprintGridRadius * 2 + 1));
	for (int32 GX = -FootprintGridRadius; GX <= FootprintGridRadius; ++GX)
	{
		for (int32 GY = -FootprintGridRadius; GY <= FootprintGridRadius; ++GY)
		{
			FootprintChecks.Add(FVector2D(
				Half * static_cast<float>(GX) / static_cast<float>(FootprintGridRadius),
				Half * static_cast<float>(GY) / static_cast<float>(FootprintGridRadius)));
		}
	}

	auto CountFootprintLand = [&](const FVector& CandidateWorld) -> int32
	{
		int32 Count = 0;
		for (const FVector2D& Local : FootprintChecks)
		{
			if (IsLandWorld(CandidateWorld + FVector(Local.X, Local.Y, 0.f)))
			{
				++Count;
			}
		}
		return Count;
	};
	auto CountNearbyWater = [&](const FVector& CandidateWorld) -> int32
	{
		int32 Count = 0;
		const float Distances[] = { Half + 900.f, Half + 2600.f, Half + 5200.f, Half + 8400.f, Half + 12400.f };
		for (int32 Step = 0; Step < 16; ++Step)
		{
			const float Angle = (2.f * PI * static_cast<float>(Step)) / 16.f;
			const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			for (float Distance : Distances)
			{
				if (!IsLandWorld(CandidateWorld + Dir * Distance))
				{
					++Count;
					break;
				}
			}
		}
		return Count;
	};

	const int32 RequiredLand = FootprintChecks.Num();
	FVector BestWorld = OriginalWorld;
	FVector2D BestLonLat(Lon, Lat);
	int32 BestLand = CountFootprintLand(OriginalWorld);
	int32 BestWater = CountNearbyWater(OriginalWorld);
	float BestScore = static_cast<float>(BestLand) * 1000.f + static_cast<float>(BestWater) * 45.f;
	bool bBestCompleteCoastal = BestLand == RequiredLand && BestWater > 0;

	const float CandidateDistances[] = { 0.f, 1200.f, 2400.f, 3600.f, 5000.f, 6800.f, 9000.f, 11600.f, 14500.f, 17800.f, 21600.f };
	for (float Distance : CandidateDistances)
	{
		const int32 DirectionSteps = Distance <= KINDA_SMALL_NUMBER ? 1 : 32;
		for (int32 Step = 0; Step < DirectionSteps; ++Step)
		{
			const float Angle = (2.f * PI * static_cast<float>(Step)) / 32.f;
			const FVector Dir = Distance <= KINDA_SMALL_NUMBER
				? FVector::ZeroVector
				: FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			const FVector CandidateWorld = OriginalWorld + Dir * Distance;
			if (!IsLandWorld(CandidateWorld))
			{
				continue;
			}

			const int32 LandCount = CountFootprintLand(CandidateWorld);
			const int32 WaterCount = CountNearbyWater(CandidateWorld);
			const bool bCompleteCoastal = LandCount == RequiredLand && WaterCount > 0;
			const FVector2D CandidateLonLat = WorldToLonLatForCampaign(CandidateWorld, TheaterCenterLonLat, DetailWorldUnitsPerKm);
			const float Score =
				(bCompleteCoastal ? 100000.f : 0.f)
				+ static_cast<float>(LandCount) * 1000.f
				+ static_cast<float>(WaterCount) * 45.f
				- Distance * 0.24f;
			if ((!bBestCompleteCoastal && bCompleteCoastal) || (bBestCompleteCoastal == bCompleteCoastal && Score > BestScore))
			{
				bBestCompleteCoastal = bCompleteCoastal;
				BestScore = Score;
				BestWorld = CandidateWorld;
				BestLonLat = CandidateLonLat;
				BestLand = LandCount;
				BestWater = WaterCount;
			}
		}
	}

	const float MoveKm = (BestWorld - OriginalWorld).Size2D() / FMath::Max(1.f, DetailWorldUnitsPerKm);
	if (MoveKm > 0.2f)
	{
		UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D port city land nudge: lon=%.3f lat=%.3f -> lon=%.3f lat=%.3f move=%.1fkm land=%d/%d water=%d cityHalf=%.0f"),
			Lon,
			Lat,
			BestLonLat.X,
			BestLonLat.Y,
			MoveKm,
			BestLand,
			RequiredLand,
			BestWater,
			Half);
	}
	return BestLonLat;
}

void AWLCampaign3DView::BuildBorderOutpostLayer()
{
	struct FBorderOutpostSpec
	{
		const TCHAR* Id;
		const TCHAR* Name;
		FVector2D RoadA;
		FVector2D RoadB;
		float PreferredSideSign;
	};

	// Standard de colocacion: un fuerte por corredor fronterizo jugable, fuera de la huella de ciudad.
	// El solver de AddBorderOutpost mantiene separacion contra ciudades y mueve el fuerte al lado mas
	// limpio de la carretera. Los cruces compartidos no se duplican por pais para no saturar la frontera.
	static const FBorderOutpostSpec Outposts[] = {
		// Venezuela primero: Colombia, Brasil y Guayana.
		{ TEXT("BO-CO-VE-TACHIRA"), TEXT("Tachira border fort"), FVector2D(-72.44f, 7.82f), FVector2D(-72.50f, 7.90f), 1.f },
		{ TEXT("BO-CO-VE-GUAJIRA"), TEXT("Guajira border fort"), FVector2D(-72.05f, 11.28f), FVector2D(-72.24f, 11.38f), -1.f },
		{ TEXT("BO-CO-VE-ARAUCA"), TEXT("Arauca border fort"), FVector2D(-70.76f, 7.08f), FVector2D(-70.80f, 7.24f), 1.f },
		{ TEXT("BO-CO-VE-ORINOCO"), TEXT("Orinoco border fort"), FVector2D(-67.49f, 6.19f), FVector2D(-67.62f, 5.66f), -1.f },
		{ TEXT("BO-VE-BR-SANTA-ELENA"), TEXT("Santa Elena border fort"), FVector2D(-61.11f, 4.60f), FVector2D(-61.13f, 4.48f), 1.f },
		{ TEXT("BO-VE-GY-GUAYANA"), TEXT("Guayana border fort"), FVector2D(-60.70f, 7.55f), FVector2D(-59.78f, 8.20f), 1.f },

		// Colombia y Ecuador.
		{ TEXT("BO-CO-EC-RUMICHACA"), TEXT("Rumichaca border fort"), FVector2D(-77.64f, 0.83f), FVector2D(-77.72f, 0.81f), -1.f },
		{ TEXT("BO-CO-BR-LETICIA"), TEXT("Leticia border fort"), FVector2D(-69.94f, -4.22f), FVector2D(-69.94f, -4.25f), 1.f },
		{ TEXT("BO-EC-PE-HUAQUILLAS"), TEXT("Huaquillas border fort"), FVector2D(-80.23f, -3.48f), FVector2D(-80.45f, -3.57f), 1.f },

		// Peru, Chile y Andes.
		{ TEXT("BO-PE-CL-TACNA"), TEXT("Tacna Arica border fort"), FVector2D(-70.25f, -18.01f), FVector2D(-70.31f, -18.48f), 1.f },
		{ TEXT("BO-PE-BR-INAPARI"), TEXT("Inapari border fort"), FVector2D(-69.18f, -12.59f), FVector2D(-68.75f, -11.01f), -1.f },
		{ TEXT("BO-PE-BO-DESAGUADERO"), TEXT("Desaguadero border fort"), FVector2D(-70.02f, -15.84f), FVector2D(-69.04f, -16.56f), -1.f },
		{ TEXT("BO-CL-AR-LIBERTADORES"), TEXT("Los Libertadores border fort"), FVector2D(-70.65f, -33.45f), FVector2D(-68.84f, -32.89f), -1.f },
		{ TEXT("BO-CL-AR-SAMORE"), TEXT("Cardenal Samore border fort"), FVector2D(-72.94f, -41.47f), FVector2D(-71.31f, -41.13f), 1.f },

		// Argentina, Bolivia, Uruguay y Paraguay.
		{ TEXT("BO-AR-BO-LAQUIACA"), TEXT("La Quiaca border fort"), FVector2D(-65.59f, -22.10f), FVector2D(-65.59f, -22.09f), 1.f },
		{ TEXT("BO-AR-PY-ENCARNACION"), TEXT("Encarnacion border fort"), FVector2D(-55.90f, -27.37f), FVector2D(-55.87f, -27.33f), -1.f },
		{ TEXT("BO-AR-UY-FRAY-BENTOS"), TEXT("Fray Bentos border fort"), FVector2D(-58.60f, -33.50f), FVector2D(-58.30f, -33.12f), 1.f },
		{ TEXT("BO-AR-BR-IGUAZU"), TEXT("Iguazu border fort"), FVector2D(-54.58f, -25.60f), FVector2D(-54.59f, -25.54f), -1.f },
		{ TEXT("BO-BO-BR-CORUMBA"), TEXT("Corumba border fort"), FVector2D(-57.80f, -18.98f), FVector2D(-57.65f, -19.01f), -1.f },
		{ TEXT("BO-BO-PY-CHACO"), TEXT("Chaco border fort"), FVector2D(-60.95f, -22.20f), FVector2D(-60.03f, -22.35f), 1.f },
		{ TEXT("BO-UY-BR-RIVERA"), TEXT("Rivera border fort"), FVector2D(-55.55f, -30.90f), FVector2D(-55.53f, -30.89f), 1.f },
		{ TEXT("BO-UY-BR-CHUY"), TEXT("Chuy border fort"), FVector2D(-53.46f, -33.69f), FVector2D(-53.45f, -33.69f), -1.f },
		{ TEXT("BO-PY-BR-GUAIRA"), TEXT("Guaira border fort"), FVector2D(-54.31f, -24.06f), FVector2D(-54.24f, -24.08f), 1.f },
		{ TEXT("BO-PY-BR-PJC"), TEXT("Pedro Juan border fort"), FVector2D(-55.75f, -22.55f), FVector2D(-55.72f, -22.53f), -1.f },

		// Brasil norte: Guayanas.
		{ TEXT("BO-BR-GY-LETHEM"), TEXT("Lethem border fort"), FVector2D(-59.80f, 3.38f), FVector2D(-60.67f, 2.82f), -1.f },
		{ TEXT("BO-BR-GF-OIAPOQUE"), TEXT("Oiapoque border fort"), FVector2D(-51.83f, 3.84f), FVector2D(-51.80f, 3.89f), 1.f }
	};

	for (const FBorderOutpostSpec& Spec : Outposts)
	{
		AddBorderOutpost(Spec.Id, Spec.Name, Spec.RoadA, Spec.RoadB, Spec.PreferredSideSign);
	}
}

bool AWLCampaign3DView::ShouldBuildPortFacility(const FString& CityId) const
{
	static const TCHAR* FeaturedPorts[] = {
		TEXT("VE-PUERTO-CABELLO"),
		TEXT("CO-CARTAGENA"),
		TEXT("EC-MANTA"),
		TEXT("PE-TRUJILLO"),
		TEXT("CL-VALPARAISO"),
		TEXT("AR-MAR-DEL-PLATA"),
		TEXT("BO-PUERTO-SUAREZ"),
		TEXT("UY-COLONIA"),
		TEXT("PY-ASUNCION"),
		TEXT("BR-RIO")
	};
	for (const TCHAR* PortId : FeaturedPorts)
	{
		if (CityId.Equals(PortId, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

void AWLCampaign3DView::AddPortFacility(
	const FString& CityId,
	float Lon,
	float Lat,
	const FString& CountryIso,
	EWLCampaignSettlementType Type)
{
	if (!PortFacilityMesh)
	{
		return;
	}

	float Yaw = 0.f;
	FVector LocalFacilityOffset(3600.f, -1200.f, 0.f);
	float FacilityScale = 1.f;
	if (CityId.Equals(TEXT("VE-PUERTO-CABELLO"), ESearchCase::IgnoreCase))
	{
		Yaw = -6.f;
		LocalFacilityOffset = FVector(8400.f, -1800.f, 0.f);
	}
	else if (CityId.Equals(TEXT("CO-CARTAGENA"), ESearchCase::IgnoreCase))
	{
		Yaw = -8.f;
		LocalFacilityOffset = FVector(10000.f, -1900.f, 0.f);
	}
	else if (CityId.Equals(TEXT("EC-MANTA"), ESearchCase::IgnoreCase))
	{
		Yaw = 0.f;
		LocalFacilityOffset = FVector(0.f, -8200.f, 0.f);
		FacilityScale = 0.92f;
	}
	else if (CityId.Equals(TEXT("PE-TRUJILLO"), ESearchCase::IgnoreCase))
	{
		Yaw = 0.f;
		LocalFacilityOffset = FVector(0.f, -8200.f, 0.f);
		FacilityScale = 0.92f;
	}
	else if (CityId.Equals(TEXT("CL-VALPARAISO"), ESearchCase::IgnoreCase))
	{
		Yaw = -18.f;
		LocalFacilityOffset = FVector(9800.f, -2100.f, 0.f);
	}
	else if (CityId.Equals(TEXT("AR-MAR-DEL-PLATA"), ESearchCase::IgnoreCase))
	{
		Yaw = 180.f;
		LocalFacilityOffset = FVector(0.f, -8200.f, 0.f);
		FacilityScale = 0.92f;
	}
	else if (CityId.Equals(TEXT("BO-PUERTO-SUAREZ"), ESearchCase::IgnoreCase))
	{
		Yaw = 12.f;
		LocalFacilityOffset = FVector(10500.f, -2300.f, 0.f);
		FacilityScale = 0.82f;
	}
	else if (CityId.Equals(TEXT("UY-COLONIA"), ESearchCase::IgnoreCase))
	{
		Yaw = 10.f;
		LocalFacilityOffset = FVector(10500.f, -2200.f, 0.f);
		FacilityScale = 0.92f;
	}
	else if (CityId.Equals(TEXT("PY-ASUNCION"), ESearchCase::IgnoreCase))
	{
		Yaw = 28.f;
		LocalFacilityOffset = FVector(-16500.f, -7600.f, 0.f);
		FacilityScale = 0.84f;
	}
	else if (CityId.Equals(TEXT("BR-RIO"), ESearchCase::IgnoreCase))
	{
		Yaw = 74.f;
		LocalFacilityOffset = FVector(10400.f, -2200.f, 0.f);
		FacilityScale = 1.12f;
	}

	const bool bCoreCountry = FWLCampaignRegionGeometry::IsTheaterIso(CountryIso);
	const EWLVisualBiome PortBiome = FWLCampaignVisualStyle::ClassifyVisualBiome(Lon, Lat, bCoreCountry);
	const float SurfaceZ = FWLCampaignVisualStyle::VisualBiomeZOffset(PortBiome, bCoreCountry);
	const FVector CityGround = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, SurfaceZ + 125.f);

	auto IsWaterLonLat = [this](const FVector2D& LonLat) -> bool
	{
		if (SettlementLandGeometry.Num() == 0)
		{
			return false;
		}
		for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
		{
			if (FWLCampaignRegionGeometry::PointInAnyLonLatRing(LonLat, Country.Rings))
			{
				return false;
			}
		}
		return true;
	};
	auto IsWaterWorld = [this, &IsWaterLonLat](const FVector& World) -> bool
	{
		return IsWaterLonLat(WorldToLonLatForCampaign(World, TheaterCenterLonLat, DetailWorldUnitsPerKm));
	};
	auto ApplyWaterAnchoredPlacement = [&]() -> bool
	{
		if (CountryIso.Equals(TEXT("BO"), ESearchCase::IgnoreCase)
			|| CountryIso.Equals(TEXT("PY"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		FVector CityWorld = ProjectLonLat(Lon, Lat);
		CityWorld.Z = 0.f;

		const FVector LocalWaterChecks[] = {
			FVector(0.f, 0.f, 0.f),
			FVector(-2500.f, -1750.f, 0.f),
			FVector(2500.f, -1750.f, 0.f),
			FVector(-2800.f, 0.f, 0.f),
			FVector(2800.f, 0.f, 0.f),
			FVector(-1900.f, 950.f, 0.f),
			FVector(1900.f, 950.f, 0.f),
			FVector(0.f, 1650.f, 0.f),
			FVector(-2400.f, 1650.f, 0.f),
			FVector(2400.f, 1650.f, 0.f),
			FVector(-820.f, -4220.f, 0.f),
			FVector(1660.f, -4450.f, 0.f)
		};
		const float CandidateDistances[] = { 10400.f, 11800.f, 13400.f, 15200.f, 17400.f, 19800.f, 22600.f, 25800.f };
		FVector2D BestDir(0.f, -1.f);
		float BestDistance = CandidateDistances[0];
		float BestYaw = Yaw;
		int32 BestBodyWaterCount = -1;
		float BestScore = -TNumericLimits<float>::Max();
		const float DirectionSampleKm[] = { 7.f, 11.f, 16.f, 23.f, 32.f, 45.f, 64.f, 90.f };
		for (int32 Step = 0; Step < 32; ++Step)
		{
			const float Angle = (2.f * PI * static_cast<float>(Step)) / 32.f;
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			int32 DirectionWaterCount = 0;
			for (float SampleKm : DirectionSampleKm)
			{
				const FVector SampleWorld = CityWorld + FVector(Dir.X, Dir.Y, 0.f) * SampleKm * DetailWorldUnitsPerKm;
				if (IsWaterWorld(SampleWorld))
				{
					++DirectionWaterCount;
				}
			}

			const float CandidateYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.X, -Dir.Y));
			const FRotator CandidateRotation(0.f, CandidateYaw, 0.f);
			for (float Distance : CandidateDistances)
			{
				const FVector CandidateOrigin = CityWorld + FVector(Dir.X, Dir.Y, 0.f) * Distance;
				int32 BodyWaterCount = 0;
				for (const FVector& LocalCheck : LocalWaterChecks)
				{
					if (IsWaterWorld(CandidateOrigin + CandidateRotation.RotateVector(LocalCheck * FacilityScale)))
					{
						++BodyWaterCount;
					}
				}
				const float Score =
					static_cast<float>(BodyWaterCount) * 1000.f
					+ static_cast<float>(DirectionWaterCount) * 35.f
					- FMath::Abs(Distance - 10800.f) * 0.05f;
				if (BodyWaterCount > BestBodyWaterCount || (BodyWaterCount == BestBodyWaterCount && Score > BestScore))
				{
					BestBodyWaterCount = BodyWaterCount;
					BestScore = Score;
					BestDir = Dir;
					BestDistance = Distance;
					BestYaw = CandidateYaw;
				}
			}
		}
		if (BestBodyWaterCount < 0)
		{
			return false;
		}

		Yaw = BestYaw;
		LocalFacilityOffset = FVector(0.f, -BestDistance / FMath::Max(0.1f, FacilityScale), 0.f);
		UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D port water anchor: %s dir=(%.2f,%.2f) dist=%.0f bodyWater=%d"),
			*CityId,
			BestDir.X,
			BestDir.Y,
			BestDistance,
			BestBodyWaterCount);
		return BestBodyWaterCount >= 10;
	};
	ApplyWaterAnchoredPlacement();

	const FRotator Rotation(0.f, Yaw, 0.f);
	const FVector Origin = CityGround + Rotation.RotateVector(LocalFacilityOffset * FacilityScale);

	FBorderOutpostMeshBuffer Buffer;
	const FLinearColor Concrete(0.42f, 0.43f, 0.38f, 1.f);
	const FLinearColor Quay(0.24f, 0.23f, 0.20f, 1.f);
	const FLinearColor Asphalt(0.13f, 0.16f, 0.16f, 1.f);
	const FLinearColor Warehouse(0.55f, 0.55f, 0.48f, 1.f);
	const FLinearColor Roof(0.28f, 0.34f, 0.34f, 1.f);
	const FLinearColor Crane(0.88f, 0.66f, 0.24f, 1.f);
	const FLinearColor Steel(0.23f, 0.28f, 0.30f, 1.f);
	const FLinearColor ShipHull(0.10f, 0.17f, 0.22f, 1.f);
	const FLinearColor ShipDeck(0.70f, 0.72f, 0.66f, 1.f);
	const FLinearColor ContainerA(0.58f, 0.16f, 0.12f, 1.f);
	const FLinearColor ContainerB(0.18f, 0.31f, 0.50f, 1.f);
	const FLinearColor ContainerC(0.54f, 0.45f, 0.20f, 1.f);
	const FLinearColor ContainerD(0.20f, 0.42f, 0.32f, 1.f);

	auto AddLocalBox = [&](const FVector& Local, const FVector& Size, const FLinearColor& Color)
	{
		AddOutpostBox(
			Buffer,
			Origin + Rotation.RotateVector(Local * FacilityScale),
			Rotation,
			Size * FacilityScale,
			Color);
	};
	auto AddLocalRotatedBox = [&](const FVector& Local, const FVector& Size, const FLinearColor& Color, float LocalYaw)
	{
		AddOutpostBox(
			Buffer,
			Origin + Rotation.RotateVector(Local * FacilityScale),
			FRotator(0.f, Yaw + LocalYaw, 0.f),
			Size * FacilityScale,
			Color);
	};
	auto AddCrane = [&](const FVector& Anchor, float BoomSign)
	{
		AddLocalBox(Anchor + FVector(0.f, 0.f, 70.f), FVector(190.f, 190.f, 820.f), Crane);
		AddLocalBox(Anchor + FVector(520.f, BoomSign * 760.f, 890.f), FVector(1820.f, 135.f, 115.f), Crane);
		AddLocalBox(Anchor + FVector(-430.f, -BoomSign * 100.f, 790.f), FVector(560.f, 230.f, 170.f), Steel);
		AddLocalBox(Anchor + FVector(1190.f, BoomSign * 1470.f, 560.f), FVector(120.f, 105.f, 470.f), Steel);
	};
	auto AddShip = [&](const FVector& Anchor, float LengthScale)
	{
		AddLocalBox(Anchor + FVector(0.f, 0.f, 25.f), FVector(1800.f * LengthScale, 430.f, 150.f), ShipHull);
		AddLocalBox(Anchor + FVector(220.f, 0.f, 180.f), FVector(760.f * LengthScale, 340.f, 170.f), ShipDeck);
		AddLocalBox(Anchor + FVector(-520.f, 0.f, 180.f), FVector(540.f * LengthScale, 330.f, 130.f), ContainerB);
		AddLocalBox(Anchor + FVector(-40.f, 0.f, 330.f), FVector(320.f, 260.f, 250.f), ShipDeck);
	};

	AddLocalBox(FVector(0.f, 0.f, 0.f), FVector(5600.f, 3200.f, 95.f), Concrete);
	AddLocalBox(FVector(-2550.f, 1880.f, 8.f), FVector(2850.f, 360.f, 55.f), Asphalt);
	AddLocalBox(FVector(650.f, -1780.f, 85.f), FVector(6250.f, 470.f, 165.f), Quay);
	AddLocalBox(FVector(1900.f, -2480.f, 42.f), FVector(4900.f, 290.f, 120.f), Quay);
	AddLocalBox(FVector(-800.f, -3040.f, 75.f), FVector(720.f, 1780.f, 130.f), Quay);
	AddLocalBox(FVector(1520.f, -3160.f, 75.f), FVector(720.f, 1920.f, 130.f), Quay);

	const FVector2D FacilityToCity2D(-LocalFacilityOffset.X, -LocalFacilityOffset.Y);
	const float FacilityCityDistance = FacilityToCity2D.Size();
	if (FacilityCityDistance > 5200.f)
	{
		const FVector2D FacilityToCityDir = FacilityToCity2D / FacilityCityDistance;
		const float PortFacingRadius = 0.5f
			* (FMath::Abs(FacilityToCityDir.X) * 5600.f + FMath::Abs(FacilityToCityDir.Y) * 3200.f);
		constexpr float CityFootprintWidth = 5800.f;
		constexpr float CityConnectorClearance = CityFootprintWidth * 0.5f + 1200.f;
		const float AvailableConnectorLength = FacilityCityDistance - PortFacingRadius - CityConnectorClearance;
		if (AvailableConnectorLength > 1200.f)
		{
			const float ConnectorLength = FMath::Min(AvailableConnectorLength, 7600.f);
			const float ConnectorCenter = PortFacingRadius + ConnectorLength * 0.5f;
			const float ConnectorYaw = FMath::RadiansToDegrees(FMath::Atan2(FacilityToCityDir.Y, FacilityToCityDir.X));
			const FVector ConnectorLocal(
				FacilityToCityDir.X * ConnectorCenter,
				FacilityToCityDir.Y * ConnectorCenter,
				62.f);
			AddLocalRotatedBox(ConnectorLocal, FVector(ConnectorLength, 500.f, 110.f), Concrete, ConnectorYaw);
			AddLocalRotatedBox(ConnectorLocal + FVector(0.f, 0.f, 68.f), FVector(ConnectorLength * 0.9f, 180.f, 38.f), Asphalt, ConnectorYaw);
			AddLocalRotatedBox(ConnectorLocal + FVector(0.f, 0.f, 128.f), FVector(ConnectorLength * 0.78f, 34.f, 42.f), Quay, ConnectorYaw);
		}
	}

	AddLocalBox(FVector(-1350.f, 1000.f, 95.f), FVector(1720.f, 670.f, 430.f), Warehouse);
	AddLocalBox(FVector(-1350.f, 1000.f, 525.f), FVector(1840.f, 760.f, 120.f), Roof);
	AddLocalBox(FVector(920.f, 1120.f, 95.f), FVector(1520.f, 620.f, 390.f), Warehouse);
	AddLocalBox(FVector(920.f, 1120.f, 485.f), FVector(1620.f, 710.f, 110.f), Roof);
	AddLocalBox(FVector(2440.f, 720.f, 95.f), FVector(820.f, 520.f, 330.f), Warehouse);

	const FLinearColor Containers[] = { ContainerA, ContainerB, ContainerC, ContainerD };
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Col = 0; Col < 5; ++Col)
		{
			const float X = -980.f + Col * 520.f;
			const float Y = -220.f + Row * 330.f;
			const float Z = 115.f + ((Row + Col) % 2) * 135.f;
			AddLocalBox(FVector(X, Y, 108.f), FVector(430.f, 190.f, 150.f), Containers[(Row + Col) % 4]);
			if (((Row + Col) % 2) == 0)
			{
				AddLocalBox(FVector(X, Y, 258.f), FVector(430.f, 190.f, 135.f), Containers[(Row + Col + 1) % 4]);
			}
		}
	}

	AddCrane(FVector(-500.f, -1460.f, 100.f), -1.f);
	AddCrane(FVector(1830.f, -1460.f, 100.f), -1.f);
	AddLocalBox(FVector(-2300.f, -820.f, 95.f), FVector(180.f, 180.f, 760.f), Steel);
	AddLocalBox(FVector(2920.f, -920.f, 95.f), FVector(180.f, 180.f, 720.f), Steel);
	AddLocalBox(FVector(-2300.f, -820.f, 850.f), FVector(520.f, 100.f, 90.f), Crane);
	AddLocalBox(FVector(2920.f, -920.f, 810.f), FVector(520.f, 100.f, 90.f), Crane);

	AddShip(FVector(-820.f, -4220.f, 70.f), 1.0f);
	AddShip(FVector(1660.f, -4450.f, 70.f), 0.78f);
	AddLocalBox(FVector(3230.f, -2360.f, 65.f), FVector(760.f, 330.f, 120.f), ShipHull);
	AddLocalBox(FVector(3320.f, -2360.f, 185.f), FVector(360.f, 240.f, 180.f), ShipDeck);

	CommitOutpostSection(PortFacilityMesh, VertexColorMaterial, Buffer);
	PortFacilityMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D port facility placed: %s lon=%.3f lat=%.3f yaw=%.1f scale=%.2f type=%d"),
		*CityId,
		Lon,
		Lat,
		Yaw,
		FacilityScale,
		static_cast<int32>(Type));
}

FVector2D AWLCampaign3DView::ResolveBorderOutpostPlacement(
	const FVector2D& RoadA,
	const FVector2D& RoadB,
	float PreferredSideSign,
	float MinCityClearanceKm,
	float DesiredRoadOffsetKm) const
{
	FVector WorldA = ProjectLonLat(RoadA.X, RoadA.Y);
	FVector WorldB = ProjectLonLat(RoadB.X, RoadB.Y);
	WorldA.Z = 0.f;
	WorldB.Z = 0.f;
	FVector RoadDir = WorldB - WorldA;
	if (RoadDir.SizeSquared2D() < 1.f)
	{
		return (RoadA + RoadB) * 0.5f;
	}
	RoadDir.Z = 0.f;
	RoadDir.Normalize();
	const FVector RoadSide(-RoadDir.Y, RoadDir.X, 0.f);
	const FVector RoadCenter = (WorldA + WorldB) * 0.5f;
	const float PreferredSide = PreferredSideSign < 0.f ? -1.f : 1.f;

	auto IsLand = [this](const FVector2D& LonLat) -> bool
	{
		if (SettlementLandGeometry.Num() == 0)
		{
			return true;
		}
		for (const FWLRegionalCountryGeometry& Country : SettlementLandGeometry)
		{
			if (FWLCampaignRegionGeometry::PointInAnyLonLatRing(LonLat, Country.Rings))
			{
				return true;
			}
		}
		return false;
	};

	auto MinDistanceToCityKm = [this](const FVector2D& LonLat) -> float
	{
		float Best = TNumericLimits<float>::Max();
		for (const FWLCampaign3DCityView& City : CityViews)
		{
			Best = FMath::Min(Best, LonLatDistanceKm(LonLat, FVector2D(City.Lon, City.Lat)));
		}
		if (Best == TNumericLimits<float>::Max())
		{
			for (const FCityFlatPad& Pad : CityFlatPads)
			{
				Best = FMath::Min(Best, LonLatDistanceKm(LonLat, FVector2D(Pad.Lon, Pad.Lat)));
			}
		}
		return Best == TNumericLimits<float>::Max() ? 9999.f : Best;
	};

	const float SideSigns[] = { PreferredSide, -PreferredSide };
	const float RoadOffsetsKm[] = {
		DesiredRoadOffsetKm,
		DesiredRoadOffsetKm + 4.f,
		DesiredRoadOffsetKm + 8.f,
		FMath::Max(8.f, DesiredRoadOffsetKm - 3.f)
	};
	const float AlongOffsetsKm[] = { 0.f, 4.f, -4.f, 8.f, -8.f };

	FVector2D BestLonLat = (RoadA + RoadB) * 0.5f;
	float BestScore = -TNumericLimits<float>::Max();
	bool bFound = false;
	for (float SideSign : SideSigns)
	{
		for (float RoadOffsetKm : RoadOffsetsKm)
		{
			for (float AlongOffsetKm : AlongOffsetsKm)
			{
				const FVector CandidateWorld = RoadCenter
					+ RoadSide * SideSign * RoadOffsetKm * DetailWorldUnitsPerKm
					+ RoadDir * AlongOffsetKm * DetailWorldUnitsPerKm;
				const FVector2D CandidateLonLat = WorldToLonLatForCampaign(CandidateWorld, TheaterCenterLonLat, DetailWorldUnitsPerKm);
				if (!IsLand(CandidateLonLat))
				{
					continue;
				}

				const float CityClearanceKm = MinDistanceToCityKm(CandidateLonLat);
				const bool bMeetsClearance = CityClearanceKm >= MinCityClearanceKm;
				const float Score =
					(bMeetsClearance ? 10000.f : 0.f)
					+ CityClearanceKm * 12.f
					- FMath::Abs(RoadOffsetKm - DesiredRoadOffsetKm) * 6.f
					- FMath::Abs(AlongOffsetKm) * 2.f
					+ (SideSign == PreferredSide ? 20.f : 0.f);
				if (!bFound || Score > BestScore)
				{
					bFound = true;
					BestScore = Score;
					BestLonLat = CandidateLonLat;
				}
			}
		}
	}

	if (!bFound)
	{
		const FVector CandidateWorld = RoadCenter + RoadSide * PreferredSide * DesiredRoadOffsetKm * DetailWorldUnitsPerKm;
		BestLonLat = WorldToLonLatForCampaign(CandidateWorld, TheaterCenterLonLat, DetailWorldUnitsPerKm);
	}
	return BestLonLat;
}

void AWLCampaign3DView::AddBorderOutpost(
	const FString& OutpostId,
	const FString& Name,
	const FVector2D& RoadA,
	const FVector2D& RoadB,
	float PreferredSideSign)
{
	const FVector2D OutpostLonLat = ResolveBorderOutpostPlacement(
		RoadA,
		RoadB,
		PreferredSideSign,
		BorderOutpostMinCityClearanceKm,
		BorderOutpostDesiredRoadOffsetKm);

	if (bCollectFlatPadsOnly)
	{
		RegisterBorderOutpostFlatPad(OutpostLonLat.X, OutpostLonLat.Y);
		return;
	}

	auto SurfaceWorld = [this](const FVector2D& LonLat) -> FVector
	{
		const bool bCoreCountry = true;
		const EWLVisualBiome Biome = FWLCampaignVisualStyle::ClassifyVisualBiome(LonLat.X, LonLat.Y, bCoreCountry);
		const float SurfaceOffset = FWLCampaignVisualStyle::VisualBiomeZOffset(Biome, bCoreCountry);
		return ProjectLonLat(LonLat.X, LonLat.Y) + FVector(0.f, 0.f, SurfaceOffset + 16.f);
	};

	FVector WorldA = ProjectLonLat(RoadA.X, RoadA.Y);
	FVector WorldB = ProjectLonLat(RoadB.X, RoadB.Y);
	WorldA.Z = 0.f;
	WorldB.Z = 0.f;
	FVector RoadDir = WorldB - WorldA;
	if (RoadDir.SizeSquared2D() < 1.f)
	{
		return;
	}
	RoadDir.Z = 0.f;
	RoadDir.Normalize();

	const float FortYaw = FMath::RadiansToDegrees(FMath::Atan2(RoadDir.Y, RoadDir.X));
	const FRotator FortRot(0.f, FortYaw, 0.f);
	const FVector Base = SurfaceWorld(OutpostLonLat);
	const FVector2D RoadCenterLonLat = (RoadA + RoadB) * 0.5f;
	const FVector RoadCenterWorld = SurfaceWorld(RoadCenterLonLat);
	const FVector FortToRoad = RoadCenterWorld - Base;
	const float GateSide = FVector::DotProduct(FortToRoad.GetSafeNormal2D(), FortRot.RotateVector(FVector(0.f, 1.f, 0.f))) >= 0.f ? 1.f : -1.f;
	constexpr float OutpostXYScale = 2.45f;
	constexpr float OutpostZScale = 1.55f;

	auto Local = [&FortRot, OutpostXYScale, OutpostZScale](float X, float Y, float Z = 0.f) -> FVector
	{
		return FortRot.RotateVector(FVector(X * OutpostXYScale, Y * OutpostXYScale, Z * OutpostZScale));
	};
	FBorderOutpostMeshBuffer Buffer;
	auto AddLocalPart = [&Buffer, &Base, &FortRot, &Local, OutpostXYScale, OutpostZScale](float X, float Y, float Z, const FVector& Size, const FLinearColor& Color)
	{
		AddOutpostBox(
			Buffer,
			Base + Local(X, Y, Z),
			FortRot,
			FVector(Size.X * OutpostXYScale, Size.Y * OutpostXYScale, Size.Z * OutpostZScale),
			Color);
	};

	const FVector GateWorld = Base + Local(0.f, GateSide * 645.f, 42.f);
	FVector AccessDelta = GateWorld - RoadCenterWorld;
	AccessDelta.Z = 0.f;
	const float AccessLength = AccessDelta.Size2D();
	if (AccessLength > 120.f)
	{
		FVector DirectionToRoad = RoadCenterWorld - GateWorld;
		DirectionToRoad.Z = 0.f;
		DirectionToRoad.Normalize();
		const float DrawnAccessLength = FMath::Min(AccessLength, 4200.f);
		const float AccessYaw = FMath::RadiansToDegrees(FMath::Atan2(DirectionToRoad.Y, DirectionToRoad.X));
		const FVector AccessBottom = GateWorld + DirectionToRoad * (DrawnAccessLength * 0.5f) + FVector(0.f, 0.f, 14.f);
		AddOutpostBox(
			Buffer,
			AccessBottom,
			FRotator(0.f, AccessYaw, 0.f),
			FVector(DrawnAccessLength, 260.f, 18.f),
			FLinearColor(0.300f, 0.285f, 0.205f));
	}

	const FLinearColor Earthwork(0.350f, 0.390f, 0.255f);
	const FLinearColor Concrete(0.500f, 0.490f, 0.405f);
	const FLinearColor Wall(0.735f, 0.690f, 0.545f);
	const FLinearColor Gate(0.800f, 0.660f, 0.305f);
	const FLinearColor Building(0.560f, 0.600f, 0.465f);
	const FLinearColor Roof(0.260f, 0.355f, 0.245f);
	const FLinearColor Sandbag(0.725f, 0.600f, 0.350f);
	const FLinearColor Tower(0.650f, 0.635f, 0.505f);
	const FLinearColor Vehicle(0.230f, 0.330f, 0.215f);
	const FLinearColor Antenna(0.175f, 0.185f, 0.175f);
	const FLinearColor Marking(0.860f, 0.800f, 0.560f);

	AddLocalPart(0.f, 0.f, 0.f, FVector(1660.f, 1220.f, 28.f), Earthwork);
	AddLocalPart(0.f, 0.f, 18.f, FVector(1320.f, 920.f, 34.f), Concrete);
	AddLocalPart(0.f, GateSide * 170.f, 56.f, FVector(520.f, 250.f, 10.f), Marking);
	AddLocalPart(-65.f, GateSide * 112.f, 68.f, FVector(38.f, 210.f, 8.f), Marking);
	AddLocalPart(65.f, GateSide * 112.f, 68.f, FVector(38.f, 210.f, 8.f), Marking);
	AddLocalPart(0.f, GateSide * 112.f, 70.f, FVector(170.f, 34.f, 8.f), Marking);

	const float FrontY = GateSide * 520.f;
	const float BackY = -GateSide * 520.f;
	const float LeftX = -660.f;
	const float RightX = 660.f;
	AddLocalPart(-455.f, FrontY, 46.f, FVector(430.f, 54.f, 138.f), Wall);
	AddLocalPart(455.f, FrontY, 46.f, FVector(430.f, 54.f, 138.f), Wall);
	AddLocalPart(0.f, BackY, 46.f, FVector(1320.f, 58.f, 150.f), Wall);
	AddLocalPart(LeftX, 0.f, 46.f, FVector(58.f, 1040.f, 150.f), Wall);
	AddLocalPart(RightX, 0.f, 46.f, FVector(58.f, 1040.f, 150.f), Wall);
	AddLocalPart(0.f, FrontY + GateSide * 64.f, 58.f, FVector(320.f, 44.f, 72.f), Gate);

	AddLocalPart(-210.f, -GateSide * 80.f, 58.f, FVector(430.f, 310.f, 230.f), Building);
	AddLocalPart(-210.f, -GateSide * 80.f, 288.f, FVector(500.f, 360.f, 54.f), Roof);
	AddLocalPart(330.f, -GateSide * 275.f, 58.f, FVector(380.f, 205.f, 150.f), Building);
	AddLocalPart(330.f, -GateSide * 275.f, 208.f, FVector(430.f, 245.f, 42.f), Roof);
	AddLocalPart(335.f, GateSide * 185.f, 58.f, FVector(360.f, 175.f, 126.f), Building);
	AddLocalPart(335.f, GateSide * 185.f, 184.f, FVector(400.f, 215.f, 40.f), Roof);
	AddLocalPart(-445.f, GateSide * 260.f, 58.f, FVector(240.f, 155.f, 118.f), Building);
	AddLocalPart(-445.f, GateSide * 260.f, 176.f, FVector(280.f, 190.f, 38.f), Roof);

	for (const FVector2D& TowerLocal : {
		FVector2D(LeftX * 0.82f, BackY * 0.78f),
		FVector2D(RightX * 0.82f, BackY * 0.78f),
		FVector2D(LeftX * 0.82f, FrontY * 0.74f),
		FVector2D(RightX * 0.82f, FrontY * 0.74f) })
	{
		AddLocalPart(TowerLocal.X, TowerLocal.Y, 58.f, FVector(74.f, 74.f, 370.f), Tower);
		AddLocalPart(TowerLocal.X, TowerLocal.Y, 428.f, FVector(170.f, 170.f, 104.f), Tower);
		AddLocalPart(TowerLocal.X, TowerLocal.Y, 532.f, FVector(205.f, 205.f, 34.f), Roof);
	}

	AddLocalPart(-505.f, -GateSide * 235.f, 58.f, FVector(38.f, 38.f, 780.f), Antenna);
	AddLocalPart(-505.f, -GateSide * 235.f, 838.f, FVector(210.f, 28.f, 28.f), Antenna);
	AddLocalPart(-505.f, -GateSide * 235.f, 940.f, FVector(130.f, 22.f, 22.f), Antenna);
	AddLocalPart(65.f, GateSide * 155.f, 60.f, FVector(170.f, 86.f, 58.f), Vehicle);
	AddLocalPart(130.f, GateSide * 255.f, 60.f, FVector(138.f, 78.f, 52.f), Vehicle);
	AddLocalPart(-75.f, GateSide * 275.f, 60.f, FVector(125.f, 70.f, 50.f), Vehicle);

	for (int32 Index = -4; Index <= 4; ++Index)
	{
		AddLocalPart(Index * 105.f, FrontY + GateSide * 150.f, 58.f, FVector(84.f, 48.f, 48.f), Sandbag);
	}
	for (int32 Index = -3; Index <= 3; ++Index)
	{
		AddLocalPart(LeftX + 84.f, Index * 122.f, 58.f, FVector(74.f, 42.f, 44.f), Sandbag);
		AddLocalPart(RightX - 84.f, Index * 122.f, 58.f, FVector(74.f, 42.f, 44.f), Sandbag);
	}

	CommitOutpostSection(BorderOutpostMesh, VertexColorMaterial, Buffer);
	if (BorderOutpostMesh)
	{
		BorderOutpostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// El fuerte es una BASE DE RECLUTAMIENTO (estilo Total War): lo registramos como una "fuerza/
	// guarnicion" seleccionable (vacia al inicio) -> al hacer clic abre el panel de reclutamiento y se
	// entrena tropa por turnos en este fuerte. El pais sale del id ("BO-CO-VE-..." -> CO) para el tesoro.
	{
		FWLCampaign3DForceView Garrison;
		Garrison.Id = OutpostId;
		Garrison.Name = Name;
		TArray<FString> IdParts;
		OutpostId.ParseIntoArray(IdParts, TEXT("-"), true);
		Garrison.CountryIso = IdParts.IsValidIndex(1) ? IdParts[1].ToUpper() : TEXT("CO");
		Garrison.CountryName = Garrison.CountryIso;
		Garrison.ForceType = TEXT("Fuerte fronterizo (base de reclutamiento)");
		Garrison.MarkerCategory = TEXT("land");
		Garrison.LocationName = Name;
		Garrison.NearbyCity = Name;
		Garrison.Lon = OutpostLonLat.X;
		Garrison.Lat = OutpostLonLat.Y;
		Garrison.WorldLocation = Base + FVector(0.f, 0.f, 1000.f);
		Garrison.bMovable = false;            // el FUERTE (edificio) no se mueve: solo recluta
		Garrison.bIsRecruitmentBase = true;   // -> el panel muestra opciones de reclutar
		// El fuerte es un EDIFICIO clicable SIN token (nada de tanque flotante). Al reclutar tropas, sale
		// un EJERCITO movible aparte (token tanque) generado por SyncRecruitedArmyTokens, que avanza por
		// carretera (modelo Total War). bSpawnTokenMesh=false -> el fuerte solo aporta la hitbox de clic.
		AddMilitaryForceMarker(Garrison, false);
	}

	float NearestCityKm = 9999.f;
	for (const FWLCampaign3DCityView& City : CityViews)
	{
		NearestCityKm = FMath::Min(NearestCityKm, LonLatDistanceKm(OutpostLonLat, FVector2D(City.Lon, City.Lat)));
	}
	UE_LOG(
		LogWorldLeader,
		Log,
		TEXT("Campaign3D border outpost placed: %s (%s) lon=%.3f lat=%.3f nearestCityKm=%.1f"),
		*OutpostId,
		*Name,
		OutpostLonLat.X,
		OutpostLonLat.Y,
		NearestCityKm);
}

void AWLCampaign3DView::AddSettlementCluster(
	const FString& CityId,
	const FString& Name,
	const FString& CountryIso,
	const FString& TerritoryId,
	const FString& TerritoryName,
	float Lon,
	float Lat,
	EWLCampaignSettlementType Type,
	const FLinearColor& AccentColor)
{
	const bool bFeaturedPortFacility = ShouldBuildPortFacility(CityId);
	// Maracaibo esta colocada a mano en la ribera OESTE del estrecho. El nudge "a tierra"
	// la empujaria hacia el sur (porque al norte esta el golfo) y la meteria al lago, que
	// el nudge no conoce. La dejamos fija.
	// Los puertos normales quedan en la costa. Los puertos destacados separan dos reglas: ciudad completa
	// en tierra, instalacion portuaria en agua y conectada a la ciudad.
	const bool bSkipNudge = CityId.Equals(TEXT("VE-MARACAIBO"), ESearchCase::IgnoreCase)
		|| Type == EWLCampaignSettlementType::Port;
	if (bFeaturedPortFacility && Type == EWLCampaignSettlementType::Port)
	{
		const FVector2D LandLonLat = NudgePortSettlementFootprintToLand(Lon, Lat, 8000.f);  // = BuildMeshCity Port TargetWidth (sync)
		Lon = LandLonLat.X;
		Lat = LandLonLat.Y;
	}
	else if (!bSkipNudge)
	{
		const FVector2D LandLonLat = NudgeSettlementToLand(Lon, Lat);
		Lon = LandLonLat.X;
		Lat = LandLonLat.Y;
	}

	// Pre-pasada de flat pads (antes de mallar el terreno): registra el disco de aplanado en la
	// posicion YA nudgeada (para que pad y ciudad coincidan) y no construye nada mas.
	if (bCollectFlatPadsOnly)
	{
		RegisterCityFlatPad(Lon, Lat, Type, CountryIso);
		return;
	}

	FWLCampaignSettlementSpec Spec;
	Spec.Name = Name;
	Spec.Lon = Lon;
	Spec.Lat = Lat;
	Spec.Type = Type;
	Spec.AccentColor = AccentColor;
	// Cada ciudad = UN modelo 3D cohesivo (ciudad entera: manzanas+calles+edificios, generada en
	// Blender) escalado al tamaño del asentamiento y anclado al terreno (estilo settlement de Total
	// War). Si los modelos no estan cargados, cae al footprint procedural como respaldo.
	const bool bUseMeshCity = CityModelLarge != nullptr;
	if (bUseMeshCity)
	{
		// La ciudad se ancla SOBRE la superficie visible del bioma (offset, igual que el terreno); sin
		// eso el terreno tapaba calles/aceras/parques. BuildMeshCity deduce el bioma desde el ISO.
		BuildMeshCity(Lon, Lat, Type, CountryIso);
		if (bFeaturedPortFacility)
		{
			AddPortFacility(CityId, Lon, Lat, CountryIso, Type);
		}
		if (Type == EWLCampaignSettlementType::Port && !bFeaturedPortFacility)
		{
			const bool bCoreCountry = FWLCampaignRegionGeometry::IsTheaterIso(CountryIso);
			const EWLVisualBiome PortBiome = FWLCampaignVisualStyle::ClassifyVisualBiome(Lon, Lat, bCoreCountry);
			const float SurfaceZ = FWLCampaignVisualStyle::VisualBiomeZOffset(PortBiome, bCoreCountry);
			const FVector Ground = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, SurfaceZ + 120.f);
			const FRotator NorthFacing(0.f, 0.f, 0.f);

			AddInstance(PortInstances, Ground + FVector(1750.f, -720.f, 80.f), NorthFacing, FVector(30.f, 3.1f, 0.70f));
			AddInstance(PortInstances, Ground + FVector(2580.f, 760.f, 82.f), NorthFacing, FVector(24.f, 2.8f, 0.70f));
			AddInstance(PortInstances, Ground + FVector(900.f, 0.f, 88.f), NorthFacing, FVector(8.5f, 11.0f, 0.75f));
			AddInstance(PortInstances, Ground + FVector(1320.f, 650.f, 90.f), NorthFacing, FVector(6.0f, 4.2f, 0.70f));
			AddInstance(SettlementBlockInstances, Ground + FVector(1020.f, -520.f, 180.f), NorthFacing, FVector(2.8f, 1.0f, 0.85f));
			AddInstance(SettlementBlockInstances, Ground + FVector(1040.f, -160.f, 180.f), NorthFacing, FVector(2.7f, 1.0f, 0.85f));
			AddInstance(SettlementBlockInstances, Ground + FVector(1080.f, 300.f, 180.f), NorthFacing, FVector(3.0f, 1.0f, 0.85f));
			AddInstance(SettlementBlockInstances, Ground + FVector(1340.f, 680.f, 180.f), NorthFacing, FVector(2.6f, 1.0f, 0.85f));
			AddInstance(SettlementTowerInstances, Ground + FVector(1680.f, -980.f, 640.f), NorthFacing, FVector(0.45f, 0.45f, 6.0f));
			AddInstance(SettlementTowerInstances, Ground + FVector(1680.f, -980.f, 1230.f), NorthFacing, FVector(0.45f, 4.6f, 0.45f));
			AddInstance(SettlementTowerInstances, Ground + FVector(2240.f, 1060.f, 620.f), NorthFacing, FVector(0.45f, 0.45f, 5.7f));
			AddInstance(SettlementTowerInstances, Ground + FVector(2240.f, 1060.f, 1180.f), NorthFacing, FVector(0.45f, 4.2f, 0.45f));
		}
	}
	else
	{
		FWLCampaignSettlementBuilder::AddSettlement(
			SettlementMesh,
			VertexColorMaterial,
			Spec,
			[this](float InLon, float InLat)
			{
				return ProjectLonLat(InLon, InLat);
			});
	}

	const bool bCapital = Type == EWLCampaignSettlementType::Capital;
	const bool bSmall = Type == EWLCampaignSettlementType::Frontier;
	const FVector Base = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, bCapital ? 2450.f : 2050.f);
	FWLCampaign3DCityView City;
	City.Id = CityId;
	City.Name = Name;
	City.CountryIso = CountryIso;
	City.TerritoryId = TerritoryId;
	City.TerritoryName = TerritoryName;
	City.CityType = SettlementTypeToPanelText(Type);
	City.StrategicRole = SettlementStrategicRole(Type);
	City.DetailLevel = TEXT("high-detail theater");
	City.WorldLocation = Base;
	City.Lon = Lon;
	City.Lat = Lat;
	City.bCapital = bCapital;
	City.bPort = Type == EWLCampaignSettlementType::Port;
	AddCitySelectionProxy(City, bCapital ? 1.28f : (City.bPort ? 1.16f : 1.0f));

	// El nombre debe FLOTAR sobre la ciudad (los edificios ahora son altos); si no, queda enterrado.
	float LabelZ;
	switch (Type)
	{
	case EWLCampaignSettlementType::Capital:    LabelZ = 10800.f; break;
	case EWLCampaignSettlementType::LargeCity:  LabelZ = 5600.f; break;
	case EWLCampaignSettlementType::Port:       LabelZ = 4800.f; break;
	case EWLCampaignSettlementType::Industrial: LabelZ = 5200.f; break;
	case EWLCampaignSettlementType::Frontier:   LabelZ = 2600.f; break;
	default:                                    LabelZ = 5200.f; break;
	}
	const float GroundZ = Base.Z - (bCapital ? 2450.f : 2050.f);
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetWorldLocation(FVector(Base.X, Base.Y, GroundZ + LabelZ));
	Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	// Tamano BASE; ApplyZoomLOD lo reescala por altura de camara (tamano casi constante en
	// pantalla). Antes Cucuta/San Cristobal no tenian label (se saltaban por solape cuando el
	// mapa estaba comprimido); con el mapa estirado + escala por pantalla, ya se distinguen.
	const float LabelBaseSize = bCapital ? 1250.f : (bSmall ? 720.f : 900.f);
	Label->SetWorldSize(LabelBaseSize);
	Label->SetText(FText::FromString(Name));
	Label->SetTextRenderColor(bCapital ? FColor(230, 210, 140) : FColor(195, 205, 190));
	SettlementLabels.Add(Label);
	SettlementLabelBaseSizes.Add(LabelBaseSize);
	// LOD de etiqueta por IMPORTANCIA: de lejos solo capitales; al acercar aparecen grandes, puertos/
	// industriales y por ultimo fronterizas -> los nombres no se amontonan ni se cortan. Es la altura
	// MAXIMA de camara a la que se muestra el nombre (mas grande = visible desde mas lejos).
	float LabelMaxHeight;
	switch (Type)
	{
	case EWLCampaignSettlementType::Capital:    LabelMaxHeight = 360000.f; break;
	case EWLCampaignSettlementType::LargeCity:  LabelMaxHeight = 235000.f; break;
	case EWLCampaignSettlementType::Industrial: LabelMaxHeight = 195000.f; break;
	case EWLCampaignSettlementType::Port:       LabelMaxHeight = 170000.f; break;
	case EWLCampaignSettlementType::Frontier:   LabelMaxHeight = 110000.f; break;
	default:                                    LabelMaxHeight = 170000.f; break;
	}
	SettlementLabelMaxHeights.Add(LabelMaxHeight);
}

void AWLCampaign3DView::BuildMeshCity(float Lon, float Lat, EWLCampaignSettlementType Type, const FString& CountryIso)
{
	const bool bCoreCountry = FWLCampaignRegionGeometry::IsTheaterIso(CountryIso);
	// Elige el modelo de ciudad y su ancho objetivo (u de mundo) por tipo. TargetWidth calibrado a
	// los pares mas cercanos de Venezuela para que dos ciudades NO se solapen:
	//   San Antonio<->San Cristobal ~7600u (Frontier) | Valencia<->Pto Cabello ~9600u (Large/Port)
	//   Valencia<->Maracay ~14000u | Caracas (capital) sin vecina cercana -> puede ser grande.
	// Para hacerlas mas grandes/chicas, ajusta estos TargetWidth.
	const TArray<UStaticMesh*>* Variants = &CityModelMediumVariants;
	float TargetWidth = 7000.f;
	switch (Type)
	{
	case EWLCampaignSettlementType::Capital:    Variants = &CityModelLargeVariants;  TargetWidth = 17000.f; break;
	case EWLCampaignSettlementType::LargeCity:  Variants = &CityModelMediumVariants; TargetWidth = 7200.f;  break;
	case EWLCampaignSettlementType::Port:       Variants = &CityModelMediumVariants; TargetWidth = 8000.f;  break;  // ciudades-puerto mayores (Valparaiso/Mar del Plata): antes 5800 = se veian diminutas vs capitales (mantener sync con RegisterCityFlatPad + nudge)
	case EWLCampaignSettlementType::Industrial: Variants = &CityModelMediumVariants; TargetWidth = 6600.f;  break;
	case EWLCampaignSettlementType::Frontier:   Variants = &CityModelSmallVariants;  TargetWidth = 4400.f;  break;
	default:                                    Variants = &CityModelMediumVariants; TargetWidth = 6000.f;  break;
	}
	// Elige una VARIANTE por hash de lon/lat (estable, y con salt distinto al del giro) para que las
	// ciudades vecinas del mismo tipo NO se vean clones identicas.
	UStaticMesh* Model = nullptr;
	if (Variants && Variants->Num() > 0)
	{
		const float VarHash = GridHash01(FMath::RoundToInt(Lon * 131.f), FMath::RoundToInt(Lat * 117.f), 23);
		const int32 Idx = FMath::Clamp(static_cast<int32>(VarHash * Variants->Num()), 0, Variants->Num() - 1);
		Model = (*Variants)[Idx];
	}
	if (!Model)
	{
		Model = CityModelLarge ? CityModelLarge : (CityModelMedium ? CityModelMedium : CityModelSmall);
	}
	if (!Model)
	{
		return;
	}

	// Escala UNIFORME al ancho objetivo (conserva las proporciones del modelo). Giro en pasos de 90
	// (mantiene las calles alineadas).
	const FVector Ext = Model->GetBounds().BoxExtent;
	const float ModelWidth = FMath::Max(1.f, FMath::Max(Ext.X, Ext.Y) * 2.f);
	const float UniformScale = TargetWidth / ModelWidth;
	float VerticalPresence = 1.45f;
	switch (Type)
	{
	case EWLCampaignSettlementType::Capital:    VerticalPresence = 1.78f; break;
	case EWLCampaignSettlementType::Industrial: VerticalPresence = 1.62f; break;
	case EWLCampaignSettlementType::LargeCity:  VerticalPresence = 1.58f; break;
	case EWLCampaignSettlementType::Port:       VerticalPresence = 1.52f; break;
	case EWLCampaignSettlementType::Frontier:   VerticalPresence = 1.32f; break;
	default:                                    VerticalPresence = 1.45f; break;
	}
	const FVector Scale(UniformScale, UniformScale, UniformScale * VerticalPresence);

	const float YawHash = GridHash01(FMath::RoundToInt(Lon * 97.f), FMath::RoundToInt(Lat * 89.f), 7);
	const FRotator Rot(0.f, FMath::RoundToFloat(YawHash * 4.f) * 90.f, 0.f);
	// Ancla el ORIGEN del modelo (z=0 = nivel de calle) a la SUPERFICIE VISIBLE del terreno. CLAVE:
	// el terreno se dibuja en ProjectLonLat.Z + VisualBiomeZOffset (p.ej. Costa +95), no en
	// ProjectLonLat crudo. Si la ciudad se ancla sin ese offset, el mesh del terreno queda ENCIMA y
	// tapa calles/aceras/parques (solo asoman los edificios altos). Sumamos el MISMO offset del bioma
	// del centro -> el piso de la ciudad queda justo sobre el terreno y se ve (calles, verde, aceras).
	const EWLVisualBiome CityBiome = FWLCampaignVisualStyle::ClassifyVisualBiome(Lon, Lat, bCoreCountry);
	const float VisualSurfaceZOffset = FWLCampaignVisualStyle::VisualBiomeZOffset(CityBiome, bCoreCountry);
	const FVector Ground = ProjectLonLat(Lon, Lat) + FVector(0.f, 0.f, VisualSurfaceZOffset);

	UStaticMeshComponent* CityComp = NewObject<UStaticMeshComponent>(this);
	if (!CityComp)
	{
		return;
	}
	CityComp->SetupAttachment(SceneRoot);
	CityComp->RegisterComponent();
	CityComp->SetStaticMesh(Model);
	// Mismo material UNLIT de vertex-color que el terreno/carreteras: la ciudad usa los vertex
	// colors del FBX (color + sombreado falso por cara) y queda COHERENTE con el mapa, sin sombras
	// de escena -> nunca se ve oscura (la causa real de "ciudades oscuras" era usar materiales LIT).
	if (VertexColorMaterial)
	{
		const int32 NumMats = FMath::Max(1, CityComp->GetNumMaterials());
		for (int32 i = 0; i < NumMats; ++i)
		{
			CityComp->SetMaterial(i, VertexColorMaterial);
		}
	}
	CityComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CityComp->SetMobility(EComponentMobility::Movable);
	CityComp->SetWorldTransform(FTransform(Rot, Ground, Scale));
	CityBuildingComponents.Add(CityComp);
}

void AWLCampaign3DView::AddCitySelectionProxy(const FWLCampaign3DCityView& City, float RadiusScale)
{
	if (City.Id.IsEmpty())
	{
		return;
	}

	USphereComponent* Marker = NewObject<USphereComponent>(this);
	Marker->SetupAttachment(SceneRoot);
	Marker->RegisterComponent();
	Marker->SetWorldLocation(City.WorldLocation + FVector(0.f, 0.f, City.bCapital ? 2400.f : 1800.f));
	// Keep selection usable but stop hidden hit spheres from swallowing neighboring cities.
	Marker->SetSphereRadius((City.bCapital ? 3800.f : 2600.f) * FMath::Max(0.65f, RadiusScale));
	Marker->SetVisibility(false, true);
	Marker->SetHiddenInGame(false, true);
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionObjectType(ECC_WorldDynamic);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->ComponentTags.Add(TEXT("WorldLeaderCampaign3DCity"));
	Marker->ComponentTags.Add(FName(*City.Id));
	CitySelectionMarkers.Add(Marker);
	CityViews.Add(City);
}

