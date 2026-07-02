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

void AWLCampaign3DView::BuildMovementNodesAndEdges()
{
	MovementNodes.Reset();
	MovementAdjacency.Reset();

	for (const FWLCampaign3DCityView& City : CityViews)
	{
		if (City.Id.IsEmpty())
		{
			continue;
		}

		FWLCampaign3DMovementNodeView Node;
		Node.Id = City.Id;
		Node.Name = City.Name;
		Node.CountryIso = City.CountryIso;
		Node.ProvinceId = City.TerritoryId;
		Node.ProvinceName = City.TerritoryName;
		Node.NodeType = City.bPort ? TEXT("puerto") : City.CityType;
		Node.WorldLocation = City.WorldLocation;
		Node.bPort = City.bPort
			|| City.Id.Equals(TEXT("VE-MARACAIBO"), ESearchCase::IgnoreCase)
			|| City.Id.Equals(TEXT("VE-PUERTO-LA-CRUZ"), ESearchCase::IgnoreCase);
		Node.Lon = City.Lon;
		Node.Lat = City.Lat;
		MovementNodes.Add(Node);
	}

	const auto AddCountryMovementTree = [this](const TCHAR* CountryIso, float MaxDegreeDistance)
	{
		TArray<const FWLCampaign3DMovementNodeView*> CountryNodes;
		for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
		{
			if (Node.CountryIso.Equals(CountryIso, ESearchCase::IgnoreCase))
			{
				CountryNodes.Add(&Node);
			}
		}
		const int32 N = CountryNodes.Num();
		if (N < 2)
		{
			return;
		}

		TArray<bool> InTree;
		InTree.Init(false, N);
		TArray<float> BestDist;
		BestDist.Init(TNumericLimits<float>::Max(), N);
		TArray<int32> BestFrom;
		BestFrom.Init(-1, N);
		InTree[0] = true;
		for (int32 Index = 1; Index < N; ++Index)
		{
			BestDist[Index] = FVector2D::Distance(
				FVector2D(CountryNodes[0]->Lon, CountryNodes[0]->Lat),
				FVector2D(CountryNodes[Index]->Lon, CountryNodes[Index]->Lat));
			BestFrom[Index] = 0;
		}

		for (int32 Added = 1; Added < N; ++Added)
		{
			int32 Next = -1;
			float NextDist = TNumericLimits<float>::Max();
			for (int32 Index = 0; Index < N; ++Index)
			{
				if (!InTree[Index] && BestDist[Index] < NextDist)
				{
					Next = Index;
					NextDist = BestDist[Index];
				}
			}
			if (Next < 0)
			{
				break;
			}

			InTree[Next] = true;
			const int32 From = BestFrom[Next];
			if (From >= 0 && NextDist <= MaxDegreeDistance)
			{
				AddMovementEdge(CountryNodes[From]->Id, CountryNodes[Next]->Id);
			}

			for (int32 Index = 0; Index < N; ++Index)
			{
				if (InTree[Index])
				{
					continue;
				}
				const float Distance = FVector2D::Distance(
					FVector2D(CountryNodes[Next]->Lon, CountryNodes[Next]->Lat),
					FVector2D(CountryNodes[Index]->Lon, CountryNodes[Index]->Lat));
				if (Distance < BestDist[Index])
				{
					BestDist[Index] = Distance;
					BestFrom[Index] = Next;
				}
			}
		}
	};

	const auto AddCountryMovementLocalLinks = [this](const TCHAR* CountryIso, int32 LinksPerNode, float MaxDegreeDistance)
	{
		TArray<const FWLCampaign3DMovementNodeView*> CountryNodes;
		for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
		{
			if (Node.CountryIso.Equals(CountryIso, ESearchCase::IgnoreCase))
			{
				CountryNodes.Add(&Node);
			}
		}

		for (const FWLCampaign3DMovementNodeView* Node : CountryNodes)
		{
			TArray<TPair<float, const FWLCampaign3DMovementNodeView*>> Nearest;
			for (const FWLCampaign3DMovementNodeView* Candidate : CountryNodes)
			{
				if (Candidate == Node)
				{
					continue;
				}

				const float Distance = FVector2D::Distance(FVector2D(Node->Lon, Node->Lat), FVector2D(Candidate->Lon, Candidate->Lat));
				if (Distance <= MaxDegreeDistance)
				{
					Nearest.Add(TPair<float, const FWLCampaign3DMovementNodeView*>(Distance, Candidate));
				}
			}
			Nearest.Sort([](const TPair<float, const FWLCampaign3DMovementNodeView*>& A, const TPair<float, const FWLCampaign3DMovementNodeView*>& B)
			{
				return A.Key < B.Key;
			});

			for (int32 Index = 0; Index < Nearest.Num() && Index < LinksPerNode; ++Index)
			{
				AddMovementEdge(Node->Id, Nearest[Index].Value->Id);
			}
		}
	};

	const auto AddNearestCountryPair = [this](const TCHAR* A, const TCHAR* B, float MaxDegreeDistance)
	{
		const FWLCampaign3DMovementNodeView* BestA = nullptr;
		const FWLCampaign3DMovementNodeView* BestB = nullptr;
		float BestDistance = MaxDegreeDistance;
		for (const FWLCampaign3DMovementNodeView& NodeA : MovementNodes)
		{
			if (!NodeA.CountryIso.Equals(A, ESearchCase::IgnoreCase))
			{
				continue;
			}
			for (const FWLCampaign3DMovementNodeView& NodeB : MovementNodes)
			{
				if (!NodeB.CountryIso.Equals(B, ESearchCase::IgnoreCase))
				{
					continue;
				}
				const float Distance = FVector2D::Distance(FVector2D(NodeA.Lon, NodeA.Lat), FVector2D(NodeB.Lon, NodeB.Lat));
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					BestA = &NodeA;
					BestB = &NodeB;
				}
			}
		}
		if (BestA && BestB)
		{
			AddMovementEdge(BestA->Id, BestB->Id);
		}
	};

	const auto IsSouthAmericaMovementIso = [](const FString& Iso)
	{
		return Iso.Equals(TEXT("CO"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("VE"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("GY"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("SR"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("GF"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("EC"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("PE"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("BO"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("BR"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("AR"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("CL"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("UY"), ESearchCase::IgnoreCase)
			|| Iso.Equals(TEXT("PY"), ESearchCase::IgnoreCase);
	};

	const float InternalCap = 13.0f;
	AddCountryMovementTree(TEXT("CO"), InternalCap);
	AddCountryMovementTree(TEXT("VE"), InternalCap);
	AddCountryMovementTree(TEXT("GY"), InternalCap);
	AddCountryMovementTree(TEXT("SR"), InternalCap);
	AddCountryMovementTree(TEXT("GF"), InternalCap);
	AddCountryMovementTree(TEXT("EC"), InternalCap);
	AddCountryMovementTree(TEXT("PE"), InternalCap);
	AddCountryMovementTree(TEXT("BO"), InternalCap);
	AddCountryMovementTree(TEXT("BR"), InternalCap);
	AddCountryMovementTree(TEXT("AR"), InternalCap);
	AddCountryMovementTree(TEXT("CL"), InternalCap);
	AddCountryMovementTree(TEXT("UY"), InternalCap);
	AddCountryMovementTree(TEXT("PY"), InternalCap);

	AddCountryMovementLocalLinks(TEXT("CO"), 3, 6.0f);
	AddCountryMovementLocalLinks(TEXT("VE"), 3, 6.0f);
	AddCountryMovementLocalLinks(TEXT("GY"), 2, 6.0f);
	AddCountryMovementLocalLinks(TEXT("SR"), 2, 6.0f);
	AddCountryMovementLocalLinks(TEXT("GF"), 2, 6.0f);
	AddCountryMovementLocalLinks(TEXT("EC"), 3, 5.0f);
	AddCountryMovementLocalLinks(TEXT("PE"), 3, 7.0f);
	AddCountryMovementLocalLinks(TEXT("BO"), 3, 7.0f);
	AddCountryMovementLocalLinks(TEXT("BR"), 3, 8.0f);
	AddCountryMovementLocalLinks(TEXT("AR"), 3, 9.0f);
	AddCountryMovementLocalLinks(TEXT("CL"), 3, 8.0f);
	AddCountryMovementLocalLinks(TEXT("UY"), 3, 5.0f);
	AddCountryMovementLocalLinks(TEXT("PY"), 3, 6.0f);

	AddNearestCountryPair(TEXT("CO"), TEXT("VE"), 4.5f);
	AddNearestCountryPair(TEXT("CO"), TEXT("EC"), 4.5f);
	AddNearestCountryPair(TEXT("CO"), TEXT("PE"), 4.5f);
	AddNearestCountryPair(TEXT("CO"), TEXT("BR"), 4.5f);
	AddNearestCountryPair(TEXT("VE"), TEXT("BR"), 4.5f);
	AddNearestCountryPair(TEXT("VE"), TEXT("GY"), 4.5f);
	AddNearestCountryPair(TEXT("GY"), TEXT("BR"), 4.5f);
	AddNearestCountryPair(TEXT("GY"), TEXT("SR"), 4.5f);
	AddNearestCountryPair(TEXT("SR"), TEXT("GF"), 4.5f);
	AddNearestCountryPair(TEXT("GF"), TEXT("BR"), 5.0f);
	AddNearestCountryPair(TEXT("EC"), TEXT("PE"), 4.5f);
	AddNearestCountryPair(TEXT("PE"), TEXT("BR"), 4.5f);
	AddNearestCountryPair(TEXT("PE"), TEXT("BO"), 4.5f);
	AddNearestCountryPair(TEXT("PE"), TEXT("CL"), 4.5f);
	AddNearestCountryPair(TEXT("BO"), TEXT("BR"), 5.0f);
	AddNearestCountryPair(TEXT("BO"), TEXT("PY"), 5.0f);
	AddNearestCountryPair(TEXT("BO"), TEXT("AR"), 5.0f);
	AddNearestCountryPair(TEXT("BO"), TEXT("CL"), 5.0f);
	AddNearestCountryPair(TEXT("BR"), TEXT("PY"), 4.5f);
	AddNearestCountryPair(TEXT("BR"), TEXT("AR"), 4.5f);
	AddNearestCountryPair(TEXT("BR"), TEXT("UY"), 4.5f);
	AddNearestCountryPair(TEXT("PY"), TEXT("AR"), 4.5f);
	AddNearestCountryPair(TEXT("AR"), TEXT("UY"), 4.5f);
	AddNearestCountryPair(TEXT("AR"), TEXT("CL"), 5.0f);

	int32 EdgeCount = 0;
	for (const TPair<FString, TArray<FString>>& Pair : MovementAdjacency)
	{
		EdgeCount += Pair.Value.Num();
	}
	EdgeCount /= 2;

	TSet<FString> Visited;
	int32 ComponentCount = 0;
	for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
	{
		if (Visited.Contains(Node.Id))
		{
			continue;
		}
		++ComponentCount;
		TArray<FString> Stack;
		Stack.Add(Node.Id);
		Visited.Add(Node.Id);
		while (!Stack.IsEmpty())
		{
			const FString Current = Stack.Pop(EAllowShrinking::No);
			const TArray<FString>* Neighbors = MovementAdjacency.Find(Current);
			if (!Neighbors)
			{
				continue;
			}
			for (const FString& Neighbor : *Neighbors)
			{
				if (!Visited.Contains(Neighbor))
				{
					Visited.Add(Neighbor);
					Stack.Add(Neighbor);
				}
			}
		}
	}

	TSet<FString> SouthAmericaVisited;
	int32 SouthAmericaNodeCount = 0;
	int32 SouthAmericaEdgeCount = 0;
	int32 SouthAmericaComponentCount = 0;
	for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
	{
		if (!IsSouthAmericaMovementIso(Node.CountryIso))
		{
			continue;
		}
		++SouthAmericaNodeCount;
		const TArray<FString>* Neighbors = MovementAdjacency.Find(Node.Id);
		if (Neighbors)
		{
			for (const FString& Neighbor : *Neighbors)
			{
				const FWLCampaign3DMovementNodeView* NeighborNode = FindMovementNodeById(Neighbor);
				if (NeighborNode && IsSouthAmericaMovementIso(NeighborNode->CountryIso))
				{
					++SouthAmericaEdgeCount;
				}
			}
		}
	}
	SouthAmericaEdgeCount /= 2;
	for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
	{
		if (!IsSouthAmericaMovementIso(Node.CountryIso) || SouthAmericaVisited.Contains(Node.Id))
		{
			continue;
		}
		++SouthAmericaComponentCount;
		TArray<FString> Stack;
		Stack.Add(Node.Id);
		SouthAmericaVisited.Add(Node.Id);
		while (!Stack.IsEmpty())
		{
			const FString Current = Stack.Pop(EAllowShrinking::No);
			const TArray<FString>* Neighbors = MovementAdjacency.Find(Current);
			if (!Neighbors)
			{
				continue;
			}
			for (const FString& Neighbor : *Neighbors)
			{
				const FWLCampaign3DMovementNodeView* NeighborNode = FindMovementNodeById(Neighbor);
				if (NeighborNode && IsSouthAmericaMovementIso(NeighborNode->CountryIso) && !SouthAmericaVisited.Contains(Neighbor))
				{
					SouthAmericaVisited.Add(Neighbor);
					Stack.Add(Neighbor);
				}
			}
		}
	}

	UE_LOG(
		LogWorldLeader,
		Log,
		TEXT("Campaign3D movement graph complete. Nodes=%d Edges=%d Components=%d."),
		MovementNodes.Num(),
		EdgeCount,
		ComponentCount);
	UE_LOG(
		LogWorldLeader,
		Log,
		TEXT("Campaign3D South America movement audit. Nodes=%d Edges=%d Components=%d."),
		SouthAmericaNodeCount,
		SouthAmericaEdgeCount,
		SouthAmericaComponentCount);

	BuildRoadFollowPolylines();   // polilineas de via para que los ejercitos conduzcan por el trazado real
}

void AWLCampaign3DView::BuildRoadFollowPolylines()
{
	RoadFollowPolylines.Reset();
	for (const FWLCampaignRouteSpec& Route : FWLCampaignRouteBuilder::GetAllRoadRoutes())
	{
		// SUAVIZADO + DENSIFICADO igual que la via visible -> el tanque sigue EXACTAMENTE el trazado dibujado
		// (curva a curva), en vez de los puntos de control crudos (que lo hacian cortar/saltar/ir por lejos).
		const TArray<FVector2D> Smoothed = FWLCampaignRouteBuilder::BuildSmoothedRouteLonLat(Route);
		if (Smoothed.Num() < 2)
		{
			continue;
		}
		TArray<FVector> Poly;
		Poly.Reserve(Smoothed.Num());
		for (const FVector2D& P : Smoothed)
		{
			Poly.Add(ProjectLonLat(P.X, P.Y));   // XY exacta de la via (la Z la aterriza el tanque al moverse)
		}
		RoadFollowPolylines.Add(MoveTemp(Poly));
	}
	UE_LOG(LogWorldLeader, Log, TEXT("Campaign3D road-follow polylines: %d vias."), RoadFollowPolylines.Num());
}

bool AWLCampaign3DView::ComputeRoadPath(const FVector& StartWorld, const FVector& EndWorld, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (RoadFollowPolylines.Num() == 0)
	{
		return false;
	}

	// Punto mas cercano (segmento + parametro t) de un punto P a una polilinea PL.
	auto NearestOnPoly = [](const TArray<FVector>& PL, const FVector& P, int32& OutSeg, float& OutT, FVector& OutProj) -> float
	{
		float BestSq = TNumericLimits<float>::Max();
		OutSeg = 0; OutT = 0.f; OutProj = PL.Num() > 0 ? PL[0] : P;
		for (int32 i = 0; i + 1 < PL.Num(); ++i)
		{
			const FVector2D A(PL[i].X, PL[i].Y);
			const FVector2D B(PL[i + 1].X, PL[i + 1].Y);
			const FVector2D AB = B - A;
			const float L2 = AB.SizeSquared();
			float t = (L2 > 1.f) ? FVector2D::DotProduct(FVector2D(P.X, P.Y) - A, AB) / L2 : 0.f;
			t = FMath::Clamp(t, 0.f, 1.f);
			const FVector2D Proj = A + AB * t;
			const float DSq = FVector2D::DistSquared(FVector2D(P.X, P.Y), Proj);
			if (DSq < BestSq)
			{
				BestSq = DSq;
				OutSeg = i;
				OutT = t;
				OutProj = FVector(Proj.X, Proj.Y, 0.f);
			}
		}
		return FMath::Sqrt(BestSq);
	};

	// Elige la via donde Start y End proyectan mas cerca (deben ir por la MISMA via para seguir su trazado).
	int32 BestPoly = INDEX_NONE;
	int32 BestSStart = 0, BestSEnd = 0;
	FVector BestProjStart, BestProjEnd;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 PolyIdx = 0; PolyIdx < RoadFollowPolylines.Num(); ++PolyIdx)
	{
		const TArray<FVector>& PL = RoadFollowPolylines[PolyIdx];
		if (PL.Num() < 2)
		{
			continue;
		}
		int32 sS, sE; float tS, tE; FVector pS, pE;
		const float dS = NearestOnPoly(PL, StartWorld, sS, tS, pS);
		const float dE = NearestOnPoly(PL, EndWorld, sE, tE, pE);
		const float Score = dS + dE;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestPoly = PolyIdx;
			BestSStart = sS; BestSEnd = sE;
			BestProjStart = pS; BestProjEnd = pE;
		}
	}
	if (BestPoly == INDEX_NONE)
	{
		return false;
	}
	// Ambos extremos deben estar razonablemente sobre la MISMA via; si no, el llamador usa recto.
	const float HalfScore = BestScore * 0.5f;
	if (HalfScore > 45000.f)
	{
		return false;
	}

	const TArray<FVector>& PL = RoadFollowPolylines[BestPoly];
	OutPoints.Add(StartWorld);          // [0] = posicion actual del ejercito
	OutPoints.Add(BestProjStart);       // entra a la via
	if (BestSStart < BestSEnd)
	{
		for (int32 i = BestSStart + 1; i <= BestSEnd; ++i) { OutPoints.Add(PL[i]); }
	}
	else if (BestSStart > BestSEnd)
	{
		for (int32 i = BestSStart; i >= BestSEnd + 1; --i) { OutPoints.Add(PL[i]); }
	}
	OutPoints.Add(BestProjEnd);         // punto destino sobre la via
	return OutPoints.Num() >= 2;
}

bool AWLCampaign3DView::NearestRoadPointWorld(const FVector& World, FVector& OutPoint) const
{
	float BestSq = TNumericLimits<float>::Max();
	bool bFound = false;
	for (const TArray<FVector>& PL : RoadFollowPolylines)
	{
		for (int32 i = 0; i + 1 < PL.Num(); ++i)
		{
			const FVector2D A(PL[i].X, PL[i].Y);
			const FVector2D B(PL[i + 1].X, PL[i + 1].Y);
			const FVector2D AB = B - A;
			const float L2 = AB.SizeSquared();
			float t = (L2 > 1.f) ? FVector2D::DotProduct(FVector2D(World.X, World.Y) - A, AB) / L2 : 0.f;
			t = FMath::Clamp(t, 0.f, 1.f);
			const FVector2D Proj = A + AB * t;
			const float DSq = FVector2D::DistSquared(FVector2D(World.X, World.Y), Proj);
			if (DSq < BestSq)
			{
				BestSq = DSq;
				OutPoint = FVector(Proj.X, Proj.Y, 0.f);
				bFound = true;
			}
		}
	}
	return bFound;
}

FVector AWLCampaign3DView::ClampTargetOffCities(const FVector& TargetWorld, const FVector& FromWorld) const
{
	// Si el objetivo cae dentro del "delantal" de una ciudad, lo empuja a su borde (hacia el ejercito) para que
	// el tanque pare FUERA de la ciudad, no montado sobre sus edificios.
	const float Apron = 6500.f;
	for (const FWLCampaign3DCityView& City : CityViews)
	{
		const FVector C = ProjectLonLat(City.Lon, City.Lat);
		const float D = FVector::Dist2D(TargetWorld, C);
		if (D < Apron)
		{
			FVector2D Away(TargetWorld.X - C.X, TargetWorld.Y - C.Y);
			if (Away.SizeSquared() < 1.f)   // objetivo casi en el centro -> aparta hacia el ejercito
			{
				Away = FVector2D(FromWorld.X - C.X, FromWorld.Y - C.Y);
			}
			if (Away.SizeSquared() < 1.f)
			{
				Away = FVector2D(1.f, 0.f);
			}
			Away.Normalize();
			return FVector(C.X + Away.X * Apron, C.Y + Away.Y * Apron, TargetWorld.Z);
		}
	}
	return TargetWorld;
}

void AWLCampaign3DView::AddMovementEdge(const FString& A, const FString& B)
{
	if (!FindMovementNodeById(A) || !FindMovementNodeById(B))
	{
		return;
	}

	MovementAdjacency.FindOrAdd(A).AddUnique(B);
	MovementAdjacency.FindOrAdd(B).AddUnique(A);
}

const FWLCampaign3DMovementNodeView* AWLCampaign3DView::FindMovementNodeById(const FString& NodeId) const
{
	for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
	{
		if (Node.Id.Equals(NodeId, ESearchCase::IgnoreCase))
		{
			return &Node;
		}
	}
	return nullptr;
}

FString AWLCampaign3DView::FindNearestMovementNodeId(const FWLCampaign3DForceView& Force) const
{
	float BestDistanceSq = TNumericLimits<float>::Max();
	FString BestNodeId;
	for (const FWLCampaign3DMovementNodeView& Node : MovementNodes)
	{
		if (Force.bNaval && !Node.bPort)
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared2D(Force.WorldLocation, Node.WorldLocation);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestNodeId = Node.Id;
		}
	}
	return BestNodeId;
}

void AWLCampaign3DView::GetValidMovementDestinations(
	const FWLCampaign3DForceView& Force,
	TArray<FWLCampaign3DMovementNodeView>& OutDestinations) const
{
	OutDestinations.Reset();
	if (!Force.bMovable || Force.bAir)
	{
		return;
	}

	const FString OriginNodeId = Force.MovementNodeId.IsEmpty() ? FindNearestMovementNodeId(Force) : Force.MovementNodeId;
	if (OriginNodeId.IsEmpty())
	{
		return;
	}

	// TODOS los nodos alcanzables por carretera (BFS), no solo los adyacentes: el jugador ordena CUALQUIER
	// ciudad y el ejercito viaja por turnos segun su cargador. (Si es naval, solo puertos.)
	TSet<FString> Visited;
	Visited.Add(OriginNodeId);
	TArray<FString> Queue;
	Queue.Add(OriginNodeId);
	while (!Queue.IsEmpty())
	{
		const FString CurrentId = Queue.Pop(EAllowShrinking::No);
		const TArray<FString>* Neighbors = MovementAdjacency.Find(CurrentId);
		if (!Neighbors)
		{
			continue;
		}
		for (const FString& NeighborId : *Neighbors)
		{
			if (Visited.Contains(NeighborId))
			{
				continue;
			}
			Visited.Add(NeighborId);
			Queue.Add(NeighborId);
			const FWLCampaign3DMovementNodeView* Node = FindMovementNodeById(NeighborId);
			if (!Node)
			{
				continue;
			}
			if (Force.bNaval && !Node->bPort)
			{
				continue;
			}
			OutDestinations.Add(*Node);
		}
	}
}

void AWLCampaign3DView::RebuildMovementDestinationMarkers(const TArray<FWLCampaign3DMovementNodeView>& Destinations)
{
	DestroyMovementDestinationMarkers();
	ActiveMovementDestinations = Destinations;

	for (const FWLCampaign3DMovementNodeView& Node : ActiveMovementDestinations)
	{
		const bool bPreview = !PreviewMovementDestinationNodeId.IsEmpty()
			&& Node.Id.Equals(PreviewMovementDestinationNodeId, ESearchCase::IgnoreCase);
		const FVector MarkerLocation = Node.WorldLocation + FVector(0.f, 0.f, bPreview ? 3300.f : 2850.f);

		UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
		if (!Marker)
		{
			continue;
		}
		Marker->SetupAttachment(SceneRoot);
		Marker->RegisterComponent();
		Marker->SetStaticMesh(ConeMesh ? ConeMesh : CityMesh);
		Marker->SetWorldLocation(MarkerLocation);
		Marker->SetWorldScale3D(bPreview ? FVector(9.5f, 9.5f, 8.0f) : FVector(7.0f, 7.0f, 5.8f));
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (UMaterialInstanceDynamic* Mat = MakeColorMaterial(bPreview
			? FLinearColor(0.96f, 0.78f, 0.30f, 1.f)
			: FLinearColor(0.28f, 0.72f, 0.64f, 1.f)))
		{
			Marker->SetMaterial(0, Mat);
		}
		MovementDestinationComponents.Add(Marker);

		USphereComponent* SelectionProxy = NewObject<USphereComponent>(this);
		if (SelectionProxy)
		{
			SelectionProxy->SetupAttachment(SceneRoot);
			SelectionProxy->RegisterComponent();
			SelectionProxy->SetWorldLocation(MarkerLocation + FVector(0.f, 0.f, 620.f));
			SelectionProxy->SetSphereRadius(12800.f);
			SelectionProxy->SetVisibility(false, true);
			SelectionProxy->SetHiddenInGame(false, true);
			SelectionProxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			SelectionProxy->SetCollisionObjectType(ECC_WorldDynamic);
			SelectionProxy->SetCollisionResponseToAllChannels(ECR_Ignore);
			SelectionProxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			SelectionProxy->ComponentTags.Add(TEXT("WorldLeaderCampaign3DMovementDestination"));
			SelectionProxy->ComponentTags.Add(FName(*Node.Id));
			MovementDestinationSelectionMarkers.Add(SelectionProxy);
		}

		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		if (Label)
		{
			Label->SetupAttachment(SceneRoot);
			Label->RegisterComponent();
			Label->SetWorldLocation(MarkerLocation + FVector(0.f, 0.f, 1480.f));
			Label->SetWorldRotation(FRotator(90.f, 180.f, 0.f));
			Label->SetHorizontalAlignment(EHTA_Center);
			Label->SetWorldSize(bPreview ? 520.f : 430.f);
			Label->SetText(FText::FromString(Node.Name));
			Label->SetTextRenderColor(bPreview ? FColor(238, 214, 132) : FColor(170, 220, 210));
			Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MovementDestinationLabels.Add(Label);
		}
	}
}

void AWLCampaign3DView::RebuildMovementRoutePreview(const TArray<FWLCampaign3DMovementNodeView>& RouteNodes)
{
	if (!MovementRoutePreviewMesh)
	{
		return;
	}

	MovementRoutePreviewMesh->ClearAllMeshSections();
	const FLinearColor RouteColor(0.26f, 0.70f, 0.64f, 0.96f);
	for (int32 Index = 0; Index + 1 < RouteNodes.Num(); ++Index)
	{
		const FVector Start = RouteNodes[Index].WorldLocation + FVector(0.f, 0.f, 2250.f);
		const FVector End = RouteNodes[Index + 1].WorldLocation + FVector(0.f, 0.f, 2250.f);
		AddMovementRoutePreviewSegment(Start, End, RouteColor, Index);
	}
	MovementRoutePreviewMesh->SetVisibility(!IsHidden(), true);
	MovementRoutePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWLCampaign3DView::AddMovementRoutePreviewSegment(
	const FVector& Start,
	const FVector& End,
	const FLinearColor& Color,
	int32 SectionIndex)
{
	if (!MovementRoutePreviewMesh)
	{
		return;
	}

	const FVector Direction = End - Start;
	const FVector FlatDirection(Direction.X, Direction.Y, 0.f);
	if (FlatDirection.SizeSquared() < 1.f)
	{
		return;
	}

	const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal()) * 760.f;
	TArray<FVector> Verts = {
		Start - Side,
		Start + Side,
		End + Side,
		End - Side
	};
	TArray<int32> Tris = { 0, 1, 2, 0, 2, 3 };
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs = { FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(1.f, 1.f), FVector2D(1.f, 0.f) };
	TArray<FColor> Colors;
	Colors.Init(FWLCampaignVisualStyle::ToVertexFColor(Color), Verts.Num());
	TArray<FProcMeshTangent> Tangents;
	MovementRoutePreviewMesh->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors, Tangents, false);
	if (UMaterialInstanceDynamic* RouteMaterial = MakeColorMaterial(Color))
	{
		MovementRoutePreviewMesh->SetMaterial(SectionIndex, RouteMaterial);
	}
	else if (VertexColorMaterial)
	{
		MovementRoutePreviewMesh->SetMaterial(SectionIndex, VertexColorMaterial);
	}
}

void AWLCampaign3DView::DestroyMovementDestinationMarkers()
{
	for (UStaticMeshComponent* Marker : MovementDestinationComponents)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	MovementDestinationComponents.Reset();

	for (UPrimitiveComponent* Marker : MovementDestinationSelectionMarkers)
	{
		if (Marker)
		{
			Marker->DestroyComponent();
		}
	}
	MovementDestinationSelectionMarkers.Reset();

	for (UTextRenderComponent* Label : MovementDestinationLabels)
	{
		if (Label)
		{
			Label->DestroyComponent();
		}
	}
	MovementDestinationLabels.Reset();
}

FVector AWLCampaign3DView::GetForceMarkerLocationForNode(
	const FWLCampaign3DForceView& Force,
	const FWLCampaign3DMovementNodeView& Node) const
{
	// Un EJERCITO reclutado (token tanque) se APOYA en el terreno del nodo destino via raycast -> no flota
	// al moverlo. Las demas fuerzas conservan su altura de marcador sobre el nodo.
	if (!Force.bNaval && !Force.bAir && Force.Id.StartsWith(TEXT("ARMY-")))
	{
		return GroundedLandTokenLocation(Node.Lon, Node.Lat);
	}
	return Node.WorldLocation + FVector(0.f, 0.f, Force.bNaval ? 450.f : 550.f);
}

FVector AWLCampaign3DView::GetForceSelectionProxyLocation(
	const FWLCampaign3DForceView& Force,
	const FVector& MarkerLocation) const
{
	if (Force.bIsRecruitmentBase)
	{
		return MarkerLocation + FVector(0.f, 0.f, 820.f);
	}
	return MarkerLocation + FVector(0.f, 0.f, Force.bNaval ? 1560.f : (Force.bAir ? 1320.f : 1080.f));
}

FVector AWLCampaign3DView::GetForceLabelLocation(
	const FWLCampaign3DForceView& Force,
	const FVector& MarkerLocation) const
{
	return MarkerLocation + FVector(0.f, 0.f, Force.bNaval ? 4940.f : (Force.bAir ? 4180.f : 3420.f));
}

void AWLCampaign3DView::StartForceMovementAnimation(
	int32 ForceIndex,
	const TArray<FWLCampaign3DMovementNodeView>& RouteNodes,
	const FVector& StartLocation)
{
	if (!ForceViews.IsValidIndex(ForceIndex))
	{
		return;
	}

	FForceMovementAnimation Animation;
	Animation.ForceId = ForceViews[ForceIndex].Id;
	Animation.ForceIndex = ForceIndex;
	Animation.Points.Add(StartLocation);
	for (int32 Index = 1; Index < RouteNodes.Num(); ++Index)
	{
		const FVector Next = GetForceMarkerLocationForNode(ForceViews[ForceIndex], RouteNodes[Index]);
		if (FVector::DistSquared(Animation.Points.Last(), Next) > 100.f)
		{
			Animation.Points.Add(Next);
		}
	}
	if (Animation.Points.Num() < 2)
	{
		Animation.Points.Reset();
		Animation.Points.Add(StartLocation);
		Animation.Points.Add(ForceViews[ForceIndex].WorldLocation);
	}

	float TotalDistance = 0.f;
	for (int32 Index = 0; Index + 1 < Animation.Points.Num(); ++Index)
	{
		TotalDistance += FVector::Dist2D(Animation.Points[Index], Animation.Points[Index + 1]);
	}
	Animation.DurationSeconds = ComputeDriveDurationSeconds(TotalDistance);   // velocidad relativa al zoom (no "salta")

	for (int32 Index = ForceMovementAnimations.Num() - 1; Index >= 0; --Index)
	{
		if (ForceMovementAnimations[Index].ForceId.Equals(Animation.ForceId, ESearchCase::IgnoreCase))
		{
			ForceMovementAnimations.RemoveAtSwap(Index);
		}
	}
	ForceMovementAnimations.Add(MoveTemp(Animation));
}

float AWLCampaign3DView::GetArmyMovementBudgetKm(const FWLCampaign3DForceView& Force) const
{
	// La velocidad la marca la unidad MAS LENTA: si lleva infanteria (a pie) o artilleria (remolcada) va
	// lento; si es todo vehiculos/blindados, rapido. Asi un destino lejano tarda varios turnos [M].
	bool bHasSlow = false;
	for (const FWLForceUnitGroup& Unit : Force.Composition)
	{
		const FString L = Unit.Label.ToLower();
		if (L.Contains(TEXT("infant")) || L.Contains(TEXT("artill")))
		{
			bHasSlow = true;
			break;
		}
	}
	return bHasSlow ? 100.f : 160.f;   // km por turno (tramo corto = avance NATURAL por turno; cruza el pais en varios [M])
}

float AWLCampaign3DView::ComputeDriveDurationSeconds(float DistanceWorld) const
{
	// Velocidad RELATIVA AL ZOOM: a mayor altura de camara hacen falta mas u/s para que el icono recorra los
	// mismos pixeles, asi que la velocidad APARENTE en pantalla queda constante y lenta. Con velocidad fija en
	// unidades de mundo, de cerca se veia "muy rapido" (cubria mucha pantalla en poco tiempo). Factor calibrado
	// para que de cerca (~70-115k de altura) el tanque conduzca despacio.
	const float CamH = ViewCamera ? ViewCamera->GetActorLocation().Z : DefaultCameraLocation.Z;
	const float EffSpeed = FMath::Max(1500.f, CamH * 0.03f);   // u/s (sube con el zoom-out)
	return FMath::Clamp(DistanceWorld / EffSpeed, 1.5f, 9.f);
}

bool AWLCampaign3DView::AdvanceForceAlongPath(int32 Index)
{
	if (!ForceViews.IsValidIndex(Index))
	{
		return false;
	}
	FWLCampaign3DForceView& Force = ForceViews[Index];
	if (Force.MovePathNodeIds.Num() < 2)
	{
		return false;   // sin orden de marcha pendiente
	}

	float Budget = GetArmyMovementBudgetKm(Force);
	const FVector StartLocation = Force.WorldLocation;

	// Etapa recorrida ESTE turno (para animar): empieza en el nodo actual.
	TArray<FWLCampaign3DMovementNodeView> Leg;
	if (const FWLCampaign3DMovementNodeView* StartNode = FindMovementNodeById(Force.MovePathNodeIds[0]))
	{
		Leg.Add(*StartNode);
	}

	bool bFirst = true;
	while (Force.MovePathNodeIds.Num() >= 2)
	{
		const FWLCampaign3DMovementNodeView* A = FindMovementNodeById(Force.MovePathNodeIds[0]);
		const FWLCampaign3DMovementNodeView* B = FindMovementNodeById(Force.MovePathNodeIds[1]);
		if (!A || !B)
		{
			Force.MovePathNodeIds.Reset();
			break;
		}
		const float EdgeKm = FVector2D::Distance(FVector2D(A->Lon, A->Lat), FVector2D(B->Lon, B->Lat)) * 111.f;
		// Siempre avanza al menos UN nodo por turno (bFirst); luego solo mientras quede cargador.
		if (!bFirst && EdgeKm > Budget)
		{
			break;
		}
		Budget -= EdgeKm;
		bFirst = false;
		Force.MovePathNodeIds.RemoveAt(0);   // ya esta en B
		Force.MovementNodeId = B->Id;
		Force.Lon = B->Lon;
		Force.Lat = B->Lat;
		Force.LocationName = B->Name;
		Force.NearbyCity = B->Name;
		Force.ProvinceId = B->ProvinceId;
		Force.ProvinceName = B->ProvinceName;
		Leg.Add(*B);
	}

	if (Leg.Num() < 2)
	{
		return false;   // no se pudo avanzar
	}

	const FWLCampaign3DMovementNodeView& EndNode = Leg.Last();
	Force.WorldLocation = GetForceMarkerLocationForNode(Force, EndNode);

	const int32 NodesLeft = FMath::Max(0, Force.MovePathNodeIds.Num() - 1);
	if (NodesLeft <= 0)
	{
		Force.MovePathNodeIds.Reset();
		Force.MovementStatus = TEXT("en posicion");
		Force.OperationalState = TEXT("desplegado");
		Force.Posture = Force.bNaval ? TEXT("fondeado en puerto") : TEXT("desplegado");
	}
	else
	{
		Force.MovementStatus = FString::Printf(TEXT("en marcha (faltan %d)"), NodesLeft);
		Force.OperationalState = TEXT("en marcha");
		Force.Posture = TEXT("en marcha por carretera");
	}

	StartForceMovementAnimation(Index, Leg, StartLocation);
	return true;
}

void AWLCampaign3DView::AdvanceArmyMovements()
{
	// Una vez por turno [M]: cada ejercito en marcha avanza el equivalente a su cargador.
	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		FWLCampaign3DForceView& Force = ForceViews[Index];
		if (!Force.bMovable)
		{
			continue;
		}
		// Nuevo turno: rellena el alcance de movimiento (lo que pueda recorrer este mes).
		Force.MoveBudgetRemainingWorld = GetArmyMovementBudgetKm(Force) * DetailWorldUnitsPerKm;
		if (Force.MovePathPoints.Num() >= 2)
		{
			AdvanceForcePolyline(Index);       // conduce SIGUIENDO el trazado de la carretera (clic derecho)
		}
		else if (Force.bHasMoveTarget)
		{
			AdvanceForceTowardTarget(Index);   // marcha recta (fallback si no hay via comun)
		}
		else if (Force.MovePathNodeIds.Num() >= 2)
		{
			AdvanceForceAlongPath(Index);      // ruta por nodos-ciudad (boton Mover)
		}
	}
}

bool AWLCampaign3DView::AdvanceForceTowardTarget(int32 Index)
{
	if (!ForceViews.IsValidIndex(Index))
	{
		return false;
	}
	FWLCampaign3DForceView& Force = ForceViews[Index];
	if (!Force.bHasMoveTarget)
	{
		return false;
	}

	const FVector OldPos = Force.WorldLocation;
	const float FullBudget = FMath::Max(1.f, GetArmyMovementBudgetKm(Force) * DetailWorldUnitsPerKm);
	// Alcance que QUEDA este turno (-1 = aun no usado este turno -> lleno). Asi spamear clics no avanza mas
	// de lo permitido por turno: cada paso descuenta del restante; al avanzar el mes se rellena.
	float BudgetWorld = (Force.MoveBudgetRemainingWorld < 0.f) ? FullBudget : Force.MoveBudgetRemainingWorld;
	if (BudgetWorld < 1.f)
	{
		return false;   // sin alcance restante este turno; continuara al avanzar el mes [M]
	}
	FVector ToTarget = Force.MoveTargetWorld - OldPos;
	ToTarget.Z = 0.f;
	const float Dist = ToTarget.Size();

	FVector2D NewXY;
	float Moved;
	if (Dist <= BudgetWorld || Dist < 1.f)
	{
		// Llega EXACTO al punto clicado (a mitad de carretera) este turno.
		NewXY = FVector2D(Force.MoveTargetWorld.X, Force.MoveTargetWorld.Y);
		Moved = Dist;
		Force.bHasMoveTarget = false;
		Force.MovementStatus = TEXT("en posicion");
		Force.OperationalState = TEXT("desplegado");
		Force.Posture = Force.bNaval ? TEXT("fondeado") : TEXT("desplegado");
	}
	else
	{
		const FVector Dir = ToTarget / Dist;
		const FVector Step = OldPos + Dir * BudgetWorld;
		NewXY = FVector2D(Step.X, Step.Y);
		Moved = BudgetWorld;
		Force.MovementStatus = TEXT("en marcha por carretera");
		Force.OperationalState = TEXT("en marcha");
		Force.Posture = TEXT("en marcha por carretera");
	}
	Force.MoveBudgetRemainingWorld = FMath::Max(0.f, BudgetWorld - Moved);

	const FVector NewPos = GroundedLandTokenLocationAtWorld(NewXY.X, NewXY.Y);
	Force.WorldLocation = NewPos;
	Force.MovementNodeId = FindNearestMovementNodeId(Force);   // nodo de referencia mas cercano

	// Animacion: desliza el token de OldPos a NewPos por la carretera (reusa FForceMovementAnimation).
	FForceMovementAnimation Anim;
	Anim.ForceId = Force.Id;
	Anim.ForceIndex = Index;
	Anim.Points.Add(OldPos);
	Anim.Points.Add(NewPos);
	Anim.ElapsedSeconds = 0.f;
	Anim.DurationSeconds = ComputeDriveDurationSeconds(FVector::Dist2D(OldPos, NewPos));   // velocidad relativa al zoom
	UE_LOG(LogWorldLeader, Log, TEXT("[ArmyMove] RECTO dist=%.0f dur=%.1fs camH=%.0f"), FVector::Dist2D(OldPos, NewPos), Anim.DurationSeconds, ViewCamera ? ViewCamera->GetActorLocation().Z : DefaultCameraLocation.Z);
	for (int32 i = ForceMovementAnimations.Num() - 1; i >= 0; --i)
	{
		if (ForceMovementAnimations[i].ForceId.Equals(Force.Id, ESearchCase::IgnoreCase))
		{
			ForceMovementAnimations.RemoveAtSwap(i);
		}
	}
	ForceMovementAnimations.Add(MoveTemp(Anim));
	return true;
}

bool AWLCampaign3DView::AdvanceForcePolyline(int32 Index)
{
	if (!ForceViews.IsValidIndex(Index))
	{
		return false;
	}
	FWLCampaign3DForceView& Force = ForceViews[Index];
	if (Force.MovePathPoints.Num() < 2)
	{
		return false;
	}

	const float FullBudget = FMath::Max(1.f, GetArmyMovementBudgetKm(Force) * DetailWorldUnitsPerKm);
	float Remaining = (Force.MoveBudgetRemainingWorld < 0.f) ? FullBudget : Force.MoveBudgetRemainingWorld;
	if (Remaining < 1.f)
	{
		return false;   // sin alcance este turno; continuara al avanzar el mes
	}

	const FVector OldPos = Force.WorldLocation;
	TArray<FVector> Traversed;
	Traversed.Add(OldPos);

	// MovePathPoints[0] = posicion actual; [1..] = trazado de la via hasta el destino. Avanza consumiendo
	// segmentos por el cargador; al quedarse sin alcance se detiene a mitad de via (continua el proximo mes).
	while (Force.MovePathPoints.Num() >= 2 && Remaining > 1.f)
	{
		const FVector A = Force.MovePathPoints[0];
		const FVector B = Force.MovePathPoints[1];
		const float Seg = FVector::Dist2D(A, B);
		if (Seg <= Remaining + 1.f)
		{
			Remaining -= Seg;
			Force.MovePathPoints.RemoveAt(0);
			const FVector Bg = GroundedLandTokenLocationAtWorld(B.X, B.Y);
			Force.MovePathPoints[0] = Bg;
			Traversed.Add(Bg);
		}
		else
		{
			const float T = Remaining / FMath::Max(1.f, Seg);
			const FVector P = FMath::Lerp(A, B, T);
			const FVector Pg = GroundedLandTokenLocationAtWorld(P.X, P.Y);
			Force.MovePathPoints[0] = Pg;
			Traversed.Add(Pg);
			Remaining = 0.f;
		}
	}

	Force.WorldLocation = Traversed.Last();
	Force.MoveBudgetRemainingWorld = FMath::Max(0.f, Remaining);
	if (Force.MovePathPoints.Num() < 2)
	{
		Force.MovePathPoints.Reset();
		Force.MovementStatus = TEXT("en posicion");
		Force.OperationalState = TEXT("desplegado");
		Force.Posture = Force.bNaval ? TEXT("fondeado") : TEXT("desplegado");
	}
	else
	{
		Force.MovementStatus = TEXT("en marcha por carretera");
		Force.OperationalState = TEXT("en marcha");
		Force.Posture = TEXT("en marcha por carretera");
	}
	Force.MovementNodeId = FindNearestMovementNodeId(Force);

	if (Traversed.Num() >= 2)
	{
		FForceMovementAnimation Anim;
		Anim.ForceId = Force.Id;
		Anim.ForceIndex = Index;
		Anim.Points = Traversed;
		Anim.ElapsedSeconds = 0.f;
		float TotalDist = 0.f;
		for (int32 i = 0; i + 1 < Traversed.Num(); ++i)
		{
			TotalDist += FVector::Dist2D(Traversed[i], Traversed[i + 1]);
		}
		Anim.DurationSeconds = ComputeDriveDurationSeconds(TotalDist);   // velocidad relativa al zoom
		UE_LOG(LogWorldLeader, Log, TEXT("[ArmyMove] CARRETERA dist=%.0f dur=%.1fs pts=%d camH=%.0f"), TotalDist, Anim.DurationSeconds, Traversed.Num(), ViewCamera ? ViewCamera->GetActorLocation().Z : DefaultCameraLocation.Z);
		for (int32 i = ForceMovementAnimations.Num() - 1; i >= 0; --i)
		{
			if (ForceMovementAnimations[i].ForceId.Equals(Force.Id, ESearchCase::IgnoreCase))
			{
				ForceMovementAnimations.RemoveAtSwap(i);
			}
		}
		ForceMovementAnimations.Add(MoveTemp(Anim));
	}
	return true;
}

bool AWLCampaign3DView::SetForceFreeMoveTarget(const FString& ForceId, const FVector& WorldClick, FWLCampaign3DForceView& OutForce)
{
	for (int32 Index = 0; Index < ForceViews.Num(); ++Index)
	{
		FWLCampaign3DForceView& Force = ForceViews[Index];
		if (!Force.Id.Equals(ForceId, ESearchCase::IgnoreCase) || !Force.bMovable)
		{
			continue;
		}
		Force.MovePathNodeIds.Reset();   // anula ruta por nodos previa (boton Mover)
		// Objetivo = punto clicado aterrizado, PERO apartado del interior de una ciudad (el ejercito para en el
		// borde, no montado sobre los edificios).
		FVector Target = GroundedLandTokenLocationAtWorld(WorldClick.X, WorldClick.Y);
		Target = ClampTargetOffCities(Target, Force.WorldLocation);
		Target = GroundedLandTokenLocationAtWorld(Target.X, Target.Y);
		// Intenta SEGUIR EL TRAZADO de la carretera (curva a curva) hasta el objetivo. Si no hay una via comun
		// cercana, cae a marcha recta. En ambos casos avanza ya su alcance este turno.
		TArray<FVector> RoadPath;
		if (ComputeRoadPath(Force.WorldLocation, Target, RoadPath) && RoadPath.Num() >= 2)
		{
			Force.MovePathPoints = MoveTemp(RoadPath);
			Force.bHasMoveTarget = false;
			AdvanceForcePolyline(Index);
		}
		else
		{
			Force.MovePathPoints.Reset();
			Force.MoveTargetWorld = Target;
			Force.bHasMoveTarget = true;
			AdvanceForceTowardTarget(Index);
		}
		OutForce = Force;
		if (SelectedForceHighlightId.Equals(Force.Id, ESearchCase::IgnoreCase))
		{
			SetSelectedForceHighlight(Force.Id);
		}
		return true;
	}
	return false;
}

void AWLCampaign3DView::UpdateForceMovementAnimations(float DeltaSeconds)
{
	for (int32 AnimIndex = ForceMovementAnimations.Num() - 1; AnimIndex >= 0; --AnimIndex)
	{
		FForceMovementAnimation& Animation = ForceMovementAnimations[AnimIndex];
		if (!ForceViews.IsValidIndex(Animation.ForceIndex) || Animation.Points.Num() < 2 || Animation.DurationSeconds <= 0.f)
		{
			ForceMovementAnimations.RemoveAtSwap(AnimIndex);
			continue;
		}

		FWLCampaign3DForceView& Force = ForceViews[Animation.ForceIndex];
		if (!Force.Id.Equals(Animation.ForceId, ESearchCase::IgnoreCase))
		{
			ForceMovementAnimations.RemoveAtSwap(AnimIndex);
			continue;
		}

		Animation.ElapsedSeconds += FMath::Max(0.f, DeltaSeconds);
		const float LinearAlpha = FMath::Clamp(Animation.ElapsedSeconds / Animation.DurationSeconds, 0.f, 1.f);
		const float SmoothAlpha = LinearAlpha * LinearAlpha * (3.f - 2.f * LinearAlpha);

		float TotalDistance = 0.f;
		for (int32 Index = 0; Index + 1 < Animation.Points.Num(); ++Index)
		{
			TotalDistance += FVector::Dist2D(Animation.Points[Index], Animation.Points[Index + 1]);
		}

		FVector Current = Animation.Points.Last();
		FVector Direction = Animation.Points.Last() - Animation.Points[Animation.Points.Num() - 2];
		float TargetDistance = TotalDistance * SmoothAlpha;
		for (int32 Index = 0; Index + 1 < Animation.Points.Num(); ++Index)
		{
			const FVector A = Animation.Points[Index];
			const FVector B = Animation.Points[Index + 1];
			const float SegmentDistance = FVector::Dist2D(A, B);
			if (SegmentDistance <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			if (TargetDistance <= SegmentDistance || Index + 2 == Animation.Points.Num())
			{
				const float SegmentAlpha = FMath::Clamp(TargetDistance / SegmentDistance, 0.f, 1.f);
				Current = FMath::Lerp(A, B, SegmentAlpha);
				Direction = B - A;
				break;
			}
			TargetDistance -= SegmentDistance;
		}

		if (UStaticMeshComponent* Marker = ForceMarkerComponents.IsValidIndex(Animation.ForceIndex) ? ForceMarkerComponents[Animation.ForceIndex] : nullptr)
		{
			Marker->SetWorldLocation(Current);
			const FVector FlatDirection(Direction.X, Direction.Y, 0.f);
			if (FlatDirection.SizeSquared() > 1.f)
			{
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(FlatDirection.Y, FlatDirection.X));
				// Gira SUAVE hacia la direccion de marcha (maniobra de vehiculo) en vez de encarar de golpe:
				// el tanque "conduce" girando hacia donde va mientras avanza.
				const FRotator SmoothRot = FMath::RInterpTo(Marker->GetComponentRotation(), FRotator(0.f, Yaw, 0.f), DeltaSeconds, 7.f);
				Marker->SetWorldRotation(SmoothRot);
			}
		}
		if (UPrimitiveComponent* Proxy = ForceSelectionMarkers.IsValidIndex(Animation.ForceIndex) ? ForceSelectionMarkers[Animation.ForceIndex] : nullptr)
		{
			Proxy->SetWorldLocation(GetForceSelectionProxyLocation(Force, Current));
		}
		if (UTextRenderComponent* Label = ForceMarkerLabels.IsValidIndex(Animation.ForceIndex) ? ForceMarkerLabels[Animation.ForceIndex] : nullptr)
		{
			Label->SetWorldLocation(GetForceLabelLocation(Force, Current));
			Label->SetText(FText::FromString(Force.Name));
		}

		if (LinearAlpha >= 1.f)
		{
			const FVector FinalLocation = Force.WorldLocation;
			if (UStaticMeshComponent* Marker = ForceMarkerComponents.IsValidIndex(Animation.ForceIndex) ? ForceMarkerComponents[Animation.ForceIndex] : nullptr)
			{
				Marker->SetWorldLocation(FinalLocation);
			}
			if (UPrimitiveComponent* Proxy = ForceSelectionMarkers.IsValidIndex(Animation.ForceIndex) ? ForceSelectionMarkers[Animation.ForceIndex] : nullptr)
			{
				Proxy->SetWorldLocation(GetForceSelectionProxyLocation(Force, FinalLocation));
			}
			if (UTextRenderComponent* Label = ForceMarkerLabels.IsValidIndex(Animation.ForceIndex) ? ForceMarkerLabels[Animation.ForceIndex] : nullptr)
			{
				Label->SetWorldLocation(GetForceLabelLocation(Force, FinalLocation));
			}
			ForceMovementAnimations.RemoveAtSwap(AnimIndex);
		}
	}
}
