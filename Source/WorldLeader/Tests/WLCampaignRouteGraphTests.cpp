// Copyright World Leader project. See ROADMAP.md.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/WLCampaignRouteGraph.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCampaignRouteGraphShortestPathTest,
	"WorldLeader.Campaign.RouteGraph.ShortestPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCampaignRouteGraphShortestPathTest::RunTest(const FString& Parameters)
{
	TMap<FString, TArray<FString>> Graph;
	FWLCampaignRouteGraph::AddUndirectedEdge(Graph, TEXT("A"), TEXT("B"));
	FWLCampaignRouteGraph::AddUndirectedEdge(Graph, TEXT("B"), TEXT("C"));
	FWLCampaignRouteGraph::AddUndirectedEdge(Graph, TEXT("C"), TEXT("D"));
	FWLCampaignRouteGraph::AddUndirectedEdge(Graph, TEXT("A"), TEXT("D"));

	TArray<FString> Path;
	const auto AllowAll = [](const FString&) { return true; };
	TestTrue(TEXT("Ruta directa encontrada"),
		FWLCampaignRouteGraph::FindShortestPath(Graph, TEXT("A"), TEXT("D"), AllowAll, Path));
	TestEqual(TEXT("Ruta usa el enlace mas corto"), Path.Num(), 2);
	if (Path.Num() == 2)
	{
		TestEqual(TEXT("Origen"), Path[0], FString(TEXT("A")));
		TestEqual(TEXT("Destino"), Path[1], FString(TEXT("D")));
	}

	TestFalse(TEXT("Sin ruta a nodo inexistente"),
		FWLCampaignRouteGraph::FindShortestPath(Graph, TEXT("A"), TEXT("Z"), AllowAll, Path));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWLCampaignRouteGraphFilterTest,
	"WorldLeader.Campaign.RouteGraph.Filters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWLCampaignRouteGraphFilterTest::RunTest(const FString& Parameters)
{
	TMap<FString, TArray<FString>> Graph;
	FWLCampaignRouteGraph::AddUndirectedEdge(Graph, TEXT("PORT-A"), TEXT("CITY-B"));
	FWLCampaignRouteGraph::AddUndirectedEdge(Graph, TEXT("CITY-B"), TEXT("PORT-C"));
	FWLCampaignRouteGraph::AddUndirectedEdge(Graph, TEXT("PORT-A"), TEXT("PORT-D"));

	const TSet<FString> Ports = { TEXT("PORT-A"), TEXT("PORT-C"), TEXT("PORT-D") };
	const auto AllowPortsOnly = [&Ports](const FString& NodeId)
	{
		return Ports.Contains(NodeId);
	};

	TArray<FString> Reachable;
	FWLCampaignRouteGraph::FindReachableNodes(Graph, TEXT("PORT-A"), AllowPortsOnly, Reachable);
	TestEqual(TEXT("Solo alcanza el puerto conectado sin atravesar ciudad"), Reachable.Num(), 1);
	if (Reachable.Num() == 1)
	{
		TestEqual(TEXT("Puerto alcanzable"), Reachable[0], FString(TEXT("PORT-D")));
	}

	TArray<FString> Path;
	TestFalse(TEXT("No atraviesa nodo bloqueado por filtro"),
		FWLCampaignRouteGraph::FindShortestPath(Graph, TEXT("PORT-A"), TEXT("PORT-C"), AllowPortsOnly, Path));
	TestTrue(TEXT("Ruta portuaria directa valida"),
		FWLCampaignRouteGraph::FindShortestPath(Graph, TEXT("PORT-A"), TEXT("PORT-D"), AllowPortsOnly, Path));

	const int32 Components = FWLCampaignRouteGraph::CountConnectedComponents(Graph, AllowPortsOnly);
	TestEqual(TEXT("Componentes filtrados"), Components, 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
