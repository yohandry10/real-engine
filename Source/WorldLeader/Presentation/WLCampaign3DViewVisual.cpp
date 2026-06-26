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

	UProceduralMeshComponent* FindRoadDetailMesh(AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}

		TArray<UProceduralMeshComponent*> Components;
		Owner->GetComponents<UProceduralMeshComponent>(Components);
		for (UProceduralMeshComponent* Component : Components)
		{
			if (Component && Component->GetFName() == TEXT("RoadDetailMesh"))
			{
				return Component;
			}
		}
		return nullptr;
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
	if (UProceduralMeshComponent* RoadDetailMesh = FindRoadDetailMesh(this))
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
	// Maracaibo esta colocada a mano en la ribera OESTE del estrecho. El nudge "a tierra"
	// la empujaria hacia el sur (porque al norte esta el golfo) y la meteria al lago, que
	// el nudge no conoce. La dejamos fija.
	// Los PUERTOS deben quedar en la COSTA (orilla del mar): el nudge (pensado para que ciudades de
	// interior no caigan al mar) los alejaba del agua, sin sentido para un puerto. Asi que no se nudgean.
	const bool bSkipNudge = CityId.Equals(TEXT("VE-MARACAIBO"), ESearchCase::IgnoreCase)
		|| Type == EWLCampaignSettlementType::Port;
	if (!bSkipNudge)
	{
		const FVector2D LandLonLat = NudgeSettlementToLand(Lon, Lat);
		Lon = LandLonLat.X;
		Lat = LandLonLat.Y;
	}

	// Pre-pasada de flat pads (antes de mallar el terreno): registra el disco de aplanado en la
	// posicion YA nudgeada (para que pad y ciudad coincidan) y no construye nada mas.
	if (bCollectFlatPadsOnly)
	{
		RegisterCityFlatPad(Lon, Lat, Type);
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
		if (Type == EWLCampaignSettlementType::Port)
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
	case EWLCampaignSettlementType::Port:       Variants = &CityModelMediumVariants; TargetWidth = 5800.f;  break;
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

