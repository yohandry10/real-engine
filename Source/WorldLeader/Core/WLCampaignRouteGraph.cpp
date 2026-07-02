// Copyright World Leader project. See ROADMAP.md.

#include "Core/WLCampaignRouteGraph.h"

FString FWLCampaignRouteGraph::NormalizeNodeId(const FString& NodeId)
{
	return NodeId.TrimStartAndEnd();
}

void FWLCampaignRouteGraph::AddUndirectedEdge(
	TMap<FString, TArray<FString>>& Graph,
	const FString& NodeA,
	const FString& NodeB)
{
	const FString A = NormalizeNodeId(NodeA);
	const FString B = NormalizeNodeId(NodeB);
	if (A.IsEmpty() || B.IsEmpty() || A.Equals(B, ESearchCase::IgnoreCase))
	{
		return;
	}

	Graph.FindOrAdd(A).AddUnique(B);
	Graph.FindOrAdd(B).AddUnique(A);
}

bool FWLCampaignRouteGraph::FindShortestPath(
	const TMap<FString, TArray<FString>>& Graph,
	const FString& OriginNodeId,
	const FString& DestinationNodeId,
	TFunctionRef<bool(const FString&)> IsNodeAllowed,
	TArray<FString>& OutPathNodeIds)
{
	OutPathNodeIds.Reset();

	const FString Origin = NormalizeNodeId(OriginNodeId);
	const FString Destination = NormalizeNodeId(DestinationNodeId);
	if (Origin.IsEmpty() || Destination.IsEmpty() || Origin.Equals(Destination, ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (!IsNodeAllowed(Origin) || !IsNodeAllowed(Destination))
	{
		return false;
	}

	TArray<FString> Queue;
	TMap<FString, FString> CameFrom;
	Queue.Add(Origin);
	CameFrom.Add(Origin, TEXT(""));

	FString ReachedDestination;
	for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
	{
		const FString Current = Queue[Cursor];
		if (Current.Equals(Destination, ESearchCase::IgnoreCase))
		{
			ReachedDestination = Current;
			break;
		}

		const TArray<FString>* Neighbors = Graph.Find(Current);
		if (!Neighbors)
		{
			continue;
		}

		for (const FString& NeighborIdRaw : *Neighbors)
		{
			const FString NeighborId = NormalizeNodeId(NeighborIdRaw);
			if (NeighborId.IsEmpty() || CameFrom.Contains(NeighborId) || !IsNodeAllowed(NeighborId))
			{
				continue;
			}

			CameFrom.Add(NeighborId, Current);
			Queue.Add(NeighborId);
		}
	}

	if (ReachedDestination.IsEmpty())
	{
		return false;
	}

	TArray<FString> ReversePath;
	FString Current = ReachedDestination;
	while (!Current.IsEmpty())
	{
		ReversePath.Add(Current);
		const FString* Previous = CameFrom.Find(Current);
		Current = Previous ? *Previous : FString();
	}

	for (int32 Index = ReversePath.Num() - 1; Index >= 0; --Index)
	{
		OutPathNodeIds.Add(ReversePath[Index]);
	}

	return OutPathNodeIds.Num() >= 2;
}

void FWLCampaignRouteGraph::FindReachableNodes(
	const TMap<FString, TArray<FString>>& Graph,
	const FString& OriginNodeId,
	TFunctionRef<bool(const FString&)> IsNodeAllowed,
	TArray<FString>& OutReachableNodeIds)
{
	OutReachableNodeIds.Reset();

	const FString Origin = NormalizeNodeId(OriginNodeId);
	if (Origin.IsEmpty() || !IsNodeAllowed(Origin))
	{
		return;
	}

	TSet<FString> Visited;
	TArray<FString> Queue;
	Visited.Add(Origin);
	Queue.Add(Origin);

	for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
	{
		const FString Current = Queue[Cursor];
		const TArray<FString>* Neighbors = Graph.Find(Current);
		if (!Neighbors)
		{
			continue;
		}

		for (const FString& NeighborIdRaw : *Neighbors)
		{
			const FString NeighborId = NormalizeNodeId(NeighborIdRaw);
			if (NeighborId.IsEmpty() || Visited.Contains(NeighborId) || !IsNodeAllowed(NeighborId))
			{
				continue;
			}

			Visited.Add(NeighborId);
			Queue.Add(NeighborId);
			OutReachableNodeIds.Add(NeighborId);
		}
	}
}

int32 FWLCampaignRouteGraph::CountConnectedComponents(
	const TMap<FString, TArray<FString>>& Graph,
	TFunctionRef<bool(const FString&)> IsNodeIncluded)
{
	TSet<FString> AllNodes;
	for (const TPair<FString, TArray<FString>>& Pair : Graph)
	{
		if (IsNodeIncluded(Pair.Key))
		{
			AllNodes.Add(Pair.Key);
		}
		for (const FString& NeighborRaw : Pair.Value)
		{
			const FString Neighbor = NormalizeNodeId(NeighborRaw);
			if (!Neighbor.IsEmpty() && IsNodeIncluded(Neighbor))
			{
				AllNodes.Add(Neighbor);
			}
		}
	}

	TSet<FString> Visited;
	int32 ComponentCount = 0;
	for (const FString& NodeId : AllNodes)
	{
		if (Visited.Contains(NodeId))
		{
			continue;
		}

		++ComponentCount;
		TArray<FString> Stack;
		Stack.Add(NodeId);
		Visited.Add(NodeId);
		while (!Stack.IsEmpty())
		{
			const FString Current = Stack.Pop(EAllowShrinking::No);
			const TArray<FString>* Neighbors = Graph.Find(Current);
			if (!Neighbors)
			{
				continue;
			}

			for (const FString& NeighborRaw : *Neighbors)
			{
				const FString Neighbor = NormalizeNodeId(NeighborRaw);
				if (Neighbor.IsEmpty() || !AllNodes.Contains(Neighbor) || Visited.Contains(Neighbor))
				{
					continue;
				}

				Visited.Add(Neighbor);
				Stack.Add(Neighbor);
			}
		}
	}

	return ComponentCount;
}
