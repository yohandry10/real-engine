void AppendSouthAmericaCorridor(
	TArray<FWLCampaignRouteSpec>& Network,
	const TCHAR* Name,
	EWLCampaignRouteType Type,
	const TArray<FVector2D>& Points,
	int32 Smoothness)
{
	FWLCampaignRouteSpec Spec;
	Spec.Name = Name;
	Spec.Type = Type;
	Spec.Points = Points;
	Spec.Smoothness = Smoothness;
	Spec.bShowJunctions = false;
	Network.Add(MoveTemp(Spec));
}

void AppendSouthAmericaRoadCorridors(TArray<FWLCampaignRouteSpec>& Network)
{
	AppendSouthAmericaCorridor(Network, TEXT("SA Panamericana Andes Pacific"), EWLCampaignRouteType::Primary, {
		FVector2D(-77.64f, 0.83f), FVector2D(-77.72f, 0.81f), FVector2D(-78.12f, 0.35f),
		FVector2D(-78.50f, -0.18f), FVector2D(-78.62f, -1.24f), FVector2D(-79.00f, -2.90f),
		FVector2D(-79.20f, -3.99f), FVector2D(-80.23f, -3.48f), FVector2D(-80.45f, -3.57f),
		FVector2D(-80.63f, -5.19f), FVector2D(-79.84f, -6.77f), FVector2D(-79.03f, -8.11f),
		FVector2D(-77.04f, -12.05f), FVector2D(-75.73f, -14.07f), FVector2D(-71.54f, -16.40f),
		FVector2D(-70.25f, -18.01f), FVector2D(-70.31f, -18.48f), FVector2D(-70.13f, -20.22f),
		FVector2D(-70.40f, -23.65f), FVector2D(-70.33f, -27.37f), FVector2D(-71.25f, -29.90f),
		FVector2D(-70.66f, -33.45f), FVector2D(-71.61f, -33.05f)
	}, 7);

	AppendSouthAmericaCorridor(Network, TEXT("SA Colombia Panamericana feeder"), EWLCampaignRouteType::Primary, {
		FVector2D(-74.08f, 4.61f), FVector2D(-75.56f, 6.25f), FVector2D(-76.52f, 3.45f),
		FVector2D(-76.61f, 2.44f), FVector2D(-77.28f, 1.21f), FVector2D(-77.64f, 0.83f)
	}, 7);

	AppendSouthAmericaCorridor(Network, TEXT("SA Cristo Redentor Buenos Aires"), EWLCampaignRouteType::Primary, {
		FVector2D(-71.61f, -33.05f), FVector2D(-70.66f, -33.45f), FVector2D(-68.85f, -32.89f),
		FVector2D(-66.34f, -33.30f), FVector2D(-64.35f, -33.13f), FVector2D(-64.18f, -31.42f),
		FVector2D(-60.64f, -32.95f), FVector2D(-58.38f, -34.60f)
	}, 7);

	AppendSouthAmericaCorridor(Network, TEXT("SA Chile Ruta 5 south"), EWLCampaignRouteType::Primary, {
		FVector2D(-70.66f, -33.45f), FVector2D(-73.05f, -36.83f), FVector2D(-72.59f, -38.74f),
		FVector2D(-73.25f, -39.81f), FVector2D(-72.94f, -41.47f), FVector2D(-72.07f, -45.57f),
		FVector2D(-72.51f, -51.73f), FVector2D(-70.92f, -53.16f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Argentina Patagonia trunk"), EWLCampaignRouteType::Secondary, {
		FVector2D(-58.38f, -34.60f), FVector2D(-57.55f, -38.00f), FVector2D(-62.27f, -38.72f),
		FVector2D(-68.06f, -38.95f), FVector2D(-71.31f, -41.13f), FVector2D(-67.50f, -45.86f),
		FVector2D(-69.22f, -51.62f), FVector2D(-68.30f, -54.80f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Bolivia central spine"), EWLCampaignRouteType::Primary, {
		FVector2D(-69.04f, -16.56f), FVector2D(-68.15f, -16.50f), FVector2D(-67.11f, -17.97f),
		FVector2D(-66.16f, -17.39f), FVector2D(-63.18f, -17.78f), FVector2D(-57.80f, -18.98f),
		FVector2D(-57.65f, -19.01f), FVector2D(-54.62f, -20.45f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Bolivia Argentina north"), EWLCampaignRouteType::Primary, {
		FVector2D(-68.15f, -16.50f), FVector2D(-67.11f, -17.97f), FVector2D(-65.75f, -19.58f),
		FVector2D(-65.59f, -22.09f), FVector2D(-65.59f, -22.10f), FVector2D(-65.41f, -24.79f),
		FVector2D(-65.22f, -26.82f), FVector2D(-64.18f, -31.42f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Brazil Atlantic Mercosur"), EWLCampaignRouteType::Primary, {
		FVector2D(-40.50f, -12.97f), FVector2D(-40.34f, -20.32f), FVector2D(-43.17f, -22.91f),
		FVector2D(-46.63f, -23.55f), FVector2D(-49.27f, -25.43f), FVector2D(-48.55f, -27.59f),
		FVector2D(-51.23f, -30.03f), FVector2D(-57.09f, -29.75f), FVector2D(-57.96f, -31.38f),
		FVector2D(-58.30f, -33.12f), FVector2D(-56.16f, -34.90f), FVector2D(-57.84f, -34.47f),
		FVector2D(-58.38f, -34.60f)
	}, 7);

	AppendSouthAmericaCorridor(Network, TEXT("SA Brazil Central Interior"), EWLCampaignRouteType::Primary, {
		FVector2D(-48.00f, -15.79f), FVector2D(-49.25f, -16.68f), FVector2D(-56.10f, -15.60f),
		FVector2D(-54.62f, -20.45f), FVector2D(-54.59f, -25.54f), FVector2D(-54.61f, -25.51f),
		FVector2D(-57.63f, -25.27f), FVector2D(-57.43f, -23.41f), FVector2D(-60.03f, -22.35f),
		FVector2D(-63.65f, -22.03f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Brazil Amazon BR364 BR163"), EWLCampaignRouteType::Secondary, {
		FVector2D(-60.02f, -3.10f), FVector2D(-63.90f, -8.76f), FVector2D(-67.81f, -9.97f),
		FVector2D(-68.75f, -11.01f), FVector2D(-68.77f, -11.03f), FVector2D(-63.90f, -8.76f),
		FVector2D(-56.10f, -15.60f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Brazil North Coast"), EWLCampaignRouteType::Secondary, {
		FVector2D(-48.50f, -1.45f), FVector2D(-44.30f, -2.53f), FVector2D(-42.80f, -5.09f),
		FVector2D(-38.54f, -3.73f), FVector2D(-35.21f, -5.79f), FVector2D(-34.88f, -8.05f),
		FVector2D(-38.50f, -12.97f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Guianas Shield corridor"), EWLCampaignRouteType::Secondary, {
		FVector2D(-51.06f, 0.03f), FVector2D(-51.83f, 3.84f), FVector2D(-51.80f, 3.89f),
		FVector2D(-52.33f, 4.94f), FVector2D(-54.03f, 5.50f), FVector2D(-54.06f, 5.50f),
		FVector2D(-55.20f, 5.85f), FVector2D(-56.99f, 5.95f), FVector2D(-57.15f, 5.88f),
		FVector2D(-58.15f, 6.80f), FVector2D(-58.30f, 6.00f), FVector2D(-59.80f, 3.38f),
		FVector2D(-60.67f, 2.82f), FVector2D(-61.15f, 4.48f), FVector2D(-61.11f, 4.60f),
		FVector2D(-61.50f, 7.30f), FVector2D(-62.60f, 8.30f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Paraguay Uruguay frontier mesh"), EWLCampaignRouteType::Secondary, {
		FVector2D(-57.63f, -25.27f), FVector2D(-57.43f, -23.41f), FVector2D(-55.75f, -22.55f),
		FVector2D(-54.31f, -24.06f), FVector2D(-54.61f, -25.51f), FVector2D(-55.90f, -27.37f),
		FVector2D(-57.86f, -27.33f), FVector2D(-57.63f, -25.27f), FVector2D(-60.03f, -22.35f)
	}, 6);

	AppendSouthAmericaCorridor(Network, TEXT("SA Uruguay national loop"), EWLCampaignRouteType::Secondary, {
		FVector2D(-56.16f, -34.90f), FVector2D(-57.84f, -34.47f), FVector2D(-58.30f, -33.12f),
		FVector2D(-57.96f, -31.38f), FVector2D(-56.47f, -30.40f), FVector2D(-55.55f, -30.90f),
		FVector2D(-54.18f, -32.37f), FVector2D(-53.46f, -33.69f), FVector2D(-54.95f, -34.95f),
		FVector2D(-56.16f, -34.90f)
	}, 6);
}
