// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignVisualStyle.h"

EWLVisualBiome FWLCampaignVisualStyle::ClassifyVisualBiome(float Lon, float Lat, bool bCoreCountry)
{
	if (!bCoreCountry)
	{
		return EWLVisualBiome::Context;
	}

	// Modelo continental: clasificamos por geografia real respecto a la cresta de los
	// Andes del norte (no por rangos fijos CO/VE). Asi cada pais "teatro" recibe biomas
	// coherentes: Andes (montana), vertiente del Pacifico (costa), Caribe (costa),
	// llanos del Orinoco (sabana) y Amazonia/escudo guayanes (jungla).
	auto AndesCrestLon = [](float L) -> float
	{
		const float Lats[] = { 11.0f, 9.0f, 7.0f, 5.0f, 2.5f, 0.0f, -2.5f, -5.5f, -8.0f, -11.0f, -14.0f, -17.0f, -20.0f, -23.0f, -27.0f, -33.0f, -40.0f, -47.0f, -53.0f, -56.0f };
		const float Lons[] = { -72.0f, -71.0f, -73.0f, -74.6f, -76.3f, -78.4f, -79.0f, -79.4f, -77.8f, -76.3f, -72.8f, -69.0f, -67.5f, -66.5f, -69.0f, -70.0f, -71.5f, -72.8f, -72.5f, -69.0f };
		const int32 N = 20;
		if (L >= Lats[0]) return Lons[0];
		if (L <= Lats[N - 1]) return Lons[N - 1];
		for (int32 i = 0; i < N - 1; ++i)
		{
			if (L <= Lats[i] && L >= Lats[i + 1])
			{
				const float T = (L - Lats[i + 1]) / (Lats[i] - Lats[i + 1]);
				return FMath::Lerp(Lons[i + 1], Lons[i], T);
			}
		}
		return Lons[N - 1];
	};

	// Costa del Caribe (solo la franja costera continental del norte de Sudamerica;
	// las islas Lat > 13.5 las trata el bloque de Mesoamerica -> tropical).
	if (Lat > 9.7f && Lat < 13.5f && Lon > -73.5f)
	{
		return EWLVisualBiome::Coast;
	}

	// Norteamerica + Mesoamerica (Lat > 13.5, al norte de Sudamerica). Un solo modelo
	// para EEUU y Mexico (comparten frontera/desierto del suroeste).
	if (Lat > 13.5f)
	{
		// Costa oeste / Pacifico: bosque humedo al norte (Pacific NW), seco al sur (California/Baja).
		if (Lon < -117.f)
		{
			return Lat > 42.f ? EWLVisualBiome::Jungle : EWLVisualBiome::Coast;
		}
		// Montanas Rocosas (oeste interior, latitudes altas).
		if (Lat > 37.f && Lon < -104.f && Lon > -116.f)
		{
			return EWLVisualBiome::Mountain;
		}
		// Desierto del suroeste de EEUU + norte arido de Mexico / Baja (mitad oeste).
		if (Lat > 23.f && Lon < -99.f)
		{
			return EWLVisualBiome::Coast;
		}
		// Grandes Llanuras de EEUU.
		if (Lat > 30.f && Lon < -95.f)
		{
			return EWLVisualBiome::Llanos;
		}
		// Este de EEUU (bosque templado), Golfo y Florida (humedo) -> verde.
		if (Lat > 27.f)
		{
			return EWLVisualBiome::Jungle;
		}
		// --- Mexico (Lat 13.5 - 27) ---
		if (Lat > 18.f && Lon < -97.f && Lon > -104.f)
		{
			return EWLVisualBiome::Mountain; // Sierra Madre / altiplano central
		}
		return EWLVisualBiome::Jungle; // sur tropical, Yucatan, Caribe, costas
	}

	// Centroamerica (istmo tropical: Honduras/Nicaragua/Costa Rica/Panama) al sur de 13.5.
	if (Lat > 6.5f && Lat < 13.6f && Lon < -78.f)
	{
		return EWLVisualBiome::Jungle;
	}

	const float CrestLon = AndesCrestLon(Lat);
	const float DeltaToAndes = Lon - CrestLon; // <0 al oeste (Pacifico), >0 al este
	// Banda andina: ancha en CO/VE (varias cordilleras), estrecha en Ecuador y ancha
	// otra vez hacia el sur (altiplano peruano-boliviano).
	float AndesBand;
	if (Lat > 2.f)
	{
		AndesBand = 1.6f;
	}
	else if (Lat > -1.f)
	{
		AndesBand = FMath::Lerp(0.9f, 1.6f, (Lat + 1.f) / 3.f);
	}
	else if (Lat > -6.f)
	{
		AndesBand = 0.9f;
	}
	else if (Lat > -28.f)
	{
		AndesBand = FMath::Min(2.5f, 0.9f + (-6.f - Lat) * 0.105f); // altiplano peruano-boliviano
	}
	else if (Lat > -34.f)
	{
		AndesBand = FMath::Lerp(2.5f, 1.2f, (-28.f - Lat) / 6.f); // transicion hacia el sur
	}
	else
	{
		AndesBand = 1.2f; // Andes patagonicos (mas angostos)
	}

	if (FMath::Abs(DeltaToAndes) <= AndesBand)
	{
		return EWLVisualBiome::Mountain;
	}
	if (DeltaToAndes < 0.f)
	{
		// Vertiente del Pacifico: desierto/costa al norte-centro, bosque humedo al sur de Chile.
		return Lat < -38.f ? EWLVisualBiome::Jungle : EWLVisualBiome::Coast;
	}

	// Sudamerica oriental (Brasil y tierras bajas del este). Solo Lon > -60, asi que
	// no afecta a los paises andinos (CO/VE/EC/PE/BO estan al oeste de -60).
	if (Lon > -60.f)
	{
		if (Lat > -6.f)
		{
			return EWLVisualBiome::Jungle; // Amazonia (norte)
		}
		if (Lat < -40.f)
		{
			return EWLVisualBiome::Coast; // estepa patagonica (atlantica)
		}
		if (Lat < -28.f)
		{
			return EWLVisualBiome::Llanos; // pampa / sur subtropical
		}
		if (Lat > -16.f && Lon > -44.f)
		{
			return EWLVisualBiome::Coast; // caatinga (nordeste semiarido)
		}
		if (Lon > -50.f && Lat < -16.f)
		{
			return EWLVisualBiome::Jungle; // Mata Atlantica (SE/S)
		}
		return EWLVisualBiome::Llanos; // cerrado (sabana central)
	}

	// Este de los Andes, cono sur (Argentina al oeste de -60).
	if (Lat < -23.f)
	{
		return Lat < -40.f ? EWLVisualBiome::Coast : EWLVisualBiome::Llanos; // estepa patagonica / pampa
	}

	if (Lat >= 4.f && Lat < 9.7f && Lon < -64.f)
	{
		return EWLVisualBiome::Llanos; // llanos del Orinoco
	}
	return EWLVisualBiome::Jungle; // Amazonia / escudo guayanes
}

FLinearColor FWLCampaignVisualStyle::VisualBiomeColor(EWLVisualBiome Biome)
{
	switch (Biome)
	{
	case EWLVisualBiome::Coast:
		return FLinearColor(0.540f, 0.490f, 0.320f);  // costa / arena (Pacifico y Caribe)
	case EWLVisualBiome::Jungle:
		return FLinearColor(0.050f, 0.235f, 0.085f);  // Amazonia / selva
	case EWLVisualBiome::Llanos:
		return FLinearColor(0.340f, 0.370f, 0.155f);  // sabana / llanos
	case EWLVisualBiome::Mountain:
		return FLinearColor(0.185f, 0.300f, 0.190f);  // Andes: verde, se funde (sin franja palida)
	case EWLVisualBiome::UrbanInfluence:
		return FLinearColor(0.300f, 0.285f, 0.205f);
	default:
		return FLinearColor(0.050f, 0.105f, 0.070f);
	}
}

FLinearColor FWLCampaignVisualStyle::ShadeTerrainVertex(const FLinearColor& Base, float Lon, float Lat, float Height)
{
	// SIN realce por altura: antes el Andes alto se aclaraba y formaba una franja palida
	// continua de Colombia a Chile que rompia el mapa. Ahora la altura NO cambia el color
	// (la montana se ve como el resto del continente); el relieve 3D sigue por geometria.
	const float Noise = 0.06f * FMath::Sin(Lon * 4.7f + Lat * 1.3f) + 0.04f * FMath::Cos(Lon * 2.1f - Lat * 3.4f);
	const float Light = FMath::Clamp(0.92f + Noise, 0.74f, 1.06f);
	FLinearColor Color(Base.R * Light, Base.G * Light, Base.B * Light, 1.f);
	Color.R = FMath::Clamp(Color.R, 0.f, 1.f);
	Color.G = FMath::Clamp(Color.G, 0.f, 1.f);
	Color.B = FMath::Clamp(Color.B, 0.f, 1.f);
	return Color;
}

FColor FWLCampaignVisualStyle::ToVertexFColor(const FLinearColor& Color)
{
	return Color.ToFColor(true);
}

float FWLCampaignVisualStyle::VisualBiomeZOffset(EWLVisualBiome Biome, bool bCoreCountry)
{
	if (!bCoreCountry)
	{
		return -120.f;
	}
	switch (Biome)
	{
	case EWLVisualBiome::Mountain:
		return 560.f;
	case EWLVisualBiome::Coast:
		return 95.f;
	case EWLVisualBiome::Llanos:
		return 135.f;
	case EWLVisualBiome::UrbanInfluence:
		return 210.f;
	default:
		return 165.f;
	}
}

FLinearColor FWLCampaignVisualStyle::CountryTerrainColor(const FString& Iso)
{
	if (Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.235f, 0.345f, 0.185f);
	}
	if (Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase))
	{
		return FLinearColor(0.215f, 0.335f, 0.175f);
	}
	return FLinearColor(0.095f, 0.160f, 0.115f);
}
