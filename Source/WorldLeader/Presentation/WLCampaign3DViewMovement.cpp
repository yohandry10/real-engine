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
		const FVector2D LonLat = FVector2D(
			TheaterCenterLonLat.X + City.WorldLocation.Y / GeoScale,
			TheaterCenterLonLat.Y + City.WorldLocation.X / GeoScale);
		Node.Lon = LonLat.X;
		Node.Lat = LonLat.Y;
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
	const TArray<FString>* Neighbors = MovementAdjacency.Find(OriginNodeId);
	if (!Neighbors)
	{
		return;
	}

	for (const FString& NeighborId : *Neighbors)
	{
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
	return Node.WorldLocation + FVector(0.f, 0.f, Force.bNaval ? 450.f : 550.f);
}
