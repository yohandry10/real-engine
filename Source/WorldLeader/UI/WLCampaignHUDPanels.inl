void DrawCampaignSelectionPanel(
		AWLCampaignHUD* HUD,
		UCanvas* Canvas,
		UFont* Font,
		UFont* SmallFont,
		const AWLCampaignPlayerController* PC,
		const UWLDataRegistry* Registry,
		const UWLStrategicTickSubsystem* Tick)
	{
		FWLCampaignSelectionPanelEntry Entry;
		bool bCityMode = false;
		if (!BuildSelectionPanelEntry(PC, Registry, Tick, Entry, bCityMode))
		{
			return;
		}

		const float W = Canvas->ClipX;
		const float H = Canvas->ClipY;
		const float PanelW = 430.f;
		const float PanelX = W - 468.f;
		const float PanelY = 154.f;
		const float PanelH = FMath::Clamp(H - PanelY - 54.f, 360.f, 690.f);
		const FLinearColor Panel(0.010f, 0.018f, 0.022f, 0.94f);
		const FLinearColor PanelSoft(0.020f, 0.036f, 0.042f, 0.90f);
		const FLinearColor Gold(0.88f, 0.70f, 0.26f, 1.f);
		const FLinearColor Text(0.88f, 0.93f, 0.94f, 1.f);
		const FLinearColor Muted(0.55f, 0.66f, 0.68f, 1.f);
		const FLinearColor Disabled(0.18f, 0.22f, 0.23f, 0.88f);

		HUD->DrawRect(Panel, PanelX, PanelY, PanelW, PanelH);
		HUD->DrawRect(FLinearColor(0.50f, 0.40f, 0.18f, 0.90f), PanelX, PanelY, PanelW, 3.f);
		HUD->DrawRect(PanelSoft, PanelX + 14.f, PanelY + 14.f, PanelW - 28.f, 62.f);
		HUD->DrawRect(FLinearColor(0.06f, 0.09f, 0.095f, 0.95f), PanelX + PanelW - 38.f, PanelY + 14.f, 24.f, 24.f);
		HUD->DrawText(TEXT("X"), Gold, PanelX + PanelW - 32.f, PanelY + 17.f, Font, 0.74f);

		float Y = PanelY + 22.f;
		const FString ModeLabel = bCityMode ? TEXT("CIUDAD / ASENTAMIENTO") : TEXT("PROVINCIA / DEPARTAMENTO");
		HUD->DrawText(ModeLabel, Gold, PanelX + 24.f, Y, SmallFont, 0.80f);
		Y += 19.f;
		HUD->DrawText(ShortenForPanel(Entry.Name, 30), Text, PanelX + 24.f, Y, Font, 1.12f);
		Y += 24.f;
		const FString Subtitle = bCityMode
			? FString::Printf(TEXT("%s  |  %s"), *Entry.Country, *Entry.TypeLabel)
			: FString::Printf(TEXT("%s  |  %s"), *Entry.Country, *Entry.TypeLabel);
		HUD->DrawText(ShortenForPanel(Subtitle, 48), Muted, PanelX + 24.f, Y, SmallFont, 0.78f);

		Y = PanelY + 92.f;
		const float MetricW = (PanelW - 58.f) * 0.5f;
		DrawMetric(HUD, SmallFont, bCityMode ? TEXT("Territorio") : TEXT("Capital / principal"),
			bCityMode ? Entry.TerritoryName : Entry.CapitalOrMainCity, PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, bCityMode ? TEXT("Puerto") : TEXT("Control"),
			bCityMode ? Entry.PortStatus : Entry.Controller, PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Poblacion"), Entry.Population, PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Infraestructura"), Entry.Infrastructure, PanelX + 30.f + MetricW, Y, MetricW);
		Y += 58.f;

		DrawSectionTitle(HUD, SmallFont, TEXT("IMPORTANCIA ESTRATEGICA"), PanelX + 18.f, Y, Gold);
		HUD->DrawText(ShortenForPanel(Entry.StrategicImportance, 58), Text, PanelX + 18.f, Y, SmallFont, 0.78f);
		Y += 25.f;

		DrawSectionTitle(HUD, SmallFont, bCityMode ? TEXT("RECURSOS CONECTADOS") : TEXT("RECURSOS PRINCIPALES"), PanelX + 18.f, Y, Gold);
		DrawTagRow(HUD, SmallFont, Entry.Resources, PanelX + 18.f, Y, PanelW - 36.f, FLinearColor(0.12f, 0.17f, 0.16f, 0.92f), Text);

		const FWLCampaignBuildingSlotCatalog& SlotCatalog = GetCampaignHudBuildingSlotCatalog();
		const EWLCampaignBuildingPanelContext SlotContext = FWLCampaignBuildingSlotRules::ContextFromCityMode(bCityMode);
		const TArray<FString>& Slots = bCityMode ? Entry.UrbanSlots : Entry.BuildingSlots;
		DrawCampaignBuildingSlotCards(HUD, SmallFont, PC, SlotCatalog, SlotContext, Slots, PanelX, PanelY, PanelW, Gold, Text, Muted);

		if (!bCityMode)
		{
			Y = FMath::Max(Y, GetCampaignHudSlotStartY(PanelY) - 63.f);
			DrawSectionTitle(HUD, SmallFont, TEXT("PUERTOS / CIUDADES"), PanelX + 18.f, Y, Gold);
			HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Puertos: %s"), *JoinDisplayList(Entry.Ports, TEXT("No aplica"))), 58),
				Muted, PanelX + 18.f, Y, SmallFont, 0.75f);
			Y += 18.f;
			HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Ciudades: %s"), *JoinDisplayList(Entry.Cities)), 58),
				Muted, PanelX + 18.f, Y, SmallFont, 0.75f);
			Y += 26.f;
		}

		DrawCampaignBuildingDetail(HUD, SmallFont, PC, SlotCatalog, SlotContext, Slots, PanelX, PanelY, PanelW, PanelH, Gold, Text, Muted, Disabled);

		HUD->DrawText(ShortenForPanel(Entry.DetailLevel, 46), Muted, PanelX + 18.f, PanelY + PanelH - 24.f, SmallFont, 0.70f);
	}

	void DrawCampaignForcePanel(
		AWLCampaignHUD* HUD,
		UCanvas* Canvas,
		UFont* Font,
		UFont* SmallFont,
		const AWLCampaignPlayerController* PC)
	{
		if (!HUD || !Canvas || !PC || PC->GetCampaignSelectionKind() != EWLCampaignSelectionKind::Force)
		{
			return;
		}

		const float W = Canvas->ClipX;
		const float H = Canvas->ClipY;
		const float PanelW = 430.f;
		const float PanelX = W - 468.f;
		const float PanelY = 154.f;
		const float PanelH = FMath::Clamp(H - PanelY - 54.f, 360.f, 690.f);
		const FLinearColor Panel(0.010f, 0.018f, 0.022f, 0.94f);
		const FLinearColor PanelSoft(0.020f, 0.036f, 0.042f, 0.90f);
		const FLinearColor Gold(0.88f, 0.70f, 0.26f, 1.f);
		const FLinearColor Text(0.88f, 0.93f, 0.94f, 1.f);
		const FLinearColor Muted(0.55f, 0.66f, 0.68f, 1.f);
		const FLinearColor Disabled(0.18f, 0.22f, 0.23f, 0.88f);

		HUD->DrawRect(Panel, PanelX, PanelY, PanelW, PanelH);
		HUD->DrawRect(FLinearColor(0.50f, 0.40f, 0.18f, 0.90f), PanelX, PanelY, PanelW, 3.f);
		HUD->DrawRect(PanelSoft, PanelX + 14.f, PanelY + 14.f, PanelW - 28.f, 68.f);
		HUD->DrawRect(FLinearColor(0.06f, 0.09f, 0.095f, 0.95f), PanelX + PanelW - 38.f, PanelY + 14.f, 24.f, 24.f);
		HUD->DrawText(TEXT("X"), Gold, PanelX + PanelW - 32.f, PanelY + 17.f, Font, 0.74f);

		float Y = PanelY + 22.f;
		HUD->DrawText(TEXT("FUERZA MILITAR PLACEHOLDER"), Gold, PanelX + 24.f, Y, SmallFont, 0.78f);
		Y += 19.f;
		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceName(), 30), Text, PanelX + 24.f, Y, Font, 1.08f);
		Y += 24.f;
		const FString Subtitle = FString::Printf(TEXT("%s  |  %s"),
			*PC->GetSelectedForceCountryName(),
			*PC->GetSelectedForceType());
		HUD->DrawText(ShortenForPanel(Subtitle, 48), Muted, PanelX + 24.f, Y, SmallFont, 0.78f);

		Y = PanelY + 98.f;
		const float MetricW = (PanelW - 58.f) * 0.5f;
		DrawMetric(HUD, SmallFont, TEXT("Ubicacion"), PC->GetSelectedForceLocation(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Provincia"), PC->GetSelectedForceProvinceName(), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Ciudad cercana"), PC->GetSelectedForceNearbyCity(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Efectivos est."), FString::Printf(TEXT("%d"), PC->GetSelectedForceEstimatedStrength()), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Movilidad"), PC->GetSelectedForceMobility(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Abastecimiento"), PC->GetSelectedForceSupply(), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 50.f;
		DrawMetric(HUD, SmallFont, TEXT("Moral"), PC->GetSelectedForceMorale(), PanelX + 18.f, Y, MetricW);
		DrawMetric(HUD, SmallFont, TEXT("Estado"), PC->GetSelectedForceOperationalState(), PanelX + 30.f + MetricW, Y, MetricW);
		Y += 58.f;

		DrawSectionTitle(HUD, SmallFont, TEXT("POSTURA / ROL"), PanelX + 18.f, Y, Gold);
		HUD->DrawText(ShortenForPanel(PC->GetSelectedForcePosture(), 54), Text, PanelX + 18.f, Y, SmallFont, 0.78f);
		Y += 18.f;
		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceStrategicRole(), 58), Muted, PanelX + 18.f, Y, SmallFont, 0.72f);
		Y += 18.f;
		HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Movimiento: %s"), *PC->GetForceMovementStatus()), 58),
			Muted, PanelX + 18.f, Y, SmallFont, 0.70f);
		if (PC->IsForceMovementModeActive())
		{
			Y += 17.f;
			const FString Destination = PC->HasForceMovementDestination()
				? PC->GetForceMovementDestinationName()
				: TEXT("elige un destino resaltado");
			HUD->DrawText(ShortenForPanel(FString::Printf(TEXT("Destino: %s"), *Destination), 58),
				Text, PanelX + 18.f, Y, SmallFont, 0.70f);
		}

		const float ButtonW = (PanelW - 48.f) * 0.5f;
		const float ButtonH = 28.f;
		const float Gap = 8.f;
		const float ButtonX0 = PanelX + 18.f;
		const float ButtonX1 = ButtonX0 + ButtonW + Gap;
		const float ActionY = PanelY + 412.f;
		HUD->DrawText(TEXT("ORDEN DE MOVIMIENTO"), Gold, PanelX + 18.f, ActionY - 24.f, SmallFont, 0.78f);
		HUD->DrawRect(PC->CanSelectedForceMove() ? FLinearColor(0.50f, 0.40f, 0.18f, 0.96f) : Disabled,
			ButtonX0, ActionY, ButtonW, ButtonH);
		HUD->DrawText(PC->IsForceMovementModeActive() ? TEXT("Cambiar destino") : TEXT("Mover"),
			PC->CanSelectedForceMove() ? Text : Muted, ButtonX0 + 9.f, ActionY + 7.f, SmallFont, 0.66f);

		float DisabledStartY = ActionY + 40.f;
		if (PC->IsForceMovementModeActive())
		{
			const FString RouteText = PC->HasForceMovementDestination()
				? FString::Printf(TEXT("Ruta: %s (%d turno placeholder)"), *PC->GetForceMovementRouteSummary(), PC->GetForceMovementEstimatedTurns())
				: TEXT("Hover/click en destino valido para previsualizar ruta.");
			HUD->DrawText(ShortenForPanel(RouteText, 60), Muted, PanelX + 18.f, ActionY + 38.f, SmallFont, 0.64f);

			const float ConfirmY = ActionY + 86.f;
			HUD->DrawRect(PC->HasForceMovementDestination() ? FLinearColor(0.38f, 0.52f, 0.32f, 0.96f) : Disabled,
				ButtonX0, ConfirmY, ButtonW, ButtonH);
			HUD->DrawText(TEXT("Confirmar movimiento"),
				PC->HasForceMovementDestination() ? Text : Muted, ButtonX0 + 9.f, ConfirmY + 7.f, SmallFont, 0.64f);
			HUD->DrawRect(FLinearColor(0.36f, 0.26f, 0.22f, 0.96f), ButtonX1, ConfirmY, ButtonW, ButtonH);
			HUD->DrawText(TEXT("Cancelar"), Text, ButtonX1 + 9.f, ConfirmY + 7.f, SmallFont, 0.64f);
			DisabledStartY = ConfirmY + 40.f;
		}

		// RECLUTAMIENTO + TROPAS (solo cuando NO estas dando una orden de movimiento). El boton "Reclutar"
		// va al MISMO Y/anchura que comprueba el handler de clic en WLCampaignPlayerControllerViews.cpp
		// (DisabledStartY = ActionY+40) -> mantener en sync.
		if (!PC->IsForceMovementModeActive())
		{
			const float RecruitBtnX = PanelX + 18.f;
			const float RecruitBtnY = DisabledStartY;
			const float RecruitBtnW = PanelW - 36.f;
			const float RecruitBtnH = 26.f;
			HUD->DrawRect(FLinearColor(0.30f, 0.42f, 0.20f, 0.96f), RecruitBtnX, RecruitBtnY, RecruitBtnW, RecruitBtnH);
			const FString RecruitLabel = PC->GetSelectedForceRecruitLabel();
			HUD->DrawText(RecruitLabel.IsEmpty() ? TEXT("Reclutar") : RecruitLabel, Text, RecruitBtnX + 12.f, RecruitBtnY + 6.f, SmallFont, 0.68f);
			HUD->DrawText(ShortenForPanel(PC->GetSelectedForceRecruitStatus(), 60), Muted, PanelX + 18.f, RecruitBtnY + 32.f, SmallFont, 0.64f);

			HUD->DrawText(TEXT("TROPAS (guarnicion)"), Gold, PanelX + 18.f, RecruitBtnY + 52.f, SmallFont, 0.76f);
			const TArray<FWLCampaignForceCompositionEntry> Comp = PC->GetSelectedForceTotalComposition();
			if (Comp.Num() == 0)
			{
				HUD->DrawText(TEXT("Sin tropas aun. Recluta y avanza el mes [M]."), Muted, PanelX + 18.f, RecruitBtnY + 72.f, SmallFont, 0.62f);
			}
			const FLinearColor CardBg(0.05f, 0.085f, 0.095f, 0.96f);
			const FLinearColor CardBand(0.34f, 0.40f, 0.22f, 0.95f);
			const int32 MaxCards = FMath::Min(Comp.Num(), 6);
			for (int32 Index = 0; Index < MaxCards; ++Index)
			{
				const int32 Col = Index % 2;
				const int32 Row = Index / 2;
				const float CardX = ButtonX0 + static_cast<float>(Col) * (ButtonW + Gap);
				const float CardY = RecruitBtnY + 72.f + static_cast<float>(Row) * (ButtonH + Gap);
				HUD->DrawRect(CardBg, CardX, CardY, ButtonW, ButtonH);
				HUD->DrawRect(CardBand, CardX, CardY, 4.f, ButtonH);              // banda de color (tipo carta)
				HUD->DrawText(FString::Printf(TEXT("%d"), Comp[Index].Count), Gold, CardX + 12.f, CardY + 5.f, SmallFont, 0.88f);
				HUD->DrawText(ShortenForPanel(Comp[Index].Label, 13), Text, CardX + 52.f, CardY + 7.f, SmallFont, 0.64f);
			}
		}

		HUD->DrawText(ShortenForPanel(PC->GetSelectedForceDetailLevel(), 46), Muted, PanelX + 18.f, PanelY + PanelH - 24.f, SmallFont, 0.70f);
	}
