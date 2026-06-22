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

	FVector ScaleToBounds(UStaticMesh* Mesh, float TargetX, float TargetY, float TargetZ)
	{
		if (!Mesh)
		{
			return FVector::OneVector;
		}

		const FVector Ext = Mesh->GetBounds().BoxExtent;
		return FVector(
			TargetX / FMath::Max(1.f, Ext.X * 2.f),
			TargetY / FMath::Max(1.f, Ext.Y * 2.f),
			TargetZ / FMath::Max(1.f, Ext.Z * 2.f));
	}

	FTransform MakeBottomAnchoredTransform(
		UStaticMesh* Mesh,
		const FVector& DesiredBottomCenter,
		const FRotator& Rotation,
		const FVector& Scale)
	{
		if (!Mesh)
		{
			return FTransform(Rotation, DesiredBottomCenter, Scale);
		}

		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector LocalBottomCenter(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		const FVector Location = DesiredBottomCenter - Rotation.RotateVector(LocalBottomCenter * Scale);
		return FTransform(Rotation, Location, Scale);
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

	// Carretera del teatro = UNA sola cinta continua (RoadMesh procedural). Cubre CO y VE
	// con calzada ancha, asfalto y linea central. Se descarto la capa de tiles de asset
	// (SM_road_001): al ser piezas discretas se veia "saltada"/rota en curvas y pendientes
	// y no se podia ensanchar a varias calzadas sin estirar la textura. La cinta es
	// continua, drapea sobre el relieve y permite ancho + marcas de carril controladas.
	FWLCampaignRouteBuilder::BuildDefaultTheaterRoutes(
		RoadMesh,
		VertexColorMaterial,
		[this](float Lon, float Lat)
		{
			return ProjectLonLat(Lon, Lat);
		});

	const auto AddTheaterCountryLabel = [this](const FString& Text, float Lon, float Lat, const FColor& Color, float WorldSize)
	{
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

	AddVegetationScatter(-75.5f, -70.0f, 0.8f, 6.6f, 7, 6, true);
	AddVegetationScatter(-68.0f, -62.2f, 3.4f, 7.4f, 7, 5, true);
	AddVegetationScatter(-70.5f, -64.8f, 7.2f, 9.5f, 6, 4, false);
	AddVegetationScatter(-76.8f, -73.5f, 8.5f, 11.8f, 4, 3, false);

	BuildIntercityRoads();
	BuildMovementNodesAndEdges();
	AddMilitaryForceMarkers();
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
	if (!CityId.Equals(TEXT("VE-MARACAIBO"), ESearchCase::IgnoreCase))
	{
		const FVector2D LandLonLat = NudgeSettlementToLand(Lon, Lat);
		Lon = LandLonLat.X;
		Lat = LandLonLat.Y;
	}

	FWLCampaignSettlementSpec Spec;
	Spec.Name = Name;
	Spec.Lon = Lon;
	Spec.Lat = Lat;
	Spec.Type = Type;
	Spec.AccentColor = AccentColor;
	FWLCampaignSettlementBuilder::AddSettlement(
		SettlementMesh,
		VertexColorMaterial,
		Spec,
		[this](float InLon, float InLat)
		{
			return ProjectLonLat(InLon, InLat);
		});

	// Vertical slice: Venezuela keeps a legible procedural city footprint and adds
	// real Cartoon_City_Free building meshes on top. Avoid instanced pack detail
	// because its materials render black in Standalone without project-wide edits.
	if (CountryIso.Equals(TEXT("VE"), ESearchCase::IgnoreCase) && CityBuildingMeshes.Num() > 0)
	{
		BuildMeshCity(Lon, Lat, Type);
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
	City.bCapital = bCapital;
	City.bPort = Type == EWLCampaignSettlementType::Port;
	AddCitySelectionProxy(City, bCapital ? 1.28f : (City.bPort ? 1.16f : 1.0f));

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(SceneRoot);
	Label->RegisterComponent();
	Label->SetWorldLocation(Base + FVector(0.f, 0.f, bCapital ? 2350.f : 1720.f));
	Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(bCapital ? 720.f : (bSmall ? 430.f : 540.f));
	Label->SetText(FText::FromString(Name));
	Label->SetTextRenderColor(bCapital ? FColor(230, 210, 140) : FColor(195, 205, 190));
	SettlementLabels.Add(Label);
}

void AWLCampaign3DView::BuildMeshCity(float Lon, float Lat, EWLCampaignSettlementType Type)
{
	if (CityBuildingMeshes.Num() == 0)
	{
		return;
	}

	int32 Radius;
	float DowntownHeight; // altura del nucleo (unidades de mundo; el mapa exagera lo vertical)
	float Fill;           // densidad 0..1
	switch (Type)
	{
	// Radios reducidos: la mancha de mallas reales era ~6840u (capital) = ~85 km y pegaba
	// las ciudades. Nucleo compacto -> ciudades separadas de verdad sobre el mapa.
	case EWLCampaignSettlementType::Capital:    Radius = 3; DowntownHeight = 1850.f; Fill = 0.78f; break;
	case EWLCampaignSettlementType::LargeCity:  Radius = 2; DowntownHeight = 1350.f; Fill = 0.72f; break;
	case EWLCampaignSettlementType::Port:       Radius = 2; DowntownHeight = 1120.f; Fill = 0.66f; break;
	case EWLCampaignSettlementType::Industrial: Radius = 2; DowntownHeight = 980.f;  Fill = 0.64f; break;
	case EWLCampaignSettlementType::Frontier:   Radius = 1; DowntownHeight = 560.f;  Fill = 0.46f; break;
	default:                                    Radius = 2; DowntownHeight = 900.f;  Fill = 0.60f; break;
	}

	const float Cell = 560.f;
	const float CityGroundLift = 900.f; // mismo orden que el builder procedural: sobre el mapa, sin hundirse.
	for (int32 GX = -Radius; GX <= Radius; ++GX)
	{
		for (int32 GY = -Radius; GY <= Radius; ++GY)
		{
			const float DistN = FMath::Sqrt(static_cast<float>(GX * GX + GY * GY)) / static_cast<float>(FMath::Max(1, Radius));
			if (DistN > 1.05f)
			{
				continue;
			}

			const float H1 = GridHash01(GX, GY, 1);
			const float H2 = GridHash01(GX, GY, 2);
			const float H3 = GridHash01(GX, GY, 3);
			const bool bMainAvenue = GX == 0 || GY == 0 || ((GX % 3) == 0 && FMath::Abs(GY) <= Radius - 1) || ((GY % 3) == 0 && FMath::Abs(GX) <= Radius - 1);

			const float CellLon = (static_cast<float>(GY) * Cell) / GeoScale;
			const float CellLat = (static_cast<float>(GX) * Cell) / GeoScale;
			const FVector Ground = ProjectLonLat(Lon + CellLon, Lat + CellLat) + FVector(0.f, 0.f, CityGroundLift);

			if (bMainAvenue || H1 > Fill * (1.f - DistN * 0.18f))
			{
				continue;
			}

			const int32 MeshIdx = FMath::Clamp(static_cast<int32>(H2 * CityBuildingMeshes.Num()), 0, CityBuildingMeshes.Num() - 1);
			UStaticMesh* Mesh = CityBuildingMeshes[MeshIdx];
			if (!Mesh)
			{
				continue;
			}

			const float Foot = Cell * (0.42f + H3 * 0.16f);
			float TargetH = FMath::Max(240.f, DowntownHeight * (1.f - DistN * 0.5f) * (0.6f + H3 * 0.8f));
			TargetH = FMath::Min(TargetH, Foot * 3.35f); // limita la esbeltez (sin losas/agujas)
			const FVector Scale = ScaleToBounds(Mesh, Foot, Foot, TargetH);

			const FRotator Rot(0.f, 90.f * FMath::RoundToFloat(H2 * 3.f), 0.f);
			UStaticMeshComponent* Building = NewObject<UStaticMeshComponent>(this);
			if (!Building)
			{
				continue;
			}
			Building->SetupAttachment(SceneRoot);
			Building->RegisterComponent();
			Building->SetStaticMesh(Mesh);
			Building->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Building->SetMobility(EComponentMobility::Movable);
			Building->SetWorldTransform(MakeBottomAnchoredTransform(Mesh, Ground + FVector(0.f, 0.f, 30.f), Rot, Scale));
			CityBuildingComponents.Add(Building);
		}
	}
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
	Marker->SetSphereRadius((City.bCapital ? 12500.f : 9800.f) * FMath::Max(0.65f, RadiusScale));
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

