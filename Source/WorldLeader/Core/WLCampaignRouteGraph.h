// Copyright World Leader project. See ROADMAP.md.

#pragma once

#include "CoreMinimal.h"

/**
 * Backend puro para rutas de campania.
 *
 * Mantiene el calculo de alcanzables/pathfinding fuera del actor visual
 * Campaign3D, para que las carreteras funcionen como grafo testeable.
 */
class WORLDLEADER_API FWLCampaignRouteGraph
{
public:
	static void AddUndirectedEdge(
		TMap<FString, TArray<FString>>& Graph,
		const FString& NodeA,
		const FString& NodeB);

	static bool FindShortestPath(
		const TMap<FString, TArray<FString>>& Graph,
		const FString& OriginNodeId,
		const FString& DestinationNodeId,
		TFunctionRef<bool(const FString&)> IsNodeAllowed,
		TArray<FString>& OutPathNodeIds);

	static void FindReachableNodes(
		const TMap<FString, TArray<FString>>& Graph,
		const FString& OriginNodeId,
		TFunctionRef<bool(const FString&)> IsNodeAllowed,
		TArray<FString>& OutReachableNodeIds);

	static int32 CountConnectedComponents(
		const TMap<FString, TArray<FString>>& Graph,
		TFunctionRef<bool(const FString&)> IsNodeIncluded);

private:
	static FString NormalizeNodeId(const FString& NodeId);
};
