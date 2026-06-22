// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaign3DView.h"

#include "WorldLeader.h"
#include "ProceduralMeshComponent.h"
#include "Components/SphereComponent.h"

namespace
{
	struct FScaleAuditPoint
	{
		const TCHAR* Id = TEXT("");
		const TCHAR* Name = TEXT("");
		float Lon = 0.f;
		float Lat = 0.f;
	};

	struct FScaleAuditPair
	{
		const TCHAR* Label = TEXT("");
		const TCHAR* A = TEXT("");
		const TCHAR* B = TEXT("");
	};

	double HaversineKm(float LonA, float LatA, float LonB, float LatB)
	{
		constexpr double EarthRadiusKm = 6371.0;
		const double LatARad = FMath::DegreesToRadians(static_cast<double>(LatA));
		const double LatBRad = FMath::DegreesToRadians(static_cast<double>(LatB));
		const double DLat = FMath::DegreesToRadians(static_cast<double>(LatB - LatA));
		const double DLon = FMath::DegreesToRadians(static_cast<double>(LonB - LonA));
		const double SinLat = FMath::Sin(DLat * 0.5);
		const double SinLon = FMath::Sin(DLon * 0.5);
		const double A = SinLat * SinLat + FMath::Cos(LatARad) * FMath::Cos(LatBRad) * SinLon * SinLon;
		return EarthRadiusKm * 2.0 * FMath::Atan2(FMath::Sqrt(A), FMath::Sqrt(1.0 - A));
	}

	const FScaleAuditPoint* FindPoint(const TArray<FScaleAuditPoint>& Points, const TCHAR* Id)
	{
		for (const FScaleAuditPoint& Point : Points)
		{
			if (FCString::Stricmp(Point.Id, Id) == 0)
			{
				return &Point;
			}
		}
		return nullptr;
	}

	void AccumulateSphereStats(
		const TArray<UPrimitiveComponent*>& Components,
		int32& OutCount,
		float& OutMinRadius,
		float& OutMaxRadius,
		float& OutAverageRadius)
	{
		OutCount = 0;
		OutMinRadius = TNumericLimits<float>::Max();
		OutMaxRadius = 0.f;
		OutAverageRadius = 0.f;

		for (const UPrimitiveComponent* Component : Components)
		{
			const USphereComponent* Sphere = Cast<USphereComponent>(Component);
			if (!Sphere)
			{
				continue;
			}

			const float Radius = Sphere->GetScaledSphereRadius();
			OutMinRadius = FMath::Min(OutMinRadius, Radius);
			OutMaxRadius = FMath::Max(OutMaxRadius, Radius);
			OutAverageRadius += Radius;
			++OutCount;
		}

		if (OutCount > 0)
		{
			OutAverageRadius /= static_cast<float>(OutCount);
		}
		else
		{
			OutMinRadius = 0.f;
		}
	}
}

void AWLCampaign3DView::LogScaleAudit() const
{
	int32 CityProxyCount = 0;
	float CityProxyMin = 0.f;
	float CityProxyMax = 0.f;
	float CityProxyAvg = 0.f;
	AccumulateSphereStats(CitySelectionMarkers, CityProxyCount, CityProxyMin, CityProxyMax, CityProxyAvg);

	int32 ForceProxyCount = 0;
	float ForceProxyMin = 0.f;
	float ForceProxyMax = 0.f;
	float ForceProxyAvg = 0.f;
	AccumulateSphereStats(ForceSelectionMarkers, ForceProxyCount, ForceProxyMin, ForceProxyMax, ForceProxyAvg);

	int32 ActiveRoadAssetComponents = 0;
	for (const UStaticMeshComponent* Road : RoadAssetComponents)
	{
		if (Road)
		{
			++ActiveRoadAssetComponents;
		}
	}

	const float OriginLatRad = FMath::DegreesToRadians(TheaterCenterLonLat.Y);
	const float StrategicUnitsPerKmNorth = GeoScale / 111.32f;
	const float StrategicUnitsPerKmEast = GeoScale / (111.32f * FMath::Max(0.01f, FMath::Cos(OriginLatRad)));

	UE_LOG(
		LogWorldLeader,
		Log,
		TEXT("Campaign3D ScaleAudit summary: GeoScale=%.1f DetailUUPerKm=%.1f CenterLonLat=(%.2f,%.2f) StrategicUUPerKm(N/E)=%.1f/%.1f RoadMeshSections=%d RoadAssetMeshes=%d RoadAssetComponents=%d Cities=%d CityLabels=%d CountryLabels=%d MovementNodes=%d Forces=%d"),
		GeoScale,
		DetailWorldUnitsPerKm,
		TheaterCenterLonLat.X,
		TheaterCenterLonLat.Y,
		StrategicUnitsPerKmNorth,
		StrategicUnitsPerKmEast,
		RoadMesh ? RoadMesh->GetNumSections() : 0,
		RoadAssetMeshes.Num(),
		ActiveRoadAssetComponents,
		CityViews.Num(),
		SettlementLabels.Num(),
		Labels.Num(),
		MovementNodes.Num(),
		ForceViews.Num());

	UE_LOG(
		LogWorldLeader,
		Log,
		TEXT("Campaign3D ScaleAudit selection: CityProxy count=%d radius(min/avg/max)=%.0f/%.0f/%.0f ForceProxy count=%d radius(min/avg/max)=%.0f/%.0f/%.0f"),
		CityProxyCount,
		CityProxyMin,
		CityProxyAvg,
		CityProxyMax,
		ForceProxyCount,
		ForceProxyMin,
		ForceProxyAvg,
		ForceProxyMax);

	if (RoadAssetMeshes.Num() > 0 && ActiveRoadAssetComponents == 0)
	{
		UE_LOG(
			LogWorldLeader,
			Warning,
			TEXT("Campaign3D ScaleAudit road assets: road meshes are loaded, but no RoadAssetComponents are active. Confirm Standalone before treating the asset-road layer as live."));
	}

	const TArray<FScaleAuditPoint> Points = {
		{ TEXT("VE-MARACAIBO"), TEXT("Maracaibo"), -71.82f, 10.58f },
		{ TEXT("VE-SAN-CRISTOBAL"), TEXT("San Cristobal"), -72.23f, 7.77f },
		{ TEXT("VE-SAN-ANTONIO"), TEXT("San Antonio"), -72.44f, 7.82f },
		{ TEXT("CO-CUCUTA"), TEXT("Cucuta"), -72.50f, 7.90f },
		{ TEXT("CO-PAMPLONA"), TEXT("Pamplona"), -72.65f, 7.38f },
		{ TEXT("VE-MERIDA"), TEXT("Merida"), -71.14f, 8.60f },
		{ TEXT("VE-VALERA"), TEXT("Valera"), -70.60f, 9.32f },
		{ TEXT("VE-BARINAS"), TEXT("Barinas"), -70.21f, 8.62f },
		{ TEXT("VE-BARQUISIMETO"), TEXT("Barquisimeto"), -69.35f, 10.07f }
	};

	const TArray<FScaleAuditPair> Pairs = {
		{ TEXT("Maracaibo -> San Cristobal"), TEXT("VE-MARACAIBO"), TEXT("VE-SAN-CRISTOBAL") },
		{ TEXT("Maracaibo -> Merida"), TEXT("VE-MARACAIBO"), TEXT("VE-MERIDA") },
		{ TEXT("San Cristobal -> Cucuta"), TEXT("VE-SAN-CRISTOBAL"), TEXT("CO-CUCUTA") },
		{ TEXT("San Cristobal -> San Antonio"), TEXT("VE-SAN-CRISTOBAL"), TEXT("VE-SAN-ANTONIO") },
		{ TEXT("San Antonio -> Cucuta"), TEXT("VE-SAN-ANTONIO"), TEXT("CO-CUCUTA") },
		{ TEXT("Cucuta -> Pamplona"), TEXT("CO-CUCUTA"), TEXT("CO-PAMPLONA") },
		{ TEXT("San Cristobal -> Merida"), TEXT("VE-SAN-CRISTOBAL"), TEXT("VE-MERIDA") },
		{ TEXT("Merida -> Valera"), TEXT("VE-MERIDA"), TEXT("VE-VALERA") },
		{ TEXT("San Cristobal -> Barinas"), TEXT("VE-SAN-CRISTOBAL"), TEXT("VE-BARINAS") },
		{ TEXT("Valera -> Barquisimeto"), TEXT("VE-VALERA"), TEXT("VE-BARQUISIMETO") }
	};

	for (const FScaleAuditPair& Pair : Pairs)
	{
		const FScaleAuditPoint* PointA = FindPoint(Points, Pair.A);
		const FScaleAuditPoint* PointB = FindPoint(Points, Pair.B);
		if (!PointA || !PointB)
		{
			continue;
		}

		const FVector WorldA = ProjectLonLat(PointA->Lon, PointA->Lat);
		const FVector WorldB = ProjectLonLat(PointB->Lon, PointB->Lat);
		const float DistanceUU = FVector::Dist2D(WorldA, WorldB);
		const double DistanceKm = HaversineKm(PointA->Lon, PointA->Lat, PointB->Lon, PointB->Lat);
		const double UnitsPerKm = DistanceKm > UE_DOUBLE_KINDA_SMALL_NUMBER ? DistanceUU / DistanceKm : 0.0;
		const bool bProxyRisk = CityProxyMax > 0.f && DistanceUU < CityProxyMax * 2.f;

		UE_LOG(
			LogWorldLeader,
			Log,
			TEXT("Campaign3D ScaleAudit pair %s: km=%.1f uu=%.0f uu_per_km=%.1f heightA=%.0f heightB=%.0f proxyRisk=%s"),
			Pair.Label,
			DistanceKm,
			DistanceUU,
			UnitsPerKm,
			WorldA.Z,
			WorldB.Z,
			bProxyRisk ? TEXT("yes") : TEXT("no"));
	}
}
